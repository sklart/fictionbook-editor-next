#include "stdafx.h"

#include "StartupTrace.h"
#include "utils.h"
#include "..\\common\\DeploymentContext.h"
#include <algorithm>
#include <string>
#include <vector>
#include <psapi.h>
#include <thread>
#include <map>
#include <mutex>
#include <climits>
#include "ThemeManager.h"
#include "../version.h"
#include "../common/DiagnosticPrivacy.h"
#include "../common/DiagnosticLog.h"

namespace
{
    std::mutex logCleanupMutex;
    thread_local const std::atomic_bool* taskCancellation = nullptr;
    bool TaskCancelled() { return taskCancellation && taskCancellation->load(std::memory_order_relaxed); }
	struct ZipEntry { std::string name; DWORD crc; DWORD size; DWORD offset; };
	class ZipStoreWriter
	{
	public:
		ZipStoreWriter() : file(INVALID_HANDLE_VALUE) {}
		~ZipStoreWriter() { if (file != INVALID_HANDLE_VALUE) ::CloseHandle(file); }
		bool Open(const CString& path) { file = ::CreateFile(path, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL); return file != INVALID_HANDLE_VALUE; }
		bool Add(const char* name, const std::vector<BYTE>& bytes)
		{
			if (file == INVALID_HANDLE_VALUE || !ok || entries.size() >= 65535 || strlen(name) > 0xffff || bytes.size() > 0xffffffff || totalBytes + bytes.size() > 256ULL * 1024ULL * 1024ULL) return false;
			ZipEntry entry = { name, Crc32(bytes), static_cast<DWORD>(bytes.size()), Tell() };
			if (!ok) return false;
			Write32(0x04034b50); Write16(20); Write16(0x0800); Write16(0); Write16(0); Write16(0); Write32(entry.crc); Write32(entry.size); Write32(entry.size); Write16(static_cast<WORD>(entry.name.size())); Write16(0); Write(entry.name.data(), static_cast<DWORD>(entry.name.size())); if (entry.size) Write(&bytes[0], entry.size);
			entries.push_back(entry); totalBytes += bytes.size(); return ok;
		}
		bool Close()
		{
			const DWORD directory = Tell(); if (!ok) { ::CloseHandle(file); file = INVALID_HANDLE_VALUE; return false; }
			for (size_t index = 0; index < entries.size(); ++index) { const ZipEntry& entry = entries[index]; Write32(0x02014b50); Write16(20); Write16(20); Write16(0x0800); Write16(0); Write16(0); Write16(0); Write32(entry.crc); Write32(entry.size); Write32(entry.size); Write16(static_cast<WORD>(entry.name.size())); Write16(0); Write16(0); Write16(0); Write16(0); Write32(0); Write32(entry.offset); Write(entry.name.data(), static_cast<DWORD>(entry.name.size())); }
			const DWORD end = Tell(); if (!ok || end < directory) { ::CloseHandle(file); file = INVALID_HANDLE_VALUE; return false; } const DWORD size = end - directory; Write32(0x06054b50); Write16(0); Write16(0); Write16(static_cast<WORD>(entries.size())); Write16(static_cast<WORD>(entries.size())); Write32(size); Write32(directory); Write16(0);
			bool result = ok; if (!::FlushFileBuffers(file)) result = false; if (!::CloseHandle(file)) result = false; file = INVALID_HANDLE_VALUE; return result;
		}
	private:
		HANDLE file; bool ok = true; ULONGLONG totalBytes = 0; std::vector<ZipEntry> entries;
		DWORD Tell() { const DWORD result = ::SetFilePointer(file, 0, NULL, FILE_CURRENT); if (result == INVALID_SET_FILE_POINTER && ::GetLastError() != ERROR_SUCCESS) ok = false; return result; }
		void Write(const void* data, DWORD size) { const BYTE* cursor = static_cast<const BYTE*>(data);
            while (ok && size) {
                if (TaskCancelled()) { ok=false; break; }
                const DWORD chunk=(std::min)(size,65536UL); DWORD written=0;
                if (!::WriteFile(file,cursor,chunk,&written,NULL) || written!=chunk) { ok=false; break; }
                cursor+=chunk; size-=chunk;
            } }
		void Write16(WORD value) { BYTE bytes[2] = { static_cast<BYTE>(value), static_cast<BYTE>(value >> 8) }; Write(bytes, 2); }
		void Write32(DWORD value) { BYTE bytes[4] = { static_cast<BYTE>(value), static_cast<BYTE>(value >> 8), static_cast<BYTE>(value >> 16), static_cast<BYTE>(value >> 24) }; Write(bytes, 4); }
		DWORD Crc32(const std::vector<BYTE>& bytes) { DWORD crc = 0xffffffff; for (size_t index = 0; index < bytes.size(); ++index) { if ((index & 65535)==0 && TaskCancelled()) { ok=false; return 0; } crc ^= bytes[index]; for (int bit = 0; bit < 8; ++bit) crc = (crc & 1) ? (crc >> 1) ^ 0xedb88320 : crc >> 1; } return crc ^ 0xffffffff; }
	};

	bool ReadBytes(const CString& path, std::vector<BYTE>& bytes)
	{
		HANDLE file = ::CreateFile(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, NULL);
		if (file == INVALID_HANDLE_VALUE) return false;
		BY_HANDLE_FILE_INFORMATION info={};
        if(!::GetFileInformationByHandle(file,&info) || (info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))) { ::CloseHandle(file); return false; }
		LARGE_INTEGER size = {}; const bool valid = ::GetFileSizeEx(file, &size) && size.QuadPart >= 0 && size.QuadPart <= 64 * 1024 * 1024;
		if (!valid) { ::CloseHandle(file); return false; }
		bytes.resize(static_cast<size_t>(size.QuadPart)); bool result=true;
        for (size_t offset=0; offset<bytes.size();) {
            if (TaskCancelled()) { result=false; break; }
            const DWORD chunk=static_cast<DWORD>((std::min)(bytes.size()-offset,static_cast<size_t>(65536))); DWORD read=0;
            if (!::ReadFile(file,bytes.data()+offset,chunk,&read,NULL) || read!=chunk) { result=false; break; }
            offset+=read;
        }
        ::CloseHandle(file); if(!result) bytes.clear(); return result;
	}

	bool ContainsUnsafeContent(const std::vector<BYTE>& bytes)
	{
        if(TaskCancelled()) return true;
		CString text;
        if (bytes.size() >= 2 && bytes[0] == 0xff && bytes[1] == 0xfe && bytes.size() % 2) return true;
		if (bytes.size() >= 2 && bytes[0] == 0xff && bytes[1] == 0xfe) text.SetString(reinterpret_cast<const wchar_t*>(&bytes[2]), static_cast<int>((bytes.size() - 2) / 2));
		else if (bytes.size() >= 2 && bytes[0] == 0xfe && bytes[1] == 0xff) { for (size_t index = 2; index + 1 < bytes.size(); index += 2) text += static_cast<wchar_t>((bytes[index] << 8) | bytes[index + 1]); }
		else { const int chars = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.empty() ? "" : reinterpret_cast<const char*>(&bytes[0]), static_cast<int>(bytes.size()), NULL, 0); if (!chars && !bytes.empty()) return true; if (chars) { ::MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&bytes[0]), static_cast<int>(bytes.size()), text.GetBuffer(chars), chars); text.ReleaseBuffer(chars); } }
		for(int i=0;i<text.GetLength();++i) if(text[i]==0) return true;
        CString lower(text); lower.MakeLower();
        if (text.Find(L'<') >= 0 || text.Find(L'>') >= 0) return true;
		if (lower.Find(L"book_private_") >= 0 || lower.Find(L"file://") >= 0 || lower.Find(L"data:") >= 0 || lower.Find(L"<html") >= 0 || lower.Find(L"<?xml") >= 0 || lower.Find(L"\\\\") >= 0) return true;
		for (int index = 0; index + 2 < text.GetLength(); ++index) if (((text[index] >= L'A' && text[index] <= L'Z') || (text[index] >= L'a' && text[index] <= L'z')) && text[index + 1] == L':' && (text[index + 2] == L'\\' || text[index + 2] == L'/')) return true;
		int run = 0; for (int index = 0; index < text.GetLength(); ++index) { const wchar_t c = text[index]; const bool base64 = (c >= L'A' && c <= L'Z') || (c >= L'a' && c <= L'z') || (c >= L'0' && c <= L'9') || c == L'+' || c == L'/' || c == L'='; run = base64 ? run + 1 : 0; if (run >= 128) return true; }
		return false;
	}

	std::vector<BYTE> Utf8(const CString& value)
	{
		const int count = ::WideCharToMultiByte(CP_UTF8, 0, value, value.GetLength(), NULL, 0, NULL, NULL); std::vector<BYTE> bytes(count); if (count) ::WideCharToMultiByte(CP_UTF8, 0, value, value.GetLength(), reinterpret_cast<char*>(&bytes[0]), count, NULL, NULL); return bytes;
	}

	bool SessionPartNumber(const CString& name, const CString& sessionStem, unsigned int& part)
	{
		part = 0;
		if (name.CompareNoCase(sessionStem + L".log") == 0) return true;
		const CString prefix(sessionStem + L"-part");
		if (name.Left(prefix.GetLength()).CompareNoCase(prefix) != 0 || name.Right(4).CompareNoCase(L".log") != 0) return false;
		const CString number(name.Mid(prefix.GetLength(), name.GetLength() - prefix.GetLength() - 4));
		if (number.IsEmpty()) return false;
		for (int index = 0; index < number.GetLength(); ++index) { if (number[index] < L'0' || number[index] > L'9') return false; if(part > (UINT_MAX-static_cast<unsigned int>(number[index]-L'0'))/10) return false; part = part * 10 + number[index] - L'0'; }
		return true;
	}

	struct SessionFile { CString path; unsigned int part; };
	void AddSessionFiles(const CString& currentLog, std::vector<CString>& files)
	{
		const int slash = currentLog.ReverseFind(L'\\'); if (slash < 0) return;
		const CString directory = currentLog.Left(slash); CString name = currentLog.Mid(slash + 1);
		const int part = name.Find(L"-part"); if (part >= 0) name = name.Left(part) + L".log";
		if (name.Right(4).CompareNoCase(L".log") != 0) return;
		const CString sessionStem = name.Left(name.GetLength() - 4);
		std::vector<SessionFile> selected;
		WIN32_FIND_DATA find = {}; HANDLE search = ::FindFirstFile(directory + L"\\fbe-trace-*.log", &find); if (search == INVALID_HANDLE_VALUE) return;
		do { if(TaskCancelled()) break; unsigned int segment = 0; if (!(find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && SessionPartNumber(find.cFileName, sessionStem, segment)) { SessionFile file = { directory + L"\\" + find.cFileName, segment }; selected.push_back(file); } } while (::FindNextFile(search, &find)); ::FindClose(search);
		std::sort(selected.begin(), selected.end(), [](const SessionFile& left, const SessionFile& right) { return left.part < right.part; });
		for (size_t index = 0; index < selected.size(); ++index) files.push_back(selected[index].path);
	}


    CString Decode(const std::vector<BYTE>& bytes)
    {
        CString text;
        if (bytes.size() >= 2 && bytes[0] == 0xff && bytes[1] == 0xfe) {
            if (bytes.size() % 2) return CString();
            for (size_t i = 2; i + 1 < bytes.size(); i += 2) text += static_cast<wchar_t>(bytes[i] | (bytes[i + 1] << 8));
        } else if (!bytes.empty()) {
            const int count = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, reinterpret_cast<const char*>(bytes.data()), static_cast<int>(bytes.size()), NULL, 0);
            if (!count) return CString();
            ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, reinterpret_cast<const char*>(bytes.data()), static_cast<int>(bytes.size()), text.GetBuffer(count), count); text.ReleaseBuffer(count);
        }
        return text;
    }
    CString Field(const CString& text, const CString& key)
    {
        CString result; int position = 0;
        while (position >= 0) {
            CString line = text.Tokenize(L"\n", position); line.Trim();
            if (line.Left(key.GetLength() + 1) == key + L"=") result = line.Mid(key.GetLength() + 1);
        }
        return result;
    }
    CString LogSessionId(const CString& text)
    {
        const int marker = text.Find(L"session_id="); if (marker < 0) return CString();
        CString id = text.Mid(marker + 11); const int end = id.FindOneOf(L"; \r\n"); if (end >= 0) id = id.Left(end);
        return id;
    }
    CString ReadText(const CString& path) { std::vector<BYTE> bytes; return ReadBytes(path, bytes) ? Decode(bytes) : CString(); }
    CString FileSessionStarted(const CString& name,int offset) {
        SYSTEMTIME local={},utc={}; local.wYear=static_cast<WORD>(_wtoi(name.Mid(offset,4))); local.wMonth=static_cast<WORD>(_wtoi(name.Mid(offset+4,2))); local.wDay=static_cast<WORD>(_wtoi(name.Mid(offset+6,2)));
        local.wHour=static_cast<WORD>(_wtoi(name.Mid(offset+9,2))); local.wMinute=static_cast<WORD>(_wtoi(name.Mid(offset+11,2))); local.wSecond=static_cast<WORD>(_wtoi(name.Mid(offset+13,2))); local.wMilliseconds=static_cast<WORD>(_wtoi(name.Mid(offset+16,3)));
        if(!::TzSpecificLocalTimeToSystemTime(NULL,&local,&utc)) return CString();
        CString value; value.Format(L"%04u-%02u-%02uT%02u:%02u:%02u.%03uZ",utc.wYear,utc.wMonth,utc.wDay,utc.wHour,utc.wMinute,utc.wSecond,utc.wMilliseconds); return value;
    }
    bool ValidSessionId(const CString& id) {
        if(id.GetLength()!=36) return false;
        for(int i=0;i<36;++i) {
            if(i==8 || i==13 || i==18 || i==23) { if(id[i]!=L'-') return false; }
            else if(!iswxdigit(id[i])) return false;
        }
        return true;
    }
    bool LegacyTraceName(const CString& name) {
        if(name.GetLength()<38 || name.Right(4)!=L".log" || name.Left(10)!=L"fbe-trace-" || name[18]!=L'-' || name[25]!=L'-' || name.Mid(29,4)!=L"-pid") return false;
        for(int i=10;i<29;++i) if(i!=18 && i!=25 && !(name[i]>=L'0' && name[i]<=L'9')) return false;
        CString rest=name.Mid(33,name.GetLength()-37); int p=0; unsigned int fields=0;
        while(p>=0) { CString value=rest.Tokenize(L"-",p); if(value.IsEmpty()) continue; ++fields; for(int i=0;i<value.GetLength();++i) if(value[i]<L'0' || value[i]>L'9') return false; }
        return fields>=1 && fields<=2;
    }
    CString ReadLogHeader(const CString& path) {
        HANDLE file=::CreateFile(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
        if(file==INVALID_HANDLE_VALUE) return CString();
        std::vector<BYTE> bytes(4096); DWORD count=0; const bool ok=::ReadFile(file,bytes.data(),4096,&count,NULL)!=FALSE; ::CloseHandle(file);
        if(!ok) return CString(); bytes.resize(count);
        // UTF-8 may end mid-codepoint; only the complete first record is needed.
        auto newline=std::find(bytes.begin(),bytes.end(),static_cast<BYTE>('\n')); if(newline!=bytes.end()) bytes.erase(newline+1,bytes.end());
        return Decode(bytes);
    }
#include "diagnostics/CrashCatalogIndex.inl"

	CString ExtractCategoryLines(const CString& trace, const wchar_t* category)
	{
		CString result; int position = 0; while (position >= 0) { CString line = trace.Tokenize(L"\n", position); if (line.Find(CString(L"category=") + category + L";") >= 0) result += line + L"\r\n"; } return result;
	}
}

namespace
{
    CString Leaf(const CString& path) { return path.Mid(path.ReverseFind(L'\\') + 1); }
    bool Excluded(const std::vector<CString>& excluded, const CString& name) { return std::find(excluded.begin(), excluded.end(), name) != excluded.end(); }
    CString JsonString(const CString& value) {
        CString out(L"\"");
        for (int i = 0; i < value.GetLength(); ++i) {
            const wchar_t c = value[i];
            if (c == L'"' || c == L'\\') { out += L'\\'; out += c; }
            else if (c < 32) { CString escape; escape.Format(L"\\u%04x", c); out += escape; }
            else out += c;
        }
        return out + L"\"";
    }
    CString UtcNow() {
        SYSTEMTIME now = {}; ::GetSystemTime(&now); CString value;
        value.Format(L"%04u-%02u-%02uT%02u:%02u:%02u.%03uZ", now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond, now.wMilliseconds); return value;
    }
    bool NumericText(const CString& value, const wchar_t* extra = L".") {
        if (value.IsEmpty() || value.GetLength() > 64) return false;
        for (int i=0; i<value.GetLength(); ++i) if (!(value[i]>=L'0' && value[i]<=L'9') && !wcschr(extra,value[i])) return false;
        return true;
    }
    bool StatisticsKey(const CString& key) {
        return key.Left(6)==L"stats-" || key==L"active-editor-mode" || key==L"operation-statistics-source";
    }
    bool StatisticsField(const CString& key, const CString& value) {
        if(value.Find(L'|')>=0) return false;
        const CString operations(L"|none|load|save|validate|script|view-switch|");
        if(key==L"operation-statistics-source") return value==L"selected-session-at-shutdown" || value==L"selected-current-session-at-preview" || value==L"historical-session-unavailable";
        if(key==L"active-editor-mode") return value==L"unknown" || value==L"body" || value==L"description" || value==L"source";
        if(key==L"stats-last-completed" || key==L"stats-in-progress") return operations.Find(L"|"+value+L"|")>=0 && !value.IsEmpty();
        if(key==L"stats-last-result") return value==L"unknown" || value==L"success" || value==L"failure";
        if(!NumericText(value,L"") || value.GetLength()>20) return false;
        if(key==L"stats-dropped" || key==L"stats-last-duration-ms" || key==L"stats-in-progress-ms") return true;
        const wchar_t* names[]={L"load",L"save",L"validate",L"script",L"view-switch"};
        const CString fields(L"|started|completed|failed|active|total-ms|last-ms|max-ms|");
        for(const auto name:names) {
            const CString prefix=CString(L"stats-")+name+L"-";
            if(key.Left(prefix.GetLength())==prefix) { const CString field=key.Mid(prefix.GetLength()); return !field.IsEmpty() && fields.Find(L"|"+field+L"|")>=0; }
        }
        return false;
    }
    bool EnvironmentSchema(const CString& text) {
        int pos=0;
        while(pos>=0) {
            CString line=text.Tokenize(L"\n",pos); line.Trim(); if(line.IsEmpty()) continue;
            int split=line.Find(L'='); if(split<0) return false;
            CString key=line.Left(split), value=line.Mid(split+1);
            if(key==L"session_id") { if(!FbeDiagnosticPrivacy::Identifier(std::wstring(value.GetString()))) return false; }
            else if(key==L"started" || key==L"ended") { if(!NumericText(value,L"-:TZ.")) return false; }
            else if(key==L"version" || key==L"fbe" || key==L"windows") { if(!NumericText(value)) return false; }
            else if(key==L"PID" || key==L"native-architecture") { if(!NumericText(value,L"")) return false; }
            else if(key==L"outcome") { if(value!=L"normal" && value!=L"unknown" && value!=L"crash") return false; }
            else if(key==L"commit") {
                CString hash(value); if(hash.Right(6)==L"-dirty") hash=hash.Left(hash.GetLength()-6);
                if(value!=L"unknown" && (hash.GetLength()<7 || !NumericText(hash,L"abcdefABCDEF"))) return false;
            }
            else if(key==L"configuration") { if(value!=L"Release" && value!=L"Debug" && value!=L"unknown") return false; }
            else if(key==L"deployment") { if(value!=L"portable" && value!=L"installed") return false; }
            else if(key==L"environment-source") { if(value!=L"selected-session-at-start" && value!=L"selected-current-session-at-preview" && value!=L"historical-session-unavailable") return false; }
            else if(StatisticsKey(key)) { if(!StatisticsField(key,value)) return false; }
            else if(key==L"metadata-status") { if(value!=L"missing") return false; }
            else if(key==L"module") {
                int fields=0, p=0;
                while(p>=0) {
                    CString field=line.Tokenize(L";",p); field.Trim(); if(field.IsEmpty()) continue; const int eq=field.Find(L'='); if(eq<0) return false;
                    CString k=field.Left(eq), v=field.Mid(eq+1); ++fields;
                    if(k==L"module") { if(CString(L"|FBE.exe|mshtml.dll|ieframe.dll|urlmon.dll|jscript.dll|msxml6.dll|Scintilla.dll|Lexilla.dll|").Find(L"|"+v+L"|")<0) return false; }
                    else if(k==L"present" || k==L"loaded") { if(v!=L"0" && v!=L"1") return false; }
                    else if(k==L"file-version") { if(v!=L"not-loaded" && v!=L"unknown" && v!=L"unavailable" && !NumericText(v)) return false; }
                    else if(k==L"architecture") { if(v!=L"x86" && v!=L"x64" && v!=L"unknown") return false; }
                    else return false;
                }
                if(fields!=5) return false;
            } else return false;
        }
        return true;
    }
    bool TraceSchema(const CString& text, const CString& sessionId) {
        if(text.IsEmpty()) return false;
        int pos=0;
        while(pos>=0) {
            CString line=text.Tokenize(L"\n",pos); line.Trim(); if(line.IsEmpty()) continue;
            if(line.GetLength()<25 || !NumericText(line.Left(23),L"-: .")) return false;
            int message=line.Find(L"; message="); if(message<0) return false;
            CString details=line.Mid(message+10);
            if(details!=CString(FbeDiagnosticPrivacy::Details(details).c_str())) return false;
            CString envelope=line.Mid(25,message-25); int p=0, fields=0;
            while(p>=0) {
                CString field=envelope.Tokenize(L";",p); field.Trim(); if(field.IsEmpty()) continue;
                const int eq=field.Find(L'='); if(eq<0) return false;
                const CString key=field.Left(eq),value=field.Mid(eq+1); ++fields;
                if(key==L"session_id") { if(value!=sessionId) return false; }
                else if(key==L"level") { if(value!=L"info" && value!=L"warning" && value!=L"error") return false; }
                else if(key==L"category" || key==L"code") { if(!FbeDiagnosticPrivacy::Identifier(std::wstring(value.GetString()))) return false; }
                else if(key==L"seq" || key==L"elapsed" || key==L"delta" || key==L"PID" || key==L"TID") { if(!FbeDiagnosticPrivacy::Number(std::wstring(value.GetString()))) return false; }
                else return false;
            }
            if(fields!=9) return false;
        }
        return true;
    }
    CString SafeCrashReport(const CString& original)
    {
        // Crash reports remain available separately. Export only typed technical values;
        // old free-form snapshots and symbol strings have no trusted payload schema.
        CString result(L"Crash report technical projection; free-text details omitted.\r\n");
        bool exceptionFound=false;
        const wchar_t* numeric[] = { L"Process ID: ", L"Crash thread ID: ", L"Exception code: ", L"Minidump error: " };
        int pos = 0;
        while (pos >= 0) {
            CString line = original.Tokenize(L"\n", pos); line.Trim();
            for (size_t i = 0; i < _countof(numeric); ++i) {
                CString key(numeric[i]);
                if (line.Left(key.GetLength()) == key && FbeDiagnosticPrivacy::Number(std::wstring(line.Mid(key.GetLength()).GetString()))) { result += line + L"\r\n"; if(i==2) exceptionFound=true; }
            }
        }
        if(!exceptionFound) return CString();
        int stackPos=0;
        while(stackPos>=0) {
            CString line=original.Tokenize(L"\n",stackPos); line.Trim();
            const int address=line.ReverseFind(L'['), end=line.ReverseFind(L']'), space=line.Find(L' ');
            if(line.Left(1)==L"#" && space>1 && address>space && end>address) {
                CString frame=line.Mid(1,space-1), pc=line.Mid(address+1,end-address-1);
                if(FbeDiagnosticPrivacy::Number(std::wstring(frame.GetString())) && FbeDiagnosticPrivacy::Number(std::wstring(pc.GetString()))) result+=L"stack-frame="+frame+L"; pc="+pc+L"\r\n";
            }
        }
        CString id(Field(original, L"session_id"));
        if (FbeDiagnosticPrivacy::Identifier(std::wstring(id.GetString()))) result += L"session_id=" + id + L"\r\n";
        return result;
    }
}

namespace {
    bool StoredReviewMatches(const CString& path,const CString& fingerprint) {
        wchar_t saved[128]={}; const CString ini=CString(DeploymentContext::SettingsDirectory().c_str())+L"diagnostics-reviewed.ini";
        ::GetPrivateProfileString(L"Reviewed",Leaf(path),L"",saved,_countof(saved),ini);
        if(fingerprint.IsEmpty()) return false;
        if(fingerprint==saved) return true;
        // Upgrade pre-change-time markers once; keep previously acknowledged reports quiet.
        const size_t length=wcslen(saved);
        if(length==59 && fingerprint.GetLength()==76 && fingerprint.Left(59)==saved) {
            return ::WritePrivateProfileString(L"Reviewed",Leaf(path),fingerprint,ini)!=FALSE;
        }
        return false;
    }
    CString CrashReviewIni() { return CString(DeploymentContext::SettingsDirectory().c_str())+L"diagnostics-reviewed.ini"; }
    CString CrashReviewKey(const StartupTrace::DiagnosticSession& session) {
        const CString name=Leaf(session.crashPath);
        DWORD pid=0; return CrashFileName(name,pid) ? name : CString();
    }
    CString CrashReviewFingerprint(const StartupTrace::DiagnosticSession& session) {
        if(session.crashPath.IsEmpty() || session.crashAmbiguous) return CString();
        HANDLE file=::CreateFile(session.crashPath,FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);
        if(file==INVALID_HANDLE_VALUE) return CString();
        BY_HANDLE_FILE_INFORMATION info={}; const bool valid=::GetFileInformationByHandle(file,&info)!=FALSE && !(info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT));
        const CString fingerprint=valid ? CrashFingerprint(info,file) : CString();
        ::CloseHandle(file); return fingerprint;
    }
}
StartupTrace::DiagnosticTaskScope::DiagnosticTaskScope(const std::atomic_bool& cancelled) : previous(taskCancellation) { taskCancellation=&cancelled; }
StartupTrace::DiagnosticTaskScope::~DiagnosticTaskScope() { taskCancellation=previous; }
bool StartupTrace::IsDiagnosticCrashReviewed(const DiagnosticSession& session) {
    const CString key=CrashReviewKey(session), fingerprint=CrashReviewFingerprint(session);
    if(key.IsEmpty() || fingerprint.IsEmpty() || fingerprint!=session.crashFingerprint) return false;
    return StoredReviewMatches(session.crashPath,fingerprint);
}
bool StartupTrace::MarkDiagnosticCrashReviewed(const DiagnosticSession& session) {
    const CString key=CrashReviewKey(session), fingerprint=CrashReviewFingerprint(session);
    if(key.IsEmpty() || fingerprint.IsEmpty() || fingerprint!=session.crashFingerprint) return false;
    return ::WritePrivateProfileString(L"Reviewed",key,fingerprint,CrashReviewIni())!=FALSE;
}
bool StartupTrace::HasUnreviewedDiagnosticCrashes(const std::vector<DiagnosticSession>& sessions) {
    return std::any_of(sessions.begin(),sessions.end(),[](const DiagnosticSession& item) {
        return item.outcome==L"crash" && !item.crashPath.IsEmpty() && !IsDiagnosticCrashReviewed(item);
    });
}

#include "diagnostics/CrashMaintenance.inl"

std::vector<StartupTrace::DiagnosticSession> StartupTrace::EnumerateDiagnosticSessions() { return EnumerateDiagnosticSessions(nullptr); }
std::vector<StartupTrace::DiagnosticSession> StartupTrace::EnumerateDiagnosticSessions(DiagnosticCatalogMetrics* metrics)
{
    if(metrics) *metrics=DiagnosticCatalogMetrics();
    std::vector<DiagnosticSession> sessions;
    const auto directories=DiagnosticDirectories();
    std::map<std::wstring,size_t> positions;
    for (const auto& directory : directories) {
    const auto crashes=BuildCrashIndex(directory,metrics);
    WIN32_FIND_DATA find = {}; HANDLE search = ::FindFirstFile(directory + L"fbe-session-*.txt", &find);
    if (search != INVALID_HANDLE_VALUE) {
        do { if(TaskCancelled()) break;
            if (find.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) continue;
            DiagnosticSession item; item.metadataPath = directory + find.cFileName;
            if(metrics) ++metrics->metadataReads;
            CString text(ReadText(item.metadataPath)); item.id = Field(text, L"session_id");
            if (!ValidSessionId(item.id) || Leaf(item.metadataPath) != L"fbe-session-" + item.id + L".txt") continue;
            item.started = Field(text, L"started"); item.ended = Field(text, L"ended"); item.version = Field(text, L"version");
            item.processId = wcstoul(Field(text, L"PID"), NULL, 10); item.outcome = Field(text, L"outcome");
            item.crashPath = FindCrashTextForSession(item, crashes, &item.crashAmbiguous); if ((!item.crashPath.IsEmpty() || item.crashAmbiguous)) item.outcome = L"crash";
            positions.emplace(std::wstring(item.id),sessions.size()); sessions.push_back(item);
        } while (::FindNextFile(search, &find)); ::FindClose(search);
    }
    search = ::FindFirstFile(directory + L"fbe-trace-*.log", &find);
    if (search != INVALID_HANDLE_VALUE) {
        do { if(TaskCancelled()) break;
            if (find.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) continue;
            CString name(find.cFileName); if (name.Find(L"-part") >= 0 || !LegacyTraceName(name)) continue;
            const CString path = directory + name, text = ReadLogHeader(path), id = LogSessionId(text);
            const auto position=positions.find(std::wstring(id)); const bool found=!id.IsEmpty() && position!=positions.end();
            if(found) sessions[position->second].logPath=path;
            if (!found && (id.IsEmpty() || ValidSessionId(id))) {
                DiagnosticSession item; item.legacy = id.IsEmpty(); item.id = item.legacy ? name.Left(name.GetLength() - 4) : id; item.logPath = path;
                if(!item.legacy) item.metadataPath=directory+L"fbe-session-"+id+L".txt";
                const int marker = name.Find(L"-pid"); if (marker < 0) continue;
                item.processId = wcstoul(name.Mid(marker + 4), NULL, 10); item.outcome = L"unknown";
                item.started = FileSessionStarted(name,10); item.version = item.legacy ? L"legacy" : L"unknown";
                item.crashPath = FindCrashTextForSession(item, crashes, &item.crashAmbiguous); if ((!item.crashPath.IsEmpty() || item.crashAmbiguous)) item.outcome = L"crash";
                positions.emplace(std::wstring(item.id),sessions.size()); sessions.push_back(item);
            }
        } while (::FindNextFile(search, &find)); ::FindClose(search);
    }
    for(const auto& report:crashes.reports) {
        if(TaskCancelled()) break;
        if(!ValidSessionId(report.sessionId) || positions.count(std::wstring(report.sessionId))) continue;
        DiagnosticSession item; item.id=report.sessionId; item.outcome=L"crash"; item.started=FileSessionStarted(Leaf(report.path),14); item.version=L"unknown";
        item.metadataPath=directory+L"fbe-session-"+item.id+L".txt";
        item.crashPath=FindCrashTextForSession(item,crashes,&item.crashAmbiguous);
        positions.emplace(std::wstring(item.id),sessions.size()); sessions.push_back(item);
    }
    }
    if (std::none_of(sessions.begin(), sessions.end(), [](const DiagnosticSession& item) { return item.id == SessionId(); }) && !SessionId().IsEmpty()) {
        DiagnosticSession item; item.id = SessionId(); item.started = SessionStarted(); item.version = FBE_VERSION_WSTRING;
        item.processId = ::GetCurrentProcessId(); item.outcome = L"unknown"; item.logPath = CurrentLogPath(); item.metadataPath=CurrentLogDirectory()+L"\\fbe-session-"+item.id+L".txt"; sessions.push_back(item);
    }
    if(TaskCancelled()) return {};
    for(auto& session:sessions) session.crashFingerprint=CrashReviewFingerprint(session);
    std::sort(sessions.begin(), sessions.end(), [](const DiagnosticSession& a, const DiagnosticSession& b) { if(a.started!=b.started) return a.started>b.started;
        if(a.processId!=b.processId) return a.processId>b.processId;
        const auto suffix=[](const CString& path) { const int marker=path.Find(L"-pid"); const int dash=marker<0 ? -1 : path.Find(L'-',marker+4); return dash<0 ? 0UL : wcstoul(path.Mid(dash+1),NULL,10); };
        return suffix(a.logPath)>suffix(b.logPath); });
    return sessions;
}

bool StartupTrace::PrepareDiagnosticPackage(const DiagnosticSession& selected, const std::vector<CString>& excluded,
    std::shared_ptr<const DiagnosticPackageSnapshot>& snapshot, CString& error)
{
    snapshot.reset(); error.Empty(); if(TaskCancelled()) { error=L"Cancelled."; return false; } Flush();
    // Resolve from the catalog again; callers cannot introduce arbitrary book paths.
    const auto catalog = EnumerateDiagnosticSessions(); DiagnosticSession session; unsigned int found = 0;
    for (const auto& item : catalog) if (item.id == selected.id) { session = item; ++found; }
    if (found != 1) { error = L"Selected session is unavailable or ambiguous."; return false; }
    auto result = std::make_shared<DiagnosticPackageSnapshot>();
    ULONGLONG snapshotBytes=0;
    auto add = [&](const CString& name, const std::vector<BYTE>& bytes, bool optional, const CString& status = L"included") {
        DiagnosticComponent component; component.name = name; component.optional = optional;
        component.status = optional && Excluded(excluded, name) ? L"excluded-by-user" : status;
        if (component.status == L"included" && ContainsUnsafeContent(bytes)) component.status = L"privacy-rejected";
        if(component.status==L"included" && snapshotBytes+bytes.size()>255ULL*1024ULL*1024ULL) component.status=L"size-limit";
        if (component.status == L"included") { component.bytes = bytes; snapshotBytes+=bytes.size(); }
        result->components.push_back(component);
    };
    const bool metadataExists=::GetFileAttributes(session.metadataPath)!=INVALID_FILE_ATTRIBUTES;
    CString environment = ReadText(session.metadataPath);
    const bool metadataReadable=!environment.IsEmpty();
    if (environment.IsEmpty()) {
        if(session.id==SessionId()) { environment=BasicEnvironmentReport(); environment.Replace(L"environment-source=selected-session-at-start",L"environment-source=selected-current-session-at-preview"); }
        else environment=L"environment-source=historical-session-unavailable\r\n";
        environment+=L"metadata-status=missing\r\n";
    }
    const bool validEnvironment=EnvironmentSchema(environment) && (!metadataExists || (metadataReadable && Field(environment,L"session_id")==session.id));
    add(L"environment-report.txt", Utf8(environment), false, metadataExists && !metadataReadable ? L"read-failed" : (validEnvironment ? L"included" : L"privacy-rejected"));
    CString statistics;
    if(session.id==SessionId()) statistics=OperationStatisticsReport();
    else {
        statistics=L"session_id="+session.id+L"\r\n";
        int p=0; bool recorded=false;
        while(p>=0) {
            CString line=environment.Tokenize(L"\n",p); line.Trim(); const int eq=line.Find(L'=');
            if(eq>=0 && StatisticsKey(line.Left(eq))) { statistics+=line+L"\r\n"; if(line.Left(eq)==L"operation-statistics-source") recorded=true; }
        }
        if(!recorded) statistics+=L"operation-statistics-source=historical-session-unavailable\r\n";
    }
    add(L"operation-statistics.txt",Utf8(statistics),true,validEnvironment && EnvironmentSchema(statistics) ? L"included" : L"privacy-rejected");
    CString modules; int modulePos=0; while(modulePos>=0) { CString line=environment.Tokenize(L"\n",modulePos); if(line.Left(7)==L"module=") modules+=line+L"\n"; }
    add(L"diagnostic-modules.txt", Utf8(modules), true, validEnvironment ? L"included" : L"privacy-rejected");
    CString traceText; std::vector<CString> logs; if (!session.logPath.IsEmpty()) AddSessionFiles(session.logPath, logs);
    for (const auto& path : logs) {
        std::vector<BYTE> bytes;
        CString status = ReadBytes(path, bytes) ? L"included" : L"read-failed";
        if (!session.legacy && LogSessionId(Decode(bytes)) != session.id) status = L"session-mismatch";
        else if (!session.legacy && !TraceSchema(Decode(bytes), session.id)) status = L"privacy-rejected";
        // Legacy free-text records cannot be certified by token scanning alone.
        if (session.legacy) status = L"legacy-untrusted-text";
        add(L"trace/" + Leaf(path), bytes, true, status);
        if(status==L"included") traceText += Decode(bytes);
    }
    if (logs.empty()) add(L"trace/absent.txt", Utf8(L"Extended trace was not recorded for this session.\r\n"), false);
    CString eventRoot = session.metadataPath.Left(session.metadataPath.ReverseFind(L'\\') + 1);
    if(eventRoot.IsEmpty()) eventRoot = CurrentLogDirectory() + L"\\";
    const CString eventPath = eventRoot + L"fbe-events-" + session.id + L".log";
    if (::GetFileAttributes(eventPath) != INVALID_FILE_ATTRIBUTES) {
        std::vector<BYTE> bytes; const bool read = ReadBytes(eventPath, bytes);
        add(L"plugin-and-error-events.txt", bytes, true, !read ? L"read-failed" : (LogSessionId(Decode(bytes)) != session.id ? L"session-mismatch" : (TraceSchema(Decode(bytes),session.id) ? L"included" : L"privacy-rejected")));
    }
    if (session.crashAmbiguous) add(L"crash/latest-crash-report.txt", {}, true, L"ambiguous-crash-match");
    if (!session.crashPath.IsEmpty()) {
        CString text = ReadText(session.crashPath);
        const CString safe=SafeCrashReport(text);
        add(L"crash/latest-crash-report.txt", Utf8(safe), true, text.IsEmpty() ? L"read-failed" : (safe.IsEmpty() ? L"damaged-crash-report" : L"included"));
    }
    if(session.id==SessionId()) {
        PROCESS_MEMORY_COUNTERS memory={}; memory.cb=sizeof(memory); const bool memoryAvailable=::GetProcessMemoryInfo(::GetCurrentProcess(),&memory,sizeof(memory))!=FALSE;
        HDC dc=::GetDC(NULL); const int dpi=dc ? ::GetDeviceCaps(dc,LOGPIXELSX) : 0; if(dc) ::ReleaseDC(NULL,dc);
        CrashTraceSnapshot trace={}; const bool available=TryGetCrashTraceSnapshot(trace);
        CString state; state.Format(L"source=selected-current-session-at-preview\r\nsession_id=%s\r\ndpi-aware=%u\r\ndpi=%d\r\ninterface-theme=%d\r\nmemory-available=%u\r\nworking-set-bytes=%llu\r\nprivate-pagefile-bytes=%llu\r\ntrace-snapshot-available=%u\r\n",
            (LPCWSTR)session.id,::IsProcessDPIAware()?1:0,dpi,static_cast<int>(ThemeManager::GetSelectedTheme()),memoryAvailable?1:0,static_cast<ULONGLONG>(memory.WorkingSetSize),static_cast<ULONGLONG>(memory.PagefileUsage),available?1:0);
        const auto code=[](const wchar_t* value) { return FbeDiagnosticPrivacy::Identifier(value) ? CString(value) : CString(L"unknown"); };
        state+=L"last-event-code="+code(trace.lastEventCode)+L"\r\nlast-document-code="+code(trace.lastDocumentStage)+L"\r\nlast-script-operation-code="+code(trace.lastScriptOperationStage)+L"\r\n";
        state+=L"active-editor-mode="+ActiveEditorModeName()+L"\r\n";
        add(L"runtime-state.txt",Utf8(state),true);
    }
    add(L"fbelib-report.txt", Utf8(ExtractCategoryLines(traceText,L"typelib")), true);
    bool valid = true; ULONGLONG total = 0; CString filesJson, omittedJson, listing;
    for (const auto& item : result->components) {
        if (item.status == L"included") {
            CString size; size.Format(L"%llu", static_cast<ULONGLONG>(item.bytes.size()));
            if (!filesJson.IsEmpty()) filesJson += L",";
            filesJson += L"{\"name\":" + JsonString(item.name) + L",\"size\":" + size + L"}";
            listing += item.name + L"=" + size + L"\r\n"; total += item.bytes.size();
        } else {
            if (!omittedJson.IsEmpty()) omittedJson += L",";
            omittedJson += L"{\"name\":" + JsonString(item.name) + L",\"reason\":" + JsonString(item.status) + L"}";
            listing += L"omitted=" + item.name + L"; " + item.status + L"\r\n";
            if (!item.optional) valid = false;
        }
    }
    CString manifest, json;
    ULONGLONG textSize=0, jsonSize=0;
    const CString created=UtcNow();
    // Include both manifests in their inventories; size digit counts converge quickly.
    for(unsigned int pass=0; pass<8; ++pass) {
        CString ownText; ownText.Format(L"package-manifest.txt=%llu\r\nmanifest.json=%llu\r\n",textSize,jsonSize);
        CString ownJson; ownJson.Format(L"{\"name\":\"package-manifest.txt\",\"size\":%llu},{\"name\":\"manifest.json\",\"size\":%llu}",textSize,jsonSize);
        manifest = L"FBE Next diagnostic package\r\nformat-version=2\r\nsession_id=" + session.id + L"\r\n";
        manifest += L"session-match=" + CString(session.legacy ? L"legacy-PID-time-incomplete" : L"session-id") + L"\r\n";
        manifest += L"crash-report-included=" + CString(std::any_of(result->components.begin(),result->components.end(),[](const DiagnosticComponent& item) { return item.name==L"crash/latest-crash-report.txt" && item.status==L"included"; }) ? L"1" : L"0") + L"\r\n";
        manifest += L"privacy=checked-final-bytes; omissions-listed\r\nCrash dumps, books, settings, recovery files, scripts, images and clipboard data are excluded.\r\n" + listing + ownText;
        json = L"{\"format_version\":2,\"session_id\":" + JsonString(session.id) + L",\"version\":" + JsonString(session.version);
        json += L",\"started\":" + JsonString(session.started) + L",\"ended\":" + JsonString(session.ended) + L",\"created\":" + JsonString(created);
        json += L",\"commit\":" + JsonString(Field(environment,L"commit"));
        json += L",\"session_match\":" + JsonString(session.legacy ? L"legacy-PID-time-incomplete" : L"session-id");
        json += L",\"outcome\":" + JsonString(session.outcome) + L",\"privacy\":\"checked-final-bytes\",\"files\":[" + filesJson + (filesJson.IsEmpty()?L"":L",") + ownJson + L"],\"omitted\":[" + omittedJson + L"]}\r\n";
        const ULONGLONG nextText=Utf8(manifest).size(), nextJson=Utf8(json).size();
        if(nextText==textSize && nextJson==jsonSize) break;
        textSize=nextText; jsonSize=nextJson;
    }
    add(L"package-manifest.txt", Utf8(manifest), false); add(L"manifest.json", Utf8(json), false);
    for (const auto& item : result->components) if (!item.optional && item.status != L"included") valid = false;
    result->valid = valid && total <= 255ULL * 1024ULL * 1024ULL;
    if(TaskCancelled()) { error=L"Cancelled."; return false; }
    snapshot = result;
    if (!result->valid) { error = L"Privacy scan rejected mandatory content or package size limit exceeded."; return false; }
    return true;
}

bool StartupTrace::SaveDiagnosticPackage(const DiagnosticPackageSnapshot& snapshot, const CString& path, CString& error)
{
    error.Empty(); if(TaskCancelled()) { error=L"Cancelled."; return false; } if (!snapshot.Valid()) { error = L"Unverified snapshot."; return false; }
    for (const auto& item : snapshot.Components()) if (item.status == L"included" && ContainsUnsafeContent(item.bytes)) { error = L"Privacy scan rejected final bytes."; return false; }
    bool success = false;
    {
        if(TaskCancelled()) { error=L"Cancelled."; return false; }
        ZipStoreWriter zip;
        if (!zip.Open(path)) { error = L"Could not create the diagnostic package (choose a new file name)."; return false; }
        bool added = true;
        for (const auto& item : snapshot.Components()) if (added && item.status == L"included") {
            CStringA name(CW2A(item.name, CP_UTF8)); added = zip.Add(name, item.bytes);
        }
        const bool closed = zip.Close(); success = added && closed && !TaskCancelled();
    }
    if (!success) { ::DeleteFile(path); error = L"Could not write the diagnostic package."; }
    return success;
}

bool StartupTrace::SaveDiagnosticPackageUnique(const DiagnosticPackageSnapshot& snapshot, const CString& requested, CString& actual, CString& error)
{
    actual.Empty(); const int slash=(std::max)(requested.ReverseFind(L'\\'),requested.ReverseFind(L'/'));
    const int dot=requested.ReverseFind(L'.'); const bool extension=dot>slash;
    const CString stem=extension ? requested.Left(dot) : requested, suffix=extension ? requested.Mid(dot) : CString(L".zip");
    for(unsigned int attempt=0; attempt<10000; ++attempt) {
        if(TaskCancelled()) { error=L"Cancelled."; return false; }
        CString path=requested;
        if(attempt) { CString number; number.Format(L" (%u)",attempt); path=stem+number+suffix; }
        if(::GetFileAttributes(path)!=INVALID_FILE_ATTRIBUTES) continue;
        if(SaveDiagnosticPackage(snapshot,path,error)) { actual=path; return true; }
        // CREATE_NEW is the final authority, including concurrent save races.
        if(::GetFileAttributes(path)==INVALID_FILE_ATTRIBUTES) return false;
    }
    error=L"No unused package name is available."; return false;
}

bool StartupTrace::CreateDiagnosticPackage(CString& packagePath, CString& error)
{
    packagePath.Empty(); const auto sessions = EnumerateDiagnosticSessions();
    for (const auto& session : sessions) if (session.id == SessionId()) {
        std::shared_ptr<const DiagnosticPackageSnapshot> snapshot;
        if (!PrepareDiagnosticPackage(session, {}, snapshot, error)) return false;
        SYSTEMTIME now = {}; ::GetLocalTime(&now); CString path;
        path.Format(L"%s\\FBE-Diagnostics-%04u%02u%02u-%02u%02u%02u-%03u.zip", (LPCWSTR)CurrentLogDirectory(), now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond, now.wMilliseconds);
        if (!SaveDiagnosticPackage(*snapshot, path, error)) return false;
        packagePath = path; return true;
    }
    error = L"Current session is unavailable."; return false;
}

#include "testing/DiagnosticPackageTest.inl"
#include "diagnostics/DiagnosticCrashDiscovery.h"
#include "testing/DiagnosticCatalogTest.inl"

StartupTrace::DiagnosticLogCleanupResult StartupTrace::CleanupDiagnosticSessions(size_t keep, bool preview)
{
    DiagnosticLogCleanupResult result;
    // Startup retention and an explicit cleanup may now overlap. Serialize their
    // destructive passes; async callers can still cancel while waiting.
    std::unique_lock<std::mutex> cleanupLock(logCleanupMutex,std::defer_lock);
    while(!cleanupLock.try_lock()) { if(TaskCancelled()) return result; ::Sleep(10); }
    if(TaskCancelled()) return result;
    const auto sessions = EnumerateDiagnosticSessions();
    for (size_t i = keep; i < sessions.size(); ++i) {
        if(TaskCancelled()) break;
        const auto& session = sessions[i];
        // Absence of a shutdown marker is not proof of a crash. Keep all explicit crash evidence.
        if (session.id == SessionId() || !session.crashPath.IsEmpty() || session.crashAmbiguous) continue;
        HANDLE process = ::OpenProcess(SYNCHRONIZE,FALSE,session.processId);
        const bool running = process ? ::WaitForSingleObject(process,0)==WAIT_TIMEOUT : ::GetLastError()==ERROR_ACCESS_DENIED; if(process) ::CloseHandle(process); if(running) continue;
        std::vector<CString> files;
        if (!session.logPath.IsEmpty()) AddSessionFiles(session.logPath, files);
        if (!session.metadataPath.IsEmpty()) files.push_back(session.metadataPath);
        const CString events = session.metadataPath.Left(session.metadataPath.ReverseFind(L'\\') + 1) + L"fbe-events-" + session.id + L".log";
        if (!session.legacy && ::GetFileAttributes(events) != INVALID_FILE_ATTRIBUTES) files.push_back(events);
        ++result.sessionsFound; bool deleted = false, failed = false;
        for (const auto& file : files) {
            if(TaskCancelled()) break;
            WIN32_FILE_ATTRIBUTE_DATA data = {};
            if (!::GetFileAttributesEx(file, GetFileExInfoStandard, &data) || (data.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))) { ++result.filesFailed; failed = true; continue; }
            if (preview || ::DeleteFile(file)) {
                ++result.filesDeleted; deleted = true;
                result.bytesDeleted += (static_cast<ULONGLONG>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
            } else { ++result.filesFailed; result.lastError = ::GetLastError(); failed = true; }
        }
        if (failed && deleted) ++result.sessionsPartiallyDeleted;
        else if (failed) ++result.sessionsFailed;
        else ++result.sessionsFullyDeleted;
    }
    return result;
}
StartupTrace::DiagnosticLogCleanupResult StartupTrace::ClearOldLogSessions() { return CleanupDiagnosticSessions(0); }
