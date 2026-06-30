// stdafx.cpp : source file that includes just the standard includes
//	FBE.pch will be the pre-compiled header
//	stdafx.obj will contain the pre-compiled type information

#include "stdafx.h"

ATL::CImage::CInitGDIPlus ATL::CImage::s_initGDIPlus;
ATL::CImage::CDCCache ATL::CImage::s_cache;

#if (_ATL_VER < 0x0700)
#include <atlimpl.cpp>
#endif //(_ATL_VER < 0x0700)
