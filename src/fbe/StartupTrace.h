#pragma once

namespace StartupTrace
{
	void Start();
	void Mark(const wchar_t* stage);
	void Finish();
}
