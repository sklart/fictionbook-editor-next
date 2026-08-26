#pragma once

#include "stdafx.h"
#include "..\\common\\DeploymentContext.h"

// Keep release artifact selection independent from the updater UI and network
// code.  Command-line diagnostics and the updater both use this production
// selector, so a portable build cannot silently fall back to setup.exe.
struct UpdateArtifact
{
	enum class Kind { Setup, Portable };
	Kind kind;
	CString fileName;
	CString manifestUrlElement;
	CString manifestSha256Element;
};

inline UpdateArtifact SelectUpdateArtifact(
	DeploymentContext::Mode mode,
	const CString& baseVersion)
{
	const bool portable = mode == DeploymentContext::Mode::Portable;
	UpdateArtifact artifact = {};
	artifact.kind = portable ? UpdateArtifact::Kind::Portable : UpdateArtifact::Kind::Setup;
	artifact.fileName.Format(
		portable
			? L"FictionBookEditorNext-%s-win32-portable.zip"
			: L"FictionBookEditorNext-%s-win32-setup.exe",
		static_cast<const wchar_t*>(baseVersion));
	artifact.manifestUrlElement = portable ? L"PortableUrl" : L"SetupUrl";
	artifact.manifestSha256Element = portable ? L"PortableSHA256" : L"SetupSHA256";
	return artifact;
}
