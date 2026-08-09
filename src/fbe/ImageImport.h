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
	bool converted;
	bool hasAlpha;
	ImageImportResult() : converted(false), hasAlpha(false) {}
};

// Reads and validates an image, returning FB2-safe JPEG or PNG bytes.  This
// layer deliberately has no knowledge of MSHTML, DOM, or binary-id handling.
HRESULT ImportImageForFb2(const CString& sourceFile, const ImageImportOptions& options,
	ImageImportResult& result, CString& errorMessage);
