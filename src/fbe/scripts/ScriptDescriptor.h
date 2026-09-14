#pragma once

struct ScriptDescriptor
{
	CString name;
	CString path;
	CString relativePath;
	// Persistent identity.  Unlike relativePath this survives a rename or move
	// within the Scripts tree.
	CString uid;
	CString order;
	CString id;
	CString parentId;
	bool isFolder;
	int commandId;
};
