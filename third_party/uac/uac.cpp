// Copyright (C) Anders Kjersem. Licensed under the zlib/libpng license, see License.txt for details.

#include "uac.h"
#include <objbase.h> // CoInitialize
using namespace NSIS;

CHECKTOKENMEMBERSHIP	_CheckTokenMembership=0;
#ifdef FEAT_CUSTOMRUNASDLG
GETUSERNAMEEX			_GetUserNameEx=0;
CREATEPROCESSWITHLOGONW	_CreateProcessWithLogonW=0;
SHGETVALUEA _SHGetValueA=0;
#endif

UAC_GLOBALS globals;
HINSTANCE g_hInst;
extra_parameters* g_pXP;

FORCEINLINE UAC_GLOBALS& G()         {return globals;}
FORCEINLINE UAC_GLOBALS_OUTER& OG()  {return G().outer;}
FORCEINLINE bool IsInnerInstance()   {return G().RunMode==RUNMODE_INNER;}
FORCEINLINE bool IsOuterInstance()   {return G().RunMode==RUNMODE_OUTER;}
FORCEINLINE HWND xxxGetOuterIPCWND() {return G().hwndOuter;}
FORCEINLINE HWND xxxGetOuterNSISMainWnd() {
	DWORD_PTR result=0;
	if (!SendMessageTimeout(xxxGetOuterIPCWND(),OWM_GETOUTERSTATE,GOSI_HWNDMAINWND,0,
		SMTO_ABORTIFHUNG|SMTO_BLOCK,5000,&result)) return NULL;
	return (HWND)result;
}

#ifdef FEAT_AUTOPAGEJUMP
#	define Outer_GetOuterNSISMainWnd() xxxGetOuterNSISMainWnd()
#	define Inner_GetOuterNSISMainWnd() xxxGetOuterNSISMainWnd()
#endif
#ifdef FEAT_AUTOPAGEJUMP
#	define Inner_GetInnerNSISMainWnd() G().hwndNSIS
#endif
#define Inner_GetOuterIPCWND xxxGetOuterIPCWND

static void FlashInstallerWindow(HWND hwnd)
{
	if (!IsWindow(hwnd)) return;
	FLASHWINFO info = { sizeof(info), hwnd, FLASHW_TRAY | FLASHW_TIMERNOFG, 3, 0 };
	FlashWindowEx(&info);
	TRACEF("UAC: foreground activation denied; flashed installer taskbar button\n");
}

static void ShowInstallerWindow(HWND hwnd)
{
	if (!IsWindow(hwnd)) return;
	if (IsIconic(hwnd)) ShowWindow(hwnd, SW_RESTORE);
	else if (!IsWindowVisible(hwnd)) ShowWindow(hwnd, SW_SHOWNORMAL);
	SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0,
		SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
	if (!SetForegroundWindow(hwnd) && GetForegroundWindow() != hwnd) FlashInstallerWindow(hwnd);
}

static bool SameExecutableImage(HANDLE other)
{
	WCHAR current[MAX_PATH * 4], candidate[MAX_PATH * 4];
	DWORD currentLength=ARRAYSIZE(current), candidateLength=ARRAYSIZE(candidate);
	return QueryFullProcessImageNameW(GetCurrentProcess(),0,current,&currentLength) &&
		QueryFullProcessImageNameW(other,0,candidate,&candidateLength) &&
		lstrcmpiW(current,candidate)==0;
}

static void CloseRemoteHandle(HANDLE process,HANDLE remote)
{
	if (!remote) return;
	HANDLE local=NULL;
	if (DuplicateHandle(process,remote,GetCurrentProcess(),&local,0,false,
		DUPLICATE_SAME_ACCESS|DUPLICATE_CLOSE_SOURCE)) CloseHandle(local);
}

static bool BuildElevationParameters(TCHAR* target,SIZE_T capacity,HWND ipcWindow,LPCTSTR arguments)
{
	const SIZE_T argumentChars=lstrlen(arguments);
	const SIZE_T fixedChars=5+sizeof(UINT_PTR)*2+6+1; // /UAC:, hex HWND, /NCRC, NUL
	if (capacity<fixedChars || argumentChars>capacity-fixedChars) return false;
	TCHAR* out=target;
	for (LPCTSTR p=_T("/UAC:");*p;) *out++=*p++;
	UINT_PTR value=(UINT_PTR)ipcWindow;
	TCHAR reversed[sizeof(UINT_PTR)*2];
	SIZE_T digits=0;
	do {
		const UINT nibble=(UINT)(value&15);
		reversed[digits++]=(TCHAR)(nibble<10?_T('0')+nibble:_T('A')+nibble-10);
		value>>=4;
	} while(value);
	while(digits) *out++=reversed[--digits];
	for (LPCTSTR p=_T(" /NCRC");*p;) *out++=*p++;
	while(*arguments) *out++=*arguments++;
	*out=0;
	return true;
}

static bool IsOriginalUser()
{
	if (!IsInnerInstance()) return true;
	if (!G().hOuterProcess) return false;
	HANDLE currentToken=NULL, outerToken=NULL;
	BYTE currentUser[256], outerUser[256];
	DWORD currentBytes=0, outerBytes=0;
	bool same=false;
	if (OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&currentToken) &&
		OpenProcessToken(G().hOuterProcess,TOKEN_QUERY,&outerToken) &&
		GetTokenInformation(currentToken,TokenUser,currentUser,sizeof(currentUser),&currentBytes) &&
		GetTokenInformation(outerToken,TokenUser,outerUser,sizeof(outerUser),&outerBytes))
		same=EqualSid(((TOKEN_USER*)currentUser)->User.Sid,((TOKEN_USER*)outerUser)->User.Sid)!=FALSE;
	if (outerToken) CloseHandle(outerToken);
	if (currentToken) CloseHandle(currentToken);
	return same;
}


bool SysQuery_IsServiceRunning(LPCTSTR servicename) 
{
	bool retval=false;
	SC_HANDLE scdbh=NULL,hsvc;
	scdbh=OpenSCManager(NULL,NULL,GENERIC_READ);
	if (scdbh) 
	{
		if (hsvc=OpenService(scdbh,servicename,SERVICE_QUERY_STATUS)) 
		{
			SERVICE_STATUS ss;
			if (QueryServiceStatus(hsvc,&ss))retval=(ss.dwCurrentState==SERVICE_RUNNING);
			CloseServiceHandle(hsvc);
		}
		CloseServiceHandle(scdbh);
	}
	else 
	{
		//Guest account can't open OpenSCManager, let's just lie and pretend everything is ok!
		retval = ERROR_ACCESS_DENIED==GetLastError();
	}
	return retval;
}

inline bool NT5_IsSecondaryLogonSvcRunning() 
{
	return SysQuery_IsServiceRunning(_T("seclogon"));
}


BOOL QueryIsAdminOrGetIL(UINT32*pIL)
{
	BOOL isAdmin=false;
	OSVERSIONINFO ovi;
	ZEROSTRUCT(ovi), ovi.dwOSVersionInfoSize = sizeof(ovi);
	SetLastError(NO_ERROR);
	GetVersionEx(&ovi);
	const bool OSAtleastVista = ovi.dwMajorVersion >= 6;
	HANDLE hToken = NULL;
	if (VER_PLATFORM_WIN32_NT != ovi.dwPlatformId)
	{
		isAdmin = true;
	}
	else 
	{
		if (OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&hToken))
		{
			BYTE sidbuf[8+(sizeof(DWORD)*2)];
			PSID psid = (PSID) sidbuf;
			((UINT32*)psid)[0] = 1 | 2<<8;
			((UINT32*)psid)[1] = 5<<24;
			((UINT32*)psid)[2] = SECURITY_BUILTIN_DOMAIN_RID;
			((UINT32*)psid)[3] = DOMAIN_ALIAS_RID_ADMINS;
			if (_CheckTokenMembership)
			{
				if (!_CheckTokenMembership(0,psid,&isAdmin)) isAdmin=false;
			}
			else 
			{
				DWORD cbTokenGrps;
				if (!GetTokenInformation(hToken,TokenGroups,0,0,&cbTokenGrps)&&GetLastError()==ERROR_INSUFFICIENT_BUFFER) 
				{
					TOKEN_GROUPS*ptg=0;
					if (ptg=(TOKEN_GROUPS*)NSIS::MemAlloc(cbTokenGrps)) 
					{
						if (GetTokenInformation(hToken,TokenGroups,ptg,cbTokenGrps,&cbTokenGrps)) 
						{
							for (UINT i=0; i<ptg->GroupCount; ++i) 
							{
								if (EqualSid(ptg->Groups[i].Sid,psid)) isAdmin=true;
							}
						}
						NSIS::MemFree(ptg);
					}
				}
			}	
		}
	}
	UINT32 il = isAdmin ? 0x3000 : 0x2000;
	if (OSAtleastVista)
	{
		BYTE bufTokMandLbl[30];
		PTOKEN_MANDATORY_LABEL pTML=(PTOKEN_MANDATORY_LABEL)bufTokMandLbl;
		DWORD RetLen;
		il = 0;
		if (GetTokenInformation(hToken,TokenIntegrityLevel,pTML,sizeof(bufTokMandLbl),&RetLen))
		{
			PSID psid = pTML->Label.Sid;
			if ((1|1<<8) == ((UINT32*)psid)[0] && (16<<24) == ((UINT32*)psid)[1])
			{
				il = ((UINT32*)psid)[2];
			}
		}
		isAdmin = il >= 0x3000;
	}
	if (pIL) *pIL = il;
	CloseHandle(hToken);
	return isAdmin;
}
inline UINT32 GetIntegrityLevel() 
{
	UINT32 il = 0;
	QueryIsAdminOrGetIL(&il);
	return il;
}
bool IsCurrentProcessAdmin() 
{
	return !!QueryIsAdminOrGetIL(NULL);
}


bool SysQuery_UAC_IsActive() 
{
	//TODO: check AppInfo service?
	FARPROC CEE = GetProcAddress(LoadLibraryA("KERNEL32"),"CheckElevationEnabled");
	BOOL bEE;
	if (CEE && 0==((DWORD(WINAPI*)(BOOL*))CEE)(&bEE)) return !!bEE;
	ASSERT(_SHGetValueA);
	DWORD type,cbio=sizeof(DWORD),data;
	return !_SHGetValueA(HKEY_LOCAL_MACHINE,"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System","EnableLUA",&type,&data,&cbio) && data!=0;
}


LPTSTR FindExePathEnd(LPTSTR p) 
{
	if ( *p=='"' && *(++p) ) 
	{
		while( *p && *p!='"' )p=StrNextChar(p);
		if (*p)
			p=StrNextChar(p);
		else 
			return 0;
	}
	else 
		if ( *p!='/' )while( *p && *p!=' ' )p=StrNextChar(p);
	return p;
}


#ifdef FEAT_MSRUNASDLGMODHACK
HHOOK g_MSRunAsHook;
FORCEINLINE void MSRunAsDlgMod_Unload() 
{
	UnhookWindowsHookEx((HHOOK)g_MSRunAsHook);
}
LRESULT CALLBACK MSRunAsDlgMod_ShellProc(int nCode,WPARAM wp,LPARAM lp) 
{
	CWPRETSTRUCT*pCWPS;
	if (nCode >= 0 && (pCWPS=(CWPRETSTRUCT*)lp) && WM_INITDIALOG==pCWPS->message)
	{
		TCHAR buf[30];
		GetClassName(pCWPS->hwnd,buf,ARRAYSIZE(buf));
		if (!lstrcmpi(buf,_T("#32770"))) 
		{
#ifdef FEAT_AUTOPAGEJUMP
			if (!MyIsWindowVisible(G().hwndNSIS))
			{
				WndModifyLong(pCWPS->hwnd,GWL_EXSTYLE,WS_EX_APPWINDOW,WS_EX_APPWINDOW);
				SetForegroundWindow(pCWPS->hwnd);
			}		
#endif
			const UINT IDC_USRSAFER=0x106,IDC_OTHERUSER=0x104,IDC_SYSCRED=0x105;
			GetClassName(GetDlgItem(pCWPS->hwnd,IDC_SYSCRED),buf,ARRAYSIZE(buf));
			if (!lstrcmpi(buf,_T("SysCredential"))) //make sure this is the run as dialog
			{
				SndDlgItemMsg(pCWPS->hwnd,IDC_USRSAFER,BM_SETCHECK,BST_UNCHECKED);
				SndDlgItemMsg(pCWPS->hwnd,IDC_OTHERUSER,BM_CLICK);
				DBGONLY(UAC_DbgHlpr_LoadPasswordInRunAs(pCWPS->hwnd));
			}
		}
	}
	return CallNextHookEx(g_MSRunAsHook,nCode,wp,lp);
}
void MSRunAsDlgMod_Init() 
{
	g_MSRunAsHook=NULL;
	if(GetOSVerMaj()==5 && GetOSVerMin()>=1) //only XP/2003
		g_MSRunAsHook=SetWindowsHookEx(WH_CALLWNDPROCRET,MSRunAsDlgMod_ShellProc,0,GetCurrentThreadId());
}
#endif


FORCEINLINE void* DelayLoadFunction(HMODULE hLib,LPCSTR Export) 
{
	return GetProcAddress(hLib,Export);
}

DWORD DelayLoadFunctions() 
{
#ifdef UNICODE
#	define __DLD_FUNCSUFFIX "W"
#else
#	define __DLD_FUNCSUFFIX "A"
#endif
	HMODULE hModAdvAPI=LoadLibraryA("ADVAPI32")
		,hModShlWAPI=LoadLibraryA("ShlWAPI") ;
	// Optional:
	_CheckTokenMembership=(CHECKTOKENMEMBERSHIP)DelayLoadFunction(hModAdvAPI,"CheckTokenMembership");
#ifdef FEAT_CUSTOMRUNASDLG
	_GetUserNameEx=(GETUSERNAMEEX)DelayLoadFunction(LoadLibraryA("SECUR32"),"GetUserNameEx" __DLD_FUNCSUFFIX);//We never free this library
	_CreateProcessWithLogonW=(CREATEPROCESSWITHLOGONW)DelayLoadFunction(hModAdvAPI,"CreateProcessWithLogonW");
	_SHGetValueA=(SHGETVALUEA)DelayLoadFunction(hModShlWAPI,"SHGetValueA");
#endif
	return NO_ERROR;
}


bool GetOuterHwndFromCommandline(HWND&hwndOut) 
{
	LPTSTR p=FindExePathEnd(GetCommandLine());
	while(p && *p==' ')p=CharNext(p);
	TRACEF("GetOuterHwndFromCommandline: checking IPC argument\n");
	if (p && *p++=='/'&&*p++=='U'&&*p++=='A'&&*p++=='C'&&*p++==':') 
	{
		hwndOut=(HWND)StrToUIntPtr(p,true);
		return (/*IsWindow*/(hwndOut))!=0;
	}
	return false;
}


UINT_PTR __cdecl NSISPluginCallback(enum NSPIM Event) 
{
	TRACEF("NSISPluginCallback:%d %d:%d\n",Event,GetCurrentProcessId(),GetCurrentThreadId());
	switch(Event) 
	{
	case NSPIM_UNLOAD:
		if (IsOuterInstance()) 
		{
			if (IsWindow(G().hwndOuter)) PostMessage(G().hwndOuter,WM_CLOSE,0,0);
			if (OG().hOuterThread) {
				WaitForSingleObject(OG().hOuterThread,10000);
				CloseHandle(OG().hOuterThread);
			}
			if (OG().hInnerProcess) CloseHandle(OG().hInnerProcess);
		}
		if (G().PerformOp) CloseHandle(G().PerformOp);
		if (G().CompletedOp) CloseHandle(G().CompletedOp);
		if (G().pShared) UnmapViewOfFile(G().pShared);
		if (G().hSharedMap) CloseHandle(G().hSharedMap);
		if (G().hOuterProcess) CloseHandle(G().hOuterProcess);
		if (G().hOuterIPCThread) CloseHandle(G().hOuterIPCThread);
		TRACEF("NSISPluginCallback:NSPIM_UNLOAD completed cleanup, ready to be unloaded...\n");
		break;
	}
	return NULL;
}


BOOL CALLBACK ProcessThreadMessages(HWND hwndDlg=NULL,DWORD*pExitCode=NULL) 
{
	MSG msg;
	while( PeekMessage(&msg,NULL,0,0,PM_REMOVE) ) 
	{
		if (msg.message==WM_QUIT) 
		{
			TRACEF("ProcessThreadMessages got WM_QUIT %d:%d, code=%u hDlg=%X\n",GetCurrentProcessId(),GetCurrentThreadId(),msg.wParam,hwndDlg);
			if (pExitCode) *pExitCode = (DWORD) msg.wParam;
			return true;
		//	ASSERT(!"TODO: should we handle WM_QUIT?");
		//	ec=ERROR_CANCELLED;
		//	break;
		}
		if (!hwndDlg || !IsDialogMessage(hwndDlg,&msg)) 
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	return false;
}


bool Inner_DoOpInOuter(UINT opid,UINT Direction) 
{
	if (G().ipcError || !G().pShared || !G().PerformOp || !G().CompletedOp ||
		!G().hOuterProcess || !G().hOuterIPCThread) return false;
	OP_HDR*pOP=(OP_HDR*)G().pShared;
	pOP->Op=opid;
	pOP->Direction=Direction;
//	TRACEF("DoOp %d %d PID=%u\n",opid,Direction,GetCurrentProcessId());

	if (ResetEvent(G().CompletedOp) && SetEvent(G().PerformOp))
	{
		const DWORD started=GetTickCount();
		// A code segment may perform a lengthy installation; register exchange is bounded.
		const DWORD deadline=opid==OPID_CALLCODESEGMENT ? 30*60*1000 : 30*1000;
		for(DWORD w;;) 
		{
			const DWORD elapsed=GetTickCount()-started;
			if (elapsed>=deadline) { G().ipcError=WAIT_TIMEOUT; return false; }
			HANDLE waits[]={G().CompletedOp,G().hOuterProcess,G().hOuterIPCThread};
			w=MsgWaitForMultipleObjects(ARRAYSIZE(waits),waits,false,deadline-elapsed,QS_ALLINPUT);
			switch(w) 
			{
			case WAIT_OBJECT_0:
				if (pOP->Op==opid) return true;
				G().ipcError=ERROR_INVALID_DATA;
				return false;
			case WAIT_OBJECT_0+1:
			case WAIT_OBJECT_0+2:
				G().ipcError=ERROR_BROKEN_PIPE;
				return false;
			case WAIT_OBJECT_0+3:
				if (ProcessThreadMessages()) { G().ipcError=ERROR_CANCELLED; return false; }
				continue;
			default:
				TRACEF("DoOp: outer exited, timed out, or wait failed (%u)\n",w);
				G().ipcError=w==WAIT_TIMEOUT ? WAIT_TIMEOUT : GetLastError();
				if (!G().ipcError) G().ipcError=ERROR_GEN_FAILURE;
				return false;
			}
		}
	}
	G().ipcError=GetLastError();
	if (!G().ipcError) G().ipcError=ERROR_GEN_FAILURE;
	ASSERT(!"DoOp failed!");
	return false;
}


static bool ValidSyncRegisterRange(UINT offset,UINT count)
{
	if (!G().NSIScchStr || !count || count>10 || offset>=__INST_LAST || count>__INST_LAST-offset)
		return false;
	const SIZE_T cbOneVar=(SIZE_T)G().NSIScchStr*sizeof(NSISCH);
	return sizeof(OP_SYNCREG)+cbOneVar*count<=G().SharedMapBytes;
}

void OP_SYNCREG_CopyRegisters(bool VarToShared,UINT offset,const UINT count)
{
	const UINT cbOneVar=G().NSIScchStr*sizeof(NSISCH);
	offset*=cbOneVar;
	BYTE*pD=G().pShared+sizeof(OP_SYNCREG),*pS=((BYTE*)G().NSISVars)+offset;
	if (!VarToShared){BYTE*temp=pD;pD=pS;pS=temp;}
	MemCopy(pD,pS,count*cbOneVar);
}


bool Inner_DoSyncRegisters(UINT Dir,UINT regoffset=INST_0,UINT count=10) 
{
	OP_SYNCREG*pOP=(OP_SYNCREG*)G().pShared;
	pOP->RegOffset=regoffset;
	pOP->Count=count;
//	TRACEF("*** Inner_DoSyncRegisters BEGIN PID=%u\n",GetCurrentProcessId());
	for (int i=0;i<2;++i) 
	{
		if (!ValidSyncRegisterRange(pOP->RegOffset,pOP->Count)) return false;
		if (Dir==OPDIR_I2O)OP_SYNCREG_CopyRegisters(true,pOP->RegOffset,pOP->Count);//CopyRegisters(pSharedBaseOffset,G().NSISVars,pOP->RegOffset,pOP->Count);
		if (!Inner_DoOpInOuter(OPID_SYNCREG,Dir))return false;
		if (Dir==OPDIR_O2I)OP_SYNCREG_CopyRegisters(false,pOP->RegOffset,pOP->Count);//CopyRegisters(G().NSISVars,pSharedBaseOffset,pOP->RegOffset,pOP->Count);
		pOP->RegOffset=INST_R0;
	}
//	TRACEF("*** Inner_DoSyncRegisters END\n");
	return true;
}


inline bool Inner_SyncNSISVar(UINT Dir,bool instdir,bool outdir)
{
	const UINT first=instdir?INST_INSTDIR:INST_OUTDIR;
	return Inner_DoSyncRegisters(Dir,first,(INST_OUTDIR+1)-first);
}


LRESULT CALLBACK OuterWndProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) 
{
	switch(msg) 
	{
	case WM_DESTROY:
#ifdef UAC_HACK_FGNDWND2011
		SendMessage(hwnd,OWM_HIDESPLASH,0,0);
#endif
		PostQuitMessage(0);
		break;
	case WM_CLOSE:
		return DestroyWindow(hwnd);
	case OWM_INITINNER:
		TRACEF("OuterWndProc:OWM_INITINNER %X\n",OG().hInnerProcess);
		if (OG().hInnerProcess && OG().expectedInnerPid &&
			GetProcessId(OG().hInnerProcess)==OG().expectedInnerPid)
		{
			const HANDLE hThisProcess=GetCurrentProcess(),hTargetProcess=OG().hInnerProcess;
			HANDLE destMap=NULL, destEvents[2]={NULL,NULL};
			if (DuplicateHandle(hThisProcess,G().hSharedMap,hTargetProcess,&destMap,FILE_MAP_WRITE|FILE_MAP_READ,false,0))
			{
				HANDLE src[]={G().PerformOp,G().CompletedOp};
				for (int i=0;i<ARRAYSIZE(src);++i)
				{
					if (!DuplicateHandle(hThisProcess,src[i],hTargetProcess,&destEvents[i],
						SYNCHRONIZE|EVENT_MODIFY_STATE,false,0))
					{
						TRACEF("DuplicateHandle on src[%d] failed with %d\n",i,GetLastError());
						for (int j=0;j<i;++j) CloseRemoteHandle(hTargetProcess,destEvents[j]);
						CloseRemoteHandle(hTargetProcess,destMap);
						return NULL;
					}
				}
				HANDLE* shared=(HANDLE*)G().pShared;
				shared[0]=destEvents[0];
				shared[1]=destEvents[1];
				HANDLE destThread=NULL;
				if (!DuplicateHandle(hThisProcess,OG().hOuterThread,hTargetProcess,&destThread,
					SYNCHRONIZE,false,0)) {
					for (int j=0;j<ARRAYSIZE(destEvents);++j)
						CloseRemoteHandle(hTargetProcess,destEvents[j]);
					CloseRemoteHandle(hTargetProcess,destMap);
					return NULL;
				}
				WINDOW_SWITCH_STATE* state=(WINDOW_SWITCH_STATE*)
					(G().pShared+G().SharedMapBytes-sizeof(WINDOW_SWITCH_STATE));
				state->outerMainWindow=G().hwndNSIS;
				state->pageJump=G().JumpPageOffset;
				state->outerThreadHandle=destThread;
				TRACEF("OuterWndProc:OWM_INITINNER duped handles, returning shared handle %X\n",destMap);
				return (LRESULT)destMap;
			}
			else 
			{
				TRACEF("DuplicateHandle on G().hSharedMap failed with %d\n",GetLastError());
			}
		}
		return NULL;
	case OWM_ISREADYFORINIT:
		return OG().expectedInnerPid && !OG().hInnerProcess ? 0x666 : false;
	case OWM_PERFORMWINDOWSWITCH:
		TRACEF("OuterWndProc:OWM_PERFORMWINDOWSWITCH %X:%X\n",wp,lp);
		if (IsWindow((HWND)wp) && IsWindow((HWND)lp))
		{
			HWND hOuterNSIS=(HWND)wp;
			HWND hInnerNSIS=(HWND)lp;
			DWORD outerPid=0, innerPid=0;
			GetWindowThreadProcessId(hOuterNSIS,&outerPid);
			GetWindowThreadProcessId(hInnerNSIS,&innerPid);
			if (outerPid != GetCurrentProcessId() || !OG().expectedInnerPid ||
				innerPid != OG().expectedInnerPid) return ERROR_INVALID_WINDOW_HANDLE;
			if (hOuterNSIS!=G().hwndNSIS ||
				!PostMessage(hOuterNSIS,OMWM_HIDEAFTERSWITCH,(WPARAM)hInnerNSIS,0))
				return ERROR_INVALID_WINDOW_HANDLE;
		}
		else return ERROR_INVALID_WINDOW_HANDLE;
		return NO_ERROR;

#ifdef FEAT_AUTOPAGEJUMP
	case OWM_GETOUTERSTATE:
		switch(wp) 
		{
		case GOSI_HWNDMAINWND:return (LRESULT)G().hwndNSIS;
		case GOSI_PAGEJUMP:return G().JumpPageOffset;
#ifdef GOSI_PID
		case GOSI_PID:return GetCurrentProcessId();
#endif
		}
		return NULL;
#endif
	case OWM_SETOUTERSTATE:
		TRACEF("OWM_SETOUTERSTATE %X %X\n",wp,lp);
		switch(wp)
		{
		case SOSI_PROCESSDUPHANDLE:
			if (!OG().hInnerProcess && lp && OG().expectedInnerPid &&
				GetProcessId((HANDLE)lp)==OG().expectedInnerPid)
			{
				//TODO: Verfiy that this handle has the correct PID?
				TRACEF("OWM_SETOUTERSTATE:SOSI_PROCESSDUPHANDLE got %X\n",lp);
				OG().hInnerProcess=(HANDLE)lp;
				return 0x666;
			}
			break;
#ifdef FEAT_AUTOPAGEJUMP
		case SOSI_ELEVATIONPAGE_GUIINITERRORCODE:
			OG().ecInner_InitSharedData = (DWORD) lp;
			break;
#endif
		}
		return NULL;
#ifdef UAC_HACK_INNERPARENT2010
	case OWM_PARENTIFY:
		{
			TRACEF("OWM_PARENTIFY %X %X\n",wp,lp);
			HWND hOuterNSIS=(HWND)wp;
			HWND hInnerNSIS=(HWND)lp;
			SetWindowLongPtr(hOuterNSIS,GWL_STYLE,WS_CHILD|WS_VISIBLE);
			SetParent(hOuterNSIS,hInnerNSIS);
			SetWindowPos(hOuterNSIS,HWND_BOTTOM,0,0,0,0,SWP_NOACTIVATE|SWP_ASYNCWINDOWPOS|SWP_FRAMECHANGED|SWP_NOOWNERZORDER);//SWP_NOSENDCHANGING
			TRACEF("\tOWM_PARENTIFY completed...\n");
		}
		return false;
#endif
#ifdef UAC_HACK_FGNDWND2011
	case OWM_HIDESPLASH:
		TRACEF("OWM_HIDESPLASH %X\n",OG().hwndSplash);
		if (IsWindow(OG().hwndSplash)) {
			ShowWindow(OG().hwndSplash,SW_HIDE);
			// The dialog belongs to the NSIS thread, not this IPC window thread.
			PostMessage(OG().hwndSplash,WM_CLOSE,0,0);
		}
		OG().hwndSplash=NULL;
		break;
#endif
	}
	return DefWindowProc(hwnd,msg,wp,lp);
}


DWORD CALLBACK OuterWndThread(LPVOID param) 
{
	DWORD ec=NO_ERROR;
	CoInitialize(NULL); // Why?
	if ( G().hwndOuter=CreateWindowEx(WS_EX_TOOLWINDOW,_T("Static"),NULL,/*WS_VISIBLE*/0,0,0,0,0,NULL,0,g_hInst,NULL) )
	{
		TRACEF("OuterWndThread: Created outer IPC window %X\n",G().hwndOuter);
		SetWindowLongPtr(G().hwndOuter,GWLP_WNDPROC,(LONG_PTR)OuterWndProc);
		SetEvent(G().CompletedOp);//Tell the other thread we have a window and will now process messages
		do
		{	//TODO: Since DoOp now calls MsgWaitForMultipleObjects, we can probably move this back to the RunElevated msgloop
			const HANDLE hWaitObjs[]={G().PerformOp};
			const UINT waitCount=ARRAYSIZE(hWaitObjs);
			DWORD w=MsgWaitForMultipleObjects(waitCount,hWaitObjs,false,INFINITE,QS_ALLINPUT);
			switch(w) 
			{
			case WAIT_OBJECT_0:
				{
					OP_HDR*pHdr=(OP_HDR*)G().pShared;
					TRACEF("UAC perform outer op: %d %d PID=%u:%u \n",pHdr->Op,pHdr->Direction,GetCurrentProcessId(),GetCurrentThreadId());
					switch(pHdr->Op) 
					{
					case OPID_SYNCREG:
						{
							OP_SYNCREG*pSR=(OP_SYNCREG*)G().pShared;
							if (pSR->OpHdr.Direction>OPDIR_O2I ||
								!ValidSyncRegisterRange(pSR->RegOffset,pSR->Count)) pHdr->Op=OPID_NOP;
							else OP_SYNCREG_CopyRegisters(pSR->OpHdr.Direction==OPDIR_O2I,pSR->RegOffset,pSR->Count);
						}
						break;
					case OPID_CALLCODESEGMENT:
						TRACEF("OPID_CALLCODESEGMENT: %u\n",((OP_CALLCODESEGMENT*)pHdr)->Pos);
						if (pHdr->Direction!=OPDIR_I2O ||
							((OP_CALLCODESEGMENT*)pHdr)->Pos<0 ||
							(((OP_CALLCODESEGMENT*)pHdr)->Flags & ~OPCCSF_SETOUTPATH)) pHdr->Op=OPID_NOP;
						else {
							if (((OP_CALLCODESEGMENT*)pHdr)->Flags&OPCCSF_SETOUTPATH)
								SetCurrentDirectory(NSIS_UNSAFE_GETVAR(INST_OUTDIR,G().NSISVars,G().NSIScchStr));
							g_pXP->ExecuteCodeSegment(((OP_CALLCODESEGMENT*)pHdr)->Pos,NULL);
						}
						break;
/*					case OPID_GETOUTERSTATE:
						MemCopy(G().pShared,&G(),sizeof(UAC_GLOBALS));
						break;*/
					default:
						TRACEF("OPID_???\n");
						pHdr->Op=OPID_NOP;
					}
				}
				SetEvent(G().CompletedOp);
				break;
			case WAIT_OBJECT_0+1:
				if (ProcessThreadMessages(NULL))ec=ERROR_CANCELLED;
				break;
			default:
				ec=GetLastError();
				ASSERT(ec);
				TRACEF("OuterWndThread MsgWait failed with %u\n",ec);
			}
		}
		while( G().hwndOuter && NO_ERROR==ec );
	}
	else 
		ec=GetLastError();
//	CoUninitialize();
	TRACEF("OuterWndThread %d:%d ending with %d\n",GetCurrentProcessId(),GetCurrentThreadId(),ec);
	return ec;
}


FORCEINLINE DWORD Outer_InitShared(UINT NSIScchStr) 
{
	if (!NSIScchStr || NSIScchStr>65536) return ERROR_INVALID_PARAMETER;
	UINT ec,cbMap=sizeof(OP_SYNCREG) + ((sizeof(NSISCH)*NSIScchStr)*10) +
		sizeof(WINDOW_SWITCH_STATE);
	G().SharedMapBytes=cbMap;
	if (true
		&& (G().PerformOp=CreateEvent(/*&sa*/NULL,false,false,NULL))
		&& (G().CompletedOp=CreateEvent(/*&sa*/NULL,false,false,NULL))
		&& (G().hSharedMap=CreateFileMapping(INVALID_HANDLE_VALUE,/*&sa*/NULL,PAGE_READWRITE|SEC_COMMIT,0,cbMap,NULL))
		&& (G().pShared=(BYTE*)MapViewOfFile(G().hSharedMap,FILE_MAP_ALL_ACCESS,0,0,0))
		) 
		ec=NO_ERROR;
	else
		ec=GetLastError();
	return ec;
}

FORCEINLINE DWORD Outer_CreateOuterWindow() 
{
	DWORD ec=NO_ERROR;
	if ( !(OG().hOuterThread=CreateThread(0,1,OuterWndThread,NULL,0,NULL)) )return GetLastError();
	HANDLE waits[]={G().CompletedOp,OG().hOuterThread};
	DWORD wait=WaitForMultipleObjects(ARRAYSIZE(waits),waits,false,10000);
	if (wait==WAIT_OBJECT_0+1) GetExitCodeThread(OG().hOuterThread,&ec);
	else if (wait!=WAIT_OBJECT_0) ec=wait==WAIT_TIMEOUT?WAIT_TIMEOUT:GetLastError();
	return ec;
}


DWORD Inner_InitSharedData() 
{
	if (G().pShared) return NO_ERROR;
	if (!IsWindow(G().hwndOuter) || !G().NSIScchStr || G().NSIScchStr>65536)
		return ERROR_INVALID_WINDOW_HANDLE;
	DWORD pid=0;
	GetWindowThreadProcessId(G().hwndOuter,&pid);
	if (!pid || pid==GetCurrentProcessId()) return ERROR_INVALID_WINDOW_HANDLE;
	G().hOuterProcess=OpenProcess(PROCESS_DUP_HANDLE|PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,false,pid);
	if (!G().hOuterProcess) return GetLastError();
	DWORD ec=NO_ERROR;
	DWORD_PTR reply=0;
	DWORD flags=0;
	HANDLE remoteSelf=NULL;
	WINDOW_SWITCH_STATE* state=NULL;
	if (!SameExecutableImage(G().hOuterProcess)) { ec=ERROR_ACCESS_DENIED; goto failed; }
	{
		const DWORD started=GetTickCount();
		for (;;) {
			DWORD peer=WaitForSingleObject(G().hOuterProcess,0);
			if (peer==WAIT_OBJECT_0) { ec=ERROR_BROKEN_PIPE; goto failed; }
			if (peer==WAIT_FAILED) { ec=GetLastError(); goto failed; }
			if (!SendMessageTimeout(G().hwndOuter,OWM_ISREADYFORINIT,0,0,
				SMTO_ABORTIFHUNG|SMTO_BLOCK,5000,&reply)) {
				ec=GetLastError(); if (!ec) ec=WAIT_TIMEOUT; goto failed;
			}
			if (reply==0x666) break;
			if (GetTickCount()-started>=10000) { ec=WAIT_TIMEOUT; goto failed; }
			Sleep(50);
		}
	}
	if (!DuplicateHandle(GetCurrentProcess(),GetCurrentProcess(),G().hOuterProcess,&remoteSelf,
		PROCESS_DUP_HANDLE|PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,false,0)) {
		ec=GetLastError(); goto failed;
	}
	if (!SendMessageTimeout(G().hwndOuter,OWM_SETOUTERSTATE,SOSI_PROCESSDUPHANDLE,
		(LPARAM)remoteSelf,SMTO_ABORTIFHUNG|SMTO_BLOCK,5000,&reply)) {
		// The outer thread may have accepted the handle just before timeout.
		// Leave its ownership to the outer process in this ambiguous case.
		ec=GetLastError(); if (!ec) ec=WAIT_TIMEOUT;
		goto failed;
	}
	if (reply!=0x666) {
		ec=GetLastError(); if (!ec) ec=ERROR_INVALID_WINDOW_HANDLE;
		CloseRemoteHandle(G().hOuterProcess,remoteSelf);
		goto failed;
	}
	TRACEF("Inner_InitSharedData: process verified, requesting shared handles\n");
	if (!SendMessageTimeout(G().hwndOuter,OWM_INITINNER,0,0,
		SMTO_ABORTIFHUNG|SMTO_BLOCK,5000,&reply) || !reply) {
		ec=GetLastError(); if (!ec) ec=ERROR_INVALID_HANDLE;
		goto failed;
	}
	G().hSharedMap=(HANDLE)reply;
	G().pShared=(BYTE*)MapViewOfFile(G().hSharedMap,FILE_MAP_ALL_ACCESS,0,0,0);
	if (!G().pShared) { ec=GetLastError(); goto failed; }
	G().SharedMapBytes=sizeof(OP_SYNCREG)+sizeof(NSISCH)*G().NSIScchStr*10+
		sizeof(WINDOW_SWITCH_STATE);
	G().PerformOp=((HANDLE*)G().pShared)[0];
	G().CompletedOp=((HANDLE*)G().pShared)[1];
	state=(WINDOW_SWITCH_STATE*)
		(G().pShared+G().SharedMapBytes-sizeof(WINDOW_SWITCH_STATE));
	G().hOuterIPCThread=state->outerThreadHandle;
	if (!G().PerformOp || !G().CompletedOp || !G().hOuterIPCThread) {
		ec=ERROR_INVALID_HANDLE; goto failed;
	}
	if (!GetHandleInformation(G().PerformOp,&flags) ||
		!GetHandleInformation(G().CompletedOp,&flags) ||
		!GetHandleInformation(G().hOuterIPCThread,&flags)) {
		ec=ERROR_INVALID_HANDLE; goto failed;
	}
	TRACEF("Inner_InitSharedData: IPC ready\n");
	return NO_ERROR;
failed:
	TRACEF("Inner_InitSharedData failed: %u\n",ec);
	if (G().PerformOp) { CloseHandle(G().PerformOp); G().PerformOp=NULL; }
	if (G().CompletedOp) { CloseHandle(G().CompletedOp); G().CompletedOp=NULL; }
	if (G().pShared) { UnmapViewOfFile(G().pShared); G().pShared=NULL; }
	if (G().hSharedMap) { CloseHandle(G().hSharedMap); G().hSharedMap=NULL; }
	if (G().hOuterProcess) { CloseHandle(G().hOuterProcess); G().hOuterProcess=NULL; }
	if (G().hOuterIPCThread) { CloseHandle(G().hOuterIPCThread); G().hOuterIPCThread=NULL; }
	return ec;
}


#ifdef FEAT_AUTOPAGEJUMP
LRESULT CALLBACK OuterMainWndSubProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) 
{
	switch(msg)
	{
	case WM_NOTIFY_OUTER_NEXT:
		G().JumpPageOffset += (WORD) wp;
		TRACEF("WM_NOTIFY_OUTER_NEXT: now=%d change=%d\n",G().JumpPageOffset,wp);
		break;
	case OMWM_HIDEAFTERSWITCH:
		{
			DWORD innerPid=0;
			GetWindowThreadProcessId((HWND)wp,&innerPid);
			if (hwnd==G().hwndNSIS && innerPid==OG().expectedInnerPid &&
				OG().hInnerProcess && WaitForSingleObject(OG().hInnerProcess,0)==WAIT_TIMEOUT)
				ShowWindow(hwnd,SW_HIDE);
			return 0;
		}
	}
	return CallWindowProc(G().OrgMainWndProc,hwnd,msg,wp,lp);
}
#endif


#ifdef FEAT_AUTOPAGEJUMP
LRESULT CALLBACK InnerMainWndSubProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) 
{
	switch(msg) 
	{
	case WM_WINDOWPOSCHANGED:
		{
			WINDOWPOS&wp=*(WINDOWPOS*)lp;
			if (SWP_SHOWWINDOW&wp.flags && G().JumpPageOffset!=-1) 
			{
				TRACEF("InnerMainWndSubProc:WM_WINDOWPOSCHANGED: completing GUI handoff from shared state");
				G().JumpPageOffset=-1;
				WINDOW_SWITCH_STATE* state=G().pShared ? (WINDOW_SWITCH_STATE*)
					(G().pShared+G().SharedMapBytes-sizeof(WINDOW_SWITCH_STATE)) : NULL;
				HWND hOuterMainWnd=state ? state->outerMainWindow : NULL;
				DWORD outerPid=0;
				if (IsWindow(hOuterMainWnd) &&
					GetWindowThreadProcessId(hOuterMainWnd,&outerPid) &&
					G().hOuterProcess && outerPid==GetProcessId(G().hOuterProcess)) {
					RECT rect;
					if (GetWindowRect(hOuterMainWnd,&rect))
						SetWindowPos(hwnd,HWND_TOP,rect.left,rect.top,0,0,
							SWP_NOSIZE|SWP_NOACTIVATE);
					PostMessage(hwnd,WM_NOTIFY_OUTER_NEXT,state->pageJump,0);
					ShowInstallerWindow(hwnd);
					PostMessage(hOuterMainWnd,OMWM_HIDEAFTERSWITCH,(WPARAM)hwnd,0);
				}
				else ShowInstallerWindow(hwnd);
			}
		}
		break;
/*	case IWM_HELPWITHWINDOWSWITCH:
		TRACEF("InnerMainWndSubProc:IWM_HELPWITHWINDOWSWITCH\n");
		SetParent(Inner_GetOuterNSISMainWnd(),hwnd);
		return TRUE;
*/	}
	return CallWindowProc(G().OrgMainWndProc,hwnd,msg,wp,lp);
}
#endif


#define GetVarStr(varid) ((LPTSTR)(Vars+((varid)*NSIScchStr)))
#define SetVarUINT(varid,data) wsprintf((Vars+((varid)*NSIScchStr)),_T("%u"),(data))
#define NSISHlpr_SetErrorFlag() pXP->exec_flags->exec_error=1
#define NSISHlpr_ClearErrorFlag() pXP->exec_flags->exec_error=0


INT_PTR CALLBACK SplashDialogProc(HWND hwnd,UINT uMsg,WPARAM wp,LPARAM lp) 
{
	if (uMsg==WM_CLOSE) { DestroyWindow(hwnd); return TRUE; }
	return false;
}


extern "C" void __declspec(dllexport) __cdecl _(HWND hwndNSIS,UINT NSIScchStr,NSISCH*Vars,NSIS::stack_t**StackTop,NSIS::extra_parameters* pXP) 
{
	stack_t*sp1=(*StackTop);
	bool freeSP1=true;
	NSISCH*p=sp1->text;
	DWORD ec=NO_ERROR;
	if (G().RunMode==RUNMODE_INIT) 
	{
		G().NSIScchStr=NSIScchStr;
		G().NSISVars=Vars;
		g_pXP=pXP;
		pXP->RegisterPluginCallback(g_hInst,NSISPluginCallback);
		if ( GetOSVerMaj() < 5 )
		{
			G().RunMode=RUNMODE_LEGACY;
		}
		else
		{
			G().RunMode = GetOuterHwndFromCommandline(G().hwndOuter) ? RUNMODE_INNER : RUNMODE_OUTER ;
			if ( !(ec=DelayLoadFunctions()) ) 
			{
				if ( G().RunMode==RUNMODE_OUTER && !(ec=Outer_InitShared(NSIScchStr)) ) ec=Outer_CreateOuterWindow();
			}
		}
	}
	TRACEF("EXPORT cmd=%c outer=%d %d:%d ec=%d\n",*(p),IsOuterInstance(),GetCurrentProcessId(),GetCurrentThreadId(),ec);
	
	NSISHlpr_ClearErrorFlag();
	if (!ec) switch(*(p))
	{
	case '0':/*** RunElevated ***/
		{
			bool IsAdmin=IsCurrentProcessAdmin();
			BYTE UACMode=0;
			const UINT OSVMaj=GetOSVerMaj();
			DWORD ForkExitCode=ERROR_CANCELLED;//is this the best value to use? is it required?
			TRACEF("UAC:RunElevated: admin=%d os=%d.%d runmode=%u\n",IsAdmin,OSVMaj,GetOSVerMin(),G().RunMode);
			
#ifdef FEAT_AUTOPAGEJUMP
			G().hwndNSIS=hwndNSIS;
#endif

			if (IsAdmin) 
			{
				UACMode=2;
				if (IsInnerInstance()) 
				{
					ec=Inner_InitSharedData();
					ASSERT(!ec);
#ifdef UAC_HACK_FGNDWND2011
					PostMessage(Inner_GetOuterIPCWND(),OWM_HIDESPLASH,0,0);
#endif
				}
			}
			else 
			{
				if (IsInnerInstance()) //non-admin used in runas dialog
				{
					if (!(ec=Inner_InitSharedData())) 
					{
						UACMode=1;//fake value so the script calls quit without further processing
						pXP->exec_flags->errlvl=0;
						((OP_HDR*)G().pShared)->Op=OPID_OUTERMUSTRETRY; //DoOp(OPID_OUTERMUSTRETRY,0);//tell the outer process about this problem
					}
					else 
					{
						ASSERT(!ec);
					}
				}
				else 
				{
					SHELLEXECUTEINFO sei;
					MemZero(&sei,sizeof(sei));
					LPTSTR pszExePathBuf=0;
					LPTSTR pszParamBuf=0;
					LPTSTR p,pCL=GetCommandLine();
					UINT len;
					LPCTSTR verb_runas = _T("runas");

					sei.cbSize=sizeof(sei);
					sei.hwnd=hwndNSIS?hwndNSIS:G().hwndOuter;
					sei.nShow=SW_SHOWNORMAL;
					sei.fMask=SEE_MASK_NOCLOSEPROCESS|SEE_MASK_NOZONECHECKS|SEE_MASK_FLAG_DDEWAIT;
					//TODO: |= SEE_MASK_UNICODE ??
					sei.lpVerb=verb_runas;

#ifdef UAC_HACK_FGNDWND2011
					if (!MyIsWindowVisible(hwndNSIS)) 
					{
						HMODULE hExeMod=GetModuleHandle(0);
						if (!OG().hwndSplash) OG().hwndSplash=CreateDialog(hExeMod,MAKEINTRESOURCE(111),0,SplashDialogProc);
						if (OG().hwndSplash) 
						{
							sei.hwnd=OG().hwndSplash;
							//MySetDlgItemText(sei.hwnd,1030,NULL);
							LONG xs = WS_EX_TOOLWINDOW;
							// Hacky invisible splash screen, 
							// you can hex edit "runas" > "Runas" to remove this hack and display it.
							if (*verb_runas=='r') xs |= WS_EX_TRANSPARENT|WS_EX_LAYERED;
							WndModifyLong(sei.hwnd,GWL_EXSTYLE,xs,xs);
							SetWindowPos(sei.hwnd,0,0,0,0,0,SWP_FRAMECHANGED|SWP_NOSIZE|SWP_NOMOVE|SWP_NOZORDER);
							HICON hIcon=LoadIcon(hExeMod,MAKEINTRESOURCE(103));
							HWND hCtl = NULL;
							for(;hCtl = FindWindowEx(sei.hwnd,hCtl,NULL,NULL);) ShowWindow(hCtl,SW_HIDE);
							HWND hIcoCtl=GetDlgItem(sei.hwnd,1030);
							ShowWindow(hIcoCtl,SW_SHOW);
							RECT r;
							GetClientRect(sei.hwnd,&r);
							SetWindowPos(hIcoCtl,0,0,0,r.right,r.bottom,SWP_NOZORDER|SWP_NOACTIVATE);
							WndModifyLong(hIcoCtl,GWL_STYLE,0,SS_ICON|SS_CENTERIMAGE);
							SNDMSG(hIcoCtl,STM_SETICON,(WPARAM)hIcon,0);
							SNDMSG(sei.hwnd,WM_SETICON,ICON_BIG,(LPARAM)hIcon);
							ShowWindow(sei.hwnd,SW_SHOW);
						}
					}
#endif
					
					p=FindExePathEnd(pCL);
					len=p?(UINT)(p-pCL):0;
					ec=ERROR_FILE_NOT_FOUND;

					if (p && len)
					{
						for (;ec!=ERROR_OUTOFMEMORY;) 
						{
							NSIS::MemFree(pszExePathBuf);
							if (!(pszExePathBuf=(LPTSTR)NSIS::MemAlloc((++len)*sizeof(TCHAR))))
								ec=ERROR_OUTOFMEMORY;
							else
							{
								DWORD pathChars=GetModuleFileName(NULL,pszExePathBuf,len);
								if (!pathChars) { ec=GetLastError(); break; }
								if (pathChars < len)
								{
									TRACEF("RunElevated: executable path resolved\n");
									ec=0;
									break; 
								}
							}
							len+=MAX_PATH;
						}
						if (!ec)
						{
							sei.lpFile=pszExePathBuf;
							len=lstrlen(p);
							len+=20;//space for "/UAC:xxxxxx /NCRC\0"
							if (!(pszParamBuf=(LPTSTR)NSIS::MemAlloc(len*sizeof(TCHAR))))
								ec=ERROR_OUTOFMEMORY;
							else 
							{
								if (!BuildElevationParameters(pszParamBuf,len,G().hwndOuter,p))
									ec=ERROR_INSUFFICIENT_BUFFER;
								sei.lpParameters=pszParamBuf;// The IPC argument must remain first.
								
								if (OSVMaj==5) 
								{
									bool hasseclogon=NT5_IsSecondaryLogonSvcRunning();
									TRACEF("SysNT5IsSecondaryLogonSvcRunning=%d\n",hasseclogon);
									if (!hasseclogon)ec=ERROR_SERVICE_NOT_ACTIVE;
								}
								if (!ec) 
								{
									SetForegroundWindow(sei.hwnd);
									TRACEF("UAC:Elevate: starting child process\n");

									#ifdef FEAT_CUSTOMRUNASDLG
									if (OSVMaj>=6 && !SysQuery_UAC_IsActive()) //NOTE: v6 not hardcoded, will MS ever fix this?
									{
										ec=MyRunAs(g_hInst,sei);
										TRACEF("MyRunAs ret, ec=%u\n",ec);
									}
									else
									#endif
									{
										#ifdef FEAT_MSRUNASDLGMODHACK
										MSRunAsDlgMod_Init();
										#endif
										
										ec=(ShellExecuteEx(&sei)?NO_ERROR:GetLastError());
										TRACEF("ShellExecuteEx ret, gle=%u\n",ec);

										#ifdef FEAT_MSRUNASDLGMODHACK
										MSRunAsDlgMod_Unload();
										#endif
									}

									if (!ec) 
									{
										OG().expectedInnerPid=GetProcessId(sei.hProcess);
										if (!OG().expectedInnerPid) ec=GetLastError();
										if (!ec) AllowSetForegroundWindow(OG().expectedInnerPid);
									}
									if (!ec)
									{
										bool endWait=false;
										TRACEF("RunElevated: %d:%d waiting for fork hP=%X\n",GetCurrentProcessId(),GetCurrentThreadId(),sei.hProcess);
										
										do 
										{
											DWORD w=MsgWaitForMultipleObjects(1,&sei.hProcess,false,INFINITE,QS_ALLINPUT);
											switch(w)
											{
											case WAIT_OBJECT_0:
												VERIFY(GetExitCodeProcess(sei.hProcess,&ForkExitCode));
												endWait=true;
												UACMode= ((OP_HDR*)G().pShared)->Op==OPID_OUTERMUSTRETRY ? 3 : 1;
												pXP->exec_flags->errlvl=ForkExitCode;
												TRACEF("UAC:RunElevated: Inner process exit with %d, UACMode=%d\n",ForkExitCode,UACMode);
												break;
											case WAIT_OBJECT_0+1:
												ProcessThreadMessages(hwndNSIS);
												break;
											default:
												endWait=true;
												ec=GetLastError();
												TRACEF("UAC:RunElevated:MsgWaitForMultipleObjects failed, ec=%u w=%u\n",ec,w);
											}
										}
										while(!endWait && NO_ERROR==ec);
										CloseHandle(sei.hProcess);
										sei.hProcess=NULL;
										if (OG().ecInner_InitSharedData) 
										{
											TRACEF("inner process completed, but never got access to shared mem? ec=%u forkexit=%u ecInner_InitSharedData=%u\n",ec,ForkExitCode,OG().ecInner_InitSharedData);
											ec=OG().ecInner_InitSharedData;
										}
									}
									if (sei.hProcess) { CloseHandle(sei.hProcess); sei.hProcess=NULL; }
								}
							}
						}
					}
				}
			}
			if (IsOuterInstance()) 
			{
				PostMessage(G().hwndOuter,OWM_HIDESPLASH,0,0);
				if (ec || UACMode==3) ShowInstallerWindow(hwndNSIS);
				if(G().JumpPageOffset)--G().JumpPageOffset;
				CloseHandle(OG().hInnerProcess);
				OG().hInnerProcess=NULL;
				OG().expectedInnerPid=0;
			}

			pXP->exec_flags->errlvl=ec?ec:ForkExitCode;//We now set the errlvl no matter what, is this a good idea?
			SetVarUINT(INST_0,ec);
			SetVarUINT(INST_1,UACMode);
			SetVarUINT(INST_2,ForkExitCode);
			SetVarUINT(INST_3,IsAdmin);
			TRACEF("UAC:RunElevated end: ec=%u mode=%d forkexit=%u admin=%d isouter=%d\n",ec,UACMode,ForkExitCode,IsAdmin,IsOuterInstance());
		}
		break;
	case '1':/*** ExecCodeSegment(flags) ***/
		{
			int pos=StrToUInt(&p[1]);
			if (pos--)
			{
				UINT ecsflags=0;
				TCHAR*pFlags=&p[1];
				while(*pFlags && *pFlags!=':')++pFlags;
				if (*pFlags) 
				{
					++pFlags;
					TRACEF("ExecCodeSegment>Flags=%s|\n",pFlags);
					ecsflags=StrToUInt(pFlags);
				}

				if (IsInnerInstance())
				{
					if (!(ec=Inner_InitSharedData())) 
					{
						const bool SyncRegisters=(ecsflags&UAC_SYNCREGISTERS)!=0;
						const bool SyncInstDir=(ecsflags&UAC_SYNCINSTDIR)!=0;
						const bool SyncOutDir=(ecsflags&UAC_SYNCOUTDIR)!=0;
						bool ok=!SyncRegisters || Inner_DoSyncRegisters(OPDIR_I2O);
						if (ok) ok=Inner_SyncNSISVar(OPDIR_I2O,SyncInstDir,SyncOutDir);
						if (ok) {
							OP_CALLCODESEGMENT*pOP=(OP_CALLCODESEGMENT*)G().pShared;
							pOP->Pos=pos;
							pOP->Flags=SyncOutDir?OPCCSF_SETOUTPATH:0;
							ok=Inner_DoOpInOuter(OPID_CALLCODESEGMENT,OPDIR_I2O);
						}
						if (ok && SyncRegisters) ok=Inner_DoSyncRegisters(OPDIR_O2I);
						if (ok) ok=Inner_SyncNSISVar(OPDIR_O2I,SyncInstDir,SyncOutDir);
						if (ok && SyncOutDir) SetCurrentDirectory(NSIS_UNSAFE_GETVAR(INST_OUTDIR,G().NSISVars,G().NSIScchStr));
						if (!ok) { ec=G().ipcError?G().ipcError:ERROR_BROKEN_PIPE; TRACEF("ExecCodeSegment: IPC operation failed: %u\n",ec); }
					}
					else 
					{
						ASSERT(!ec);
					}
				}
				else
				{
					pXP->ExecuteCodeSegment(pos,NULL);
				}
			}
		}
		break;
	case '2':/*** IsAdmin ***/
		TRACEF("UAC_IsAdmin: %d:%d outer=%d admin=%d\n",GetCurrentProcessId(),GetCurrentThreadId(),IsOuterInstance(),IsCurrentProcessAdmin());
		if (p[1]=='s')freeSP1=false;
		wsprintf(freeSP1 ? (Vars+(INST_0*NSIScchStr)) : p ,_T("%u"),IsCurrentProcessAdmin());
		ec=GetLastError();
		TRACEF("UAC:IsAdmin PID=%X ret=%d useReg=%d ec=%X\n",GetCurrentProcessId(),IsCurrentProcessAdmin(),freeSP1,ec);
		break;
	case '3':/*** IsInnerInstance ***/
		TRACEF("UAC_IsInnerInstance: %d:%d outer=%d admin=%d\n",GetCurrentProcessId(),GetCurrentThreadId(),IsOuterInstance(),IsCurrentProcessAdmin());
		if (p[1]=='s')freeSP1=false;
		wsprintf(freeSP1 ? (Vars+(INST_0*NSIScchStr)) : p ,_T("%u"),IsInnerInstance());
		TRACEF("UAC:IsInnerInstance PID=%d ret=%d useReg=%d\n",GetCurrentProcessId(),IsInnerInstance(),freeSP1);
		break;
#ifdef FEAT_AUTOPAGEJUMP
	case '4':/*** Notify_OnGuiInit / PageElevation_OnGuiInit ***/
		ASSERT(hwndNSIS);
		TRACEF("UAC_Notify_OnGuiInit: %d:%d outer=%d admin=%d\n",GetCurrentProcessId(),GetCurrentThreadId(),IsOuterInstance(),IsCurrentProcessAdmin());

		if (!(G().OrgMainWndProc=(WNDPROC)SetWindowLongPtr(hwndNSIS,GWLP_WNDPROC,(LONG_PTR)(IsInnerInstance()?InnerMainWndSubProc:OuterMainWndSubProc))))
		{
			ec=ERROR_INVALID_WINDOW_HANDLE;
		}
		if (IsInnerInstance()) 
		{
			if (!ec) ec=Inner_InitSharedData();
			ASSERT(!ec);
			TRACEF("Inner_InitSharedData ret %u\n",ec);
		}
		else 
		{
			DBGONLY(SetForegroundWindow(FindWindowA("dbgviewClass",0)));
		}
		//SetVarUINT(INST_0,ec);
		break;
	case '5':/*** PageElevation_OnInit ***/
		ASSERT(!ec);
		if (IsInnerInstance()) 
		{
			ec=Inner_InitSharedData();
			DWORD_PTR ignored=0;
			SendMessageTimeout(Inner_GetOuterIPCWND(),OWM_SETOUTERSTATE,
				SOSI_ELEVATIONPAGE_GUIINITERRORCODE,ec,SMTO_ABORTIFHUNG|SMTO_BLOCK,5000,&ignored);
		}
		SetVarUINT(INST_0,ec);
		break;
#endif
	case '6':/*** GetTokenIntegrityLevel ***/
		{
			const UINT32 il = GetIntegrityLevel();
			wsprintf(p,_T("%u"),il);
			freeSP1 = false;
		}
		break;
	case '7':/*** IsOriginalUser ***/
		wsprintf(p,_T("%u"),IsOriginalUser()?1:0);
		freeSP1=false;
		TRACEF("UAC:IsOriginalUser=%s\n",*p=='1'?"yes":"no/unknown");
		break;
/*	case '?':/*** FindCmdLineParams *** /
		{
		freeSP1=false;
		NSISCH*pStart=&p[1];
		NSISCH*pParams=FindExePathEnd( *pStart == ':' ? ++pStart : pStart=GetCommandLine() );
		pParams=StrSkipWhitespace(pParams);
		MemCopy(p,pParams,(lstrlen(pParams)+1)*sizeof(NSISCH));
		}
		break;*/
	}

	if (freeSP1) 
	{
		*StackTop=(*StackTop)->next;
		NSIS::MemFree(sp1);
	}
	if (ec) NSISHlpr_SetErrorFlag();
}



#ifdef _DEBUG
EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInst,DWORD Event,LPVOID lpReserved) 
#else
EXTERN_C BOOL WINAPI _DllMainCRTStartup(HINSTANCE hInst,ULONG Event,LPVOID lpReserved) 
#endif
{
	if(Event==DLL_PROCESS_ATTACH) 
	{
		g_hInst=hInst;
		MemZero(&globals,sizeof(UAC_GLOBALS));
		ASSERT(G().RunMode==RUNMODE_INIT);
		DBGONLY(HWND dummy;if(!GetOuterHwndFromCommandline(dummy))DBG_RESETDBGVIEW(););
		TRACEF("*** UAC DllMain PID=%u TID=%d\n",GetCurrentProcessId(),GetCurrentThreadId());
	}
	TRACEF("*** UAC DllMain Event=%d\n",Event);
	return true;
}
