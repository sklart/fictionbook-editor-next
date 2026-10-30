#include "script-assignment-test-pch.h"
#include "../../src/common/DeploymentContext.h"
#include "../../src/fbe/scripts/ScriptRegistry.h"
#include "../../src/fbe/scripts/ScriptToolbarPersistence.h"
#include "../../src/fbe/toolbars/PortableToolbarStore.h"
#include "../../src/fbe/toolbars/ToolbarsV2Codec.h"
#include "../../src/fbe/toolbars/ScriptToolbarItemText.h"
#include <cstdio>
#include <stdexcept>

using FbeScripts::ScriptRegistry;

static void Require(bool ok, const char* message) { if(!ok) throw std::runtime_error(message); }
static CString Settings() { return CString(DeploymentContext::SettingsDirectory().c_str()); }
static void Write(const CString& path, const CString& text)
{
	HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
	Require(file != INVALID_HANDLE_VALUE, "fixture open");
	DWORD written = 0; const DWORD bytes = static_cast<DWORD>(text.GetLength()) * sizeof(wchar_t);
	const bool ok = WriteFile(file, text, bytes, &written, NULL) && written == bytes;
	CloseHandle(file); Require(ok, "fixture write");
}
static CString Read(const CString& path)
{
	HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	Require(file != INVALID_HANDLE_VALUE, "fixture read open");
	const DWORD bytes = GetFileSize(file, NULL); std::vector<wchar_t> data(bytes / 2 + 1, 0); DWORD read = 0;
	const bool ok = ReadFile(file, data.data(), bytes, &read, NULL) && read == bytes;
	CloseHandle(file); Require(ok, "fixture read"); return CString(data.data());
}
static void Reset()
{
	DeleteFileW(Settings() + L"ScriptRegistry.xml"); DeleteFileW(Settings() + L"Toolbars.xml");
}
static ScriptDescriptor Script(const wchar_t* relative, const wchar_t* text = L"function Run() {}")
{
	ScriptDescriptor item = {}; item.relativePath = relative; item.name = relative;
	item.path = Settings() + relative; Write(item.path, text); return item;
}
static std::vector<ScriptDescriptor> Resolve(ScriptRegistry& registry, const ScriptDescriptor& item)
{
	std::vector<ScriptDescriptor> items(1, item); Require(registry.Resolve(items), "resolve"); return items;
}
static void TestIdentity()
{
	Reset(); ScriptRegistry registry;
	auto original = Resolve(registry, Script(L"z-original.js")); const CString uid = original[0].uid;
	std::vector<ScriptDescriptor> copied = { Script(L"a-copy.js"), original[0] };
	Require(registry.Resolve(copied), "copied scripts");
	Require(copied[1].uid == uid && copied[0].uid != uid, "new identical file stole existing path UID");
	Reset(); ScriptRegistry duplicates;
	std::vector<ScriptDescriptor> identical = { Script(L"one.js"), Script(L"two.js") };
	Require(duplicates.Resolve(identical), "duplicate content");
	const CString first = identical[0].uid, second = identical[1].uid;
	auto moved = Resolve(duplicates, Script(L"moved.js"));
	Require(moved[0].uid != first && moved[0].uid != second, "ambiguous fingerprint reused");
	Reset(); ScriptRegistry rename; auto before = Resolve(rename, Script(L"before.js"));
	auto after = Resolve(rename, Script(L"after.js")); Require(before[0].uid == after[0].uid, "unique rename lost UID");
	Reset(); ScriptRegistry ambiguousCopies;
	auto old = Resolve(ambiguousCopies, Script(L"old.js", L"same content"));
	PortableToolbarLayout layout; layout.scriptsToolbarPresent = true;
	PortableToolbarItem item = {}; item.scriptUid = old[0].uid; layout.scripts.push_back(item);
	Require(FbeScripts::ScriptToolbarPersistence::Save(layout), "save ambiguous assignment");
	std::vector<ScriptDescriptor> replacements = { Script(L"new-a.js", L"same content"), Script(L"new-b.js", L"same content") };
	Require(ambiguousCopies.Resolve(replacements), "ambiguous replacements resolve");
	Require(replacements[0].uid != old[0].uid && replacements[1].uid != old[0].uid &&
		replacements[0].uid != replacements[1].uid, "ambiguous replacements guessed old UID");
	Require(ambiguousCopies.FindByUid(old[0].uid) != nullptr, "ambiguous old identity was discarded");
	const CString firstReplacement = replacements[0].uid, secondReplacement = replacements[1].uid;
	ScriptRegistry reopened;
	Require(reopened.Resolve(replacements), "reopen ambiguous replacements");
	Require(replacements[0].uid == firstReplacement && replacements[1].uid == secondReplacement &&
		reopened.FindByUid(old[0].uid) != nullptr, "ambiguous identities changed after reopen");
	PortableToolbarLayout retained;
	Require(FbeScripts::ScriptToolbarPersistence::Load(retained) && retained.scripts.size() == 1 &&
		retained.scripts[0].scriptUid == old[0].uid, "ambiguous replacement changed toolbar assignment");
}
static void TestStaleAndFailure()
{
	Reset(); ScriptRegistry first, stale, other;
	auto a = Resolve(first, Script(L"a.js", L"a")); Require(stale.Load(), "stale load");
	auto b = Resolve(other, Script(L"b.js", L"b"));
	auto c = Resolve(stale, Script(L"c.js", L"c"));
	ScriptRegistry loaded; Require(loaded.Load() && loaded.FindByUid(a[0].uid) && loaded.FindByUid(b[0].uid) && loaded.FindByUid(c[0].uid), "stale writer erased identities");
	const CString old = Read(Settings() + L"ScriptRegistry.xml");
	HANDLE blocked = CreateFileW(Settings() + L"ScriptRegistry.xml", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	Require(blocked != INVALID_HANDLE_VALUE, "replacement blocker");
	std::vector<ScriptDescriptor> failed(1, Script(L"failed.js", L"failed"));
	const bool result = stale.Resolve(failed); CloseHandle(blocked);
	Require(!result && failed[0].uid.IsEmpty(), "failed save exposed new UID");
	Require(Read(Settings() + L"ScriptRegistry.xml") == old, "failed save damaged registry");
	Write(Settings() + L"ScriptRegistry.xml", L"<Scripts version=\"1\"><Script uid=\"broken\" /></Scripts>");
	const CString malformed = Read(Settings() + L"ScriptRegistry.xml");
	Require(!stale.Resolve(failed) && Read(Settings() + L"ScriptRegistry.xml") == malformed, "malformed registry overwritten");
}
static void TestMetadata()
{
	Reset(); ScriptRegistry registry;
	auto scripts = Resolve(registry, Script(L"saved.js"));
	PortableToolbarLayout layout; layout.scriptsToolbarPresent = true;
	PortableToolbarItem item = {}; item.scriptUid = scripts[0].uid; layout.scripts.push_back(item);
	Require(FbeScripts::ScriptToolbarPersistence::Save(layout), "describe save");
	PortableToolbarLayout saved; Require(FbeScripts::ScriptToolbarPersistence::Load(saved), "describe reload");
	const auto& described = saved.scripts[0];
	Require(described.scriptName == scripts[0].name && described.relativePath == scripts[0].relativePath, "assignment metadata missing");
	DeleteFileW(Settings() + L"ScriptRegistry.xml");
	Require(FbeScripts::ScriptToolbarPersistence::Load(saved) && saved.scripts[0].relativePath == L"saved.js", "missing registry lost metadata");
	Require(ScriptToolbarItemText::UnavailableLabel(saved.scripts[0], L"Недоступный скрипт").Find(L"saved.js") >= 0, "unavailable label missing path");
	Require(ScriptToolbarItemText::UnavailableDetails(saved.scripts[0], L"Недоступный скрипт").Find(scripts[0].uid) >= 0, "unavailable tooltip missing UID");
	ScriptRegistry recovered; auto returned = Resolve(recovered, Script(L"saved.js"));
	Require(returned[0].uid == scripts[0].uid, "assignment did not recover deleted registry");
	Require(FbeScripts::ScriptToolbarPersistence::Save(saved), "unavailable save");
	// Старая v2 без метаданных читается; неизвестный UID остаётся видимым.
	PortableToolbarLayout legacy;
	Require(ToolbarsV2Codec::Parse(L"<Toolbars version=\"2\"><Toolbar id=\"scripts-main\" kind=\"scripts\"><Script uid=\"legacy-unknown\" /></Toolbar></Toolbars>", legacy), "legacy v2");
	Require(ScriptToolbarItemText::UnavailableLabel(legacy.scripts[0], L"Недоступен").Find(L"legacy-unknown") >= 0, "unknown UID invisible");
	item.scriptName = L"Имя & \"кавычки\" <текст>"; item.relativePath = L"папка/скрипт & имя.js";
	layout.scripts[0] = item;
	PortableToolbarLayout escaped; Require(ToolbarsV2Codec::Parse(ToolbarsV2Codec::Serialize(layout), escaped), "metadata XML parse");
	Require(escaped.scripts[0].scriptName == item.scriptName && escaped.scripts[0].relativePath == item.relativePath, "metadata XML escaping");
	Write(Settings() + L"ScriptRegistry.xml", L"<Scripts version=\"1\"><Script uid=\"legacy-name\" path=\"legacy.js\" fingerprint=\"\" /></Scripts>");
	layout.scripts[0].scriptUid = L"legacy-name";
	Require(FbeScripts::ScriptToolbarPersistence::Save(layout) && FbeScripts::ScriptToolbarPersistence::Load(saved) && saved.scripts[0].scriptName == item.scriptName, "legacy registry erased saved name");
}
static PROCESS_INFORMATION Worker(const CString& token)
{
	wchar_t exe[32768] = {}; GetModuleFileNameW(NULL, exe, _countof(exe));
	CString command; command.Format(L"\"%s\" %s --worker %s", exe,
		DeploymentContext::CurrentMode() == DeploymentContext::Mode::Portable ? L"--portable" : L"--installed", static_cast<LPCWSTR>(token));
	STARTUPINFOW startup = {}; startup.cb = sizeof(startup); PROCESS_INFORMATION process = {};
	Require(CreateProcessW(exe, command.GetBuffer(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &startup, &process) != FALSE, "worker create");
	command.ReleaseBuffer(); CloseHandle(process.hThread); return process;
}
static void TestProcesses()
{
	Reset(); Script(L"parallel.js");
	HANDLE lock = CreateFileW(Settings() + L"ScriptRegistry.xml.lock", GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_FLAG_DELETE_ON_CLOSE, NULL);
	Require(lock != INVALID_HANDLE_VALUE, "test lock");
	PROCESS_INFORMATION left = Worker(L"left"), right = Worker(L"right");
	Sleep(150); Require(WaitForSingleObject(left.hProcess, 0) == WAIT_TIMEOUT, "worker ignored profile lock");
	CloseHandle(lock);
	for(auto process : { left.hProcess, right.hProcess }) {
		Require(WaitForSingleObject(process, 10000) == WAIT_OBJECT_0, "worker timeout"); DWORD code = 1;
		GetExitCodeProcess(process, &code); CloseHandle(process); Require(code == 0, "worker failed");
	}
	Require(Read(Settings() + L"left.uid") == Read(Settings() + L"right.uid"), "concurrent processes assigned different UIDs");
}
int wmain(int argc, wchar_t** argv)
{
	try {
		Require(DeploymentContext::CurrentMode() == DeploymentContext::Mode::Portable || !DeploymentContext::TestSettingsDirectory().empty(), "fixture must be isolated");
		if(DeploymentContext::CurrentMode() == DeploymentContext::Mode::Portable) CreateDirectoryW(DeploymentContext::DataRoot().c_str(), NULL);
		CreateDirectoryW(Settings(), NULL);
		if(argc == 4 && wcscmp(argv[2], L"--worker") == 0) {
			ScriptDescriptor item = {}; item.name = L"parallel"; item.relativePath = L"parallel.js"; item.path = Settings() + L"parallel.js";
			ScriptRegistry registry; auto assigned = Resolve(registry, item); Write(Settings() + argv[3] + L".uid", assigned[0].uid); return 0;
		}
		TestIdentity(); TestStaleAndFailure(); TestMetadata(); TestProcesses();
		puts("Script assignments: identity, stale writers, failure, metadata, recovery and process locking passed."); return 0;
	} catch(const std::exception& error) { fprintf(stderr, "%s\n", error.what()); return 1; }
}
