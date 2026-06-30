#include "stdafx.h"

static CString QuoteCommandLineArgument(const CString& argument)
{
  CString quoted(_T("\""));
  int backslashCount=0;

  for (int i=0;i<argument.GetLength();++i) {
    const TCHAR ch=argument[i];
    if (ch==_T('\\')) {
      ++backslashCount;
      continue;
    }

    if (ch==_T('"')) {
      for (int slash=0;slash<backslashCount*2+1;++slash)
        quoted+=_T('\\');
      quoted+=ch;
    } else {
      for (int slash=0;slash<backslashCount;++slash)
        quoted+=_T('\\');
      quoted+=ch;
    }
    backslashCount=0;
  }

  for (int slash=0;slash<backslashCount*2;++slash)
    quoted+=_T('\\');
  quoted+=_T('"');
  return quoted;
}

// IShellExtInit
HRESULT	ContextMenu::Initialize(LPCITEMIDLIST pidlFolder,IDataObject *obj,HKEY progid)
{
  Lock();
  m_files.RemoveAll();
  m_folders=false;
  Unlock();

  if (!obj) {
    // we call as a directory background context menu
    CString   tmp;
    TCHAR     *cp=tmp.GetBuffer(MAX_PATH);
    if (SHGetPathFromIDList(pidlFolder,cp)) {
      tmp.ReleaseBuffer();
      Lock();
      m_files.Add(tmp);
      Unlock();
      m_folders=true;
    }
    return S_OK;
  }

  HRESULT   hr;

  STGMEDIUM stg;
  FORMATETC etc={CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
  UINT	    count;

  if (FAILED(hr = obj->GetData(&etc,&stg)))
    return hr;

  count = ::DragQueryFile((HDROP)stg.hGlobal, (UINT)-1, NULL, 0);

  Lock();

  for (UINT i=0;i<count;++i) {
    UINT  len=::DragQueryFile((HDROP)stg.hGlobal, i, NULL, 0);
    if (!len)
      continue;

    CString fname;
    TCHAR   *cp=fname.GetBuffer(len+1);
    len=::DragQueryFile((HDROP)stg.hGlobal, i, cp, len+1);
    fname.ReleaseBuffer(len);

    if (fname.Right(4).CompareNoCase(_T(".fb2"))==0)
      m_files.Add(fname);
    else {
      DWORD attr=::GetFileAttributes(fname);
      if (attr!=INVALID_FILE_ATTRIBUTES && attr&FILE_ATTRIBUTE_DIRECTORY) {
	m_files.Add(fname);
	m_folders=true;
      }
    }
  }

  Unlock();

  ::ReleaseStgMedium(&stg);

  return S_OK;
}

// context menu commands
static struct {
  const wchar_t	  *verb;
  const wchar_t	  *text;
  const wchar_t	  *foldertext;
  const wchar_t	  *desc;
} g_menu_items[]={
  { L"Validate",      L"Validate",  L"Validate FictionBook Documents", L"Validate selected documents" },
};
#define	NCOMMANDS (sizeof(g_menu_items)/sizeof(g_menu_items[0]))

// IContextMenu
HRESULT	ContextMenu::QueryContextMenu(HMENU hMenu,UINT idx,UINT cmdFirst,UINT cmdLast,
				      UINT flags)
{
  if (!(flags & CMF_DEFAULTONLY) && m_files.GetSize()>0) {
    UINT  cmd;

    Lock();
    for (cmd=0;cmd<NCOMMANDS && cmd+cmdFirst<=cmdLast;++cmd)
      ::InsertMenu(hMenu,idx+cmd,MF_BYPOSITION|MF_STRING,cmdFirst+cmd,
	m_folders ? g_menu_items[cmd].foldertext : g_menu_items[cmd].text);
    Unlock();

    return MAKE_HRESULT(SEVERITY_SUCCESS, 0, (USHORT)cmd);
  }

  return MAKE_HRESULT(SEVERITY_SUCCESS, 0, (USHORT)0);
}

HRESULT	ContextMenu::GetCommandString(UINT_PTR cmd,UINT flags,UINT *,LPSTR name,UINT namelen)
{
  if (cmd >= NCOMMANDS)
    return E_INVALIDARG;

  switch (flags) {
  case GCS_HELPTEXTA:
    ::WideCharToMultiByte(CP_ACP,0,g_menu_items[cmd].desc,-1,name,namelen,NULL,NULL);
    break;
  case GCS_HELPTEXTW:
    lstrcpynW((wchar_t *)name,g_menu_items[cmd].desc,namelen);
    break;
  case GCS_VERBA:
    ::WideCharToMultiByte(CP_ACP,0,g_menu_items[cmd].verb,-1,name,namelen,NULL,NULL);
    break;
  case GCS_VERBW:
    lstrcpynW((wchar_t *)name,g_menu_items[cmd].verb,namelen);
    break;
  }

  return S_OK;
}

HRESULT	ContextMenu::InvokeCommand(LPCMINVOKECOMMANDINFO pici) {
  bool			fEx = pici->cbSize >= sizeof(CMINVOKECOMMANDINFOEX);
  bool			fUnicode = fEx && (pici->fMask & CMIC_MASK_UNICODE);

  CMINVOKECOMMANDINFOEX	*iciex=(CMINVOKECOMMANDINFOEX *)pici;

  UINT			cmd=LOWORD(iciex->lpVerb);

  if (fUnicode && HIWORD(iciex->lpVerbW)) {
    for (cmd=0;cmd<NCOMMANDS;++cmd)
      if (lstrcmpW(iciex->lpVerbW,g_menu_items[cmd].verb)==0)
	break;
  } else if (!fUnicode && HIWORD(pici->lpVerb)) {
    CString	tmp(pici->lpVerb);
    for (cmd=0;cmd<NCOMMANDS;++cmd)
      if (tmp==pici->lpVerb)
	break;
  }

  if (cmd>=NCOMMANDS)
    return E_INVALIDARG;

  Lock();

  // get program dir
  CString     tmp;
  TCHAR	      *cp=tmp.GetBuffer(MAX_PATH);
  const DWORD modulePathLength=::GetModuleFileName(_Module.GetModuleInstance(),cp,MAX_PATH);
  tmp.ReleaseBuffer(modulePathLength<MAX_PATH ? modulePathLength : 0);
  if (modulePathLength==0 || modulePathLength>=MAX_PATH) {
    const DWORD code=modulePathLength==0 ? ::GetLastError() : ERROR_INSUFFICIENT_BUFFER;
    Unlock();
    return HRESULT_FROM_WIN32(code);
  }

  int	      slash=tmp.ReverseFind(_T('\\'));
  if (slash>=0)
    tmp.Delete(slash+1,tmp.GetLength()-slash-1);

  // dispatch the verb
  if (lstrcmpW(g_menu_items[cmd].verb,L"Validate")==0) {
    tmp+=_T("FBV.exe");

    CString commandLine=QuoteCommandLineArgument(tmp);
    for (int i=0;i<m_files.GetSize();++i) {
      commandLine+=_T(' ');
      commandLine+=QuoteCommandLineArgument(m_files[i]);
    }

    PROCESS_INFORMATION	pi;
    STARTUPINFO		si;

    memset(&si,0,sizeof(si));
    si.cb=sizeof(si);
    si.wShowWindow=pici->nShow;
    si.dwFlags=STARTF_USESHOWWINDOW;

    CString workingDirectory=tmp.Left(tmp.ReverseFind(_T('\\')));
    TCHAR *buf=commandLine.GetBuffer(commandLine.GetLength()+1);

    const BOOL processCreated=::CreateProcess(
      tmp,buf,NULL,NULL,FALSE,0,NULL,workingDirectory,&si,&pi);
    const DWORD processError=processCreated ? ERROR_SUCCESS : ::GetLastError();
    commandLine.ReleaseBuffer();

    if (processCreated) {
      ::CloseHandle(pi.hProcess);
      ::CloseHandle(pi.hThread);
    } else {
      CString	msg;
      msg.Format(_T("Can't run %s: "),(const wchar_t *)tmp);
      int	len=msg.GetLength();
      TCHAR	*vp=msg.GetBuffer(len+1024);
      int fl=::FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,NULL,processError,0,vp+len,1024,NULL);
      msg.ReleaseBuffer(fl+len);
      ::MessageBox(pici->hwnd,msg,_T("Error"),MB_OK|MB_ICONERROR);
    }
  }

  Unlock();

  return S_OK;
}
