#include "../stdafx.h"
#include "SourceViewDiagnostics.h"
#include <psapi.h>

namespace FbeSourceDiagnostics
{
namespace
{
std::vector<SourceProfileSample> g_profileSamples;
}

SourceViewPhaseProfiler::SourceViewPhaseProfiler(bool enabled) :
	m_enabled(enabled), m_frequency(0), m_start(0)
{
	if (!m_enabled) return;
	LARGE_INTEGER frequency = {}; LARGE_INTEGER start = {};
	::QueryPerformanceFrequency(&frequency); ::QueryPerformanceCounter(&start);
	m_frequency = frequency.QuadPart; m_start = start.QuadPart;
	g_profileSamples.clear();
}

void SourceViewPhaseProfiler::Mark(const char* phase) const
{
	if (!m_enabled) return;
	LARGE_INTEGER now = {}; ::QueryPerformanceCounter(&now);
	SourceProfileSample sample = {};
	sample.phase = phase;
	sample.elapsedMilliseconds = (now.QuadPart - m_start) * 1000.0 / m_frequency;
	g_profileSamples.push_back(sample);
}

const std::vector<SourceProfileSample>& ProfileSamples()
{
	return g_profileSamples;
}

ProcessMemorySnapshot GetProcessMemorySnapshot()
{
	ProcessMemorySnapshot snapshot = {};
	PROCESS_MEMORY_COUNTERS_EX counters = {}; counters.cb = sizeof(counters);
	if (::GetProcessMemoryInfo(::GetCurrentProcess(),
		reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters)))
	{
		snapshot.privateBytes = counters.PrivateUsage;
		snapshot.workingSetBytes = counters.WorkingSetSize;
	}
	SYSTEM_INFO systemInfo = {}; ::GetSystemInfo(&systemInfo);
	for (BYTE* address = NULL; address < systemInfo.lpMaximumApplicationAddress; )
	{
		MEMORY_BASIC_INFORMATION memory = {};
		const SIZE_T result = ::VirtualQuery(address, &memory, sizeof(memory));
		if (result == 0) break;
		if (memory.State == MEM_COMMIT) snapshot.committedBytes += memory.RegionSize;
		else if (memory.State == MEM_RESERVE) snapshot.reservedBytes += memory.RegionSize;
		BYTE* const nextAddress = static_cast<BYTE*>(memory.BaseAddress) + memory.RegionSize;
		if (nextAddress <= address) break;
		address = nextAddress;
	}
	return snapshot;
}
}
