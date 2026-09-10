#include "stdafx.h"
#include "DocumentSavePlan.h"

DocumentSavePlan DocumentSavePlan::Create(bool askName, bool nameValid, const DocumentLocation& location)
{
	if (askName || !nameValid || location.containerKind == DocumentContainerKind::Rar)
		return { DocumentSaveTarget::SaveAs };
	return { location.IsArchive() ? DocumentSaveTarget::CurrentArchive : DocumentSaveTarget::CurrentFile };
}
