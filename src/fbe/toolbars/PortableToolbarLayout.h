#pragma once

#include <atlstr.h>
#include <vector>

struct PortableToolbarItem
{
	bool separator;
	int command;
	int width;
	CString relativePath;
	CString scriptUid;
};

struct ScriptToolbarDefinition
{
	CString id;
	CString name;
	bool visible;
	std::vector<PortableToolbarItem> items;
	ScriptToolbarDefinition() : visible(true) {}
};

struct PortableToolbarLayout
{
	std::vector<PortableToolbarItem> commands;
	std::vector<PortableToolbarItem> scripts;
	std::vector<ScriptToolbarDefinition> scriptToolbars;
	bool commandToolbarPresent;
	bool scriptsToolbarPresent;
	CString lastScript;

	PortableToolbarLayout() : commandToolbarPresent(false), scriptsToolbarPresent(false) {}
};
