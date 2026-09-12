#include "stdafx.h"
#include "StructuralTrace.h"

namespace FbeStructure {

namespace {

void Sanitize(wchar_t* destination, size_t destinationCount, const wchar_t* value)
{
	if (!value) value = L"";
	::wcsncpy_s(destination, destinationCount, value, _TRUNCATE);
	for (wchar_t* current = destination; *current; ++current) {
		if (*current == L'\t' || *current == L'\r' || *current == L'\n') *current = L' ';
	}
}

} // namespace

StructuralTrace::StructuralTrace(const wchar_t* path, const wchar_t* operation, const wchar_t* caseName)
	: m_file(INVALID_HANDLE_VALUE), m_operation(operation), m_caseName(caseName), m_writeFailure(false), m_lastError(S_OK)
{
	if (!path || !*path) return;
	m_file = ::CreateFile(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (!IsEnabled()) { m_writeFailure = true; m_lastError = HRESULT_FROM_WIN32(::GetLastError()); return; }
	const wchar_t bom = 0xFEFF;
	WriteRaw(&bom, sizeof(bom));
	const wchar_t* header = L"timestamp\toperation\tbackend\tcase\tphase\tevent\thresult\tdetails\r\n";
	WriteRaw(header, static_cast<DWORD>(wcslen(header) * sizeof(wchar_t)));
}

StructuralTrace::~StructuralTrace()
{
	if (IsEnabled()) ::CloseHandle(m_file);
}

void StructuralTrace::Before(const wchar_t* phase, const wchar_t* details)
{
	Write(phase, L"before", L"NA", details);
}

void StructuralTrace::After(const wchar_t* phase, const wchar_t* details)
{
	wchar_t completed[1024] = {};
	::swprintf_s(completed, _countof(completed), L"completed%s%s", details && *details ? L": " : L"", details && *details ? details : L"");
	Write(phase, L"after", L"NA", completed);
}

void StructuralTrace::Hr(const wchar_t* phase, HRESULT hr, const wchar_t* details)
{
	wchar_t value[16] = {};
	::swprintf_s(value, _countof(value), L"0x%08X", static_cast<unsigned int>(hr));
	Write(phase, SUCCEEDED(hr) ? L"after" : L"failure", value, details);
}

void StructuralTrace::Exception(const wchar_t* phase, HRESULT hr, const wchar_t* description)
{
	wchar_t value[16] = {};
	::swprintf_s(value, _countof(value), L"0x%08X", static_cast<unsigned int>(hr));
	Write(phase, L"exception", value, description);
}

void StructuralTrace::Write(const wchar_t* phase, const wchar_t* event, const wchar_t* hresult, const wchar_t* details)
{
	if (!IsEnabled()) return;
	SYSTEMTIME now = {};
	::GetSystemTime(&now);
	wchar_t operation[128] = {}, caseName[128] = {}, safePhase[128] = {}, safeEvent[64] = {}, safeDetails[1024] = {};
	Sanitize(operation, _countof(operation), m_operation ? m_operation : L"unknown");
	Sanitize(caseName, _countof(caseName), m_caseName ? m_caseName : L"unknown");
	Sanitize(safePhase, _countof(safePhase), phase);
	Sanitize(safeEvent, _countof(safeEvent), event);
	Sanitize(safeDetails, _countof(safeDetails), details);
	wchar_t line[2048] = {};
	::swprintf_s(line, _countof(line), L"%04u-%02u-%02uT%02u:%02u:%02u.%03uZ\t%s\textracted\t%s\t%s\t%s\t%s\t%s\r\n",
		now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond, now.wMilliseconds,
		operation, caseName, safePhase, safeEvent, hresult, safeDetails);
	WriteRaw(line, static_cast<DWORD>(wcslen(line) * sizeof(wchar_t)));
}

bool StructuralTrace::WriteRaw(const void* data, DWORD bytes)
{
	if (!IsEnabled()) return false;
	DWORD written = 0;
	const bool wrote = ::WriteFile(m_file, data, bytes, &written, nullptr) != FALSE && written == bytes;
	const bool flushed = wrote && ::FlushFileBuffers(m_file) != FALSE;
	if (!wrote || !flushed) {
		m_writeFailure = true;
		m_lastError = HRESULT_FROM_WIN32(::GetLastError());
	}
	return wrote && flushed;
}

} // namespace FbeStructure
