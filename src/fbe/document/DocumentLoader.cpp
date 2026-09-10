#include "stdafx.h"
#include "DocumentLoader.h"
#include "DocumentOpenSource.h"
#include "..\mainfrm.h"
#include "..\FBDoc.h"

DocumentOpenSource DocumentOpenSource::Normal(const CString& path)
{
	DocumentOpenSource source;
	source.location.storagePath = path;
	return source;
}

bool DocumentLoader::Load(FB::Doc& document, HWND parent, const DocumentOpenSource& source)
{
	return source.IsArchive()
		? document.Load(parent, source.location.storagePath, source.location.entryPath, source.rawBytes)
		: document.Load(parent, source.location.storagePath);
}
