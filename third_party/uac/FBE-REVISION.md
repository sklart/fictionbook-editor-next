# FBE Next UAC fork

Upstream baseline: AndersK NSIS UAC plug-in **0.2.4c** (2015-05-26), as
recorded in `History.txt`. The FBE Next fork first shipped in commit
`1501738b948c3ccd6c058044b4260c2efa1416ac`; this revision corrects its
window handoff, IPC failure handling, and installer payload placement.

The fork keeps the existing `UAC::_` entry point, `UAC.nsh` macros, NSIS
register protocol, and Win32 Unicode ABI. It adds process-image and account
checks to the elevation handshake, explicit state transfer for install scope
and components, and error reporting for the installer. The post-elevation
window handoff has no synchronous cross-process GUI call: the outer IPC thread
stores its window handle and page offset in the shared mapping. The inner GUI
reads that state, restores and activates its own window, and posts a hide
request to the outer GUI. If the state is invalid, the inner window still
becomes visible. Focus denial flashes the taskbar button.

The handshake has a ten-second readiness limit and five-second message calls.
Register exchange has a thirty-second limit; code segments have a thirty-minute
limit. An operation failure is sticky for the session, so an uncertain result
is not retried. The inner side waits on both the outer process and IPC thread;
the thread handle is duplicated during the authenticated handshake, including
when elevation uses another administrator account.

The build script selects an installed x86 MSVC 14.x toolset deterministically:
14.44 is preferred for the supported release toolchain, and an explicit
`-VcToolsVersion` can select another installed version. Only Win32 Unicode is
built. `prepare-installer.ps1` synchronizes this fork's `UAC.nsh` and DLL to
the two NSIS package locations.

The installer stores privileged shell helpers and DLLs under Common Files,
resets inherited ACLs, rejects reparse paths, and checks package SHA-256 values
before registering the DLL and property schema. The previous ProgramData
registration is removed by owned registry cleanup without loading its DLL.

Windows 7 compatibility is an intended source and toolchain target. Native
Windows 7 and interactive UAC scenarios require separate machine checks; a
successful compile is not evidence that those scenarios passed.
