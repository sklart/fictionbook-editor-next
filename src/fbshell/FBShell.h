#ifndef FBSHELL_H
#define FBSHELL_H

///////////////////////////////////////////////////////////
// Misc globals
extern const wchar_t 	*FBNS;
extern const wchar_t 	*XLINKNS;
bool	StrEQ(const wchar_t *zstr,const wchar_t *wstr,int wlen);
void	NormalizeInplace(WTL::CString& s);
WTL::CString	GetAttr(MSXML2::ISAXAttributes *attr,const wchar_t *name,const wchar_t *ns=NULL);
template<class T>
extern inline HRESULT CreateObject(CComPtr<T>& ptr) {
  T	  *obj;
  HRESULT hr=T::CreateInstance(&obj);
  if (FAILED(hr))
    return hr;
  ptr=obj;
  return S_OK;
}
void	AppendText(WTL::CString& str,const TCHAR *text,int textlen);

extern CRITICAL_SECTION	g_Lock;


#endif
