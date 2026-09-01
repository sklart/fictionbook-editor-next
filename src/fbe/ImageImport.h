#pragma once

#include <vector>

enum class ImageOutputFormat { Auto, Jpeg, Png };

struct ImageImportOptions
{
	ImageOutputFormat outputFormat;
	int jpegQuality;
	bool keepSupportedImages;
	bool flattenTransparentJpeg;
	ImageImportOptions() : outputFormat(ImageOutputFormat::Auto), jpegQuality(90), keepSupportedImages(true), flattenTransparentJpeg(false) {}
};

struct ImageImportResult
{
	std::vector<BYTE> data;
	CString logicalFileName;
	CString mimeType;
	UINT width;
	UINT height;
	bool converted;
	bool hasTransparency;
	ImageImportResult() : width(0), height(0), converted(false), hasTransparency(false) {}
};

// Reads and validates an image, returning FB2-safe JPEG or PNG bytes.  This
// layer deliberately has no knowledge of MSHTML, DOM, or binary-id handling.
HRESULT ImportImageForFb2(const CString& sourceFile, const ImageImportOptions& options,
	ImageImportResult& result, CString& errorMessage);

struct ImageImportFileType {
	CString displayName;
	CString wildcard;
};

std::vector<ImageImportFileType> ImageImportFileTypes();
// Localized, double-NUL-terminated Win32 open-file filter for legacy callers.
CString ImageImportFileFilter();
