#include "stdafx.h"
#include "StructuralTrace.h"

namespace FbeStructure {

namespace {

void Sanitize(wchar_t* value)
{
	for (; *value; ++value) {
		if (*value == L'\t' || *value == L'\r' || *value == L'\n') *value = L' ';
	}
}

} // namespace

StructuralTrace::StructuralTrace(const wchar_t* path, const wchar_t* operation, const wchar_t* caseName)
	: m_file(INVALID_HANDLE_VALUE), m_operation(operation), m_caseName(caseName)
{
	if (!path || !*path) return;
	m_file = ::CreateFile(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (!IsEnabled()) return;
	const wchar_t bom = 0xFEFF;
	DWORD written = 0;
	::WriteFile(m_file, &bom, sizeof(bom), &written, nullptr);
	const wchar_t* header = L"timestamp\toperation\tbackend\tcase\tphase\tevent\thresult\tdetails\r\n";
	::WriteFile(m_file, header, static_cast<DWORD>(wcslen(header) * sizeof(wchar_t)), &written, nullptr);
	::FlushFileBuffers(m_file);
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
	Write(phase, L"after", L"0x00000000", details);
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
	wchar_t line[2048] = {};
	::swprintf_s(line, _countof(line), L"%04u-%02u-%02uT%02u:%02u:%02u.%03uZ\t%s\textracted\t%s\t%s\t%s\t%s\t%s\r\n",
		now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond, now.wMilliseconds,
		m_operation ? m_operation : L"unknown", m_caseName ? m_caseName : L"unknown", phase, event, hresult, details ? details : L"");
	Sanitize(line);
	DWORD written = 0;
	::WriteFile(m_file, line, static_cast<DWORD>(wcslen(line) * sizeof(wchar_t)), &written, nullptr);
	::FlushFileBuffers(m_file);
}

} // namespace FbeStructure
