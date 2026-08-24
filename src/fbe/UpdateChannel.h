#pragma once

#include <atlstr.h>
#include "../version.h"

enum class UpdateChannel
{
	Stable,
	Prerelease
};

inline CString GetUpdateManifestUrl(UpdateChannel channel)
{
	return channel == UpdateChannel::Prerelease
		? CString(FBE_UPDATE_MANIFEST_PRERELEASE_URL)
		: CString(FBE_UPDATE_MANIFEST_STABLE_URL);
}
