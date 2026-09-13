#pragma once

#include <atlstr.h>
#include <vector>

namespace FbeSourceDiagnostics
{
struct SourceProfileSample
{
	CStringA phase;
	double elapsedMilliseconds;
};

class SourceViewPhaseProfiler
{
public:
	explicit SourceViewPhaseProfiler(bool enabled);
	void Mark(const char* phase) const;

private:
	bool m_enabled;
	LONGLONG m_frequency;
	LONGLONG m_start;
};

const std::vector<SourceProfileSample>& ProfileSamples();

struct ProcessMemorySnapshot
{
	SIZE_T privateBytes;
	SIZE_T workingSetBytes;
	SIZE_T committedBytes;
	SIZE_T reservedBytes;
};

ProcessMemorySnapshot GetProcessMemorySnapshot();
}
