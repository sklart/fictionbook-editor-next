#include "stdafx.h"
#include "DocumentLoader.h"
#include "../StartupTrace.h"
#include "DocumentOpenSource.h"
#include "..\apputils.h"
#include "..\FBDoc.h"

DocumentOpenSource DocumentOpenSource::Normal(const CString& path)
{
	DocumentOpenSource source;
	source.location.storagePath = path;
	return source;
}

bool DocumentLoader::Load(FB::Doc& document, HWND parent, const DocumentOpenSource& source)
{
	StartupTrace::ScopedOperation operation(StartupTrace::Operation::Load);
	const bool loaded = source.IsArchive()
		? document.Load(parent, source.location.storagePath, source.location.entryPath, source.rawBytes)
		: document.Load(parent, source.location.storagePath);
	if (loaded) operation.Succeeded();
	return loaded;
}
