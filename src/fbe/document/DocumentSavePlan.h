#pragma once

#include "DocumentLocation.h"

enum class DocumentSaveTarget
{
	CurrentFile,
	CurrentArchive,
	SaveAs
};

struct DocumentSavePlan
{
	DocumentSaveTarget target;
	static DocumentSavePlan Create(bool askName, bool nameValid, const DocumentLocation& location);
};
