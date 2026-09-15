#pragma once

#include "PortableToolbarLayout.h"

namespace PortableToolbarStore
{
struct Snapshot
{
	bool exists;
	CString text;
	Snapshot() : exists(false) {}
};

bool Load(PortableToolbarLayout& layout);
bool Save(const PortableToolbarLayout& layout);
bool CaptureSnapshot(Snapshot& snapshot);
bool RestoreSnapshot(const Snapshot& snapshot);
}
