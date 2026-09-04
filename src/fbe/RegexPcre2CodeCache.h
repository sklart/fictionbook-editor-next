#pragma once

// Immutable compiled patterns are safe to share between searches. Match data
// and contexts intentionally remain per-search in RegexBackendPcre2.cpp.
#include <atlstr.h>
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
		EnterCriticalSection(&m_lock);
		for (std::list<Entry>::iterator it = m_entries.begin(); it != m_entries.end(); ++it) {
			if (it->CompileOptions == compileOptions && it->Pattern == pattern) {
				it->Code->AddRef();
				lease.Attach(it->Code);
				m_entries.splice(m_entries.begin(), m_entries, it);
				++m_hits;
				LeaveCriticalSection(&m_lock);
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
			LeaveCriticalSection(&m_lock);
			return false;
		}

		CompiledCode* compiledCode = new(std::nothrow) CompiledCode(code);
		if (compiledCode == NULL) {
			pcre2_code_free(code);
			if (allocationError != NULL)
				*allocationError = true;
			LeaveCriticalSection(&m_lock);
			return false;
		}
		try {
			m_entries.push_front(Entry(pattern, compileOptions, compiledCode));
		}
		catch (const std::bad_alloc&) {
			compiledCode->Release();
			if (allocationError != NULL)
				*allocationError = true;
			LeaveCriticalSection(&m_lock);
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
		LeaveCriticalSection(&m_lock);
		return true;
	}

	CodeCacheStatistics GetStatistics()
	{
		EnterCriticalSection(&m_lock);
		CodeCacheStatistics statistics = {
			m_hits, m_misses, m_evictions, static_cast<unsigned int>(m_entries.size())
		};
		LeaveCriticalSection(&m_lock);
		return statistics;
	}

	void Clear()
	{
		EnterCriticalSection(&m_lock);
		for (std::list<Entry>::iterator it = m_entries.begin(); it != m_entries.end(); ++it)
			it->Code->Release();
		m_entries.clear();
		LeaveCriticalSection(&m_lock);
	}

private:
	struct Entry {
		Entry(const CString& pattern, uint32_t compileOptions, CompiledCode* code)
			: Pattern(pattern), CompileOptions(compileOptions), Code(code) {}

		CString Pattern;
		uint32_t CompileOptions;
		CompiledCode* Code;
	};

	CompiledCodeCache(const CompiledCodeCache&);
	CompiledCodeCache& operator=(const CompiledCodeCache&);

	const size_t m_capacity;
	CRITICAL_SECTION m_lock;
	std::list<Entry> m_entries;
	unsigned int m_hits;
	unsigned int m_misses;
	unsigned int m_evictions;
};

}
}
