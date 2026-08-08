#include <windows.h>
#include <oleauto.h>
#include <stdio.h>
#include <wchar.h>

struct Method { const wchar_t* name; MEMBERID id; INVOKEKIND kind; UINT params; VARTYPE result; VARTYPE input[3]; };
static const Method methods[] = {
	{L"BeginUndoUnit",1,INVOKE_FUNC,2,VT_VOID,{VT_DISPATCH,VT_BSTR}},{L"EndUndoUnit",2,INVOKE_FUNC,1,VT_VOID,{VT_DISPATCH}},{L"inflateBlock",3,INVOKE_PROPERTYGET,1,VT_I4,{VT_DISPATCH}},{L"inflateBlock",3,INVOKE_PROPERTYPUT,2,VT_VOID,{VT_DISPATCH,VT_I4}},{L"GenrePopup",4,INVOKE_FUNC,3,VT_BSTR,{VT_DISPATCH,VT_I4,VT_I4}},
	{L"GetStylePath",5,INVOKE_FUNC,0,VT_BSTR,{}},{L"GetBinarySize",6,INVOKE_FUNC,1,VT_INT,{VT_BSTR}},{L"InflateParagraphs",7,INVOKE_FUNC,1,VT_VOID,{VT_DISPATCH}},{L"GetUUID",8,INVOKE_FUNC,0,VT_BSTR,{}},{L"MsgBox",9,INVOKE_FUNC,1,VT_VOID,{VT_BSTR}},
	{L"AskYesNo",10,INVOKE_FUNC,1,VT_I4,{VT_BSTR}},{L"SaveBinary",11,INVOKE_FUNC,3,VT_I4,{VT_BSTR,VT_BSTR,VT_I4}},{L"GetExtendedStyle",12,INVOKE_FUNC,1,VT_I4,{VT_BSTR}},{L"DescShowElement",13,INVOKE_FUNC,2,VT_VOID,{VT_BSTR,VT_I4}},{L"DescShowMenu",14,INVOKE_FUNC,3,VT_BSTR,{VT_DISPATCH,VT_I4,VT_I4}},
	{L"IsFastMode",15,INVOKE_FUNC,0,VT_I4,{}},{L"SetStyleEx",16,INVOKE_FUNC,3,VT_VOID,{VT_DISPATCH,VT_DISPATCH,VT_BSTR}},{L"GetImageDimsByPath",17,INVOKE_FUNC,1,VT_BSTR,{VT_BSTR}},{L"GetImageDimsByData",18,INVOKE_FUNC,1,VT_BSTR,{VT_VARIANT}},{L"GetNBSP",19,INVOKE_FUNC,0,VT_BSTR,{}},
	{L"GetViewWidth",20,INVOKE_FUNC,0,VT_INT,{}},{L"GetViewHeight",21,INVOKE_FUNC,0,VT_INT,{}},{L"GetProgramVersion",22,INVOKE_FUNC,0,VT_BSTR,{}},{L"InputBox",23,INVOKE_FUNC,3,VT_BSTR,{VT_BSTR,VT_BSTR,VT_BSTR}},{L"GetModalResult",24,INVOKE_FUNC,0,VT_INT,{}},
	{L"SetStatusBarText",25,INVOKE_FUNC,1,VT_VOID,{VT_BSTR}},{L"GetDocumentFilePath",26,INVOKE_FUNC,0,VT_BSTR,{}},{L"GetDocumentFileName",27,INVOKE_FUNC,0,VT_BSTR,{}},{L"GetDocumentDirectory",28,INVOKE_FUNC,0,VT_BSTR,{}},
	{L"IsDiagnosticTraceEnabled",29,INVOKE_FUNC,0,VT_I4,{}},{L"TraceScript",30,INVOKE_FUNC,2,VT_VOID,{VT_BSTR,VT_BSTR}}
};

static bool Check(HRESULT hr, const wchar_t* operation) { if (FAILED(hr)) { fwprintf(stderr, L"%s failed: 0x%08lX\n", operation, static_cast<unsigned long>(hr)); return false; } return true; }
static VARTYPE BaseType(const TYPEDESC& value) { return value.vt == VT_PTR && value.lptdesc ? value.lptdesc->vt : value.vt; }

int wmain(int argc, wchar_t** argv)
{
	if (argc != 2) { fwprintf(stderr, L"Usage: fbe-typelib-runtime.exe <FBE.exe>\n"); return 2; }
	ITypeLib* library = NULL;
	if (!Check(::LoadTypeLibEx(argv[1], REGKIND_NONE, &library), L"LoadTypeLibEx(REGKIND_NONE)")) return 1;
	const IID externalHelper = {0x7269066E,0x2089,0x4408,{0xB3,0xF3,0xE8,0xD7,0x59,0x84,0xD5,0xA6}};
	ITypeInfo* info = NULL;
	if (!Check(library->GetTypeInfoOfGuid(externalHelper, &info), L"GetTypeInfoOfGuid(IExternalHelper)")) { library->Release(); return 1; }
	int failures = 0;
	for (UINT index = 0; index < _countof(methods); ++index)
	{
		LPOLESTR name = const_cast<LPOLESTR>(methods[index].name); DISPID actual = DISPID_UNKNOWN;
		if (FAILED(info->GetIDsOfNames(&name, 1, &actual)) || actual != methods[index].id) { fwprintf(stderr, L"DISPID mismatch: %s\n", methods[index].name); ++failures; continue; }
		TYPEATTR* attributes = NULL; info->GetTypeAttr(&attributes); bool found = false;
		for (UINT functionIndex = 0; attributes && functionIndex < attributes->cFuncs; ++functionIndex)
		{
			FUNCDESC* function = NULL; if (FAILED(info->GetFuncDesc(functionIndex, &function))) continue;
			if (function->memid == methods[index].id && function->invkind == methods[index].kind)
			{
				found = true;
				wprintf(L"method=%s; dispid=%ld; invkind=%u; params=%u; return-vt=%u\n", methods[index].name, static_cast<long>(function->memid), static_cast<unsigned>(function->invkind), function->cParams, static_cast<unsigned>(BaseType(function->elemdescFunc.tdesc)));
				if (function->cParams != methods[index].params || BaseType(function->elemdescFunc.tdesc) != methods[index].result) { fwprintf(stderr, L"Signature mismatch: %s\n", methods[index].name); ++failures; }
				for (UINT parameter = 0; parameter < function->cParams; ++parameter)
				{
					const USHORT flags = function->lprgelemdescParam[parameter].paramdesc.wParamFlags;
					wprintf(L"method=%s; param%u-vt=%u; param%u-flags=0x%04X\n", methods[index].name, parameter, static_cast<unsigned>(BaseType(function->lprgelemdescParam[parameter].tdesc)), parameter, flags);
					if (BaseType(function->lprgelemdescParam[parameter].tdesc) != methods[index].input[parameter] || !(flags & PARAMFLAG_FIN) || (flags & (PARAMFLAG_FOUT | PARAMFLAG_FRETVAL))) { fwprintf(stderr, L"Parameter contract mismatch: %s\n", methods[index].name); ++failures; }
				}
				if (function->cParamsOpt != 0) { fwprintf(stderr, L"Optional parameter mismatch: %s\n", methods[index].name); ++failures; }
			}
			info->ReleaseFuncDesc(function);
		}
		if (attributes) info->ReleaseTypeAttr(attributes);
		if (!found) { fwprintf(stderr, L"FUNCDESC missing: %s\n", methods[index].name); ++failures; }
	}
	info->Release(); library->Release();
	if (failures) return 1;
	wprintf(L"Embedded FBELib runtime FUNCDESC contract passed: %u methods.\n", static_cast<unsigned>(_countof(methods)));
	return 0;
}
