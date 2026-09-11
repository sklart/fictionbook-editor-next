#pragma once

#include <atlstr.h>
#include <vector>

struct PortableToolbarItem
{
	bool separator;
	int command;
	int width;
	CString relativePath;
};

struct PortableToolbarLayout
{
	std::vector<PortableToolbarItem> commands;
	std::vector<PortableToolbarItem> scripts;
	bool commandToolbarPresent;
	bool scriptsToolbarPresent;
	CString lastScript;

	PortableToolbarLayout() : commandToolbarPresent(false), scriptsToolbarPresent(false) {}
};
