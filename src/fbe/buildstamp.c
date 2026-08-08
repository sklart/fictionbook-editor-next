#include "../version.h"

#ifndef FBE_BUILD_COMMIT
#define FBE_BUILD_COMMIT "unknown"
#endif

#ifdef _DEBUG
#define FBE_BUILD_CONFIGURATION "Debug"
#else
#define FBE_BUILD_CONFIGURATION "Release"
#endif

const char *build_timestamp=__DATE__ " " __TIME__;
const char *build_name=FBE_BUILD_NAME;
const char *build_commit=FBE_BUILD_COMMIT;
const char *build_configuration=FBE_BUILD_CONFIGURATION;
