#pragma once
#include "../version.h"

#define IDI_IMPORT_EPUB 101
#define IDI_IMPORTEPUBBATCH     102

// Resource identifier required by Windows shell to display VersionInfo.
// Without this define, VS_VERSION_INFO in .rc can become a named resource
// instead of RT_VERSION resource #1, and Explorer will not show file details.
#ifndef VS_VERSION_INFO
#define VS_VERSION_INFO 1
#endif


// Версия плагина.
// Если в проекте FBE есть общий version.h и нужно использовать общую версию,
// эти макросы можно переопределить через настройки Resource Compiler.
#ifndef FBE_VERSION_NUMBER
#define FBE_VERSION_NUMBER 1,0,72,0
#endif

#ifndef FBE_VERSION_STRING
#define FBE_VERSION_STRING "1.0.72.0"
#endif

// -----------------------------------------------------------------------------
// Version resource strings.
// Keep DLL and batch utility metadata separate so Explorer and diagnostics show
// the correct file identity for each binary.
// -----------------------------------------------------------------------------
#define IMEPUB_DLL_FILE_DESCRIPTION      "FictionBook Editor EPUB import plugin"
#define IMEPUB_DLL_INTERNAL_NAME         "ImportEPUB"
#define IMEPUB_DLL_ORIGINAL_FILENAME     "ImportEPUB.dll"
#define IMEPUB_DLL_PRODUCT_NAME          "FictionBook Editor EPUB import plugin"

#define IMEPUB_BATCH_FILE_DESCRIPTION    "FictionBook Editor EPUB batch import utility"
#define IMEPUB_BATCH_INTERNAL_NAME       "ImportEPUBBatch"
#define IMEPUB_BATCH_ORIGINAL_FILENAME   "ImportEPUBBatch.exe"
#define IMEPUB_BATCH_PRODUCT_NAME        "FictionBook Editor EPUB batch import utility"

#define IMEPUB_LUNASVG_FILE_DESCRIPTION  "FictionBook Editor EPUB SVG cover converter"
#define IMEPUB_LUNASVG_INTERNAL_NAME     "ImportEPUBLunaSVG"
#define IMEPUB_LUNASVG_ORIGINAL_FILENAME "ImportEPUBLunaSVG.dll"
#define IMEPUB_LUNASVG_PRODUCT_NAME      "FictionBook Editor EPUB import plugin"

#define IMEPUB_COMPANY_NAME              "sklart"
#define IMEPUB_LEGAL_COPYRIGHT           "Copyright 2026 Artem Sklyarov aka sklart"
