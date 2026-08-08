#include <windows.h>
#include <oleauto.h>
#include <stdio.h>
#include <wchar.h>

struct Method { const wchar_t* name; MEMBERID id; INVOKEKIND kind; UINT params; };
static const Method methods[] = {
	{L"BeginUndoUnit",1,INVOKE_FUNC,2},{L"EndUndoUnit",2,INVOKE_FUNC,1},{L"inflateBlock",3,INVOKE_PROPERTYGET,1},{L"inflateBlock",3,INVOKE_PROPERTYPUT,2},{L"GenrePopup",4,INVOKE_FUNC,3},
	{L"GetStylePath",5,INVOKE_FUNC,0},{L"GetBinarySize",6,INVOKE_FUNC,1},{L"InflateParagraphs",7,INVOKE_FUNC,1},{L"GetUUID",8,INVOKE_FUNC,0},{L"MsgBox",9,INVOKE_FUNC,1},
	{L"AskYesNo",10,INVOKE_FUNC,1},{L"SaveBinary",11,INVOKE_FUNC,3},{L"GetExtendedStyle",12,INVOKE_FUNC,1},{L"DescShowElement",13,INVOKE_FUNC,2},{L"DescShowMenu",14,INVOKE_FUNC,3},
	{L"IsFastMode",15,INVOKE_FUNC,0},{L"SetStyleEx",16,INVOKE_FUNC,3},{L"GetImageDimsByPath",17,INVOKE_FUNC,1},{L"GetImageDimsByData",18,INVOKE_FUNC,1},{L"GetNBSP",19,INVOKE_FUNC,0},
	{L"GetViewWidth",20,INVOKE_FUNC,0},{L"GetViewHeight",21,INVOKE_FUNC,0},{L"GetProgramVersion",22,INVOKE_FUNC,0},{L"InputBox",23,INVOKE_FUNC,3},{L"GetModalResult",24,INVOKE_FUNC,0},
	{L"SetStatusBarText",25,INVOKE_FUNC,1},{L"GetDocumentFilePath",26,INVOKE_FUNC,0},{L"GetDocumentFileName",27,INVOKE_FUNC,0},{L"GetDocumentDirectory",28,INVOKE_FUNC,0},
	{L"IsDiagnosticTraceEnabled",29,INVOKE_FUNC,0},{L"TraceScript",30,INVOKE_FUNC,2}
};

static bool Check(HRESULT hr, const wchar_t* operation) { if (FAILED(hr)) { fwprintf(stderr, L"%s failed: 0x%08lX\n", operation, static_cast<unsigned long>(hr)); return false; } return true; }

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
				if (function->cParams != methods[index].params || function->elemdescFunc.tdesc.vt == VT_EMPTY) { fwprintf(stderr, L"Signature mismatch: %s\n", methods[index].name); ++failures; }
				for (UINT parameter = 0; parameter < function->cParams; ++parameter) if (!(function->lprgelemdescParam[parameter].paramdesc.wParamFlags & PARAMFLAG_FIN)) { fwprintf(stderr, L"Parameter flag mismatch: %s\n", methods[index].name); ++failures; }
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
