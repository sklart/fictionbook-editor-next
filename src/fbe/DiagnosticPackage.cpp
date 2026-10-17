#include "stdafx.h"

#include "StartupTrace.h"
#include "utils.h"
#include "..\\common\\DeploymentContext.h"
#include <algorithm>
#include <string>
#include <vector>
#include <psapi.h>
#include "ThemeManager.h"
#include "../version.h"
#include "../common/DiagnosticPrivacy.h"
#include "../common/DiagnosticLog.h"

namespace
{
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
		void Write(const void* data, DWORD size) { DWORD written = 0; if (!size) return; if (!::WriteFile(file, data, size, &written, NULL) || written != size) ok = false; }
		void Write16(WORD value) { BYTE bytes[2] = { static_cast<BYTE>(value), static_cast<BYTE>(value >> 8) }; Write(bytes, 2); }
		void Write32(DWORD value) { BYTE bytes[4] = { static_cast<BYTE>(value), static_cast<BYTE>(value >> 8), static_cast<BYTE>(value >> 16), static_cast<BYTE>(value >> 24) }; Write(bytes, 4); }
		static DWORD Crc32(const std::vector<BYTE>& bytes) { DWORD crc = 0xffffffff; for (size_t index = 0; index < bytes.size(); ++index) { crc ^= bytes[index]; for (int bit = 0; bit < 8; ++bit) crc = (crc & 1) ? (crc >> 1) ^ 0xedb88320 : crc >> 1; } return crc ^ 0xffffffff; }
	};

	bool ReadBytes(const CString& path, std::vector<BYTE>& bytes)
	{
		HANDLE file = ::CreateFile(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, NULL);
		if (file == INVALID_HANDLE_VALUE) return false;
		BY_HANDLE_FILE_INFORMATION info={};
        if(!::GetFileInformationByHandle(file,&info) || (info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))) { ::CloseHandle(file); return false; }
		LARGE_INTEGER size = {}; const bool valid = ::GetFileSizeEx(file, &size) && size.QuadPart >= 0 && size.QuadPart <= 64 * 1024 * 1024;
		if (!valid) { ::CloseHandle(file); return false; }
		bytes.resize(static_cast<size_t>(size.QuadPart)); DWORD read = 0; const bool result = bytes.empty() || (::ReadFile(file, &bytes[0], static_cast<DWORD>(bytes.size()), &read, NULL) && read == bytes.size()); ::CloseHandle(file); return result;
	}

	bool ContainsUnsafeContent(const std::vector<BYTE>& bytes)
	{
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
		do { unsigned int segment = 0; if (!(find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && SessionPartNumber(find.cFileName, sessionStem, segment)) { SessionFile file = { directory + L"\\" + find.cFileName, segment }; selected.push_back(file); } } while (::FindNextFile(search, &find)); ::FindClose(search);
		std::sort(selected.begin(), selected.end(), [](const SessionFile& left, const SessionFile& right) { return left.part < right.part; });
		for (size_t index = 0; index < selected.size(); ++index) files.push_back(selected[index].path);
	}

	bool CrashMatchesSession(const CString& name, DWORD processId)
	{
		const int marker = name.Find(L"-pid");
		if (marker < 0 || name.Right(4).CompareNoCase(L".txt") != 0) return false;
		const CString pidText(name.Mid(marker + 4, name.GetLength() - marker - 8));
		if (pidText.IsEmpty()) return false;
		unsigned long pid = 0; for (int index = 0; index < pidText.GetLength(); ++index) { if (pidText[index] < L'0' || pidText[index] > L'9') return false; pid = pid * 10 + pidText[index] - L'0'; }
		return pid == processId;
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
    CString FindCrashTextForSession(const StartupTrace::DiagnosticSession& session, bool* ambiguous = NULL)
    {
        CString root = session.metadataPath.IsEmpty() ? session.logPath : session.metadataPath; root = root.Left(root.ReverseFind(L'\\') + 1);
        if(root.IsEmpty()) root = StartupTrace::CurrentLogDirectory() + L"\\";
        const CString directory(root + L"Crashes\\");
        WIN32_FIND_DATA find = {}; HANDLE search = ::FindFirstFile(directory + L"FBENext-crash-*.txt", &find);
        if (search == INVALID_HANDLE_VALUE) return CString();
        CString matched; unsigned int matches = 0;
        WIN32_FILE_ATTRIBUTE_DATA traceData = {}; const bool haveTrace = ::GetFileAttributesEx(session.logPath, GetFileExInfoStandard, &traceData) != FALSE;
        const ULONGLONG sessionWindow = 30ULL * 60ULL * 10000000ULL;
        do {
            if (find.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) continue;
            const CString path = directory + find.cFileName;
            const CString id = Field(ReadText(path), L"session_id");
            bool match = !session.legacy && !id.IsEmpty() && id == session.id;
            // Never use PID/time to override an explicit, different session ID.
            if (session.legacy && id.IsEmpty() && haveTrace && CrashMatchesSession(find.cFileName, session.processId)) {
                ULARGE_INTEGER trace = {}, crash = {}; trace.LowPart = traceData.ftLastWriteTime.dwLowDateTime; trace.HighPart = traceData.ftLastWriteTime.dwHighDateTime;
                crash.LowPart = find.ftLastWriteTime.dwLowDateTime; crash.HighPart = find.ftLastWriteTime.dwHighDateTime;
                match = (trace.QuadPart > crash.QuadPart ? trace.QuadPart - crash.QuadPart : crash.QuadPart - trace.QuadPart) <= sessionWindow;
            }
            if (match) { matched = path; ++matches; }
        } while (::FindNextFile(search, &find));
        ::FindClose(search); if(ambiguous) *ambiguous = matches > 1; return matches == 1 ? matched : CString();
    }

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

std::vector<StartupTrace::DiagnosticSession> StartupTrace::EnumerateDiagnosticSessions()
{
    std::vector<DiagnosticSession> sessions;
    std::vector<CString> directories = { CString(DeploymentContext::DiagnosticsDirectory().c_str()) };
    if (DeploymentContext::CurrentMode() != DeploymentContext::Mode::Portable) {
        wchar_t temp[MAX_PATH] = {}; if (::GetTempPath(_countof(temp),temp)) directories.push_back(CString(temp) + L"FBE Next Diagnostics\\");
    }
    for (const auto& directory : directories) {
    WIN32_FIND_DATA find = {}; HANDLE search = ::FindFirstFile(directory + L"fbe-session-*.txt", &find);
    if (search != INVALID_HANDLE_VALUE) {
        do {
            if (find.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) continue;
            DiagnosticSession item; item.metadataPath = directory + find.cFileName;
            CString text(ReadText(item.metadataPath)); item.id = Field(text, L"session_id");
            if (!ValidSessionId(item.id) || Leaf(item.metadataPath) != L"fbe-session-" + item.id + L".txt") continue;
            item.started = Field(text, L"started"); item.ended = Field(text, L"ended"); item.version = Field(text, L"version");
            item.processId = wcstoul(Field(text, L"PID"), NULL, 10); item.outcome = Field(text, L"outcome");
            item.crashPath = FindCrashTextForSession(item, &item.crashAmbiguous); if ((!item.crashPath.IsEmpty() || item.crashAmbiguous)) item.outcome = L"crash";
            sessions.push_back(item);
        } while (::FindNextFile(search, &find)); ::FindClose(search);
    }
    search = ::FindFirstFile(directory + L"fbe-trace-*.log", &find);
    if (search != INVALID_HANDLE_VALUE) {
        do {
            if (find.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) continue;
            CString name(find.cFileName); if (name.Find(L"-part") >= 0 || !LegacyTraceName(name)) continue;
            const CString path = directory + name, text = ReadLogHeader(path), id = LogSessionId(text);
            bool found = false;
            for (auto& item : sessions) if (!id.IsEmpty() && item.id == id) { item.logPath = path; found = true; break; }
            if (!found && (id.IsEmpty() || ValidSessionId(id))) {
                DiagnosticSession item; item.legacy = id.IsEmpty(); item.id = item.legacy ? name.Left(name.GetLength() - 4) : id; item.logPath = path;
                if(!item.legacy) item.metadataPath=directory+L"fbe-session-"+id+L".txt";
                const int marker = name.Find(L"-pid"); if (marker < 0) continue;
                item.processId = wcstoul(name.Mid(marker + 4), NULL, 10); item.outcome = L"unknown";
                item.started = FileSessionStarted(name,10); item.version = item.legacy ? L"legacy" : L"unknown";
                item.crashPath = FindCrashTextForSession(item, &item.crashAmbiguous); if ((!item.crashPath.IsEmpty() || item.crashAmbiguous)) item.outcome = L"crash";
                sessions.push_back(item);
            }
        } while (::FindNextFile(search, &find)); ::FindClose(search);
    }
    const CString crashRoot=directory+L"Crashes\\";
    search=::FindFirstFile(crashRoot+L"FBENext-crash-*.txt",&find);
    if(search!=INVALID_HANDLE_VALUE) {
        do {
            if(find.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)) continue;
            const CString path=crashRoot+find.cFileName,text=ReadText(path),id=Field(text,L"session_id");
            if(!ValidSessionId(id) || std::any_of(sessions.begin(),sessions.end(),[&](const DiagnosticSession& item) { return item.id==id; })) continue;
            DiagnosticSession item; item.id=id; item.crashPath=path; item.outcome=L"crash"; item.started=FileSessionStarted(find.cFileName,14); item.version=L"unknown";
            // Keep the report directory for subsequent catalog resolution and export.
            item.metadataPath=directory+L"fbe-session-"+id+L".txt";
            item.crashPath=FindCrashTextForSession(item,&item.crashAmbiguous); sessions.push_back(item);
        } while(::FindNextFile(search,&find)); ::FindClose(search);
    }
    }
    if (std::none_of(sessions.begin(), sessions.end(), [](const DiagnosticSession& item) { return item.id == SessionId(); }) && !SessionId().IsEmpty()) {
        DiagnosticSession item; item.id = SessionId(); item.started = SessionStarted(); item.version = FBE_VERSION_WSTRING;
        item.processId = ::GetCurrentProcessId(); item.outcome = L"unknown"; item.logPath = CurrentLogPath(); item.metadataPath=CurrentLogDirectory()+L"\\fbe-session-"+item.id+L".txt"; sessions.push_back(item);
    }
    std::sort(sessions.begin(), sessions.end(), [](const DiagnosticSession& a, const DiagnosticSession& b) { if(a.started!=b.started) return a.started>b.started;
        if(a.processId!=b.processId) return a.processId>b.processId;
        const auto suffix=[](const CString& path) { const int marker=path.Find(L"-pid"); const int dash=marker<0 ? -1 : path.Find(L'-',marker+4); return dash<0 ? 0UL : wcstoul(path.Mid(dash+1),NULL,10); };
        return suffix(a.logPath)>suffix(b.logPath); });
    return sessions;
}

bool StartupTrace::PrepareDiagnosticPackage(const DiagnosticSession& selected, const std::vector<CString>& excluded,
    std::shared_ptr<const DiagnosticPackageSnapshot>& snapshot, CString& error)
{
    snapshot.reset(); error.Empty(); Flush();
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
    snapshot = result;
    if (!result->valid) { error = L"Privacy scan rejected mandatory content or package size limit exceeded."; return false; }
    return true;
}

bool StartupTrace::SaveDiagnosticPackage(const DiagnosticPackageSnapshot& snapshot, const CString& path, CString& error)
{
    error.Empty(); if (!snapshot.Valid()) { error = L"Unverified snapshot."; return false; }
    for (const auto& item : snapshot.Components()) if (item.status == L"included" && ContainsUnsafeContent(item.bytes)) { error = L"Privacy scan rejected final bytes."; return false; }
    bool success = false;
    {
        ZipStoreWriter zip;
        if (!zip.Open(path)) { error = L"Could not create the diagnostic package (choose a new file name)."; return false; }
        bool added = true;
        for (const auto& item : snapshot.Components()) if (added && item.status == L"included") {
            CStringA name(CW2A(item.name, CP_UTF8)); added = zip.Add(name, item.bytes);
        }
        const bool closed = zip.Close(); success = added && closed;
    }
    if (!success) { ::DeleteFile(path); error = L"Could not write the diagnostic package."; }
    return success;
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

StartupTrace::DiagnosticLogCleanupResult StartupTrace::CleanupDiagnosticSessions(size_t keep, bool preview)
{
    DiagnosticLogCleanupResult result;
    const auto sessions = EnumerateDiagnosticSessions();
    for (size_t i = keep; i < sessions.size(); ++i) {
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
