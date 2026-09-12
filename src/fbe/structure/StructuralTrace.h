#pragma once

#include <windows.h>

namespace FbeStructure {

// Test-only diagnostic sink.  The production editor supplies no trace.
class StructuralTrace
{
public:
	StructuralTrace(const wchar_t* path, const wchar_t* operation, const wchar_t* caseName);
	~StructuralTrace();

	bool IsEnabled() const { return m_file != INVALID_HANDLE_VALUE; }
	bool HasWriteFailure() const { return m_writeFailure; }
	HRESULT LastError() const { return m_lastError; }
	void Before(const wchar_t* phase, const wchar_t* details = L"");
	void After(const wchar_t* phase, const wchar_t* details = L"");
	void Hr(const wchar_t* phase, HRESULT hr, const wchar_t* details = L"");
	void Exception(const wchar_t* phase, HRESULT hr, const wchar_t* description = L"");

private:
	bool WriteRaw(const void* data, DWORD bytes);
	void Write(const wchar_t* phase, const wchar_t* event, const wchar_t* hresult, const wchar_t* details);
	HANDLE m_file;
	const wchar_t* m_operation;
	const wchar_t* m_caseName;
	bool m_writeFailure;
	HRESULT m_lastError;
};

} // namespace FbeStructure
