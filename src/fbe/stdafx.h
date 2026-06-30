// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#if !defined(AFX_STDAFX_H__149F645E_518F_4EA9_B603_63D633FFB194__INCLUDED_)
#define AFX_STDAFX_H__149F645E_518F_4EA9_B603_63D633FFB194__INCLUDED_

// check for unicode
#ifndef UNICODE
#error This program requires unicode support to run
#endif

// Change these values to use different versions
#define WINVER			0x0601
#define _WIN32_WINNT	0x0601
#define _WIN32_IE		0x0600

// Insert your headers here
//#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers

#define _ATL_FREE_THREADED

#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS	// some CString constructors will be explicit

// turns off ATL's hiding of some common and often safely ignored warning messages
#define _ATL_ALL_WARNINGS
#pragma warning(disable : 4996)

#include <atlbase.h>
#include <atlcom.h>
#include <atlwin.h>
#include <atltypes.h>
#include <atlctl.h>
#include <atlhost.h>
#include <atlstr.h>
#include <atlpath.h>
#include <atlcoll.h>
#include <atlfile.h>
#include <atlcomtime.h>
#include "atlimage.h"

#include <shellapi.h>

#pragma warning(push)
#pragma warning(disable : 4302 4838 4996)

#include <atlapp.h>

extern CAppModule _Module;

// Единая обертка для загрузки строковых ресурсов: буфер всегда остается
// корректной пустой строкой даже при ошибке загрузки ресурса.
inline int FbeLoadString(HINSTANCE instance, UINT id, wchar_t* buffer, int bufferChars)
{
	if(buffer == NULL || bufferChars <= 0)
		return 0;

	buffer[0] = L'\0';
	return ::LoadStringW(instance, id, buffer, bufferChars);
}

#define _WTL_NO_CSTRING
#define _WTL_NO_WTYPES
#include <atlmisc.h>
#include <atluser.h>

#include <atlframe.h>
#include <atlctrls.h>
#include <atldlgs.h>
#include <atlctrlw.h>
#include <atlctrlw.h>
#include <atlctrlx.h>
#include <atlsplit.h>
#include <atlddx.h>

#include <atltheme.h>

#pragma warning(pop)

// C library
#include <ctype.h>
#include <time.h>

// MSXML
#import <msxml6.dll> exclude("ISequentialStream", "_FILETIME")

// vb regexps
#import "vbscript3.tlb"

// mshtml additional includes
#include <exdispid.h>
#include <mshtmdid.h>
#include <mshtmcid.h>
#import <shdocvw.dll> no_auto_exclude rename_namespace("SHD") rename("FindText","FindTextX")
#import <mshtml.tlb> no_auto_exclude \
	rename("TranslateAccelerator","TranslateAcceleratorX") \
	rename("min","mshtml_min") \
	rename("max","mshtml_max")

// use com utils
using namespace _com_util;

// extra defines
#ifndef I_IMAGENONE
#define	I_IMAGENONE -1
#endif
#ifndef BTNS_BUTTON
#define	BTNS_BUTTON TBSTYLE_BUTTON
#endif
#ifndef BTNS_AUTOSIZE
#define BTNS_AUTOSIZE TBSTYLE_AUTOSIZE
#endif
#ifndef ODS_HOTLIGHT
#define ODS_HOTLIGHT 0x0040
#endif
#ifndef SPI_GETDROPSHADOW
#define	SPI_GETDROPSHADOW 0x1024
#endif

#define  UIS_WM_UPDATE_PROGRESS_UI	(WM_APP + 0x100)
#define  WM_RESIZE_OPENGL_WINDOW	(WM_APP + 0x101)

// scripting support
#include <activscp.h>

// XML serialization
#include "XMLSerializer/XMLSerializer.h"

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STDAFX_H__149F645E_518F_4EA9_B603_63D633FFB194__INCLUDED_)
