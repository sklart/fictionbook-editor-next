#pragma once

// Immutable compiled patterns are safe to share between searches. Match data
// and contexts intentionally remain per-search in RegexBackendPcre2.cpp.
#include <atlstr.h>
#include <atlexcept.h>
#include <cstring>
#include <list>
#include <new>
#include <windows.h>

#ifndef PCRE2_CODE_UNIT_WIDTH
#define PCRE2_CODE_UNIT_WIDTH 16
#endif
#ifndef PCRE2_STATIC
#define PCRE2_STATIC
#endif
#include "pcre2.h"

namespace AU {
namespace RegexPcre2 {

struct CodeCacheStatistics {
	unsigned int Hits;
	unsigned int Misses;
	unsigned int Evictions;
	unsigned int Entries;
};

class CompiledCode;

class CodeLease {
public:
	CodeLease() : m_code(NULL) {}
	~CodeLease() { Reset(); }

	pcre2_code* Get() const;
	void Reset();

private:
	CodeLease(const CodeLease&);
	CodeLease& operator=(const CodeLease&);
	void Attach(CompiledCode* code) { m_code = code; }

	CompiledCode* m_code;
	friend class CompiledCodeCache;
};

class CompiledCode {
public:
	explicit CompiledCode(pcre2_code* code) : m_code(code), m_references(1) {}
	~CompiledCode() { pcre2_code_free(m_code); }

	void AddRef() { InterlockedIncrement(&m_references); }
	void Release()
	{
		if (InterlockedDecrement(&m_references) == 0)
			delete this;
	}
	pcre2_code* Get() const { return m_code; }

private:
	pcre2_code* m_code;
	volatile LONG m_references;
};

class CriticalSectionGuard {
public:
	explicit CriticalSectionGuard(CRITICAL_SECTION& lock) : m_lock(lock)
	{
		EnterCriticalSection(&m_lock);
	}
	~CriticalSectionGuard()
	{
		LeaveCriticalSection(&m_lock);
	}

private:
	CriticalSectionGuard(const CriticalSectionGuard&);
	CriticalSectionGuard& operator=(const CriticalSectionGuard&);

	CRITICAL_SECTION& m_lock;
};

inline pcre2_code* CodeLease::Get() const
{
	return m_code != NULL ? m_code->Get() : NULL;
}

inline void CodeLease::Reset()
{
	if (m_code != NULL) {
		m_code->Release();
		m_code = NULL;
	}
}

class CompiledCodeCache {
public:
	explicit CompiledCodeCache(size_t capacity)
		: m_capacity(capacity), m_hits(0), m_misses(0), m_evictions(0)
	{
		InitializeCriticalSection(&m_lock);
	}

	~CompiledCodeCache()
	{
		Clear();
		DeleteCriticalSection(&m_lock);
	}

#if defined(PCRE2_CODE_CACHE_TESTING)
	void FailNextEntryAllocationForTesting()
	{
		CriticalSectionGuard guard(m_lock);
		m_failNextEntryAllocation = true;
	}
#endif

	bool Acquire(
		const CString& pattern,
		uint32_t compileOptions,
		int* errorNumber,
		PCRE2_SIZE* errorOffset,
		bool* allocationError,
		CodeLease& lease)
	{
		lease.Reset();
		if (allocationError != NULL)
			*allocationError = false;
		CriticalSectionGuard guard(m_lock);
		for (std::list<Entry>::iterator it = m_entries.begin(); it != m_entries.end(); ++it) {
			if (it->CompileOptions == compileOptions && SamePattern(it->Pattern, pattern)) {
				it->Code->AddRef();
				lease.Attach(it->Code);
				m_entries.splice(m_entries.begin(), m_entries, it);
				++m_hits;
				return true;
			}
		}

		++m_misses;
		pcre2_code* code = pcre2_compile(
			reinterpret_cast<PCRE2_SPTR>(static_cast<LPCWSTR>(pattern)),
			static_cast<PCRE2_SIZE>(pattern.GetLength()),
			compileOptions,
			errorNumber,
			errorOffset,
			NULL);
		if (code == NULL) {
			if (allocationError != NULL && errorNumber != NULL &&
				*errorNumber == PCRE2_ERROR_NOMEMORY)
				*allocationError = true;
			return false;
		}

		CompiledCode* compiledCode = new(std::nothrow) CompiledCode(code);
		if (compiledCode == NULL) {
			pcre2_code_free(code);
			if (allocationError != NULL)
				*allocationError = true;
			return false;
		}
		try {
#if defined(PCRE2_CODE_CACHE_TESTING)
			if (m_failNextEntryAllocation) {
				m_failNextEntryAllocation = false;
				throw std::bad_alloc();
			}
#endif
			m_entries.push_front(Entry(pattern, compileOptions, compiledCode));
		}
		catch (const std::bad_alloc&) {
			compiledCode->Release();
			if (allocationError != NULL)
				*allocationError = true;
			return false;
		}
		catch (const ATL::CAtlException& exception) {
			if (exception.m_hr != E_OUTOFMEMORY)
				throw;
			compiledCode->Release();
			if (allocationError != NULL)
				*allocationError = true;
			return false;
		}
		compiledCode->AddRef();
		lease.Attach(compiledCode);
		if (m_entries.size() > m_capacity) {
			CompiledCode* evicted = m_entries.back().Code;
			m_entries.pop_back();
			evicted->Release();
			++m_evictions;
		}
		return true;
	}

	CodeCacheStatistics GetStatistics()
	{
		CriticalSectionGuard guard(m_lock);
		CodeCacheStatistics statistics = {
			m_hits, m_misses, m_evictions, static_cast<unsigned int>(m_entries.size())
		};
		return statistics;
	}

	void Clear()
	{
		CriticalSectionGuard guard(m_lock);
		for (std::list<Entry>::iterator it = m_entries.begin(); it != m_entries.end(); ++it)
			it->Code->Release();
		m_entries.clear();
	}

private:
	struct Entry {
		Entry(const CString& pattern, uint32_t compileOptions, CompiledCode* code)
			: Pattern(pattern), CompileOptions(compileOptions), Code(code) {}

		CString Pattern;
		uint32_t CompileOptions;
		CompiledCode* Code;
	};

	static bool SamePattern(const CString& left, const CString& right)
	{
		const int length = left.GetLength();
		return length == right.GetLength() &&
			(length == 0 || memcmp(
				static_cast<LPCWSTR>(left),
				static_cast<LPCWSTR>(right),
				static_cast<size_t>(length) * sizeof(wchar_t)) == 0);
	}

	CompiledCodeCache(const CompiledCodeCache&);
	CompiledCodeCache& operator=(const CompiledCodeCache&);

	const size_t m_capacity;
	CRITICAL_SECTION m_lock;
	std::list<Entry> m_entries;
	unsigned int m_hits;
	unsigned int m_misses;
	unsigned int m_evictions;
#if defined(PCRE2_CODE_CACHE_TESTING)
	bool m_failNextEntryAllocation = false;
#endif
};

}
}
