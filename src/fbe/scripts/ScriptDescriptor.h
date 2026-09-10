#pragma once

struct ScriptDescriptor
{
	CString name;
	CString path;
	CString relativePath;
	CString order;
	CString id;
	CString parentId;
	bool isFolder;
	int commandId;
};
