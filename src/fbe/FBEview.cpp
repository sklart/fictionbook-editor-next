// FBEView.cpp : implementation of the CFBEView class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "CaretFocusDiagnostics.h"
#include "structure/BodyStructuralEditor.h"
#include "ReplacementPreflight.h"
#include "search/ReplacementParser.h"
#include "search/ScintillaRegexSearch.h"
#include "LinkNavigation.h"
#include "navigation/LinkDomNavigation.h"
#include "navigation/ReferenceNavigation.h"
#include "ImageImport.h"
#include "res1.h"

#include "utils.h"
#include "apputils.h"

#include "FBEView.h"
#include "FBDoc.h"
#include "SearchReplace.h"
#include "search\\SearchViewportPosition.h"
#include "search\\SearchViewportResults.h"
#include "Scintilla.h"
#include "ElementDescMnr.h"
#include "StartupTrace.h"
#include "RuntimeLocalization.h"
#include "table/TableGrid.h"
#include "table/TableStructuralEditor.h"
#include "view/VisualDomNormalizer.h"
#include "dom/MarkupUndoUnitScope.h"
#include "dom/SelectionRangeMapper.h"
#include "image/ImageDocumentInserter.h"
#include "clipboard/ClipboardPastePreparer.h"
#include "clipboard/HtmlPasteSanitizer.h"
#include "undo/UndoFailureRecovery.h"
#include "testing/RuntimeTestScenarioMode.h"
#include <vector>
#include <memory>
using FbeTable::Grid;

class CSearchHighlightOverlay;
static void DestroySearchHighlightOverlay(CSearchHighlightOverlay* overlay);
extern const IID DIID_FBEHTMLElementEvents2 = __uuidof(MSHTML::HTMLElementEvents2);

extern CElementDescMnr _EDMnr;

static bool IsSecondSetExternalFaultEnabled()
{
	wchar_t testMode[4] = {};
	const DWORD testModeLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_MODE", testMode, _countof(testMode));
	if (!StartupTrace::Enabled() || testModeLength != 1 || testMode[0] != L'1') return false;
	wchar_t fault[64] = {};
	const DWORD faultLength = ::GetEnvironmentVariable(L"FBE_NEXT_FAULT_INJECT", fault, _countof(fault));
	return faultLength == 19 && _wcsicmp(fault, L"second-set-external") == 0;
}

// Runtime regressions sometimes need the DOM produced by MSHTML before the
// editor's post-paste normalizer runs.  This is deliberately inert outside
// the named test scenario and is not an editor-facing diagnostics API.
static void CaptureClipboardTitlePasteDomForTest(MSHTML::IHTMLDocument2Ptr document)
{
	wchar_t mode[4] = {}, scenario[64] = {}, tracePath[MAX_PATH] = {};
	if (::GetEnvironmentVariable(L"FBE_NEXT_TEST_MODE", mode, _countof(mode)) != 1 || mode[0] != L'1' ||
		::GetEnvironmentVariable(L"FBE_NEXT_TEST_SCENARIO", scenario, _countof(scenario)) != wcslen(L"clipboard-title-paste") ||
		wcscmp(scenario, L"clipboard-title-paste") != 0 ||
		::GetEnvironmentVariable(L"FBE_NEXT_TEST_PASTE_TRACE", tracePath, _countof(tracePath)) == 0)
		return;
	MSHTML::IHTMLElementPtr editable(document ? MSHTML::IHTMLElementPtr(document->all->item(L"fbw_body")) : MSHTML::IHTMLElementPtr());
	if (!editable) return;
	const CStringA utf8(CW2A(static_cast<LPCWSTR>(editable->innerHTML), CP_UTF8));
	HANDLE trace = ::CreateFile(tracePath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (trace == INVALID_HANDLE_VALUE) return;
	DWORD written = 0;
	::WriteFile(trace, static_cast<LPCSTR>(utf8), static_cast<DWORD>(utf8.GetLength()), &written, NULL);
	::CloseHandle(trace);
}

static void WarnUnsupportedHtmlPaste(HWND owner)
{
	wchar_t testMode[4] = {}, scenario[64] = {};
	const bool unattendedPasteTest = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_MODE", testMode, _countof(testMode)) == 1 &&
		testMode[0] == L'1' && ::GetEnvironmentVariable(L"FBE_NEXT_TEST_SCENARIO", scenario, _countof(scenario)) > 0 &&
		(wcscmp(scenario, L"html-paste-runtime") == 0 || wcscmp(scenario, L"community-native-copy-paste") == 0);
	if (!unattendedPasteTest) U::MessageBox(owner, FbeLoadRuntimeStringByKey(L"fbe.paste.unsupported_html",
		L"This HTML cannot be pasted safely without losing content. The document was not changed."),
		L"FictionBook Editor", MB_OK | MB_ICONWARNING);
}

// normalization helpers
static void NotifyTableStructureChanged(HWND frame, HWND view);

class PasteEnableScope {
public:
	explicit PasteEnableScope(int& counter) : m_counter(counter), m_active(true) { ++m_counter; }
	~PasteEnableScope() { if (m_active) --m_counter; }
	void Close() { if (m_active) { --m_counter; m_active = false; } }

private:
	int& m_counter;
	bool m_active;
};

struct PasteChildSnapshot {
	MSHTML::IHTMLDOMNodePtr node;
	MSHTML::IHTMLDOMNodePtr clone;
	CString html;
};

static std::vector<PasteChildSnapshot> CapturePasteChildren(MSHTML::IHTMLDOMNodePtr parent,
	MSHTML::IHTMLDOMNodePtr cloneOnly = MSHTML::IHTMLDOMNodePtr(), bool cloneAll = true) {
	std::vector<PasteChildSnapshot> children;
	for (MSHTML::IHTMLDOMNodePtr child(parent ? parent->firstChild : MSHTML::IHTMLDOMNodePtr()); child; child = child->nextSibling) {
		PasteChildSnapshot snapshot;
		snapshot.node = child;
		if (cloneAll || (cloneOnly && (child == cloneOnly || child == cloneOnly->previousSibling ||
			child == cloneOnly->nextSibling))) snapshot.clone = child->cloneNode(VARIANT_TRUE);
		MSHTML::IHTMLElementPtr element(child);
		snapshot.html = element ? CString(static_cast<LPCWSTR>(element->outerHTML)) : CString(static_cast<LPCWSTR>(_bstr_t(child->nodeValue)));
		children.push_back(snapshot);
	}
	return children;
}

class PasteBlockedUndoUnit : public CComObjectRootEx<CComSingleThreadModel>, public IOleParentUndoUnit {
public:
	BEGIN_COM_MAP(PasteBlockedUndoUnit)
		COM_INTERFACE_ENTRY(IOleUndoUnit)
		COM_INTERFACE_ENTRY(IOleParentUndoUnit)
	END_COM_MAP()
	STDMETHOD(Do)(IOleUndoManager*) { return E_UNEXPECTED; }
	STDMETHOD(GetDescription)(BSTR* description) {
		if (!description) return E_POINTER;
		*description = ::SysAllocString(L"Paste internal changes");
		return *description ? S_OK : E_OUTOFMEMORY;
	}
	STDMETHOD(GetUnitType)(CLSID* classId, LONG* id) {
		if (!classId || !id) return E_POINTER;
		*classId = CLSID_NULL; *id = 0; return S_OK;
	}
	STDMETHOD(OnNextAdd)() { return S_OK; }
	STDMETHOD(Open)(IOleParentUndoUnit*) { return S_OK; }
	STDMETHOD(Close)(IOleParentUndoUnit* child, BOOL) {
		return child == static_cast<IOleParentUndoUnit*>(this) ? S_FALSE : S_OK;
	}
	STDMETHOD(Add)(IOleUndoUnit*) { return S_OK; }
	STDMETHOD(FindUnit)(IOleUndoUnit*) { return S_FALSE; }
	STDMETHOD(GetParentState)(DWORD* state) {
		if (!state) return E_POINTER;
		*state = UAS_BLOCKED; return S_OK;
	}
};

class PasteUndoRecordingScope {
public:
	explicit PasteUndoRecordingScope(IOleUndoManager* manager, bool open = true) : m_manager(manager) {
		CComObject<PasteBlockedUndoUnit>* unit = NULL;
		if (m_manager && SUCCEEDED(CComObject<PasteBlockedUndoUnit>::CreateInstance(&unit))) {
			m_block = unit;
			if (open) Open();
		}
	}
	~PasteUndoRecordingScope() { const HRESULT hr = Close(); if (FAILED(hr)) StartupTrace::HResult(L"undo", L"D415", hr, L"Recording block Close failed"); }
	bool Blocked() const { return m_open; }
	HRESULT Open() { if (m_open) return S_OK; if (!m_manager || !m_block) return E_OUTOFMEMORY; const HRESULT hr = m_manager->Open(m_block); if (SUCCEEDED(hr)) m_open = true; return hr; }
	HRESULT Close() { if (!m_open) return S_OK; const HRESULT hr = m_manager->Close(m_block, FALSE); if (SUCCEEDED(hr)) m_open = false; return hr; }
private:
	CComPtr<IOleUndoManager> m_manager;
	CComPtr<IOleParentUndoUnit> m_block;
	bool m_open = false;
};

// Keep actual removed nodes until the inverse has been accepted. Reattaching
// these nodes preserves references held by older native MSHTML undo units.
struct PasteRestoreJournal {
	MSHTML::IHTMLDOMNodePtr parent, anchor;
	std::vector<MSHTML::IHTMLDOMNodePtr> original, replacement;
	HRESULT Rollback() {
		try {
			for (const auto& node : replacement) if (node->parentNode == parent) parent->removeChild(node);
			for (const auto& node : original) if (node->parentNode == parent) parent->removeChild(node);
			for (const auto& node : original) parent->insertBefore(node, anchor.GetInterfacePtr());
			return S_OK;
		} catch (const _com_error& error) { StartupTrace::HResult(L"undo", L"D421", error.Error(), L"DOM journal restore failed"); return E_ABORT; }
	}
};

class PasteDomUndoUnit : public CComObjectRootEx<CComSingleThreadModel>, public IOleUndoUnit {
public:
	BEGIN_COM_MAP(PasteDomUndoUnit)
		COM_INTERFACE_ENTRY(IOleUndoUnit)
	END_COM_MAP()
	void Initialize(MSHTML::IHTMLDOMNodePtr parent, std::vector<PasteChildSnapshot> before, std::vector<PasteChildSnapshot> after,
		MSHTML::IHTMLTxtRangePtr beforeSelection, MSHTML::IHTMLTxtRangePtr afterSelection,
		FB::Doc* document, bool beforeWasClean, bool beforeRecoveryClean, long saveVersion) {
		m_parent = parent; m_before = std::move(before); m_after = std::move(after);
		m_beforeSelection = beforeSelection; m_afterSelection = afterSelection;
		m_document = document; m_beforeWasClean = beforeWasClean;
		m_beforeRecoveryClean = beforeRecoveryClean; m_saveVersion = saveVersion;
		m_prefix = 0; m_suffix = 0;
		m_beforeRetained.clear(); m_afterRetained.clear();
		while (m_prefix < m_before.size() && m_prefix < m_after.size() && m_before[m_prefix].html == m_after[m_prefix].html) ++m_prefix;
		while (m_suffix < m_before.size() - m_prefix && m_suffix < m_after.size() - m_prefix &&
			m_before[m_before.size() - m_suffix - 1].html == m_after[m_after.size() - m_suffix - 1].html) ++m_suffix;
	}
	bool Changed() const { return m_before.size() - m_prefix - m_suffix || m_after.size() - m_prefix - m_suffix; }
	void RetainOriginalChildren() {
		for (size_t i = m_prefix; i < m_before.size() - m_suffix; ++i) m_beforeRetained.push_back(m_before[i].node);
		for (size_t i = m_prefix; i < m_after.size() - m_suffix; ++i) m_afterRetained.push_back(m_after[i].node);
	}
	const std::vector<PasteChildSnapshot>& Before() const { return m_before; }
	bool PrepareChangedClones() {
		for (size_t i = m_prefix; i < m_before.size() - m_suffix; ++i)
			if (!m_before[i].clone) return false;
		for (size_t i = m_prefix; i < m_after.size() - m_suffix; ++i)
			m_after[i].clone = m_after[i].node->cloneNode(VARIANT_TRUE);
		return true;
	}
	HRESULT Restore(bool before, PasteRestoreJournal* transaction = nullptr) {
		if (!m_parent) return E_UNEXPECTED;
		PasteRestoreJournal local;
		PasteRestoreJournal& journal = transaction ? *transaction : local;
		bool changing = false;
		try {
			journal.parent = m_parent;
			const size_t current = (before ? m_after : m_before).size() - m_prefix - m_suffix;
			const std::vector<PasteChildSnapshot>& wanted = before ? m_before : m_after;
			MSHTML::IHTMLDOMNodePtr node(m_parent->firstChild);
			for (size_t i = 0; i < m_prefix; ++i) { if (!node) return E_UNEXPECTED; node = node->nextSibling; }
			for (size_t i = 0; i < current; ++i) {
				if (!node) return E_UNEXPECTED;
				journal.original.push_back(node); node = node->nextSibling;
			}
			journal.anchor = node;
			// All cloning and vector allocation must finish before the first edit.
			const auto& retained = before ? m_beforeRetained : m_afterRetained;
			if (!retained.empty()) journal.replacement = retained;
			else for (size_t i = m_prefix; i < wanted.size() - m_suffix; ++i)
				journal.replacement.push_back(wanted[i].clone->cloneNode(VARIANT_TRUE));
			changing = true;
			for (const auto& old : journal.original) { m_parent->removeChild(old); CheckError(FbeUndo::Inject(L"html-remove")); }
			for (const auto& fresh : journal.replacement) { m_parent->insertBefore(fresh, journal.anchor.GetInterfacePtr()); CheckError(FbeUndo::Inject(L"html-insert")); }
			return S_OK;
		} catch (const _com_error& error) { return changing && FAILED(journal.Rollback()) ? E_ABORT : error.Error(); }
		catch (const std::bad_alloc&) { return changing && FAILED(journal.Rollback()) ? E_ABORT : E_OUTOFMEMORY; }
	}
	STDMETHOD(Do)(IOleUndoManager* manager) {
		try {
		CComPtr<IOleUndoManager> recordingManager(manager);
		if (!recordingManager) {
			IServiceProviderPtr provider(MSHTML::IHTMLElementPtr(m_parent)->document);
			if (provider) provider->QueryService(SID_SOleUndoManager, IID_IOleUndoManager, reinterpret_cast<void**>(&recordingManager));
		}
		PasteUndoRecordingScope compensation(recordingManager, false), recording(recordingManager);
		if (!recording.Blocked()) return E_UNEXPECTED;
		const long version = m_document ? m_document->m_body.GetVersionNumber() : -1;
		const bool clean = m_document && !m_document->DocChanged(), recoveryClean = m_document && !m_document->DocRelChanged();
		MSHTML::IHTMLTxtRangePtr oldSelection;
		try { MSHTML::IHTMLDocument2Ptr document(MSHTML::IHTMLElementPtr(m_parent)->document); oldSelection = document->selection->createRange(); } catch (const _com_error&) {}
		PasteRestoreJournal journal;
		HRESULT result = Restore(m_applyBefore, &journal);
		if (SUCCEEDED(result)) {
			result = FbeUndo::Inject(L"html-close");
			if (SUCCEEDED(result)) result = recording.Close();
			if (SUCCEEDED(result) && manager) { result = FbeUndo::Inject(L"html-add"); if (SUCCEEDED(result)) result = manager->Add(this); }
			if (FAILED(result)) {
				if (!recording.Blocked() && FAILED(compensation.Open())) return E_ABORT;
				if (FAILED(journal.Rollback())) return E_ABORT;
			}
		}
		if (FAILED(result)) {
			try { if (oldSelection) oldSelection->select(); } catch (const _com_error&) {}
			const HRESULT compensated = compensation.Close(), closed = recording.Close();
			if (m_document && result != E_ABORT) m_document->RestoreCleanAfterPasteUndo(version, clean, recoveryClean);
			return FAILED(compensated) || FAILED(closed) ? E_ABORT : result;
		}
		try {
			MSHTML::IHTMLTxtRangePtr selection(m_applyBefore ? m_beforeSelection : m_afterSelection);
			if (selection) selection->select();
		} catch (const _com_error&) {}
		if (m_applyBefore && m_document)
			m_document->RestoreCleanAfterPasteUndo(m_saveVersion, m_beforeWasClean, m_beforeRecoveryClean);
		// A later compensation/redo must restore the exact nodes removed now,
		// including references retained by earlier native undo units.
		(m_applyBefore ? m_afterRetained : m_beforeRetained).swap(journal.original);
		m_applyBefore = !m_applyBefore;
		return S_OK;
		} catch (const _com_error& error) { return error.Error(); }
		catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
	}
	STDMETHOD(GetDescription)(BSTR* description) {
		if (!description) return E_POINTER;
		*description = ::SysAllocString(L"Paste"); return *description ? S_OK : E_OUTOFMEMORY;
	}
	STDMETHOD(GetUnitType)(CLSID* classId, LONG* id) {
		if (!classId || !id) return E_POINTER;
		*classId = FbeUndo::UnitClass(); *id = 1; return S_OK;
	}
	STDMETHOD(OnNextAdd)() { return S_OK; }
private:
	MSHTML::IHTMLDOMNodePtr m_parent;
	MSHTML::IHTMLTxtRangePtr m_beforeSelection, m_afterSelection;
	FB::Doc* m_document = NULL;
	bool m_beforeWasClean = false;
	bool m_beforeRecoveryClean = false;
	long m_saveVersion = -1;
	std::vector<PasteChildSnapshot> m_before, m_after;
	std::vector<MSHTML::IHTMLDOMNodePtr> m_beforeRetained, m_afterRetained;
	size_t m_prefix = 0, m_suffix = 0;
	bool m_applyBefore = true;
};

static CComPtr<IOleUndoManager> GetPasteUndoManager(MSHTML::IHTMLDocument2Ptr document) {
	CComPtr<IOleUndoManager> manager;
	IServiceProviderPtr provider(document);
	if (provider) provider->QueryService(SID_SOleUndoManager, IID_IOleUndoManager, reinterpret_cast<void**>(&manager));
	return manager;
}

class DescriptionIdUndoUnit : public CComObjectRootEx<CComSingleThreadModel>, public IOleUndoUnit {
public:
	BEGIN_COM_MAP(DescriptionIdUndoUnit)
		COM_INTERFACE_ENTRY(IOleUndoUnit)
	END_COM_MAP()
	void Initialize(const GUID& identity, const CString& before, const CString& after) {
		m_identity = identity; m_before = before; m_after = after;
	}
	STDMETHOD(Do)(IOleUndoManager* manager) {
		FB::Doc* owner = FB::Doc::BinaryOwner(m_identity);
		if (!owner || !manager) return E_UNEXPECTED;
		try {
			MSHTML::IHTMLDocument3Ptr document(owner->m_body.Document());
			MSHTML::IHTMLInputTextElementPtr field(document ? document->getElementById(L"diID") : NULL);
			if (!field) return E_UNEXPECTED;
			const CString previous(static_cast<LPCWSTR>(field->value));
			const CString& expected = m_restoreBefore ? m_after : m_before;
			if (previous != expected) return E_UNEXPECTED;
			field->value = static_cast<LPCWSTR>(m_restoreBefore ? m_before : m_after);
			HRESULT result = FbeUndo::Inject(L"description-id-undo-add");
			if (SUCCEEDED(result)) result = manager->Add(this);
			if (FAILED(result)) { field->value = static_cast<LPCWSTR>(previous); return result; }
			m_restoreBefore = !m_restoreBefore;
			owner->m_body.MarkDescriptionFormChanged();
			return S_OK;
		} catch (const _com_error& error) { return error.Error(); }
		catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
	}
	STDMETHOD(GetDescription)(BSTR* description) {
		if (!description) return E_POINTER;
		*description = ::SysAllocString(L"Document ID"); return *description ? S_OK : E_OUTOFMEMORY;
	}
	STDMETHOD(GetUnitType)(CLSID* classId, LONG* id) {
		if (!classId || !id) return E_POINTER;
		*classId = FbeUndo::UnitClass(); *id = 2; return S_OK;
	}
	STDMETHOD(OnNextAdd)() { return S_OK; }
private:
	GUID m_identity = GUID_NULL;
	CString m_before, m_after;
	bool m_restoreBefore = true;
};

HRESULT CFBEView::SetGeneratedDocumentId(BSTR value) {
	if (!m_ownerDocument || !value || !*value) return E_INVALIDARG;
	try {
		MSHTML::IHTMLDocument3Ptr document(Document());
		MSHTML::IHTMLInputTextElementPtr field(document ? document->getElementById(L"diID") : NULL);
		CComPtr<IOleUndoManager> manager(GetPasteUndoManager(Document()));
		if (!field || !manager) return E_UNEXPECTED;
		const CString before(static_cast<LPCWSTR>(field->value)), after(value);
		if (before == after) return S_FALSE;
		CComObject<DescriptionIdUndoUnit>* raw = NULL;
		HRESULT result = CComObject<DescriptionIdUndoUnit>::CreateInstance(&raw);
		if (FAILED(result)) return result;
		CComPtr<IOleUndoUnit> unit(raw);
		raw->Initialize(m_ownerDocument->ClipboardIdentity(), before, after);
		field->value = static_cast<LPCWSTR>(after);
		result = FbeUndo::Inject(L"description-id-add");
		if (SUCCEEDED(result)) result = manager->Add(unit);
		if (FAILED(result)) { field->value = static_cast<LPCWSTR>(before); return result; }
		MarkDescriptionFormChanged();
		return S_OK;
	} catch (const _com_error& error) { return error.Error(); }
	catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
}

void CFBEView::MarkDescriptionFormChanged() {
	m_form_changed = true; m_form_cp = true;
	::PostMessage(m_frame, AU::WM_DESCRIPTION_FORM_CHANGED, 0, 0);
}

// The canceled browser event and the deferred editor command are separate
// transactions. Remove only the proven empty native event, retaining all
// previous native units and redo entries before starting the real insertion.
struct DeferredHtmlPaste {
    CString fragment, plainText;
    GUID origin = GUID_NULL;
    bool hasHtml = false, readableHtml = false, internal = false, hasText = false;
    bool continueAtCaret = false;
    MSHTML::IHTMLTxtRangePtr selection;
    DWORD clipboardSequence = 0;
    explicit DeferredHtmlPaste(CFBEView& view) {
        CheckError(FbeUndo::Inject(L"paste-capture"));
        const DWORD sequence = ::GetClipboardSequenceNumber();
        // Deterministic STA reentrancy probe: delayed clipboard rendering / COM
        // may dispatch our already posted message while capturing another event.
        // Never pump messages on the normal editor path.
        wchar_t testMode[2]={}, testCase[64]={}, scenario[64]={};
        if(::GetEnvironmentVariableW(L"FBE_NEXT_TEST_MODE",testMode,_countof(testMode))==1 && testMode[0]==L'1' &&
           ::GetEnvironmentVariableW(L"FBE_NEXT_TEST_SCENARIO",scenario,_countof(scenario))>0 && wcscmp(scenario,L"html-paste-runtime")==0 &&
           ::GetEnvironmentVariableW(L"FBE_NEXT_TEST_HTML_PASTE_CASE",testCase,_countof(testCase))>0 &&
           (wcscmp(testCase,L"queue-reentrant")==0 || wcscmp(testCase,L"queue-reentrant-post-failure")==0)) {
            MSG message={};
            if(::PeekMessage(&message,view,AU::WM_FINALIZE_HTML_PASTE,AU::WM_FINALIZE_HTML_PASTE,PM_REMOVE)) {
                ::SetEnvironmentVariableW(L"FBE_NEXT_TEST_PASTE_REENTERED",L"1");
                StartupTrace::Event(L"clipboard",L"CP298",L"count=1");
                ::DispatchMessage(&message);
                if(wcscmp(testCase,L"queue-reentrant-post-failure")==0)
                    ::SetEnvironmentVariableW(L"FBE_NEXT_UNDO_FAILURE",L"paste-post");
            }
        }
        selection = view.Document()->selection->createRange();
        hasHtml = ::IsClipboardFormatAvailable(::RegisterClipboardFormat(L"HTML Format")) != FALSE;
        readableHtml = hasHtml && FbeClipboard::HtmlPasteSanitizer::ReadClipboardFragment(view, fragment);
        internal = FbeClipboard::HtmlPasteSanitizer::GetInternalCopyOrigin(view, origin);
        hasText = FbeClipboard::HtmlPasteSanitizer::ReadPlainText(view, plainText);
        if (::GetClipboardSequenceNumber() != sequence) CheckError(E_ABORT);
        clipboardSequence = sequence;
    }
};

struct PendingHtmlPaste {
    CComPtr<IOleUndoManager> manager;
    MSHTML::IHTMLDocument2Ptr document;
    FbeUndo::HistoryCheckpoint history;
    FB::Doc::UndoSavepoints savepoints;
    unsigned long long state;
    MSHTML::IHTMLTxtRangePtr selection;
    std::vector<std::shared_ptr<DeferredHtmlPaste>> requests;
    size_t events = 0;
    bool posted = false;
    UINT_PTR generation = 0;
    PendingHtmlPaste(IOleUndoManager* undo, FB::Doc* owner) : manager(undo),
        document(owner->m_body.Document()), history(undo), savepoints(owner->CaptureUndoSavepoints()),
        state(FbeUndo::DocumentState(document)), selection(document->selection->createRange()) {}
    HRESULT Finish(FB::Doc* owner) {
        if (!owner || owner->m_body.Document() != document) return E_UNEXPECTED;
        try {
            if (FbeUndo::DocumentState(document) != state) return E_UNEXPECTED;
            DWORD open = 0;
            const HRESULT parentState = manager->GetOpenParentState(&open);
            if (parentState != S_FALSE) {
                CString diagnostic; diagnostic.Format(L"result=0x%08lX; number=0x%08lX", static_cast<unsigned long>(parentState), static_cast<unsigned long>(open));
                StartupTrace::HResult(L"clipboard", L"CP205", FAILED(parentState) ? parentState : E_UNEXPECTED, diagnostic);
                return E_UNEXPECTED;
            }
            const auto units = history.AddedUndoUnits(manager, events);
            for (auto unit = units.rbegin(); unit != units.rend(); ++unit) {
                const HRESULT undone = manager->UndoTo(*unit);
                if (FbeUndo::DocumentState(document) != state) return E_ABORT;
                if (FAILED(undone)) {
                    CheckError(history.Restore(manager));
                    owner->RestoreUndoSavepoints(savepoints);
                    if (selection) selection->select();
                    return undone;
                }
                CComPtr<IEnumOleUndoUnits> list; CheckError(manager->EnumRedoable(&list));
                CComPtr<IOleUndoUnit> inverse, other;
                if (list->Next(1, &inverse, nullptr) == S_OK) {
                    if (list->Next(1, &other, nullptr) != S_FALSE) return E_UNEXPECTED;
                    CheckError(manager->DiscardFrom(inverse));
                }
            }
            CheckError(history.Restore(manager));
            owner->RestoreUndoSavepoints(savepoints);
            if (selection) selection->select();
            return S_OK;
        } catch (const _com_error& error) { return error.Error(); }
        catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
    }
};

// В живой сборке FBE regex режима «Дизайн» всегда идёт через наш wrapper
// поверх PCRE2, поэтому здесь больше не нужна развилка на VBScript.RegExp.
static AU::RegExp CreateSearchRegExp()
{
	return new AU::IRegExp2();
}

static AU::ReMatches ExecuteSearchRegExp(AU::RegExp re, const CString& text)
{
	return re->Execute(text);
}

static AU::ReMatches ExecuteSearchRegExp(AU::RegExp re, MSHTML::IHTMLTxtRangePtr range)
{
	_bstr_t rangeText(range->text);
	return ExecuteSearchRegExp(re, CString(static_cast<const wchar_t*>(rangeText)));
}

static UINT GetSearchWindowDpi(HWND window)
{
	typedef UINT (WINAPI* GetDpiForWindowProc)(HWND);
	HMODULE user32 = ::GetModuleHandle(L"user32.dll");
	GetDpiForWindowProc getDpiForWindow = user32
		? reinterpret_cast<GetDpiForWindowProc>(::GetProcAddress(user32, "GetDpiForWindow")) : NULL;
	if(getDpiForWindow) return getDpiForWindow(window);
	HDC dc = ::GetDC(window);
	const UINT dpi = dc ? static_cast<UINT>(::GetDeviceCaps(dc, LOGPIXELSY)) : 96;
	if(dc) ::ReleaseDC(window, dc);
	return dpi ? dpi : 96;
}

static bool IsParagraphElement(MSHTML::IHTMLElementPtr elem)
{
	return (bool)elem && U::scmp(elem->tagName, L"P") == 0;
}

static const wchar_t* HResultName(HRESULT result)
{
	if (result == S_OK) return L"S_OK";
	if (result == E_FAIL) return L"E_FAIL";
	if (result == E_NOINTERFACE) return L"E_NOINTERFACE";
	if (result == E_POINTER) return L"E_POINTER";
	if (result == E_INVALIDARG) return L"E_INVALIDARG";
	if (result == E_UNEXPECTED) return L"E_UNEXPECTED";
	if (result == E_ACCESSDENIED) return L"E_ACCESSDENIED";
	if (result == E_OUTOFMEMORY) return L"E_OUTOFMEMORY";
	return L"unknown";
}

static void TraceSelectionContainerFailure(const wchar_t* comOperation, HRESULT result,
	const wchar_t* selectionType, long controlLength = -1)
{
	CString details;
	details.Format(L"component=MSHTML; operation=SelectionContainer; com-operation=%s; HRESULT=0x%08lX; HRESULT_NAME=%s; selection.type=%s; view=BODY; documentTree=%s; treeImages=%s",
		comOperation,
		static_cast<unsigned long>(result),
		HResultName(result),
		selectionType,
		_Settings.ViewDocumentTree() ? L"enabled" : L"disabled",
		_Settings.GetDocTreeItemState(L"Image", true) ? L"enabled" : L"disabled");
	if (controlLength >= 0)
	{
		CString length;
		length.Format(L"; control.length=%ld", controlLength);
		details += length;
	}
	StartupTrace::HResult(L"mshtml", L"SC100", result, details);
}
static bool SelectTableCellRange(const MSHTML::IHTMLDocument2Ptr& document,
	const MSHTML::IHTMLElementPtr& firstCell, const MSHTML::IHTMLElementPtr& lastCell)
{
	try
	{
		MSHTML::IHTMLElementPtr body(document ? document->body : MSHTML::IHTMLElementPtr());
		if (!body || !firstCell || !lastCell || FbeTable::FindTableElement(firstCell) != FbeTable::FindTableElement(lastCell)) return false;
		MSHTML::IHTMLTxtRangePtr range(MSHTML::IHTMLBodyElementPtr(body)->createTextRange());
		MSHTML::IHTMLTxtRangePtr end(MSHTML::IHTMLBodyElementPtr(body)->createTextRange());
		if (!range || !end) return false;
		range->moveToElementText(firstCell);
		if (firstCell != lastCell)
		{
			end->moveToElementText(lastCell);
			range->setEndPoint(L"EndToEnd", end);
		}
		else
			range->collapse(VARIANT_TRUE);
		range->select();
		return true;
	}
	catch (const _com_error&) { return false; }
}



static void SetTableCellHighlight(const MSHTML::IHTMLElementPtr& cell, const wchar_t* color)
{
	try {
		IDispatchPtr dispatch(cell);
		OLECHAR* propertyName = L"runtimeStyle";
		DISPID propertyId = DISPID_UNKNOWN;
		if (!dispatch || FAILED(dispatch->GetIDsOfNames(IID_NULL, &propertyName, 1, LOCALE_USER_DEFAULT, &propertyId))) return;
		DISPPARAMS arguments = {};
		_variant_t runtimeStyle;
		if (FAILED(dispatch->Invoke(propertyId, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_PROPERTYGET, &arguments, &runtimeStyle, NULL, NULL)) || runtimeStyle.vt != VT_DISPATCH) return;
		MSHTML::IHTMLStylePtr style;
		style = runtimeStyle.pdispVal;
		if (style) style->backgroundColor = color;
	}
	catch (const _com_error&) { }
}

static void UpdateTableCellHighlights(std::vector<MSHTML::IHTMLElementPtr>& previous,
	const std::vector<MSHTML::IHTMLElementPtr>& current)
{
	for (size_t index = 0; index < previous.size(); ++index) SetTableCellHighlight(previous[index], L"");
	previous = current;
	for (size_t index = 0; index < previous.size(); ++index) SetTableCellHighlight(previous[index], L"#B8D6FB");
}



static CString GetLocalizedMainMenuText(UINT commandId, const wchar_t* fallback)
{
	const wchar_t* runtimeKey = NULL;
	switch (commandId)
	{
	case ID_TABLE_INSERT_ROW_ABOVE: runtimeKey = L"fbe.menu.idr_mainframe.table.insert_row_above"; break;
	case ID_TABLE_INSERT_ROW_BELOW: runtimeKey = L"fbe.menu.idr_mainframe.table.insert_row_below"; break;
	case ID_TABLE_DELETE_ROW: runtimeKey = L"fbe.menu.idr_mainframe.table.delete_row"; break;
	case ID_TABLE_INSERT_COLUMN_LEFT: runtimeKey = L"fbe.menu.idr_mainframe.table.insert_column_left"; break;
	case ID_TABLE_INSERT_COLUMN_RIGHT: runtimeKey = L"fbe.menu.idr_mainframe.table.insert_column_right"; break;
	case ID_TABLE_DELETE_COLUMN: runtimeKey = L"fbe.menu.idr_mainframe.table.delete_column"; break;
	case ID_TABLE_MAKE_HEADER_CELLS: runtimeKey = L"fbe.menu.idr_mainframe.table.make_header_cells"; break;
	case ID_TABLE_MAKE_NORMAL_CELLS: runtimeKey = L"fbe.menu.idr_mainframe.table.make_normal_cells"; break;
	}
	if (runtimeKey != NULL)
		return FbeLoadRuntimeStringByKey(runtimeKey, fallback);

	CMenu mainMenu;
	if (mainMenu.LoadMenu(IDR_MAINFRAME))
	{
		wchar_t text[256] = {};
		if (mainMenu.GetMenuString(commandId, text, _countof(text), MF_BYCOMMAND) > 0)
			return CString(text);
	}
	return CString(fallback);
}

static void NotifyWrappedSearch(bool wrapped)
{
	if(wrapped)
		::MessageBeep(MB_ICONASTERISK);
}

static void ReleaseSearchRegExp(AU::RegExp& re)
{
	delete re;
	re = NULL;
}

// RAII owner for the regular-expression wrapper used by search.
// It keeps the wrapper valid for the whole operation and centralizes release
// when ownership moves to the saved match state.
class ScopedSearchRegExp
{
public:
	ScopedSearchRegExp()
		: m_re(CreateSearchRegExp())
	{
	}

	~ScopedSearchRegExp()
	{
		ReleaseSearchRegExp(m_re);
	}

	AU::RegExp get() const
	{
		return m_re;
	}

	AU::IRegExp2* operator->() const
	{
		return m_re;
	}

private:
	AU::RegExp m_re;
};

static void InitSearchRegExp(AU::RegExp re, int flags, const CString& pattern)
{
	re->IgnoreCase = flags & 4 ? VARIANT_FALSE : VARIANT_TRUE;
	re->Global = VARIANT_TRUE;
	re->Pattern = (const wchar_t*)pattern;
}

static void NormalizeSearchPatternNbsp(CString& pattern)
{
	if (_Settings.GetNBSPChar().Compare(L"\u00A0") != 0)
		pattern.Replace(L"\u00A0", _Settings.GetNBSPChar());
}

static void NormalizeReplacementNbsp(CString& replacement)
{
	if (_Settings.GetNBSPChar().Compare(L"\u00A0") != 0)
		replacement.Replace(L"\u00A0", _Settings.GetNBSPChar());
}

_ATL_FUNC_INFO CFBEView::DocumentCompleteInfo=
  { CC_STDCALL, VT_EMPTY, 2, { VT_DISPATCH, (VT_BYREF | VT_VARIANT) } };
_ATL_FUNC_INFO CFBEView::BeforeNavigateInfo=
  { CC_STDCALL, VT_EMPTY, 7, {
      VT_DISPATCH,
      (VT_BYREF | VT_VARIANT),
      (VT_BYREF | VT_VARIANT),
      (VT_BYREF | VT_VARIANT),
      (VT_BYREF | VT_VARIANT),
      (VT_BYREF | VT_VARIANT),
      (VT_BYREF | VT_BOOL),
    }
  };
_ATL_FUNC_INFO CFBEView::NavigateErrorInfo=
  { CC_STDCALL, VT_EMPTY, 5, { VT_DISPATCH, (VT_BYREF | VT_VARIANT), (VT_BYREF | VT_VARIANT), (VT_BYREF | VT_VARIANT), (VT_BYREF | VT_BOOL) } };_ATL_FUNC_INFO CFBEView::VoidInfo=
  { CC_STDCALL, VT_EMPTY, 0 };
_ATL_FUNC_INFO CFBEView::EventInfo=
  { CC_STDCALL, VT_BOOL, 1, { VT_DISPATCH } };
_ATL_FUNC_INFO CFBEView::VoidEventInfo=
  { CC_STDCALL, VT_EMPTY, 1, { VT_DISPATCH } };

LRESULT CFBEView::OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /* unused: bHandled */)
{
  if (DefWindowProc(uMsg,wParam,lParam))
    return 1;
  HRESULT hr = QueryControl(&m_browser);
  StartupTrace::HResult(L"webbrowser", L"WB111", hr, L"QueryControl(IWebBrowser2)");
  if (FAILED(hr) || !m_browser)
    return 1;

  hr = BrowserEvents::DispEventAdvise(m_browser, &DIID_DWebBrowserEvents2);
  StartupTrace::HResult(L"webbrowser", L"WB112", hr, L"BrowserEvents::DispEventAdvise");
  if (FAILED(hr))
    return 1;
  if (m_normalize) {
    CMessageLoop* loop = _Module.GetMessageLoop();
    if (!loop || !loop->AddIdleHandler(this)) return -1;
    m_pasteIdleLoop = loop;
  }
  m_last_browser_event = L"BrowserEventsAdvised";
  return 0;
}

CFBEView::~CFBEView()
{
	// Run removes the module's loop registration before destroying the frame;
	// the loop itself outlives the frame and all its documents.
	if (m_pasteIdleLoop) m_pasteIdleLoop->RemoveIdleHandler(this);
	ResetPendingDrop();
	DestroySearchHighlightOverlay(m_search_highlight_overlay);
	m_search_highlight_overlay = NULL;
	if(HasDoc())
	{
		// Init can fail after acquiring the document but before all event sinks and
		// markup services are available. Teardown must be best-effort in that case.
		DocumentEvents::DispEventUnadvise(Document(), &DIID_HTMLDocumentEvents2);
		if (m_scroll_event_element)
			ScrollEvents::DispEventUnadvise(m_scroll_event_element, &DIID_FBEHTMLElementEvents2);
		try
		{
			MSHTML::IHTMLElementPtr body;
			if (SUCCEEDED(m_hdoc->get_body(&body)) && body)
				TextEvents::DispEventUnadvise(body, &DIID_HTMLTextContainerEvents2);
		}
		catch (const _com_error&)
		{
		}
		if (m_mkc && m_dirtyRangeCookie)
			m_mkc->UnRegisterForDirtyRange(m_dirtyRangeCookie);
	}
	if(m_browser)
		BrowserEvents::DispEventUnadvise(m_browser, &DIID_DWebBrowserEvents2);

	if(m_find_dlg)
	{
		CloseFindDialog(m_find_dlg);
		delete m_find_dlg;
	}
	::SendMessage(m_frame, AU::WM_DETACH_FIND_RESULTS_PANE, reinterpret_cast<WPARAM>(this), 0);
}

LRESULT CFBEView::OnSize(UINT message, WPARAM wParam, LPARAM lParam, BOOL&)
{
	// Let the ActiveX host lay out its browser children before positioning
	// the overlay. The outer pane container must not resize those descendants.
	const LRESULT result = DefWindowProc(message, wParam, lParam);
	// The highlight overlay is a separate non-activating owner popup so it is
	// not a layered child window (unsupported by Windows 7).  Resize events are
	// enough to reposition it without a permanent UI polling timer.
	RefreshSearchHighlights();
	return result;
}

// Search highlighting is deliberately a native overlay.  Styling ranges through
// MSHTML would mutate the FB2 DOM, create undo entries and mark the document
// dirty merely for showing Find All results.
class CSearchHighlightOverlay : public CWindowImpl<CSearchHighlightOverlay>
{
public:
	DECLARE_WND_CLASS(L"FbeSearchHighlightOverlay")

	BEGIN_MSG_MAP(CSearchHighlightOverlay)
		MESSAGE_HANDLER(WM_PAINT, OnPaint)
	END_MSG_MAP()

	void Update(HWND parent, const std::vector<RECT>& documentRects, long scrollLeft,
		long scrollTop, std::size_t selected)
	{
		m_documentRects = documentRects;
		m_scrollLeft = scrollLeft;
		m_scrollTop = scrollTop;
		m_selected = selected;
		RECT client = {};
		::GetClientRect(parent, &client);
		POINT origin = { client.left, client.top };
		::ClientToScreen(parent, &origin);
		if (!m_hWnd)
		{
			// Layered *child* windows are unsupported on Windows 7.  An owned
			// popup is supported there and also remains above the hosted browser.
			Create(parent, client, NULL, WS_POPUP | WS_DISABLED,
				WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW);
			if (m_hWnd)
				::SetLayeredWindowAttributes(m_hWnd, RGB(0, 0, 0), 0, LWA_COLORKEY);
		}
		if (m_hWnd)
		{
			// Keep the owned popup immediately above the hosted editor, but below
			// modeless Find/Results windows. HWND_TOP would paint hit rectangles over
			// those controls when an explicit Find All opens the Results pane.
			SetWindowPos(parent, origin.x, origin.y, client.right, client.bottom, SWP_NOACTIVATE | SWP_SHOWWINDOW);
			Invalidate();
		}
	}

	void UpdateScroll(long scrollLeft, long scrollTop)
	{
		if (m_scrollLeft == scrollLeft && m_scrollTop == scrollTop) return;
		m_scrollLeft = scrollLeft;
		m_scrollTop = scrollTop;
		if (m_hWnd) Invalidate();
	}

	void Clear()
	{
		m_documentRects.clear();
		if (m_hWnd) ShowWindow(SW_HIDE);
	}

	LRESULT OnPaint(UINT, WPARAM, LPARAM, BOOL&)
	{
		CPaintDC dc(m_hWnd);
		RECT client = {};
		GetClientRect(&client);
		dc.FillSolidRect(&client, RGB(0, 0, 0)); // colour-keyed transparent
		HBRUSH selectedBrush = ::CreateSolidBrush(RGB(255, 128, 0));
		HBRUSH normalBrush = ::CreateSolidBrush(RGB(255, 215, 0));
		for (std::size_t index = 0; index < m_documentRects.size(); ++index)
		{
			RECT rect = m_documentRects[index];
			::OffsetRect(&rect, -m_scrollLeft, -m_scrollTop);
			if (rect.right <= rect.left) rect.right = rect.left + 1;
			if (rect.bottom <= rect.top) rect.bottom = rect.top + 1;
			::FrameRect(dc, &rect, index == m_selected ? selectedBrush : normalBrush);
		}
		::DeleteObject(normalBrush);
		::DeleteObject(selectedBrush);
		return 0;
	}

private:
	std::vector<RECT> m_documentRects;
	long m_scrollLeft = 0;
	long m_scrollTop = 0;
	std::size_t m_selected = static_cast<std::size_t>(-1);
};

static void DestroySearchHighlightOverlay(CSearchHighlightOverlay* overlay)
{
	delete overlay;
}

BOOL CFBEView::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg && pMsg->message == WM_LBUTTONDOWN && m_normalize && HasDoc() &&
		::IsChild(m_hWnd, pMsg->hwnd)) {
		ResetPendingDrop();
		if (!m_drop_undo_open) {
			try {
				IDispatchPtr selected(Document()->selection->createRange());
				CComQIPtr<MSHTML::IHTMLTxtRange> text(selected);
				CComQIPtr<MSHTML::IHTMLControlRange> control(selected);
				if ((text && text->text.length() != 0) || (control && control->length > 0)) {
					// MSHTML's mouse-down editing unit precedes even ondragstart.
					// Open outside that native transaction; a plain selection click
					// later discards this empty parent after the browser's mouseup.
					OpenDropUndoGroup(m_drop_undo_manager, m_drop_undo_parent);
					m_drop_undo_open = true; m_drag_mouse_armed = true;
					m_drop_version = GetVersionNumber();
				}
			} catch (const _com_error&) { ResetPendingDrop(); }
		}
	}
	if (pMsg && pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_TAB &&
		MoveTableCell((::GetKeyState(VK_SHIFT) & 0x8000) != 0))
		return TRUE;
	return SendMessage(WM_FORWARDMSG,0,(LPARAM)pMsg)!=0;
}

// editing commands
LRESULT CFBEView::ExecCommand(int cmd) {
  const LRESULT result = ExecCommandCore(cmd);
  if (cmd == IDM_UNDO && m_ownerDocument) {
    m_ownerDocument->RestoreCleanIfBodyMatches();
    if (SUCCEEDED(result)) m_ownerDocument->ReconcileNewTemplateAfterUndo();
    if (SUCCEEDED(result)) m_ownerDocument->ReconcileDescriptionSavepoint();
  }
  return result;
}

LRESULT CFBEView::ExecCommandCore(int cmd) {
  // Native and MSHTML-owned history may both change binary nodes/expandos.
  struct BinaryHistoryInvalidation {
    FB::Doc* doc;
    ~BinaryHistoryInvalidation() { if (doc) doc->m_binaryStore.Invalidate(); }
  } binaryHistory{(cmd == IDM_UNDO || cmd == IDM_REDO) ? m_ownerDocument : nullptr};
  IOleCommandTargetPtr	  ct(m_browser);
  if (!ct) return E_NOINTERFACE;
  if ((cmd != IDM_UNDO && cmd != IDM_REDO) || !m_ownerDocument)
    return ct->Exec(&CGID_MSHTML,cmd,0,NULL,NULL);
  try {
    CComPtr<IOleUndoManager> manager(GetPasteUndoManager(Document()));
    if (!manager) return E_NOINTERFACE;
    FbeUndo::HistoryCheckpoint history(manager);
    if (!history.Owned(cmd == IDM_REDO)) return ct->Exec(&CGID_MSHTML,cmd,0,NULL,NULL);
    const auto savepoints = m_ownerDocument->CaptureUndoSavepoints();
    const unsigned long long before = FbeUndo::DocumentState(Document());
    MSHTML::IHTMLTxtRangePtr selection;
    try { selection = Document()->selection->createRange(); } catch (const _com_error&) {}
    const HRESULT hr = ct->Exec(&CGID_MSHTML,cmd,0,NULL,NULL);
    if (FAILED(hr)) {
      // A failed Do causes OLE to compensate and clear both stacks. Restore
      // retained entries only after proving the document is unchanged.
      if (before == FbeUndo::DocumentState(Document())) {
        const HRESULT restored = history.Restore(manager);
        StartupTrace::HResult(L"undo", L"D420", restored, L"Restore history after compensated undo failure");
        if (FAILED(restored)) return restored;
        m_ownerDocument->RestoreUndoSavepoints(savepoints);
        try { if (selection) selection->select(); } catch (const _com_error&) {}
      } else return E_ABORT;
    }
    return hr;
  } catch (const _com_error& error) { return error.Error(); }
  catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
}

void	  CFBEView::QueryStatus(OLECMD *cmd,int ncmd) {
  IOleCommandTargetPtr	  ct(m_browser);
  if (ct)
    ct->QueryStatus(&CGID_MSHTML,ncmd,cmd,NULL);
}

CString	  CFBEView::QueryCmdText(int cmd) {
  IOleCommandTargetPtr	  ct(m_browser);
  if (ct) {
    OLECMD oc={static_cast<ULONG>(cmd)};
    struct {
      OLECMDTEXT	oct;
      wchar_t		buffer[512];
    } oct={ { OLECMDTEXTF_NAME, 0, 512 } };
    if (SUCCEEDED(ct->QueryStatus(&CGID_MSHTML,1,&oc,&oct.oct)))
      return oct.oct.rgwz;
  }
  return CString();
}

LRESULT CFBEView::OnStyleLink(WORD, WORD, HWND, BOOL&) {
  try {
    if (Document()->execCommand(L"CreateLink",VARIANT_FALSE,_variant_t(L""))==VARIANT_TRUE)
    {
      ::SendMessage(m_frame,WM_COMMAND,MAKELONG(0,IDN_SEL_CHANGE),(LPARAM)m_hWnd);
      ::SendMessage(m_frame,WM_COMMAND,MAKELONG(IDC_HREF,IDN_WANTFOCUS),(LPARAM)m_hWnd);
    }
  }
  catch (_com_error&) { }
  return 0;
}

LRESULT CFBEView::OnStyleFootnote(WORD, WORD, HWND, BOOL&) {
  try {
    m_mk_srv->BeginUndoUnit(L"Create Footnote");
    if (Document()->execCommand(L"CreateLink",VARIANT_FALSE,_variant_t(L""))==VARIANT_TRUE) {
      MSHTML::IHTMLTxtRangePtr  r(Document()->selection->createRange());
      MSHTML::IHTMLElementPtr	pe(r->parentElement());
      if (U::scmp(pe->tagName,L"A")==0)
	pe->className=L"note";
    }
    m_mk_srv->EndUndoUnit();
    ::SendMessage(m_frame,WM_COMMAND,MAKELONG(0,IDN_SEL_CHANGE),(LPARAM)m_hWnd);
    ::SendMessage(m_frame,WM_COMMAND,MAKELONG(IDC_HREF,IDN_WANTFOCUS),(LPARAM)m_hWnd);
  }
  catch (_com_error&) { }
  return 0;
}

bool CFBEView::CheckCommand(WORD wID) {
  StartupTrace::CountUiCheckCommand();
  if (!HasDoc())
    return false;
  if (!m_normalize)
    return false;
  switch (wID) {
  case ID_EDIT_ADD_BODY:
    return true;
  case ID_EDIT_ADD_TITLE:
    return bCall(L"AddTitle",SelectionStructCon());
  case ID_EDIT_CLONE:
    return bCall(L"CloneContainer",SelectionStructCon());
  case ID_STYLE_NORMAL:
    return bCall(L"StyleNormal",SelectionStructCon());
  case ID_STYLE_SUBTITLE:
    return bCall(L"StyleSubtitle",SelectionStructCon());
  case ID_STYLE_TEXTAUTHOR:
    return bCall(L"StyleTextAuthor",SelectionStructCon());
  case ID_EDIT_INS_IMAGE:
    return bCall(L"InsImage") && !SelectionStructCode() && !SelectionHasTags(L"SPAN");
  case ID_EDIT_INS_INLINEIMAGE:
    return bCall(L"InsInlineImage");
  case ID_EDIT_ADD_IMAGE:
    return bCall(L"AddImage", SelectionStructCon()) && !SelectionStructCode() && !SelectionHasTags(L"SPAN");
  case ID_EDIT_ADD_EPIGRAPH:
    return bCall(L"AddEpigraph",SelectionStructCon());
  case ID_EDIT_ADD_ANN:
    return bCall(L"AddAnnotation",SelectionStructCon());
  case ID_EDIT_SPLIT:
    return SplitContainer(true);
  case ID_EDIT_INS_POEM:
    return InsertPoem(true);
  case ID_EDIT_INS_CITE:
    return InsertCite(true);
	case ID_EDIT_CODE:
		{
			_variant_t params[3] =
			{
				Document()->selection->createRange().GetInterfacePtr(),
				SelectionStructCon().GetInterfacePtr(),
				true
			};
			return bCall(L"StyleCode", 3, params);
		}
  case ID_INSERT_TABLE:
	  return InsertTable(true);
  case ID_TABLE_INSERT_ROW_ABOVE:
  case ID_TABLE_INSERT_ROW_BELOW:
  case ID_TABLE_DELETE_ROW:
  case ID_TABLE_INSERT_COLUMN_LEFT:
  case ID_TABLE_INSERT_COLUMN_RIGHT:
  case ID_TABLE_DELETE_COLUMN:
  case ID_TABLE_TOGGLE_HEADER_CELL:
	case ID_TABLE_MAKE_HEADER_CELLS:
	case ID_TABLE_MAKE_NORMAL_CELLS:
	  return (bool)SelectionStructTableCon();
  case ID_GOTO_FOOTNOTE:
	  {
		  const bool footnoteFound = GoToFootnote(true);
		  const bool referenceFound = GoToReference(true);
		  return footnoteFound || referenceFound;
	  }
  case ID_GOTO_REFERENCE:
	  return GoToReference(true);
  case ID_EDIT_ADD_TA:
    return bCall(L"AddTA",SelectionStructCon());
  case ID_EDIT_MERGE:
    return bCall(L"MergeContainers",SelectionStructCon());
  case ID_EDIT_REMOVE_OUTER_SECTION:
    return bCall(L"RemoveOuterContainer",SelectionStructCon());
  case ID_STYLE_LINK:
  case ID_STYLE_NOTE:
    try {
      return Document()->queryCommandEnabled(L"CreateLink")==VARIANT_TRUE;
    }
    catch (const _com_error&) { }
    break;
  }
  return false;
}

bool	CFBEView::CheckSetCommand(WORD wID) {
	if (!m_normalize)
		return false;

	switch (wID)
	{
		case ID_EDIT_CODE:
			return bCall(L"IsCode", SelectionStructCode());
	}

	return false;
}

// changes tracking
MSHTML::IHTMLDOMNodePtr	  CFBEView::GetChangedNode() {
  MSHTML::IMarkupPointerPtr	  p1,p2;
  m_mk_srv->CreateMarkupPointer(&p1);
  m_mk_srv->CreateMarkupPointer(&p2);

  m_mkc->GetAndClearDirtyRange(m_dirtyRangeCookie,p1,p2);

  MSHTML::IHTMLElementPtr	  e1,e2;
  p1->CurrentScope(&e1);
  p2->CurrentScope(&e2);
  p1.Release();
  p2.Release();

  while ((bool)e1 && e1!=e2 && e1->contains(e2)!=VARIANT_TRUE)
    e1=e1->parentElement;

  return e1;
}

static bool IsEmptyNode(MSHTML::IHTMLDOMNode *node) {
	if (node->nodeType!=1)
		return false;

	_bstr_t   name(node->nodeName);

	if (U::scmp(name,L"BR")==0)
		return false;

	if (U::scmp(name,L"P")==0) // the editor uses empty Ps to represent empty lines
		return false;

	/* if (U::scmp(name,L"EM")==0) // Keep emphasis and strong markup distinct.
	return false;

	if (U::scmp(name,L"STRONG")==0) // Keep emphasis and strong markup distinct.
	return false;*/

	// images are always empty
	if (U::scmp(name,L"DIV")==0 && U::scmp(MSHTML::IHTMLElementPtr(node)->className,L"image")==0)
		return false;
	if (U::scmp(name,L"IMG")==0)
		return false;

	if (node->hasChildNodes()==VARIANT_FALSE)
		return true;

	if (U::scmp(name,L"A")==0) // links can be meaningful even if the contain only ws
		return false;

	if ((bool)node->firstChild->nextSibling)
		return false;

	if (node->firstChild->nodeType!=3)
		return false;

	if (U::is_whitespace(node->firstChild->nodeValue.bstrVal))
		return true;

	return false;
}

// Used by remaining view-owned commands.  SplitContainer resolves parents in
// BodyStructuralEditor so that its DOM algorithm has a single home.
static MSHTML::IHTMLElementPtr GetHP(MSHTML::IHTMLElementPtr hp)
{
	while((bool)hp && U::scmp(hp->tagName,L"DIV"))
		hp = hp->parentElement;
	return hp;
}

bool CFBEView::SplitContainer(bool fCheck)
{
	const FbeStructure::StructuralOperationResult result = SplitContainerResult(fCheck);
	if (!fCheck && FAILED(result.error)) U::ReportError(result.error);
	return result.IsApplied();
}

FbeStructure::StructuralOperationResult CFBEView::SplitContainerResult(bool fCheck, FbeStructure::SplitFailurePoint failurePoint)
{
	FbeStructure::BodyStructuralEditor editor(Document(), m_mk_srv);
	return editor.SplitContainer(fCheck, failurePoint);
}

//////////////////////////////////////////////////////////////////////////////
/// @fn static bool	MergeEqualHTMLElements(MSHTML::IHTMLDOMNode *node)
///
/// Merge adjacent equivalent HTML elements.
///
/// @param node [in, out] Node whose descendants are normalized.
///
/// @note Only EM and STRONG are merged. Adjacent equal elements are collapsed,
/// for example two neighbouring EM nodes become one.
///
/// @author Legacy FBE code, 2008-03-31
//////////////////////////////////////////////////////////////////////////////
static bool	MergeEqualHTMLElements(MSHTML::IHTMLDOMNode *node, MSHTML::IHTMLDocument2 *doc)
{
	if (node->nodeType != 1) // Element node
		return false;

	bool	fRet=false;


	MSHTML::IHTMLDOMNodePtr   cur(node->firstChild);
	while ((bool)cur) 
	{
		MSHTML::IHTMLDOMNodePtr next;
		next = cur->nextSibling;

		if (MergeEqualHTMLElements(cur,doc))
		{
			cur = node->firstChild;
			continue;
		}

		// A final child has no sibling to merge with.
		if(!(bool)next)
			return false;

		_bstr_t	name(cur->nodeName);	

		if (U::scmp(name,L"EM")==0 || U::scmp(name,L"STRONG")==0) 
		{
			MSHTML::IHTMLElementPtr	curelem(cur);
			// Empty or whitespace-only emphasis can bridge equal formatting nodes.
			bstr_t curText = curelem->innerText;
			if(curText.length() == 0 || U::is_whitespace(curelem->innerText))
			{
				// Preserve the whitespace while removing the empty formatting node.
				MSHTML::IHTMLDOMNodePtr prev = cur->previousSibling;
				if((bool)prev)
				{
					if(prev->nodeType == 3)//text
					{
						prev->nodeValue = (bstr_t)prev->nodeValue.bstrVal + curelem->innerText;						
					}
					else
					{
						MSHTML::IHTMLElementPtr prevElem(prev);
						prevElem->innerHTML = prevElem->innerHTML + curelem->innerText;
					}
					cur->removeNode(VARIANT_TRUE);
					cur = prev;
					continue;
				}

				if((bool)next)
				{
					MSHTML::IHTMLDOMNodePtr parent = cur->parentNode;
					if(next->nodeType == 3)//text
					{
						next->nodeValue = (bstr_t)curelem->innerText + next->nodeValue.bstrVal;
					}
					else
					{
						MSHTML::IHTMLElementPtr nextElem(next);
						nextElem->innerHTML = curelem->innerText + nextElem->innerHTML;
					}
					cur->removeNode(VARIANT_TRUE);
					cur = parent->firstChild;
					continue;
				}
			}

			if(next->nodeType == 3) // TextNode
			{
				MSHTML::IHTMLDOMNodePtr afterNext(next->nextSibling);
				if(!(bool)afterNext)
				{
					cur = next;
					continue;
				}

				MSHTML::IHTMLElementPtr	afterNextElem(afterNext);

				bstr_t afterNextName = afterNext->nodeName;
				if(U::scmp(name, afterNextName))// The outer elements differ.
				{
					cur = next;
					continue;
				}

				// Only whitespace may separate the two equal elements.
				if(!U::is_whitespace(next->nodeValue.bstrVal))
				{
					cur = next;
					continue; // Content between the elements prevents a merge.
				}

				// Replace the three nodes with one merged element.
				MSHTML::IHTMLElementPtr	newelem(doc->createElement(name));
				MSHTML::IHTMLDOMNodePtr	newnode(newelem);
				newelem->innerHTML = curelem->innerHTML + next->nodeValue.bstrVal + afterNextElem->innerHTML;
				cur->replaceNode(newnode);
				afterNext->removeNode(VARIANT_TRUE);
				next->removeNode(VARIANT_TRUE);
				cur = newnode;
				fRet=true;
			}
			else
			{
				bstr_t nextName(next->nodeName);
				if(U::scmp(name, nextName))// The adjacent elements differ.
				{
					cur = next;
					continue;
				}

				// Merge directly adjacent equal elements.
				MSHTML::IHTMLElementPtr	nextElem(next);
				MSHTML::IHTMLElementPtr	newelem(doc->createElement(name));
				MSHTML::IHTMLDOMNodePtr	newnode(newelem);
				newelem->innerHTML = curelem->innerHTML + nextElem->innerHTML;
				cur->replaceNode(newnode);
				next->removeNode(VARIANT_TRUE);
				cur = newnode;
				fRet=true;
				continue;
			}
		}
		cur=next;
	}
	return fRet;
}
static bool   RemoveUnk(MSHTML::IHTMLDOMNode *node, MSHTML::IHTMLDocument2 *doc) {
	if (node->nodeType!=1) // Element node
		return false;

	bool	fRet=false;

restart:
	MSHTML::IHTMLDOMNodePtr   cur(node->firstChild);
	while ((bool)cur) 
	{
		MSHTML::IHTMLDOMNodePtr next;
		next = cur->nextSibling;

		if (RemoveUnk(cur,doc))
			goto restart;

		_bstr_t			name(cur->nodeName);
		MSHTML::IHTMLElementPtr	curelem(cur);

		if (U::scmp(name,L"B")==0 || U::scmp(name,L"I")==0) {
			const wchar_t		*newname=U::scmp(name,L"B")==0 ? L"STRONG" : L"EM";
			MSHTML::IHTMLElementPtr	newelem(doc->createElement(newname));
			MSHTML::IHTMLDOMNodePtr	newnode(newelem);
			newelem->innerHTML=curelem->innerHTML;
			cur->replaceNode(newnode);
			cur=newnode;
			fRet=true;
			goto restart;
		}


		if (U::scmp(name,L"P") && U::scmp(name,L"STRONG") && 
			U::scmp(name,L"STRIKE") && U::scmp(name,L"SUP") && U::scmp(name,L"SUB") && 
			U::scmp(name,L"EM") && U::scmp(name,L"A") &&
			U::scmp(name,L"TABLE") && U::scmp(name,L"TBODY") && U::scmp(name,L"TR") &&
			U::scmp(name,L"TD") && U::scmp(name,L"TH") &&
			(U::scmp(name,L"SPAN") || U::scmp(curelem->className, L"code")) &&
			U::scmp(name,L"#text") && U::scmp(name,L"BR") &&
			(U::scmp(name,L"IMG") || U::scmp(curelem->parentElement->className, L"image")) &&
			// Added by SeNS: inline images support
			(U::scmp(name,L"SPAN") || U::scmp(curelem->className, L"image")))
		{
			if (U::scmp(name,L"DIV")==0) {
				_bstr_t	  cls(curelem->className);
				_bstr_t	  id(curelem->id);
				if (!(U::scmp(cls,L"body") && U::scmp(cls,L"section") &&
					U::scmp(cls,L"table") && U::scmp(cls,L"tr") && U::scmp(cls,L"th") && U::scmp(cls,L"td") && 
					U::scmp(cls,L"output") && U::scmp(cls,L"part") && U::scmp(cls,L"output-document-class") &&
					U::scmp(cls,L"annotation") && U::scmp(cls,L"title") && U::scmp(cls,L"epigraph") &&
					U::scmp(cls,L"poem") && U::scmp(cls,L"stanza") && U::scmp(cls,L"cite") &&
					U::scmp(cls,L"date") &&
					U::scmp(cls,L"history") && U::scmp(cls,L"image")&&
					U::scmp(cls,L"code") &&
					U::scmp(id,L"fbw_desc") && U::scmp(id,L"fbw_body") && U::scmp(id,L"fbw_updater")))
					goto ok;
			}

			CElementDescriptor* ED;
			if(_EDMnr.GetElementDescriptor(cur, &ED))
				goto ok;
			MSHTML::IHTMLDOMNodePtr ce(cur->previousSibling);
			cur->removeNode(VARIANT_FALSE);
			if (ce)
				next=ce->nextSibling;
			else
				next=node->firstChild;
		}
ok:

		cur=next;
	}
	return fRet;
}

static bool IsStanza(MSHTML::IHTMLDOMNode *node) {
	MSHTML::IHTMLElementPtr   elem(node);
	return U::scmp(elem->className,L"stanza")==0;
}

static void FixupLinks(MSHTML::IHTMLDOMNode *dom) {
	MSHTML::IHTMLElement2Ptr  elem(dom);

	if (!(bool)elem)
		return;

	MSHTML::IHTMLElementCollectionPtr coll(elem->getElementsByTagName(L"a"));
	if (!(bool)coll)
		return;

	if (coll->length == 0) coll = elem->getElementsByTagName(L"A");

	for (long l=0;l<coll->length;++l) {
		MSHTML::IHTMLElementPtr a(coll->item(l));
		if (!(bool)a)
			continue;

		_variant_t	  href(a->getAttribute(L"href",2));
		if (V_VT(&href)==VT_BSTR && V_BSTR(&href) &&
			::SysStringLen(V_BSTR(&href))>11 &&
			wcsncmp(V_BSTR(&href), L"file://", 7)==0)
		{
			wchar_t* pos = wcschr((wchar_t*)V_BSTR(&href), L'#'); 
			if(!pos)
				continue;
			a->setAttribute(L"href",pos,0);
		}
	}
}

bool CFBEView::InsertPoem(bool fCheck)
{
	const FbeStructure::StructuralOperationResult result = InsertPoemResult(fCheck);
	if (!fCheck && FAILED(result.error)) U::ReportError(result.error);
	return result.IsApplied();
} // CFBEView::InsertPoem

FbeStructure::StructuralOperationResult CFBEView::InsertPoemResult(bool fCheck, FbeStructure::CitePoemFailurePoint failurePoint)
{
	FbeStructure::BodyStructuralEditor editor(Document(), m_mk_srv);
	return editor.InsertPoem(fCheck, failurePoint);
}

bool CFBEView::InsertCite(bool fCheck)
{
	const FbeStructure::StructuralOperationResult result = InsertCiteResult(fCheck);
	if (!fCheck && FAILED(result.error)) U::ReportError(result.error);
	return result.IsApplied();
} // CFBEView::InsertCite

FbeStructure::StructuralOperationResult CFBEView::InsertCiteResult(bool fCheck, FbeStructure::CitePoemFailurePoint failurePoint)
{
	FbeStructure::BodyStructuralEditor editor(Document(), m_mk_srv);
	return editor.InsertCite(fCheck, failurePoint);
}

// searching
void  CFBEView::StartIncSearch() {
  try {
    m_is_start=Document()->selection->createRange();
    m_is_start->collapse(VARIANT_TRUE);
  }
  catch (_com_error&) {
  }
}

void  CFBEView::CancelIncSearch() {
  if (m_is_start) {
    m_is_start->raw_select();
    m_is_start.Release();
  }
}

// script calls
void	      CFBEView::ImgSetURL(IDispatch *elem,const CString& url) {
  try {
    CComDispatchDriver	dd(Script());
    _variant_t	  ve(elem);
    _variant_t	  vu((const TCHAR *)url);
    dd.Invoke2(L"ImgSetURL",&ve,&vu);
  }
  catch (_com_error&) { }
}

IDispatchPtr  CFBEView::Call(const wchar_t *name) {
  try {
    CComDispatchDriver  dd(Script());
    _variant_t  ret;
    _variant_t  vt2((false));
    dd.Invoke1(name,&vt2,&ret);
    if (V_VT(&ret)==VT_DISPATCH)
      return V_DISPATCH(&ret);
  }
  catch (_com_error&) { }
  return IDispatchPtr();
}
IDispatchPtr  CFBEView::Call(const wchar_t *name,IDispatch *pDisp) {
  try {
    CComDispatchDriver  dd(Script());
    _variant_t  vt;
    if (pDisp)
      vt=pDisp;
    _variant_t  vt2(false);
    _variant_t  ret;
    dd.Invoke2(name,&vt,&vt2,&ret);
    if (V_VT(&ret)==VT_DISPATCH)
      return V_DISPATCH(&ret);
  }
  catch (_com_error&) { }
  return IDispatchPtr();
}
static bool vt2bool(const _variant_t& vt) {
  if (V_VT(&vt)==VT_DISPATCH)
    return V_DISPATCH(&vt)!=0;
  if (V_VT(&vt)==VT_BOOL)
    return V_BOOL(&vt)==VARIANT_TRUE;
  if (V_VT(&vt)==VT_I4)
    return V_I4(&vt)!=0;
  if (V_VT(&vt)==VT_UI4)
    return V_UI4(&vt)!=0;
  return false;
}

bool CFBEView::bCall(const wchar_t *name, int nParams, VARIANT* params)
{
	StartupTrace::CountUiComCall();
	try
	{
		CComDispatchDriver dd(Script());
		_variant_t ret;
		dd.InvokeN(name, params, nParams, &ret);
		return vt2bool(ret);
	}
	catch(_com_error& err)
	{
		U::ReportError(err);
	}

	return false;
}

bool  CFBEView::bCall(const wchar_t *name,IDispatch *pDisp) {
	StartupTrace::CountUiComCall();
  try {
    CComDispatchDriver  dd(Script());
    _variant_t  vt;
    if (pDisp)
      vt=pDisp;
    _variant_t  vt2(true);
    _variant_t  ret;
    dd.Invoke2(name,&vt,&vt2,&ret);
    return vt2bool(ret);
  }
  catch (_com_error&) { }
  return false;
}

bool  CFBEView::bCall(const wchar_t *name) {
	StartupTrace::CountUiComCall();
  try {
    CComDispatchDriver  dd(Script());
    _variant_t  vt2(true);
    _variant_t  ret;
    dd.Invoke1(name,&vt2,&ret);
    return vt2bool(ret);
  }
  catch (_com_error&) { }
  return false;
}

// utilities
static CString	GetPath(MSHTML::IHTMLElementPtr elem) {
  try {
    if (!(bool)elem)
      return CString();
    CString		      path;
    while (elem) {
      CString	  cur((const wchar_t *)elem->tagName);
	  CString	  cid((const wchar_t *)elem->id);
      if (cur==_T("BODY"))
        return path;
	  if(cid == _T("fbw_body"))
		  return path;
      _bstr_t	  cls(elem->className);
      if (cls.length()>0)
	cur=(const wchar_t *)cls;
      _bstr_t	  id(elem->id);
      if (id.length()>0) {
	cur+=_T(':');
	cur+=(const wchar_t *)id;
      }
      if (!path.IsEmpty())
	path=_T('/')+path;
      path=cur+path;
      elem=elem->parentElement;
    }
    return path;
  }
  catch (_com_error&) { }
  return CString();
}

CString	CFBEView::SelPath() {
  return GetPath(SelectionContainer());
}

void  CFBEView::GoTo(MSHTML::IHTMLElement *e,bool fScroll) {
  if (!e)
    return;

  if (fScroll)
    e->scrollIntoView(VARIANT_TRUE);

  MSHTML::IHTMLTxtRangePtr	r(MSHTML::IHTMLBodyElementPtr(Document()->body)->createTextRange());
  r->moveToElementText(e);
  r->collapse(VARIANT_TRUE);
  // all m$ editors like to position the pointer at the end of the preceding element,
  // which sucks. This workaround seems to work most of the time.
  if (e!=r->parentElement() && r->move(L"character",1)==1)
    r->move(L"character",-1);

  r->select();
}

MSHTML::IHTMLElementPtr CFBEView::SelectionContainerImp()
{
	// MSHTML can briefly expose a Control selection while it is moving focus to
	// its scrollbar.  The ControlRange parent helper fails with E_FAIL on older MSHTML
	// versions in that state.  SelectionContainer is only used to synchronize
	// UI (not to modify the document), so resolve the selected control directly
	// and leave a diagnostic record if the transient query is unavailable.
	IDispatchPtr selrange;
	try
	{
		selrange = Document()->selection->createRange();
	}
	catch (_com_error& err)
	{
		TraceSelectionContainerFailure(L"IHTMLSelectionObject::createRange", err.Error(), L"unknown");
		return MSHTML::IHTMLElementPtr();
	}

	MSHTML::IHTMLTxtRangePtr range;
	try
	{
		range = selrange;
	}
	catch (_com_error& err)
	{
		TraceSelectionContainerFailure(L"QueryInterface(IHTMLTxtRange)", err.Error(), L"unknown");
		return MSHTML::IHTMLElementPtr();
	}
	if (range)
	{
		try { return range->parentElement(); }
		catch (_com_error& err)
		{
			TraceSelectionContainerFailure(L"IHTMLTxtRange::parentElement", err.Error(), L"Text");
			return MSHTML::IHTMLElementPtr();
		}
	}

	MSHTML::IHTMLControlRangePtr controls;
	try { controls = selrange; }
	catch (_com_error& err)
	{
		TraceSelectionContainerFailure(L"QueryInterface(IHTMLControlRange)", err.Error(), L"unknown");
		return MSHTML::IHTMLElementPtr();
	}
	if (!controls)
		return MSHTML::IHTMLElementPtr();

	long length = 0;
	try { length = controls->length; }
	catch (_com_error& err)
	{
		TraceSelectionContainerFailure(L"IHTMLControlRange::get_length", err.Error(), L"Control");
		return MSHTML::IHTMLElementPtr();
	}
	if (length <= 0)
		return MSHTML::IHTMLElementPtr();

	MSHTML::IHTMLElementPtr selected;
	try { selected = controls->item(0); }
	catch (_com_error& err)
	{
		TraceSelectionContainerFailure(L"IHTMLControlRange::item(0)", err.Error(), L"Control", length);
		return MSHTML::IHTMLElementPtr();
	}
	if (!selected)
		return MSHTML::IHTMLElementPtr();

	try
	{
		// FBE selects the DIV/SPAN.image wrapper for a clicked illustration.
		// Keep that semantic container for the document tree.  Be defensive for
		// a raw IMG control selection created by another MSHTML code path.
		if (U::scmp(selected->tagName, L"IMG") == 0)
		{
			MSHTML::IHTMLElementPtr parent(selected->parentElement);
			if (parent && U::scmp(parent->className, L"image") == 0)
				return parent;
		}
		return selected;
	}
	catch (_com_error& err)
	{
		TraceSelectionContainerFailure(L"IHTMLElement::tagName/parentElement", err.Error(), L"Control", length);
	}

	return MSHTML::IHTMLElementPtr();
}

bool CFBEView::GetSelectionInfo(MSHTML::IHTMLElementPtr* begin, MSHTML::IHTMLElementPtr* end, int* beginChar, int* endChar, MSHTML::IHTMLTxtRangePtr range)
{
	if(range)
		return FbeDom::SelectionRangeMapper::GetSelectionInfo(range, begin, end, beginChar, endChar);
	IDispatchPtr selectionRange(Document()->selection->createRange());
	MSHTML::IHTMLTxtRangePtr textRange(selectionRange);
	if(textRange)
		return FbeDom::SelectionRangeMapper::GetSelectionInfo(textRange, begin, end, beginChar, endChar);
	MSHTML::IHTMLControlRangePtr controlRange(selectionRange);
	return FbeDom::SelectionRangeMapper::GetSelectionInfo(controlRange, begin, end, beginChar, endChar);
}

MSHTML::IHTMLElementPtr CFBEView::SelectionAnchor() {
  try {
    MSHTML::IHTMLElementPtr   cur(SelectionContainer());
    while (cur) {
      _bstr_t	tn(cur->tagName);
      if (U::scmp(tn,L"A")==0 || (U::scmp(tn,L"DIV")==0 && U::scmp(cur->className,L"image")==0))
		return cur;
	  // Added by SeNS - inline images
      if (U::scmp(tn,L"A")==0 || (U::scmp(tn,L"SPAN")==0 && U::scmp(cur->className,L"image")==0))
		return cur;
      cur=cur->parentElement;
    }
  }
  catch (_com_error&) { }
  return MSHTML::IHTMLElementPtr();
}

MSHTML::IHTMLElementPtr CFBEView::SelectionAnchor(MSHTML::IHTMLElementPtr cur) {
  try {
    while (cur) {
      _bstr_t	tn(cur->tagName);
      if (U::scmp(tn,L"A")==0 || (U::scmp(tn,L"DIV")==0 && U::scmp(cur->className,L"image")==0))
		return cur;
	  // Added by SeNS - inline images
      if (U::scmp(tn,L"A")==0 || (U::scmp(tn,L"SPAN")==0 && U::scmp(cur->className,L"image")==0))
		return cur;
      cur=cur->parentElement;
    }
  }
  catch (_com_error&) { }
  return MSHTML::IHTMLElementPtr();
}


MSHTML::IHTMLElementPtr CFBEView::SelectionStructCon() {
	StartupTrace::CountUiSelectionStructConQuery();
	try
	{
		MSHTML::IHTMLElementPtr cur(SelectionContainer());
		while (cur)
		{
			if (U::scmp(cur->tagName, L"P") == 0 || U::scmp(cur->tagName, L"DIV") == 0)
				return cur;
			cur=cur->parentElement;
		}
	}
	catch (_com_error& err)
	{
		U::ReportError(err);
	}

	return MSHTML::IHTMLElementPtr();
}

MSHTML::IHTMLElementPtr CFBEView::SelectionStructNearestCon()
{
	try
	{
		MSHTML::IHTMLElementPtr cur(SelectionContainer());
		if(cur)
		{			
			return cur;
		}
	}
	catch (_com_error& err)
	{
		U::ReportError(err);
	}

	return MSHTML::IHTMLElementPtr();
}

MSHTML::IHTMLElementPtr CFBEView::SelectionStructCode() {
	try
	{
		MSHTML::IHTMLElementPtr cur(SelectionContainer());
		while(cur)
		{
			// changed by SeNS: inline images also have a tag SPAN
			if((U::scmp(cur->tagName, L"SPAN") == 0) && (U::scmp(cur->className,L"image")!=0))
				return cur;
			cur = cur->parentElement;
		}		
	}
	catch (_com_error& err)
	{
		U::ReportError(err);
	}

	return MSHTML::IHTMLElementPtr();
}

// Modification by Pilgrim
MSHTML::IHTMLElementPtr	  CFBEView::SelectionStructSection() {
  try {
    MSHTML::IHTMLElementPtr   cur(SelectionContainer());
    while (cur) {
      if (U::scmp(cur->className,L"section")==0)
		return cur;
      cur=cur->parentElement;
    }
  }
  catch (_com_error&) {
  }
  return MSHTML::IHTMLElementPtr();
}

MSHTML::IHTMLElementPtr	  CFBEView::SelectionStructImage() {	
  try {
    MSHTML::IHTMLElementPtr   cur(SelectionContainer());
    while (cur) {
	  // changed by SeNS: inline images have a tag SPAN, regular tag DIV
      if ((U::scmp(cur->className,L"image")==0) && (U::scmp(cur->tagName,L"SPAN")!=0))
		return cur;
      cur=cur->parentElement;
    }
  }
  catch (_com_error&) {
  }
  return MSHTML::IHTMLElementPtr();
}


MSHTML::IHTMLElementPtr	  CFBEView::SelectionStructTable() {
  try {
    MSHTML::IHTMLElementPtr   cur(SelectionContainer());
    while (cur) {
      if (U::scmp(cur->className,L"table")==0)
		return cur;
      cur=cur->parentElement;
    }
  }
  catch (_com_error&) {
  }
  return MSHTML::IHTMLElementPtr();
}

MSHTML::IHTMLElementPtr	  CFBEView::SelectionStructTableCon() {
	StartupTrace::CountUiSelectionStructTableConQuery();
  try {
    MSHTML::IHTMLElementPtr   cur(SelectionContainer());
    while (cur) {
      if (U::scmp(cur->className,L"th")==0 || U::scmp(cur->className,L"td")==0)
	return cur;
      cur=cur->parentElement;
    }
	// A command-bar click can make createRange() unavailable before the command
	// is routed.  The mouse handler records this exact cell, so prefer it before
	// querying that transient MSHTML selection state.
	if (m_table_selection_anchor) return m_table_selection_anchor;
	MSHTML::IHTMLTxtRangePtr range(Document()->selection->createRange());
	if (range) {
		range->collapse(VARIANT_TRUE);
		MSHTML::IHTMLElementPtr cell(FbeTable::FindTableCell(range->parentElement()));
		if (cell) return cell;
	}
  }
  catch (_com_error&) {
  }
  return MSHTML::IHTMLElementPtr();
}

MSHTML::IHTMLElementPtr	  CFBEView::SelectionsStyleT() {
  try {
    MSHTML::IHTMLElementPtr   cur(SelectionContainer());
    while (cur) {
 	 _bstr_t	style(AU::GetAttrB(cur,L"fbstyle"));
      if (U::scmp(cur->className,L"table")==0)
		return cur;
      cur=cur->parentElement;
    }
  }
  catch (_com_error&) {
  }
  return MSHTML::IHTMLElementPtr();
}

MSHTML::IHTMLElementPtr	  CFBEView::SelectionsStyleTB(_bstr_t& style) {
  try {
    MSHTML::IHTMLElementPtr   cur(SelectionContainer());
    while (cur) {
	 if (U::scmp(cur->className,L"table")==0){		
		 style = AU::GetAttrB(cur,L"fbstyle");
		 return cur;
	 }	
     cur=cur->parentElement;
    }
  }
  catch (_com_error&) {
  }
  return MSHTML::IHTMLElementPtr();
}

MSHTML::IHTMLElementPtr	  CFBEView::SelectionsStyle() {
  try {
    MSHTML::IHTMLElementPtr   cur(SelectionContainer());
    while (cur) {
 	 _bstr_t	style(AU::GetAttrB(cur,L"fbstyle"));
      if (U::scmp(cur->className,L"th")==0 || U::scmp(cur->className,L"td")==0)
		return cur;
      cur=cur->parentElement;
    }
  }
  catch (_com_error&) {
  }
  return MSHTML::IHTMLElementPtr();
}

MSHTML::IHTMLElementPtr	  CFBEView::SelectionsStyleB(_bstr_t& style) {
  try {
    MSHTML::IHTMLElementPtr   cur(SelectionContainer());
    while (cur) {
	 if (U::scmp(cur->className,L"th")==0 || U::scmp(cur->className,L"td")==0){		
		 style = AU::GetAttrB(cur,L"fbstyle");
		 return cur;
	 }	
     cur=cur->parentElement;
    }
  }
  catch (_com_error&) {
  }
  return MSHTML::IHTMLElementPtr();
}

MSHTML::IHTMLElementPtr	  CFBEView::SelectionsColspan() {
  try {
    MSHTML::IHTMLElementPtr   cur(SelectionContainer());
    while (cur) {
      if (U::scmp(cur->className,L"th")==0 || U::scmp(cur->className,L"td")==0)
		return cur;
      cur=cur->parentElement;
    }
  }
  catch (_com_error&) {
  }
  return MSHTML::IHTMLElementPtr();
}

MSHTML::IHTMLElementPtr	  CFBEView::SelectionsColspanB(_bstr_t& colspan) {
  try {
    MSHTML::IHTMLElementPtr   cur(SelectionContainer());
    while (cur) {
	  if (U::scmp(cur->className,L"th")==0 || U::scmp(cur->className,L"td")==0){
	  	colspan = AU::GetAttrB(cur,L"fbcolspan");
		return cur;
	  }
      cur=cur->parentElement;
    }
  }
  catch (_com_error&) {
  }
  return MSHTML::IHTMLElementPtr();
}

MSHTML::IHTMLElementPtr	  CFBEView::SelectionsRowspan() {
  try {
    MSHTML::IHTMLElementPtr   cur(SelectionContainer());
    while (cur) {
      if (U::scmp(cur->className,L"th")==0 || U::scmp(cur->className,L"td")==0)
		return cur;
      cur=cur->parentElement;
    }
  }
  catch (_com_error&) {
  }
  return MSHTML::IHTMLElementPtr();
}

MSHTML::IHTMLElementPtr	  CFBEView::SelectionsRowspanB(_bstr_t& rowspan) {
  try {
    MSHTML::IHTMLElementPtr   cur(SelectionContainer());
    while (cur) {
	  if (U::scmp(cur->className,L"th")==0 || U::scmp(cur->className,L"td")==0){
		rowspan =  AU::GetAttrB(cur,L"fbrowspan");
		return cur;
	  }
      cur=cur->parentElement;
    }
  }
  catch (_com_error&) {
  }
  return MSHTML::IHTMLElementPtr();
}

MSHTML::IHTMLElementPtr	  CFBEView::SelectionsAlignTR() {
  try {
    MSHTML::IHTMLElementPtr   cur(SelectionContainer());
    while (cur) {
      if (U::scmp(cur->className,L"tr")==0)
		return cur;
      cur=cur->parentElement;
    }
  }
  catch (_com_error&) {
  }
  return MSHTML::IHTMLElementPtr();
}

MSHTML::IHTMLElementPtr	  CFBEView::SelectionsAlignTRB(_bstr_t& align) {
  try {
    MSHTML::IHTMLElementPtr   cur(SelectionContainer());
    while (cur) {
		if (U::scmp(cur->className,L"tr")==0){
			align =  AU::GetAttrB(cur,L"fbalign");
			return cur;
		}
      cur=cur->parentElement;
    }
  }
  catch (_com_error&) {
  }
  return MSHTML::IHTMLElementPtr();
}

MSHTML::IHTMLElementPtr	  CFBEView::SelectionsAlign() {
  try {
    MSHTML::IHTMLElementPtr   cur(SelectionContainer());
    while (cur) {
      if (U::scmp(cur->className,L"tr")==0 || U::scmp(cur->className,L"th")==0 || U::scmp(cur->className,L"td")==0)
		return cur;
      cur=cur->parentElement;
    }
  }
  catch (_com_error&) {
  }
  return MSHTML::IHTMLElementPtr();
}

MSHTML::IHTMLElementPtr	  CFBEView::SelectionsAlignB(_bstr_t& align) {
  try {
    MSHTML::IHTMLElementPtr   cur(SelectionContainer());
    while (cur) {
		if (U::scmp(cur->className,L"tr")==0 || U::scmp(cur->className,L"th")==0 || U::scmp(cur->className,L"td")==0){
			align =  AU::GetAttrB(cur,L"fbalign");
			return cur;
		}
      cur=cur->parentElement;
    }
  }
  catch (_com_error&) {
  }
  return MSHTML::IHTMLElementPtr();
}

MSHTML::IHTMLElementPtr	  CFBEView::SelectionsVAlign() {
  try {
    MSHTML::IHTMLElementPtr   cur(SelectionContainer());
    while (cur) {
      if (U::scmp(cur->className,L"th")==0 || U::scmp(cur->className,L"td")==0)
		return cur;
      cur=cur->parentElement;
    }
  }
  catch (_com_error&) {
  }
  return MSHTML::IHTMLElementPtr();
}

MSHTML::IHTMLElementPtr	  CFBEView::SelectionsVAlignB(_bstr_t& valign) {
  try {
    MSHTML::IHTMLElementPtr   cur(SelectionContainer());
    while (cur) {
		if (U::scmp(cur->className,L"th")==0 || U::scmp(cur->className,L"td")==0){
			valign =  AU::GetAttrB(cur,L"fbvalign");
			return cur;
		}
      cur=cur->parentElement;
    }
  }
  catch (_com_error&) {
  }
  return MSHTML::IHTMLElementPtr();
}

namespace
{
struct NormalizationDiagnostics
{
	ULONGLONG full = 0;
	ULONGLONG scoped = 0;
	void Record(bool isFull)
	{
		isFull ? ++full : ++scoped;
		const ULONGLONG total = full + scoped;
		if (StartupTrace::Enabled() && total % 128 == 0)
		{
			CString message;
			message.Format(L"normalization-full=%llu; normalization-scoped=%llu", full, scoped);
			StartupTrace::Event(L"performance", L"P420", message);
		}
	}
};

NormalizationDiagnostics g_normalizationDiagnostics;

bool IsNormalizationOwner(const MSHTML::IHTMLElementPtr& element)
{
	if (!element || U::scmp(element->tagName, L"DIV") != 0) return false;
	const _bstr_t className(element->className);
	return U::scmp(className, L"section") == 0 || U::scmp(className, L"annotation") == 0 ||
		U::scmp(className, L"history") == 0 || U::scmp(className, L"title") == 0 ||
		U::scmp(className, L"epigraph") == 0 || U::scmp(className, L"cite") == 0 ||
		U::scmp(className, L"stanza") == 0 || U::scmp(className, L"poem") == 0 ||
		U::scmp(className, L"body") == 0;
}

MSHTML::IHTMLElementPtr FindNormalizationOwner(MSHTML::IHTMLElementPtr element)
{
	for (MSHTML::IHTMLElementPtr current(element); current; current = current->parentElement)
		if (IsNormalizationOwner(current)) return current;
	return MSHTML::IHTMLElementPtr();
}

bool IsLiveDropScope(MSHTML::IHTMLDOMNodePtr scope, MSHTML::IHTMLDocument2Ptr document)
{
	MSHTML::IHTMLElementPtr element(scope);
	if (!element || !IsNormalizationOwner(element) || U::scmp(element->className, L"body") == 0) return false;
	MSHTML::IHTMLDOMNodePtr editable(document->all->item(L"fbw_body"));
	for (MSHTML::IHTMLDOMNodePtr node(scope); node; node = node->parentNode)
		if (node == editable) return true;
	return false;
}

// MSHTML bookmarks encode document-wide coordinates which change when a BR
// becomes a paragraph boundary. Keep each endpoint relative to its own owner.
struct DropSelectionEndpoint {
	MSHTML::IHTMLDOMNodePtr owner, parent;
	long index = 0, offset = -1;
	long textOffset = -1;
	bool skipBreaks = false;
	static long TextLength(MSHTML::IHTMLTxtRangePtr range) {
		CString text(static_cast<LPCWSTR>(range->text)); text.Remove(L'\r'); text.Remove(L'\n');
		return text.GetLength();
	}
	void Capture(MSHTML::IHTMLTxtRangePtr selection, bool start, MSHTML::IHTMLDocument2Ptr document) {
		MSHTML::IHTMLTxtRangePtr endpoint(selection->duplicate());
		endpoint->collapse(start ? VARIANT_TRUE : VARIANT_FALSE);
		owner = MSHTML::IHTMLDOMNodePtr(FindNormalizationOwner(endpoint->parentElement()));
		if (!IsLiveDropScope(owner, document)) return;
		parent = owner->parentNode;
		for (MSHTML::IHTMLDOMNodePtr node(owner->previousSibling); node; node = node->previousSibling) ++index;
		auto compare = [&](long position) {
			MSHTML::IHTMLTxtRangePtr probe(selection->duplicate());
			probe->moveToElementText(MSHTML::IHTMLElementPtr(owner)); probe->collapse(VARIANT_TRUE);
			const long moved = probe->move(L"character", position);
			const long order = probe->compareEndPoints(L"StartToStart", endpoint);
			if (moved < position && order < 0) CheckError(E_UNEXPECTED);
			return order;
		};
		long low = 0, high = 1;
		while (compare(high) < 0) { if (high > LONG_MAX / 2) return; high *= 2; }
		while (low < high) { const long mid = low + (high - low) / 2; if (compare(mid) < 0) low = mid + 1; else high = mid; }
		if (compare(low) == 0) offset = low;
		MSHTML::IHTMLTxtRangePtr prefix(selection->duplicate());
		prefix->moveToElementText(MSHTML::IHTMLElementPtr(owner)); prefix->setEndPoint(L"EndToStart", endpoint);
		textOffset = TextLength(prefix);
		MSHTML::IHTMLTxtRangePtr next(endpoint->duplicate()); next->moveEnd(L"character", 1);
		skipBreaks = TextLength(next) > 0;
	}
	MSHTML::IHTMLTxtRangePtr Restore(MSHTML::IHTMLDocument2Ptr document) const {
		if (offset < 0) return MSHTML::IHTMLTxtRangePtr();
		MSHTML::IHTMLDOMNodePtr current(owner);
		if (!IsLiveDropScope(current, document)) {
			current = parent ? parent->firstChild : MSHTML::IHTMLDOMNodePtr();
			for (long i = 0; current && i < index; ++i) current = current->nextSibling;
		}
		if (!IsLiveDropScope(current, document)) return MSHTML::IHTMLTxtRangePtr();
		MSHTML::IHTMLTxtRangePtr range(MSHTML::IHTMLBodyElementPtr(document->body)->createTextRange());
		range->moveToElementText(MSHTML::IHTMLElementPtr(current));
		auto point = [&](long position) {
			MSHTML::IHTMLTxtRangePtr probe(range->duplicate()); probe->collapse(VARIANT_TRUE); probe->move(L"character", position);
			return probe;
		};
		// First determine the owner's end in MSHTML movement units. Then map
		// text characters without CR/LF; BR and P boundaries have different
		// movement widths but carry the same document text.
		long low = 0, high = 1;
		while (point(high)->compareEndPoints(L"StartToEnd", range) < 0) { if (high > LONG_MAX / 2) return MSHTML::IHTMLTxtRangePtr(); high *= 2; }
		while (low < high) { const long mid = low + (high - low) / 2; if (point(mid)->compareEndPoints(L"StartToEnd", range) < 0) low = mid + 1; else high = mid; }
		const long end = low;
		low = 0; high = end;
		const bool forward = skipBreaks && textOffset < TextLength(range);
		while (low < high) {
			const long mid = low + (high - low) / 2;
			MSHTML::IHTMLTxtRangePtr prefix(range->duplicate()); prefix->setEndPoint(L"EndToStart", point(mid));
			const long length = TextLength(prefix);
			if (forward ? length <= textOffset : length < textOffset) low = mid + 1; else high = mid;
		}
		return point(forward && low ? low - 1 : low);
	}
};

// MSHTML ends markup undo units at script/event boundaries. An OLE parent
// collects the native drop and the later normalization across those boundaries.
class DropUndoUnit : public CComObjectRootEx<CComSingleThreadModel>, public IOleParentUndoUnit
{
	std::vector<CComPtr<IOleUndoUnit>> m_units;
	CComPtr<IOleParentUndoUnit> m_open;
	FB::Doc* m_document = nullptr;
	long m_version = -1;
	bool m_clean = false, m_recoveryClean = false, m_paste = false, m_pasteUndo = true;
    DropSelectionEndpoint m_pasteBefore[2], m_pasteAfter[2];
    void CapturePasteSelection(DropSelectionEndpoint* endpoints) {
        if (!m_document) return;
        MSHTML::IHTMLDocument2Ptr document(m_document->m_body.Document());
        MSHTML::IHTMLTxtRangePtr selected(document->selection->createRange());
        endpoints[0].Capture(selected, true, document);
        endpoints[1].Capture(selected, false, document);
    }
    void RestorePasteSelection() {
        if (!m_paste || !m_document) return;
        const auto& endpoints = m_pasteUndo ? m_pasteBefore : m_pasteAfter;
        try {
            MSHTML::IHTMLDocument2Ptr document(m_document->m_body.Document());
            MSHTML::IHTMLTxtRangePtr start(endpoints[0].Restore(document)), end(endpoints[1].Restore(document));
            if (start && end) { start->setEndPoint(L"EndToStart", end); start->select(); }
        } catch (const _com_error&) {}
    }
	std::unique_ptr<FbeUndo::HistoryCheckpoint> m_history;
	unsigned long long m_before = 0;
	FB::Doc::UndoSavepoints m_savepoints{};
	MSHTML::IHTMLTxtRangePtr m_selection;
public:
	void SetPaste() { m_paste = true; CapturePasteSelection(m_pasteBefore); }
    void FinishPaste() { CapturePasteSelection(m_pasteAfter); }
	void CaptureHistoryState(FB::Doc* document) {
		m_document = document;
		if (document) {
			m_version = document->m_body.GetVersionNumber();
			m_clean = !document->DocChanged(); m_recoveryClean = !document->DocRelChanged();
		}
	}
	void PrepareRollback(IOleUndoManager* manager) {
		if (m_history || !m_document) return;
		try {
			auto history = std::make_unique<FbeUndo::HistoryCheckpoint>(manager);
			const auto before = FbeUndo::DocumentState(m_document->m_body.Document());
			auto savepoints = m_document->CaptureUndoSavepoints();
			MSHTML::IHTMLTxtRangePtr selection;
			try { selection = m_document->m_body.Document()->selection->createRange(); } catch (const _com_error&) {}
			m_history = std::move(history); m_before = before;
			m_savepoints = std::move(savepoints); m_selection = selection;
		} catch (const std::bad_alloc&) { _com_issue_error(E_OUTOFMEMORY); }
	}
	HRESULT RollbackRecorded(IOleUndoManager* manager) {
		try {
			if (!m_history || !m_document) return E_UNEXPECTED;
			CheckError(manager->UndoTo(this));
			if (m_before != FbeUndo::DocumentState(m_document->m_body.Document())) return E_ABORT;
			// Only this transaction's inverse is on redo: committing it cleared
			// the old redo entries, which the checkpoint still owns.
			CComPtr<IEnumOleUndoUnits> list; CheckError(manager->EnumRedoable(&list));
			CComPtr<IOleUndoUnit> inverse;
			for (;;) { CComPtr<IOleUndoUnit> unit; const HRESULT hr = list->Next(1, &unit, nullptr); if (hr == S_FALSE) break; CheckError(hr); inverse = unit; }
			if (inverse) CheckError(manager->DiscardFrom(inverse));
			CheckError(m_history->Restore(manager));
			m_document->RestoreUndoSavepoints(m_savepoints);
			try { if (m_selection) m_selection->select(); } catch (const _com_error&) {}
			return S_OK;
		} catch (const _com_error& error) { return error.Error(); }
		catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
	}
	BEGIN_COM_MAP(DropUndoUnit)
		COM_INTERFACE_ENTRY(IOleUndoUnit)
		COM_INTERFACE_ENTRY(IOleParentUndoUnit)
	END_COM_MAP()
	STDMETHOD(Do)(IOleUndoManager* manager)
	{
		if (m_open) return E_UNEXPECTED;
		CString trace;
		trace.Format(L"children=%u; manager=%s", static_cast<unsigned>(m_units.size()), manager ? L"provided" : L"null");
		StartupTrace::Event(L"undo", L"D413", trace);
		if (!manager)
		{
			for (auto unit = m_units.rbegin(); unit != m_units.rend(); ++unit)
			{
				const HRESULT result = (*unit)->Do(nullptr);
				if (FAILED(result)) return result;
			}
			if (m_document) m_document->RestoreCleanAfterPasteUndo(m_version, m_clean, m_recoveryClean);
            RestorePasteSelection();
			return S_OK;
		}
		CComObject<DropUndoUnit>* raw = nullptr;
		HRESULT hr = CComObject<DropUndoUnit>::CreateInstance(&raw);
		if (FAILED(hr)) return hr;
		CComPtr<IOleParentUndoUnit> inverse(raw);
		raw->m_paste = m_paste; raw->m_pasteUndo = !m_pasteUndo;
        for (int i = 0; i < 2; ++i) { raw->m_pasteBefore[i] = m_pasteBefore[i]; raw->m_pasteAfter[i] = m_pasteAfter[i]; }
		raw->CaptureHistoryState(m_document);
		hr = manager->Open(inverse);
		if (FAILED(hr)) return hr;
		for (auto unit = m_units.rbegin(); unit != m_units.rend(); ++unit)
		{
			hr = (*unit)->Do(manager);
			if (FAILED(hr)) {
				CComBSTR description; (*unit)->GetDescription(&description);
				StartupTrace::HResult(L"undo", L"D414", hr, description ? description.m_str : L"Child undo unit failed");
				break;
			}
		}
		const HRESULT closed = manager->Close(inverse, !raw->m_units.empty());
		if (SUCCEEDED(hr) && SUCCEEDED(closed) && m_document)
			m_document->RestoreCleanAfterPasteUndo(m_version, m_clean, m_recoveryClean);
		if (SUCCEEDED(hr) && SUCCEEDED(closed)) RestorePasteSelection();
		return FAILED(hr) ? hr : closed;
	}
	STDMETHOD(GetDescription)(BSTR* text)
	{
		if (!text) return E_POINTER;
		*text = ::SysAllocString(m_paste ? L"Paste" : L"Drag and drop");
		return *text ? S_OK : E_OUTOFMEMORY;
	}
	STDMETHOD(GetUnitType)(CLSID* clsid, LONG* id)
	{
		if (!clsid || !id) return E_POINTER;
		*clsid = FbeUndo::UnitClass(); *id = 2; return S_OK;
	}
	STDMETHOD(OnNextAdd)() { return m_units.empty() ? S_OK : m_units.back()->OnNextAdd(); }
	STDMETHOD(Open)(IOleParentUndoUnit* unit)
	{
		if (!unit) return E_POINTER;
		if (unit == static_cast<IOleParentUndoUnit*>(this) || FindUnit(unit) == S_OK) return E_INVALIDARG;
		if (m_open) return m_open->Open(unit);
		m_open = unit; return S_OK;
	}
	STDMETHOD(Close)(IOleParentUndoUnit* unit, BOOL commit)
	{
		// S_FALSE tells the containing manager/parent to close this unit.
		// S_OK means a nested child handled the request and this unit stays open.
		if (!m_open) return S_FALSE;
		const HRESULT closed = m_open->Close(unit, commit);
		if (closed != S_FALSE) return closed;
		if (m_open.p != unit) return E_INVALIDARG;
		CComPtr<IOleParentUndoUnit> child(m_open);
		m_open.Release();
		const HRESULT added = commit ? Add(child) : S_OK;
		if (FAILED(added)) m_open = child;
		return added;
	}
	STDMETHOD(Add)(IOleUndoUnit* unit)
	{
		if (!unit) return E_POINTER;
		if (unit == static_cast<IOleUndoUnit*>(this)) return E_INVALIDARG;
		if (StartupTrace::Enabled()) { CComBSTR name; unit->GetDescription(&name); StartupTrace::Event(L"undo", L"D417", name ? name.m_str : L"Drop parent Add"); }
		if (m_open) return m_open->Add(unit);
		try { m_units.push_back(unit); }
		catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
		if (m_units.size() > 1) m_units[m_units.size() - 2]->OnNextAdd();
		return S_OK;
	}
	STDMETHOD(FindUnit)(IOleUndoUnit* unit)
	{
		if (!unit) return E_POINTER;
		if (m_open && (m_open.p == unit || m_open->FindUnit(unit) == S_OK)) return S_OK;
		for (const auto& child : m_units)
		{
			if (child.p == unit) return S_OK;
			CComQIPtr<IOleParentUndoUnit> parent(child);
			if (parent && parent->FindUnit(unit) == S_OK) return S_OK;
		}
		return S_FALSE;
	}
	STDMETHOD(GetParentState)(DWORD* state)
	{
		if (!state) return E_POINTER;
		if (m_open) return m_open->GetParentState(state);
		*state = UAS_NORMAL; return S_OK;
	}
	bool Empty() const { return m_units.empty(); }
	HRESULT CloseChildren()
	{
		if (!m_open) return S_OK;
		// Ask the immediate child to close through the ordinary parent contract;
		// never discard recorded edits or clear the manager's previous history.
		return Close(m_open, TRUE);
	}
};
}

void CFBEView::OpenDropUndoGroup(CComPtr<IOleUndoManager>& manager, CComPtr<IOleParentUndoUnit>& parent)
{
	IServiceProviderPtr services(Document());
	CheckError(services->QueryService(SID_SOleUndoManager, IID_IOleUndoManager,
		reinterpret_cast<void**>(&manager)));
	CComObject<DropUndoUnit>* unit = nullptr;
	CheckError(CComObject<DropUndoUnit>::CreateInstance(&unit));
	CComPtr<IOleParentUndoUnit> candidate(unit);
	unit->CaptureHistoryState(m_ownerDocument);
	DWORD state = 0;
	StartupTrace::HResult(L"undo", L"D416", manager->GetOpenParentState(&state), L"Open drop group: previous parent state");
	const HRESULT opened = manager->Open(candidate);
	if (FAILED(opened)) { manager.Release(); CheckError(opened); }
	parent = candidate;
}

HRESULT CFBEView::CloseDropUndoGroup(IOleUndoManager* manager, IOleParentUndoUnit* parent, bool commit)
{
	if (!manager || !parent) return E_POINTER;
	auto unit = static_cast<DropUndoUnit*>(parent);
	StartupTrace::Event(L"undo", L"D418", unit->Empty() ? L"Close empty drop group" : L"Close recorded drop group");
	HRESULT hr = FbeUndo::Inject(L"paste-parent-close");
    if(SUCCEEDED(hr)) hr = manager->Close(parent, commit && !unit->Empty());
	if (FAILED(hr))
	{
		StartupTrace::HResult(L"undo", L"D410", hr, L"Close drop group failed; closing nested units before retry");
		const HRESULT nested = unit->CloseChildren();
		if (FAILED(nested)) return nested;
		hr = manager->Close(parent, commit && !unit->Empty());
	}
	return hr;
}

HRESULT CFBEView::RollbackDropUndoGroup(IOleUndoManager* manager, IOleParentUndoUnit* parent)
{
	const HRESULT closed = CloseDropUndoGroup(manager, parent, false);
	if (FAILED(closed)) return closed;
	PasteUndoRecordingScope recording(manager);
	if (!recording.Blocked()) return E_UNEXPECTED;
	const HRESULT reverted = parent->Do(manager), blocked = recording.Close();
	return FAILED(reverted) ? reverted : blocked;
}

namespace {
class DropUndoProbe : public CComObjectRootEx<CComSingleThreadModel>, public IOleUndoUnit {
public:
	BEGIN_COM_MAP(DropUndoProbe)
		COM_INTERFACE_ENTRY(IOleUndoUnit)
	END_COM_MAP()
	int calls = 0, notifications = 0;
	IOleUndoManager* received = reinterpret_cast<IOleUndoManager*>(1);
	HRESULT result = S_OK;
	STDMETHOD(Do)(IOleUndoManager* manager) { received = manager; ++calls; return result; }
	STDMETHOD(GetDescription)(BSTR* text) { if (!text) return E_POINTER; *text = ::SysAllocString(L"Probe"); return *text ? S_OK : E_OUTOFMEMORY; }
	STDMETHOD(GetUnitType)(CLSID* clsid, LONG* id) { if (!clsid || !id) return E_POINTER; *clsid = CLSID_NULL; *id = 0; return S_OK; }
	STDMETHOD(OnNextAdd)() { ++notifications; return S_OK; }
};
}

bool CFBEView::VerifyDropUndoContractForTest()
{
	CComObject<DropUndoUnit>* raw = nullptr;
	CComObject<DropUndoUnit>* nested = nullptr;
	CComObject<DropUndoProbe>* first = nullptr;
	CComObject<DropUndoProbe>* last = nullptr;
	if (FAILED(CComObject<DropUndoUnit>::CreateInstance(&raw))) return false;
	CComPtr<IOleParentUndoUnit> parent(raw);
	if (FAILED(CComObject<DropUndoUnit>::CreateInstance(&nested))) return false;
	CComPtr<IOleParentUndoUnit> child(nested);
	if (FAILED(CComObject<DropUndoProbe>::CreateInstance(&first))) return false;
	CComPtr<IOleUndoUnit> a(first);
	if (FAILED(CComObject<DropUndoProbe>::CreateInstance(&last))) return false;
	CComPtr<IOleUndoUnit> b(last);
	DWORD state = 99;
	bool ok = parent->Add(a) == S_OK && parent->Open(child) == S_OK && parent->Add(b) == S_OK;
	ok = ok && parent->FindUnit(b) == S_OK && parent->FindUnit(nullptr) == E_POINTER;
	ok = ok && parent->Close(parent, TRUE) == E_INVALIDARG && parent->FindUnit(b) == S_OK;
	ok = ok && parent->Close(child, TRUE) == S_OK && parent->Close(parent, TRUE) == S_FALSE;
	ok = ok && parent->GetParentState(&state) == S_OK && state == UAS_NORMAL && first->notifications == 1;
	ok = ok && parent->Do(nullptr) == S_OK && first->calls == 1 && last->calls == 1 && !first->received && !last->received;
	last->result = E_FAIL;
	ok = ok && parent->Do(nullptr) == E_FAIL && first->calls == 1 && last->calls == 2;
	return ok;
}

void CFBEView::ResetPendingDrop()
{
	bool changed = true;
	try { changed = !m_drag_mouse_armed && m_drop_version >= 0 && GetVersionNumber() != m_drop_version; } catch (const _com_error&) {}
	if (m_drop_undo_open)
	{
		const HRESULT closed = CloseDropUndoGroup(m_drop_undo_manager, m_drop_undo_parent, changed);
		if (FAILED(closed))
		{
			StartupTrace::HResult(L"undo", L"D411", closed, L"Drop undo group could not be closed");
			return; // Retain ownership so lifecycle cleanup can retry.
		}
		m_drop_undo_open = false;
	}
	++m_drop_generation;
	m_pending_drop_normalization_scope = nullptr;
	m_drag_source_scope = nullptr;
	m_drop_source_html.Empty(); m_drop_target_html.Empty();
	m_internal_drag = false; m_drop_received = false; m_drag_mouse_armed = false;
	m_drop_version = -1;
	m_drop_undo_parent.Release();
	m_drop_undo_manager.Release();
}

VARIANT_BOOL CFBEView::OnDragStart(IDispatch*)
{
	if (!m_drag_mouse_armed) ResetPendingDrop();
	if (m_drop_undo_open && !m_drag_mouse_armed) return VARIANT_FALSE;
	if (m_normalize)
	{
		try {
			m_internal_drag = true;
			m_drag_mouse_armed = false;
			m_drag_source_scope = ResolveNormalizationScope();
			if (!IsLiveDropScope(m_drag_source_scope, Document())) m_drag_source_scope = nullptr;
			// MSHTML opens its editing parent BEFORE ondrop. Own the outer group
			// before that parent exists, so the browser can close its own child.
			if (!m_drop_undo_open) {
				OpenDropUndoGroup(m_drop_undo_manager, m_drop_undo_parent);
				m_drop_undo_open = true;
			}
			m_drop_version = GetVersionNumber();
			static_cast<DropUndoUnit*>(m_drop_undo_parent.p)->PrepareRollback(m_drop_undo_manager);
		} catch (const _com_error&) { ResetPendingDrop(); return VARIANT_FALSE; }
	}
	return VARIANT_TRUE;
}

VARIANT_BOOL CFBEView::OnDragHover(IDispatch*)
{
	if (!m_normalize || m_internal_drag || m_drop_undo_open) return VARIANT_TRUE;
	try {
		// External text has no dragstart in this document. Hover precedes the
		// native drop transaction; leaving the view discards this empty parent.
		OpenDropUndoGroup(m_drop_undo_manager, m_drop_undo_parent);
		m_drop_undo_open = true;
		m_drop_version = GetVersionNumber();
	} catch (const _com_error&) { ResetPendingDrop(); return VARIANT_FALSE; }
	return VARIANT_TRUE;
}

void CFBEView::OnDragLeave(IDispatch*)
{
	if (!m_internal_drag && !m_drop_received) ResetPendingDrop();
}

VARIANT_BOOL CFBEView::OnDragDrop(IDispatch* event)
{
	if (!m_normalize) return VARIANT_TRUE;
	if (m_drop_received) return VARIANT_FALSE;
	try
	{
		MSHTML::IHTMLEventObjPtr dropEvent(event);
		MSHTML::IHTMLElementPtr origin(dropEvent ? dropEvent->srcElement : nullptr);
		MSHTML::IHTMLElementPtr target(FindNormalizationOwner(origin));
		m_pending_drop_normalization_scope = MSHTML::IHTMLDOMNodePtr(target);
		if (!IsLiveDropScope(m_pending_drop_normalization_scope, Document())) m_pending_drop_normalization_scope = nullptr;
		if (m_drag_source_scope) m_drop_source_html = static_cast<LPCWSTR>(MSHTML::IHTMLElementPtr(m_drag_source_scope)->innerHTML);
		if (m_pending_drop_normalization_scope) m_drop_target_html = static_cast<LPCWSTR>(target->innerHTML);
		m_drop_version = GetVersionNumber();
		// Keep the browser's insertion, source deletion and normalization in one
		// undo unit. Neither drop nor dragend observes the final internal-move DOM.
		if (!m_drop_undo_open) {
			OpenDropUndoGroup(m_drop_undo_manager, m_drop_undo_parent);
			m_drop_undo_open = true;
		}
		m_drop_received = true;
		static_cast<DropUndoUnit*>(m_drop_undo_parent.p)->PrepareRollback(m_drop_undo_manager);
		// An internal move deletes its source after the target's Drop. Wait for
		// dragend before queuing the finalizer outside the native drag loop.
		if (!m_internal_drag && !PostMessage(AU::WM_FINALIZE_DROP, m_drop_generation)) { ResetPendingDrop(); return VARIANT_FALSE; }
	}
	catch (const _com_error&)
	{
		ResetPendingDrop();
		return VARIANT_FALSE; // Do not let an ungrouped native edit proceed.
	}
	return VARIANT_TRUE; // Preserve MSHTML's standard drop operation.
}

void CFBEView::OnDragEnd(IDispatch*)
{
	// No drop in this view means cancellation or a drop outside the editor.
	// The queued drop finalizer handles successful internal and external drops.
	if (m_drop_undo_open && m_drop_received) {
		if (!PostMessage(AU::WM_FINALIZE_DROP, m_drop_generation)) ResetPendingDrop();
	} else ResetPendingDrop();
}

LRESULT CFBEView::OnFinalizeDrop(UINT, WPARAM generation, LPARAM, BOOL&)
{
	if (generation != m_drop_generation || !m_drop_undo_open) return 0;
	try
	{
		if (m_drop_version >= 0 && GetVersionNumber() != m_drop_version)
		{
			MSHTML::IHTMLTxtRangePtr caret;
			_bstr_t bookmark;
			DropSelectionEndpoint selectionStart, selectionEnd;
			try
			{
				caret = Document()->selection->createRange();
				if (caret) {
					bookmark = caret->getBookmark();
					selectionStart.Capture(caret, true, Document());
					selectionEnd.Capture(caret, false, Document());
				}
			}
			catch (const _com_error&) {} // A control selection has no text range.
			CString selectionTrace;
			selectionTrace.Format(L"start=%ld; end=%ld; start-index=%ld; end-index=%ld", selectionStart.offset, selectionEnd.offset, selectionStart.index, selectionEnd.index);
			StartupTrace::Event(L"drop", L"D412", selectionTrace);
			CComPtr<IOleUndoManager> normalizationManager;
			CComPtr<IOleParentUndoUnit> normalizationParent;
			OpenDropUndoGroup(normalizationManager, normalizationParent);
			HRESULT normalized = S_OK;
			try
			{
				// Never broaden an unknown/stale endpoint to document.body. Each
				// surviving changed owner is normalized once; siblings are untouched.
				auto normalizeChanged = [&](MSHTML::IHTMLDOMNodePtr scope, const CString& before) {
					if (IsLiveDropScope(scope, Document()) &&
						CString(static_cast<LPCWSTR>(MSHTML::IHTMLElementPtr(scope)->innerHTML)) != before)
						CheckError(NormalizeWithoutUndo(scope));
				};
				normalizeChanged(m_drag_source_scope, m_drop_source_html);
				if (m_pending_drop_normalization_scope != m_drag_source_scope)
				{
					CheckError(FbeUndo::Inject(L"normalize-target"));
					normalizeChanged(m_pending_drop_normalization_scope, m_drop_target_html);
				}
			}
			catch (const _com_error& error) { normalized = error.Error(); }
			catch (const std::bad_alloc&) { normalized = E_OUTOFMEMORY; }
			if (SUCCEEDED(normalized)) normalized = CloseDropUndoGroup(normalizationManager, normalizationParent, true);
			if (FAILED(normalized)) {
				if (FAILED(CloseDropUndoGroup(normalizationManager, normalizationParent, true)) ||
					FAILED(CloseDropUndoGroup(m_drop_undo_manager, m_drop_undo_parent, true))) normalized = E_ABORT;
				else {
					m_drop_undo_open = false;
					if (FAILED(static_cast<DropUndoUnit*>(m_drop_undo_parent.p)->RollbackRecorded(m_drop_undo_manager))) normalized = E_ABORT;
				}
			}
			if (caret && SUCCEEDED(normalized))
			{
				MSHTML::IHTMLTxtRangePtr start(selectionStart.Restore(Document())), end(selectionEnd.Restore(Document()));
				if (start && end) { start->setEndPoint(L"EndToStart", end); start->select(); }
				else {
				MSHTML::IHTMLTxtRangePtr restored(MSHTML::IHTMLBodyElementPtr(Document()->body)->createTextRange());
				if (bookmark.length() && restored->moveToBookmark(bookmark)) restored->select();
				else caret->select();
				}
			}
			CheckError(normalized);
		}
	}
	catch (_com_error& error) { U::ReportError(error); }
	ResetPendingDrop();
	return 0;
}

MSHTML::IHTMLDOMNodePtr CFBEView::ResolveNormalizationScope()
{
	try
	{
		// Resolve both endpoints before IDM_PASTE changes the live MSHTML range.
		// A cross-owner selection can alter multiple structural containers, so
		// deliberately retain the conservative body-wide normalizer for it.
		MSHTML::IHTMLElementPtr selectionBegin, selectionEnd;
		int beginChar = 0;
		int endChar = 0;
		if (GetSelectionInfo(std::addressof(selectionBegin), std::addressof(selectionEnd), &beginChar, &endChar, MSHTML::IHTMLTxtRangePtr()))
		{
			MSHTML::IHTMLElementPtr beginOwner(FindNormalizationOwner(selectionBegin));
			MSHTML::IHTMLElementPtr endOwner(FindNormalizationOwner(selectionEnd));
			if (beginOwner && endOwner && beginOwner == endOwner)
				return MSHTML::IHTMLDOMNodePtr(beginOwner);
		}
	}
	catch (_com_error&) {}
	return MSHTML::IHTMLDOMNodePtr(Document() ? Document()->body : NULL);
}

void CFBEView::NormalizeScope(MSHTML::IHTMLDOMNodePtr dom)
{
	// OnPaste already owns the user-visible undo unit.  Keeping normalization
	// inside it makes one Undo restore the pre-paste DOM.
	CheckError(NormalizeWithoutUndo(dom));
}

HRESULT CFBEView::NormalizeWithoutUndo(MSHTML::IHTMLDOMNodePtr dom)
{
  try {
    MSHTML::IHTMLElementPtr scope(dom);
    if (!scope || (!IsNormalizationOwner(scope) && U::scmp(scope->id, L"fbw_body") != 0)) {
      MSHTML::IHTMLDOMNodePtr child(dom ? dom->firstChild : MSHTML::IHTMLDOMNodePtr());
      for (; child; child = child->nextSibling) {
        MSHTML::IHTMLElementPtr candidate(child);
        if (candidate && U::scmp(candidate->id, L"fbw_body") == 0) { scope = candidate; break; }
      }
      if (!child) return E_INVALIDARG;
    }
    const bool fullNormalization = U::scmp(scope->id, L"fbw_body") == 0;
    g_normalizationDiagnostics.Record(fullNormalization);
    MSHTML::IHTMLDOMNodePtr transform(scope);
    // Title repair may merge siblings in its containing section.
    if (U::scmp(scope->className, L"title") == 0) {
      for (MSHTML::IHTMLElementPtr ancestor(scope->parentElement); ancestor; ancestor = ancestor->parentElement)
        if (U::scmp(ancestor->className, L"section") == 0) { scope = ancestor; break; }
    }
    CComPtr<IOleUndoManager> manager(GetPasteUndoManager(Document()));
    if (!manager) return E_NOINTERFACE;
    DWORD parentState = 0;
    const HRESULT parentStatus = manager->GetOpenParentState(&parentState);
    CheckError(parentStatus);
    const bool blocked = parentStatus == S_OK && (parentState & UAS_BLOCKED) != 0;
    const auto savepoints = m_ownerDocument ? m_ownerDocument->CaptureUndoSavepoints() : FB::Doc::UndoSavepoints{};
    const long version = GetVersionNumber();
    MSHTML::IHTMLTxtRangePtr selection;
    DropSelectionEndpoint start, end;
    try {
      selection = Document()->selection->createRange();
      start.Capture(selection, true, Document()); end.Capture(selection, false, Document());
    } catch (const _com_error&) {}
    PasteUndoRecordingScope compensation(manager, false), recording(manager);
    if (!recording.Blocked()) return E_UNEXPECTED;
    PasteRestoreJournal journal;
    bool applied = false;
    HRESULT result = S_OK;
    try {
      // Work on an orphan copy: MSHTML inner/outerHTML setters can destroy
      // descendants even while COM references to them are retained. A failed
      // normalization must never touch nodes referenced by previous history.
      MSHTML::IHTMLDOMNodePtr original(scope), copy(original->cloneNode(VARIANT_TRUE));
      std::vector<long> path;
      for (MSHTML::IHTMLDOMNodePtr node(transform); node != original; node = node->parentNode) {
        long index = 0; for (MSHTML::IHTMLDOMNodePtr prior(node->previousSibling); prior; prior = prior->previousSibling) ++index;
        path.push_back(index);
      }
      MSHTML::IHTMLDOMNodePtr copyScope(copy);
      for (auto index = path.rbegin(); index != path.rend(); ++index) {
        copyScope = copyScope->firstChild;
        for (long i = 0; i < *index; ++i) copyScope = copyScope->nextSibling;
      }
      RemoveUnk(copyScope, Document()); CheckError(FbeUndo::Inject(L"normalize-remove"));
      MergeEqualHTMLElements(copyScope, Document()); CheckError(FbeUndo::Inject(L"normalize-merge"));
      FbeVisualDom::NormalizeStructure(Document(), copyScope); CheckError(FbeUndo::Inject(L"normalize-structure"));
      FixupLinks(copyScope); CheckError(FbeUndo::Inject(L"normalize-links"));
      auto before = CapturePasteChildren(original, nullptr, false), after = CapturePasteChildren(copy, nullptr, false);
      CComObject<PasteDomUndoUnit>* raw = nullptr;
      CheckError(CComObject<PasteDomUndoUnit>::CreateInstance(&raw));
      CComPtr<IOleUndoUnit> unit(raw);
      raw->Initialize(original, std::move(before), std::move(after), selection, selection, m_ownerDocument,
        savepoints.clean, savepoints.recoveryClean, version);
      if (raw->Changed()) {
        if (!blocked) {
          // Preserve MSHTML's native markup pointers for the preceding drop.
          // The orphan pass above validates the complete transformation, then
          // the live pass records ordinary native undo in the current parent.
          CheckError(recording.Close());
          CComPtr<IOleUndoManager> liveManager; CComPtr<IOleParentUndoUnit> liveParent;
          OpenDropUndoGroup(liveManager, liveParent);
          if (parentStatus == S_FALSE) static_cast<DropUndoUnit*>(liveParent.p)->PrepareRollback(liveManager);
          HRESULT liveResult = S_OK;
          try {
                FbeDom::MarkupUndoUnitScope native(m_mk_srv, L"Normalize DOM");
            RemoveUnk(transform, Document()); MergeEqualHTMLElements(transform, Document());
            FbeVisualDom::NormalizeStructure(Document(), transform); FixupLinks(transform);
            CheckError(FbeUndo::Inject(L"normalize-live"));
                native.Close();
            CheckError(FbeUndo::Inject(L"normalize-close"));
            CheckError(FbeUndo::Inject(L"normalize-add"));
          } catch (const _com_error& error) { liveResult = error.Error(); }
          catch (const std::bad_alloc&) { liveResult = E_OUTOFMEMORY; }
          const HRESULT closed = CloseDropUndoGroup(liveManager, liveParent, true);
          if (FAILED(closed)) return E_ABORT;
          if (FAILED(liveResult)) {
            if (parentStatus == S_FALSE && FAILED(static_cast<DropUndoUnit*>(liveParent.p)->RollbackRecorded(liveManager))) return E_ABORT;
            CheckError(liveResult);
          }
        } else {
        raw->RetainOriginalChildren(); // allocate all inverse state before mutation
        CheckError(raw->Restore(false, &journal)); applied = true;
        CheckError(FbeUndo::Inject(L"normalize-close")); CheckError(recording.Close());
        CheckError(FbeUndo::Inject(L"normalize-add")); CheckError(manager->Add(unit));
        }
      } else CheckError(recording.Close());
    } catch (const _com_error& error) { result = error.Error(); }
    catch (const std::bad_alloc&) { result = E_OUTOFMEMORY; }
    if (FAILED(result)) {
      if (applied) {
        if (!recording.Blocked() && FAILED(compensation.Open())) return E_ABORT;
        if (FAILED(journal.Rollback())) return E_ABORT;
      }
      const HRESULT closed = recording.Close(), compensated = compensation.Close();
      if (FAILED(closed) || FAILED(compensated)) return E_ABORT;
      if (m_ownerDocument && result != E_ABORT) m_ownerDocument->RestoreUndoSavepoints(savepoints);
    }
    try {
      if (selection) {
        MSHTML::IHTMLTxtRangePtr first(start.Restore(Document())), last(end.Restore(Document()));
        if (first && last) { first->setEndPoint(L"EndToStart", last); first->select(); }
        else selection->select();
      }
    } catch (const _com_error&) {}
    return result;
  } catch (const _com_error& error) { return error.Error(); }
  catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
}
void CFBEView::Normalize(MSHTML::IHTMLDOMNodePtr dom)
{
	CheckError(NormalizeWithoutUndo(dom));
}

LRESULT CFBEView::OnUndo(WORD, WORD, HWND, BOOL&)
{
	LRESULT result = ExecCommand(IDM_UNDO);
	::SendMessage(m_frame, WM_COMMAND, static_cast<WPARAM>(MAKELONG(0, IDN_TREE_RESTORE)), 0);
	return result;
}

LRESULT CFBEView::OnCopy(WORD, WORD, HWND, BOOL&)
{
	const DWORD clipboardBefore = ::GetClipboardSequenceNumber();
	ExecCommand(IDM_COPY);
	if (m_normalize && m_ownerDocument && ::GetClipboardSequenceNumber() != clipboardBefore)
		FbeClipboard::HtmlPasteSanitizer::MarkInternalCopy(m_hWnd, m_ownerDocument->ClipboardIdentity());
	return 0;
}

LRESULT CFBEView::OnCut(WORD, WORD, HWND, BOOL&)
{
	const DWORD clipboardBefore = ::GetClipboardSequenceNumber();
	ExecCommand(IDM_CUT);
	if (m_normalize && m_ownerDocument && ::GetClipboardSequenceNumber() != clipboardBefore)
		FbeClipboard::HtmlPasteSanitizer::MarkInternalCopy(m_hWnd, m_ownerDocument->ClipboardIdentity());
	return 0;
}

LRESULT CFBEView::OnPaste(WORD, WORD command, HWND, BOOL&)
{
	const wchar_t* pasteStage = L"CP301";
	const wchar_t* pasteRoute = L"CP401";
	try
	{
        if (m_pendingHtmlPaste) {
            BOOL handled = FALSE;
            OnDeferredHtmlPaste(0, m_pendingHtmlPaste->generation, 0, handled);
        }
        pasteStage = L"CP302";
        auto captured = std::move(m_activeHtmlPaste);
		PasteEnableScope pasteEnabled(m_enable_paste);

		FbeClipboard::ClipboardPasteOptions options;
		options.nbspReplacement = _Settings.GetNBSPChar();
		options.bitmapOutput = _Settings.GetImageType() == 0
			? FbeClipboard::ClipboardBitmapOutput::Png
			: FbeClipboard::ClipboardBitmapOutput::Jpeg;
		options.jpegQuality = _Settings.GetJpegQuality();
		pasteStage = L"CP303";
		MSHTML::IHTMLTxtRangePtr pasteRange;
		if (m_normalize)
			pasteRange = MSHTML::IHTMLTxtRangePtr(Document()->selection->createRange());
		bool insideParagraph = false;
		if (pasteRange)
		{
			for (MSHTML::IHTMLElementPtr parent(pasteRange->parentElement()); parent; parent = parent->parentElement)
			{
				if (U::scmp(parent->tagName, L"P") == 0) { insideParagraph = true; break; }
				if (U::scmp(parent->id, L"fbw_body") == 0) break;
			}
		}
		CString insertHtml;
		bool preparedHtml = false;
        CString nativePlainText(captured ? captured->plainText : CString());
        bool nativeText = captured && !captured->hasHtml && captured->hasText;
        // The direct command must use the same recorded native text insertion
        // as deferred events. MSHTML's IDM_PASTE may otherwise mutate the DOM
        // without adding an undo unit while a previous redo is available.
        if (!captured && m_normalize && command != ID_EDIT_PASTE_TEXT &&
            !::IsClipboardFormatAvailable(::RegisterClipboardFormat(L"HTML Format")) &&
            !::IsClipboardFormatAvailable(CF_BITMAP) && !::IsClipboardFormatAvailable(CF_DIB) &&
            !::IsClipboardFormatAvailable(CF_DIBV5))
            nativeText = FbeClipboard::HtmlPasteSanitizer::ReadPlainText(m_hWnd, nativePlainText);
		bool htmlClipboardDetected = false;
		if (m_normalize && command == ID_EDIT_PASTE_TEXT)
		{
			CString plainText;
			if (FbeClipboard::HtmlPasteSanitizer::ReadPlainText(m_hWnd, plainText))
			{
				insertHtml = FbeClipboard::HtmlPasteSanitizer::PlainTextToHtml(plainText, options.nbspReplacement, insideParagraph);
				preparedHtml = true;
			}
		}
		else if (m_normalize && command != ID_EDIT_PASTE_TEXT)
		{
			pasteStage = L"CP304";
            GUID clipboardOrigin = GUID_NULL;
            if (captured) clipboardOrigin = captured->origin;
			const bool internalPaste = captured ? captured->internal : FbeClipboard::HtmlPasteSanitizer::GetInternalCopyOrigin(m_hWnd, clipboardOrigin);
			pasteRoute = internalPaste ? L"CP402" : L"CP403";
			const bool sameDocument = internalPaste && m_ownerDocument &&
				::IsEqualGUID(clipboardOrigin, m_ownerDocument->ClipboardIdentity());
			CString fragment(captured ? captured->fragment : CString());
			const bool hasHtml = captured ? captured->hasHtml : ::IsClipboardFormatAvailable(::RegisterClipboardFormat(L"HTML Format")) != FALSE;
			htmlClipboardDetected = hasHtml;
			const bool readableHtml = captured ? captured->readableHtml : hasHtml && FbeClipboard::HtmlPasteSanitizer::ReadClipboardFragment(m_hWnd, fragment);
			if (readableHtml && internalPaste) fragment = FbeClipboard::HtmlPasteSanitizer::NormalizeInternalLinks(
				fragment, U::UrlFromPath(U::GetProgDirFile(L"main.html")));
			FbeClipboard::HtmlPasteOutcome outcome = FbeClipboard::HtmlPasteOutcome::PlainText;
			if (readableHtml) {
				if (internalPaste && FbeClipboard::HtmlPasteSanitizer::IsSafeInternalFragment(fragment, sameDocument,
					[this](const CString& id, FbeClipboard::InternalReferenceKind kind) {
						if (kind == FbeClipboard::InternalReferenceKind::Image) {
							_variant_t binary;
							const CString binaryRef(L"#" + id);
							return m_ownerDocument && m_ownerDocument->GetBinary(binaryRef, binary) &&
								binary.vt != VT_EMPTY && binary.vt != VT_NULL;
						}
						MSHTML::IHTMLElementPtr target(FBELinkNavigation::FindTargetElement(Document(), id));
						if (!target) return false;
						if (kind != FbeClipboard::InternalReferenceKind::Note) return true;
						if (U::scmp(target->tagName, L"DIV") != 0 || U::scmp(target->className, L"section") != 0) return false;
						for (MSHTML::IHTMLElementPtr parent(target->parentElement); parent; parent = parent->parentElement)
							if (U::scmp(parent->className, L"body") == 0) {
								const CString name(AU::GetAttrCS(parent, L"fbname"));
								return name.CompareNoCase(L"notes") == 0 || name.CompareNoCase(L"comments") == 0;
							}
						return false;
					},
					[document = Document()](const CString& id) {
						return !!FBELinkNavigation::FindTargetElement(document, id);
					})) {
					insertHtml = fragment;
					outcome = FbeClipboard::HtmlPasteOutcome::Sanitized;
				} else if (!internalPaste)
					outcome = FbeClipboard::HtmlPasteSanitizer::Sanitize(fragment, options.nbspReplacement, insertHtml);
			}
			preparedHtml = readableHtml && outcome == FbeClipboard::HtmlPasteOutcome::Sanitized;
			if (hasHtml && outcome != FbeClipboard::HtmlPasteOutcome::Sanitized)
			{
				CString lower(fragment); lower.MakeLower();
				const bool structuralHtml = internalPaste || !readableHtml || outcome == FbeClipboard::HtmlPasteOutcome::Legacy ||
					lower.Find(L"<table") >= 0 || lower.Find(L"<img") >= 0 ||
					lower.Find(L"<ul") >= 0 || lower.Find(L"<ol") >= 0 ||
					lower.Find(L"<li") >= 0 || lower.Find(L"<math") >= 0;
				CString plainText(captured ? captured->plainText : CString());
				if (!structuralHtml && (captured ? captured->hasText : FbeClipboard::HtmlPasteSanitizer::ReadPlainText(m_hWnd, plainText)))
				{
					insertHtml = FbeClipboard::HtmlPasteSanitizer::PlainTextToHtml(plainText, options.nbspReplacement, insideParagraph);
					preparedHtml = true;
				}
				else { WarnUnsupportedHtmlPaste(m_hWnd); return 0; }
			}
		}
		if (!pasteRange) {
			if (m_normalize && htmlClipboardDetected) { WarnUnsupportedHtmlPaste(m_hWnd); return 0; }
			preparedHtml = false;
		}
		if (command == ID_EDIT_PASTE_TEXT && m_normalize && !preparedHtml)
			return 0;
        if (captured && !preparedHtml && !nativeText) {
            // Preserve the existing native bitmap/other-format path. Never
            // substitute unrelated newer clipboard data for a queued request.
            if (captured->clipboardSequence != ::GetClipboardSequenceNumber()) CheckError(E_ABORT);
        }

		// MSHTML moves the live selection while pasting. Keep the pre-paste
		// endpoints so a cross-owner insertion cannot be normalized too narrowly.
		pasteStage = L"CP305";
		MSHTML::IHTMLDOMNodePtr normalizationScope;
		if (m_normalize)
			normalizationScope = ResolveNormalizationScope();
		if (preparedHtml || nativeText)
		{
			MSHTML::IHTMLElementPtr undoScope(normalizationScope);
			if (!undoScope || (!IsNormalizationOwner(undoScope) && U::scmp(undoScope->id, L"fbw_body") != 0))
				undoScope = Document()->all->item(L"fbw_body");
			CComPtr<IOleUndoManager> manager(GetPasteUndoManager(Document()));
			if (!undoScope || !manager) return 0;
            CString lowerInsert(insertHtml); lowerInsert.MakeLower();
            const int firstParagraph = lowerInsert.Find(L"<p");
			MSHTML::IHTMLElementPtr paragraphAtEnd;
			if (firstParagraph >= 0 || lowerInsert.Find(L"<table") >= 0 ||
				lowerInsert.Find(L"<div") >= 0) {
				for (MSHTML::IHTMLElementPtr element(pasteRange->parentElement()); element; element = element->parentElement) {
					if (U::scmp(element->tagName, L"P") == 0) {
						MSHTML::IHTMLTxtRangePtr endRange(MSHTML::IHTMLBodyElementPtr(Document()->body)->createTextRange());
						endRange->moveToElementText(element); endRange->collapse(VARIANT_FALSE);
						if (pasteRange->compareEndPoints(L"StartToStart", endRange) == 0 &&
							pasteRange->compareEndPoints(L"EndToEnd", endRange) == 0) paragraphAtEnd = element;
						break;
					}
					if (element == undoScope) break;
				}
				if (!paragraphAtEnd) {
					MSHTML::IHTMLTxtRangePtr preceding(pasteRange->duplicate());
					if (preceding->move(L"character", -1) == -1) {
						for (MSHTML::IHTMLElementPtr element(preceding->parentElement()); element; element = element->parentElement) {
							if (U::scmp(element->tagName, L"P") == 0) {
								MSHTML::IHTMLTxtRangePtr endRange(MSHTML::IHTMLBodyElementPtr(Document()->body)->createTextRange());
								endRange->moveToElementText(element); endRange->collapse(VARIANT_FALSE);
								if (pasteRange->compareEndPoints(L"StartToStart", endRange) == 0 &&
									pasteRange->compareEndPoints(L"EndToEnd", endRange) == 0) paragraphAtEnd = element;
								if (!paragraphAtEnd && pasteRange->compareEndPoints(L"StartToEnd", pasteRange) == 0) {
									MSHTML::IHTMLDOMNodePtr candidate(element), original(pasteRange->parentElement());
									while (original && original->parentNode != candidate->parentNode) original = original->parentNode;
									if (original && original == candidate->nextSibling) paragraphAtEnd = element;
								}
								break;
							}
							if (element == undoScope) break;
						}
					}
				}
			}
            MSHTML::IHTMLElementPtr splitParagraph;
            CString splitId;
            MSHTML::IHTMLDOMNodePtr splitNext;
            for (MSHTML::IHTMLElementPtr element(pasteRange->parentElement()); element && element != undoScope; element = element->parentElement) {
                if (U::scmp(element->tagName, L"P") == 0) {
                    splitParagraph = element; splitId = static_cast<LPCWSTR>(element->id);
                    splitNext = MSHTML::IHTMLDOMNodePtr(element)->nextSibling; break;
                }
            }
            // Keep MSHTML's native edit units and markup pointers. Replacing
            // their nodes with snapshot clones breaks earlier native history.
            CComPtr<IOleUndoManager> pasteManager;
            CComPtr<IOleParentUndoUnit> pasteParent;
            pasteStage = L"CP306";
            OpenDropUndoGroup(pasteManager, pasteParent);
            auto transaction = static_cast<DropUndoUnit*>(pasteParent.p);
            HRESULT result = S_OK;
            try {
                transaction->SetPaste();
                pasteStage = L"CP307";
                transaction->PrepareRollback(pasteManager);
                pasteStage = L"CP308";
                FbeDom::MarkupUndoUnitScope native(m_mk_srv, L"Paste HTML");
                pasteStage = L"CP309";
                if (nativeText) {
                    // TextRange uses MSHTML's text insertion rules (including
                    // merging the first/last line into the current paragraph),
                    // while reading the immutable event-time clipboard value.
                    CString text(nativePlainText);
                    text.Replace(L"\u00a0", options.nbspReplacement);
                    pasteRange->text = _bstr_t(text);
                    pasteRange->collapse(VARIANT_FALSE); pasteRange->select();
                } else if (paragraphAtEnd) {
					MSHTML::IHTMLDOMNodePtr paragraphNode(paragraphAtEnd);
					MSHTML::IHTMLDOMNodePtr nextOriginal(paragraphNode->nextSibling);
					paragraphAtEnd->insertAdjacentHTML(L"afterEnd", _bstr_t(insertHtml));
					MSHTML::IHTMLDOMNodePtr lastInserted;
					for (MSHTML::IHTMLDOMNodePtr node(paragraphNode->nextSibling); node && node != nextOriginal; node = node->nextSibling)
						if (node->nodeType == 1) lastInserted = node;
					if (lastInserted) {
						MSHTML::IHTMLTxtRangePtr placed(MSHTML::IHTMLBodyElementPtr(Document()->body)->createTextRange());
						placed->moveToElementText(MSHTML::IHTMLElementPtr(lastInserted));
						placed->collapse(VARIANT_FALSE); placed->select();
					}
				} else pasteRange->pasteHTML(_bstr_t(insertHtml));
                // pasteHTML copies the original paragraph's ID to its split
                // tail. Keep the anchor on the original node only; record the
                // removal natively so Undo restores the original attributes.
                if (splitParagraph && !splitId.IsEmpty()) {
                    for (MSHTML::IHTMLDOMNodePtr node(MSHTML::IHTMLDOMNodePtr(splitParagraph)->nextSibling); node && node != splitNext; node = node->nextSibling) {
                        MSHTML::IHTMLElementPtr paragraph(node);
                        if (paragraph && U::scmp(paragraph->tagName, L"P") == 0 && splitId == CString(static_cast<LPCWSTR>(paragraph->id)))
                            paragraph->removeAttribute(L"id", 0);
                    }
                }
                CaptureClipboardTitlePasteDomForTest(Document());
                pasteStage = L"CP310";
                native.Close();
                pasteEnabled.Close();
                pasteStage = L"CP311";
                NormalizeScope(normalizationScope);
                pasteStage = L"CP312";
                transaction->FinishPaste();
            } catch (const _com_error& error) { result = error.Error(); CString diagnostic; diagnostic.Format(L"stage=%s; operation-stage=%s", pasteStage, pasteRoute); StartupTrace::HResult(L"clipboard", L"CP200", result, diagnostic); }
            catch (const std::bad_alloc&) { result = E_OUTOFMEMORY; }
            catch (...) { result = E_UNEXPECTED; }
            pasteStage = L"CP313";
            const HRESULT closed = CloseDropUndoGroup(pasteManager, pasteParent, true);
            if (FAILED(closed)) { StartupTrace::HResult(L"clipboard", L"CP203", closed, L"close parent undo group"); CheckError(E_ABORT); }
            if (FAILED(result)) {
                if (!transaction->Empty()) {
                    const HRESULT rollback = transaction->RollbackRecorded(pasteManager);
                    if (FAILED(rollback)) { StartupTrace::HResult(L"clipboard", L"CP204", rollback, L"paste rollback"); CheckError(E_ABORT); }
                }
                CheckError(result);
            }
			return 0;
		}
		FbeDom::MarkupUndoUnitScope undo(m_mk_srv, L"Paste");
		const FbeClipboard::ClipboardPastePreparationResult preparation =
			FbeClipboard::ClipboardPastePreparer::Prepare(m_hWnd, options);
		if (preparation.HasPreparedBitmap())
			AddImage(preparation.temporaryImagePath, true);
		IOleCommandTargetPtr(m_browser)->Exec(&CGID_MSHTML, IDM_PASTE, 0, NULL, NULL);
		CaptureClipboardTitlePasteDomForTest(Document());
		pasteEnabled.Close();
		if(m_normalize)
			NormalizeScope(normalizationScope);
		undo.Close();
	}
	catch(_com_error& err)
	{
		CString diagnostic; diagnostic.Format(L"stage=%s; operation-stage=%s", pasteStage, pasteRoute);
		StartupTrace::HResult(L"clipboard", L"CP201", err.Error(), diagnostic);
		U::ReportError(err);
	}

	return 0;
}

// searching
bool CFBEView::DoSearch(bool fMore)
{
	m_last_search_error.Empty();
	if(m_fo.pattern.IsEmpty())
	{
		if(m_is_start)
			m_is_start->raw_select();
		return true;
	}

	NormalizeSearchPatternNbsp(m_fo.pattern);

	return m_fo.fRegexp ? DoSearchRegexp(fMore) : DoSearchStd(fMore);
}

bool CFBEView::DoSearchFromScopeStart()
{
	m_last_search_error.Empty();
	if (m_fo.pattern.IsEmpty())
		return true;
	NormalizeSearchPatternNbsp(m_fo.pattern);
	return DoSearchNative(true, m_fo.fRegexp ? AU::Search::SearchMode::Regex : AU::Search::SearchMode::Literal, true);
}

// Removes HTML tags
void RemoveTags(CString &src)
{
	int openTag = 0, closeTag=0;
	while (openTag != -1)
	{
		openTag = src.Find(L"<", 0);
		closeTag = src.Find(L">", openTag+1);
		if (openTag != -1 && closeTag > openTag)
			src.Delete(openTag, closeTag-openTag+1);
	}
}

// Returns text offset including inline images (treated as a 3 chars each)
int CFBEView::TextOffset(MSHTML::IHTMLTxtRange *rng, AU::ReMatch rm, CString txt, CString htmlTxt)
{
	CString text(txt);
	CString match(static_cast<const wchar_t*>(rm->Value));
	// special fix for "Words" dialog
	match = match.TrimRight(10);
	match = match.TrimRight(13);
	int num = 0, pos = 1;
	if (text.IsEmpty()) text.SetString(rng->text);
	while (num < text.GetLength())
	{
		num = text.Find (match, num);
		if ((num == rm->FirstIndex) || (num == -1)) break;
		num += 1;
		pos++;
	}
	CString html(htmlTxt);
	if (html.IsEmpty()) html.SetString(rng->htmlText);

	// change <IMG to "afro-american" O<IMG LOL
	html.Replace (L"<IMG", L"O<IMG");
	RemoveTags(html);

	num = 0;
	for (int i=0; i<pos; i++)
	{
		num = html.Find (match, num);
		num += 1;
	}
	// find number of inline images occurences
	html = html.Left(num);
	pos = num = 0;
	while (num < html.GetLength())
	{
		num = html.Find(L"O", num);
		if (num == -1) break;
		num += 1;
		pos++;
	}
	return pos*3;
}

static void MoveRangeToRegexMatch(MSHTML::IHTMLTxtRange* range, AU::ReMatch match, int offset)
{
	range->collapse(VARIANT_TRUE);
	range->move(L"character", match->FirstIndex + offset);
	if(range->moveStart(L"character", 1) == 1)
		range->move(L"character", -1);
	range->moveEnd(L"character", match->Length);
}

void CFBEView::SelMatch(MSHTML::IHTMLTxtRange *tr,AU::ReMatch rm) 
{
	// SeNS: fix for issue #147
	int numImages = TextOffset (tr, rm);

	MoveRangeToRegexMatch(tr, rm, numImages);
	// set focus to editor if selection empty
	if (!rm->Length)
		SetFocus();
	tr->select();
	PositionFoundRange(tr);
	m_fo.ClearMatch();
	m_fo.match = new AU::IMatch2(*rm);
	m_fo.hasMatch = true;
}

CString CFBEView::SearchResultStatus()
{
	const std::uint64_t version = SearchDocumentGeneration();
	const AU::Search::SearchResults& results = m_design_search.Coordinator().GetResults();
	if (!results.IsValidFor(version) ||
		results.GetSelected() == NULL)
		return CString();
	CString status;
	status.Format(L"%u / %u",
		static_cast<unsigned>(results.GetSelectedIndex() + 1),
		static_cast<unsigned>(results.GetCount()));
	return status;
}

CString CFBEView::FindAllResultStatus()
{
	const std::uint64_t version = SearchDocumentGeneration();
	const AU::Search::SearchResults& results = m_design_search.Coordinator().GetResults();
	if (!results.IsValidFor(version))
		return CString();
	CString status;
	status.Format(FbeLoadRuntimeStringByKey(L"fbe.search.results.found", L"%u found"),
		static_cast<unsigned>(results.GetCount()));
	return status;
}

std::size_t CFBEView::FindResultCount() const
{
	return m_design_search.Coordinator().GetResults().GetCount();
}

void CFBEView::SetFindResultsCompletionStatus(const CString& status)
{
	m_find_results_completion_status = status;
	::SendMessage(m_frame, AU::WM_REFRESH_FIND_RESULTS_PANE, reinterpret_cast<WPARAM>(this), 0);
}

CString CFBEView::FindResultPreview(std::size_t index) const
{
	std::wstring preview;
	return m_design_search.Coordinator().GetResultPreview(index, &preview)
		? CString(preview.c_str()) : CString();
}

bool CFBEView::FindResultPreviewMatch(std::size_t index, std::size_t* start, std::size_t* length) const
{
	return start != NULL && length != NULL &&
		m_design_search.Coordinator().GetResultPreview(index, NULL, start, length);
}

bool CFBEView::AreFindResultsCurrent()
{
	return m_design_search.Coordinator().GetResults().IsValidFor(SearchDocumentGeneration());
}

std::uint64_t CFBEView::FindResultsRevision() const
{
	return m_design_search.Coordinator().GetResults().GetRevision();
}

bool CFBEView::SelectFindResult(std::size_t index)
{
	try
	{
		if (!Document() || !AreFindResultsCurrent())
			return false;
		if (m_design_search.Coordinator().SelectResult(Document(), SearchDocumentGeneration(), index) == NULL)
			return false;
		PositionFoundRange(MSHTML::IHTMLTxtRangePtr(Document()->selection->createRange()));
		RefreshSearchHighlights();
		return true;
	}
	catch (const _com_error&)
	{
		return false;
	}
}

void CFBEView::ClearSearchHighlights()
{
	if (m_search_highlight_overlay)
		m_search_highlight_overlay->Clear();
}

void CFBEView::AdvanceSearchDocumentGeneration(bool refreshFindResultsPane)
{
	// Do this for every real dirty-range notification, including editor-owned
	// operations performed under m_ignore_changes. That flag suppresses the
	// application's dirty UI notification; it must never preserve stale search
	// offsets, replacement previews or highlight geometry.
	m_design_search.Advance();
	m_find_results_completion_status.Empty();
	ClearSearchHighlights();
	if (refreshFindResultsPane)
		::SendMessage(m_frame, AU::WM_REFRESH_FIND_RESULTS_PANE, reinterpret_cast<WPARAM>(this), 0);
}

LRESULT CFBEView::OnFinalizeReplaceAllCompletion(UINT, WPARAM, LPARAM, BOOL&)
{
	if (!m_design_search.ReplaceAllCompletionPending())
		return 0;

	// This posted turn follows the Replace All mutation and lets MSHTML deliver
	// queued RANGE_SINK notifications while the pending flag protects the pane.
	// Publish one final state, never an intermediate stale one.
	const int replaced = m_design_search.TakeReplaceAllCompletion();
	AdvanceSearchDocumentGeneration(false);
	CString completion;
	completion.Format(FbeLoadRuntimeStringByKey(L"fbe.replace.preview.completed", L"Replaced: %d"), replaced);
	m_find_results_completion_status = completion;
	::SendMessage(m_frame, AU::WM_REFRESH_FIND_RESULTS_PANE, reinterpret_cast<WPARAM>(this), 0);
	return 0;
}

void CFBEView::UpdateSearchHighlightsForScroll()
{
	// The scroll event is post-scroll. Recompute a bounded viewport subset so
	// entering new content creates highlights without touching all Find All rows.
	RefreshSearchHighlights();
}

bool CFBEView::TryGetViewportSearchRange(std::size_t* start, std::size_t* end)
{
	if (start == NULL || end == NULL || !Document())
		return false;
	MSHTML::IHTMLElement2Ptr scrollElement(MSHTML::IHTMLDocument3Ptr(Document())->documentElement);
	MSHTML::IHTMLBodyElementPtr body(Document()->body);
	if (!scrollElement || !body || scrollElement->clientHeight <= 0)
		return false;
	MSHTML::IHTMLTxtRangePtr topRange(body->createTextRange());
	MSHTML::IHTMLTxtRangePtr bottomRange(body->createTextRange());
	if (!topRange || !bottomRange)
		return false;
	const long probeX = (std::max)(1L, scrollElement->clientWidth / 2);
	const long bottomY = (std::max)(1L, scrollElement->clientHeight - 2);
	try
	{
		// Real viewport points avoid the left-edge ambiguity of elementFromPoint.
		topRange->moveToPoint(probeX, 1);
		bottomRange->moveToPoint(probeX, bottomY);
	}
	catch (const _com_error&)
	{
		// Old MSHTML engines occasionally reject moveToPoint over a control. Use
		// the same central probes only as a safe fallback, never x=0.
		MSHTML::IHTMLElementPtr topElement(Document()->elementFromPoint(probeX, 1));
		MSHTML::IHTMLElementPtr bottomElement(Document()->elementFromPoint(probeX, bottomY));
		if (!topElement || !bottomElement)
			return false;
		topRange->moveToElementText(topElement);
		bottomRange->moveToElementText(bottomElement);
	}
	const std::uint64_t generation = SearchDocumentGeneration();
	AU::Search::SearchRange topSearch = {}, bottomSearch = {};
	if (!m_design_search.Coordinator().TryGetSearchRange(generation, topRange, &topSearch) ||
		!m_design_search.Coordinator().TryGetSearchRange(generation, bottomRange, &bottomSearch))
		return false;
	*start = (std::min)(topSearch.Start, bottomSearch.Start);
	*end = (std::max)(topSearch.Start + topSearch.Length, bottomSearch.Start + bottomSearch.Length);
	return true;
}

void CFBEView::RefreshSearchHighlights()
{
	try
	{
		// Geometry extraction crosses the COM boundary and is particularly costly
		// for short/common queries.  Keep the Results pane and navigation complete,
		// but bound the visual overlay so Find All cannot monopolize the UI thread.
		const std::size_t maxOverlayRects = 128;
		if (!Document() || !AreFindResultsCurrent() || FindResultCount() == 0)
		{
			ClearSearchHighlights();
			return;
		}
		MSHTML::IHTMLElement2Ptr scrollElement(MSHTML::IHTMLDocument3Ptr(Document())->documentElement);
		if (!scrollElement)
		{
			ClearSearchHighlights();
			return;
		}
		std::size_t viewportStart = 0, viewportEnd = 0;
		if (!TryGetViewportSearchRange(&viewportStart, &viewportEnd))
		{
			ClearSearchHighlights();
			return;
		}
		const std::uint64_t generation = SearchDocumentGeneration();
		const AU::Search::SearchResults& results = m_design_search.Coordinator().GetResults();
		const AU::Search::SearchViewportSubset subset = AU::Search::SelectViewportResults(results, viewportStart, viewportEnd, maxOverlayRects);
		std::vector<RECT> documentRects;
		std::size_t selectedRect = static_cast<std::size_t>(-1);
		for (std::size_t index = subset.FirstIndex, remaining = subset.Count; remaining > 0; ++index, --remaining)
		{
			MSHTML::IHTMLTxtRangePtr range;
			if (!m_design_search.Coordinator().CreateResultRange(Document(), generation, index, range) || !range)
				continue;
			MSHTML::IHTMLTextRangeMetrics2Ptr metrics(range);
			MSHTML::IHTMLRectPtr rect(metrics ? metrics->getBoundingClientRect() : MSHTML::IHTMLRectPtr());
			if (!rect || rect->right < rect->left || rect->bottom < rect->top)
				continue;
			RECT documentRect = { rect->left + scrollElement->scrollLeft, rect->top + scrollElement->scrollTop,
				rect->right + scrollElement->scrollLeft, rect->bottom + scrollElement->scrollTop };
			documentRects.push_back(documentRect);
			if (index == m_design_search.Coordinator().GetSelectedResultIndex())
				selectedRect = documentRects.size() - 1;
		}
		if (documentRects.empty())
		{
			ClearSearchHighlights();
			return;
		}
		if (!m_search_highlight_overlay)
			m_search_highlight_overlay = new CSearchHighlightOverlay();
		m_search_highlight_overlay->Update(m_hWnd, documentRects, scrollElement->scrollLeft, scrollElement->scrollTop,
			selectedRect);
	}
	catch (const _com_error&)
	{
		ClearSearchHighlights();
	}
}

void CFBEView::ShowFindResults()
{
	::SendMessage(m_frame, AU::WM_SHOW_FIND_RESULTS_PANE, reinterpret_cast<WPARAM>(this), 0);
}

void CFBEView::PositionFoundRange(MSHTML::IHTMLTxtRange* range)
{
	try
	{
		if(!range || !Document()) return;
		MSHTML::IHTMLTextRangeMetrics2Ptr rangeMetrics(range);
		MSHTML::IHTMLRectPtr matchRect(rangeMetrics ? rangeMetrics->getBoundingClientRect() : MSHTML::IHTMLRectPtr());
		MSHTML::IHTMLElement2Ptr scrollElement(MSHTML::IHTMLDocument3Ptr(Document())->documentElement);
		if(!matchRect || !scrollElement || scrollElement->clientHeight <= 0) return;

		RECT viewportPixels = {};
		::GetClientRect(m_hWnd, &viewportPixels);
		const long viewportWidth = viewportPixels.right - viewportPixels.left;
		const long viewportHeight = viewportPixels.bottom - viewportPixels.top;
		POINT viewportTopLeft = { viewportPixels.left, viewportPixels.top };
		POINT viewportBottomRight = { viewportPixels.right, viewportPixels.bottom };
		::ClientToScreen(m_hWnd, &viewportTopLeft);
		::ClientToScreen(m_hWnd, &viewportBottomRight);

		std::vector<FBESearchViewport::Rect> obstructions;
		HWND dialogs[] = { m_find_dlg ? m_find_dlg->m_hWnd : NULL, m_replace_dlg ? m_replace_dlg->m_hWnd : NULL };
		for(unsigned index = 0; index < _countof(dialogs); ++index)
		{
			RECT dialogBounds = {};
			if(!dialogs[index] || !::IsWindowVisible(dialogs[index]) || !::GetWindowRect(dialogs[index], &dialogBounds)) continue;
			if(dialogBounds.right <= viewportTopLeft.x || dialogBounds.left >= viewportBottomRight.x || dialogBounds.bottom <= viewportTopLeft.y || dialogBounds.top >= viewportBottomRight.y) continue;
			FBESearchViewport::Rect obstruction = {
				FBESearchViewport::ScaleToViewport(dialogBounds.left - viewportTopLeft.x, viewportWidth, scrollElement->clientWidth),
				FBESearchViewport::ScaleToViewport(dialogBounds.top - viewportTopLeft.y, viewportHeight, scrollElement->clientHeight),
				FBESearchViewport::ScaleToViewport(dialogBounds.right - viewportTopLeft.x, viewportWidth, scrollElement->clientWidth),
				FBESearchViewport::ScaleToViewport(dialogBounds.bottom - viewportTopLeft.y, viewportHeight, scrollElement->clientHeight) };
			obstructions.push_back(obstruction);
		}
		const UINT dpi = GetSearchWindowDpi(m_hWnd);
		FBESearchViewport::Rect match = { matchRect->left, matchRect->top, matchRect->right, matchRect->bottom };
		FBESearchViewport::PlacementInput input = {
			scrollElement->scrollTop, scrollElement->scrollHeight, scrollElement->clientHeight, match,
			FBESearchViewport::PreferredMatchTop(scrollElement->clientHeight, dpi),
			FBESearchViewport::MinimumContextTopForDpi(dpi) / 4,
			obstructions.empty() ? NULL : &obstructions[0], static_cast<unsigned>(obstructions.size()) };
		scrollElement->scrollTop = FBESearchViewport::ScrollTopForMatch(input);
	}
	catch(const _com_error&) { }
}

bool CFBEView::DoSearchRegexp(bool fMore)
{
	// Search Core owns matching for both Find and Replace.  The native hit is
	// adapted to IMatch2 by DoSearchNative only at the replacement-template
	// boundary below; Replace must never restore paragraph-by-paragraph search.
	return DoSearchNative(fMore, AU::Search::SearchMode::Regex);
}

bool CFBEView::DoSearchStd(bool fMore)
{
	// Literal Design-mode Find has completed parity checks against the former
	// MSHTML algorithm.  Keep MSHTML at the adapter boundary only: matching,
	// caret-relative navigation and wrap now all go through Search Core.
	return DoSearchNative(fMore, AU::Search::SearchMode::Literal);
}

static AU::Search::ReplacementMatch BuildReplacementMatch(AU::ReMatch match)
{
	AU::Search::ReplacementMatch result;
	if (!match)
		return result;
	result.Value = match->Value;
	AU::ReSubMatches subMatches(match->SubMatches);
	if (!subMatches)
		return result;
	for (long index = 0; index < subMatches->Count; ++index)
		result.SubMatches.push_back(subMatches->Item[index]);
	return result;
}

static CString PrepareRegexReplacementText(const CString& replacementTemplate, AU::ReMatch match,
	std::vector<AU::Search::ReplacementFormattingRun>& formatting)
{
	CString replacement(AU::Search::ExpandRegexReplacement(
		replacementTemplate, BuildReplacementMatch(match), formatting));
	NormalizeReplacementNbsp(replacement);
	return replacement;
}

static void ApplyReplacementFormatting(MSHTML::IHTMLTxtRangePtr sel, const CString& repl, const std::vector<AU::Search::ReplacementFormattingRun>& rl, bool injectFailure = false, bool injectCommandFailure = false, bool checkCommandResult = false)
{
	for (std::size_t i = 0; i < rl.size(); ++i)
	{
		const AU::Search::ReplacementFormattingRun& rr = rl[i];
		MSHTML::IHTMLTxtRangePtr range = sel->duplicate();
		range->move(L"character", rr.Start - repl.GetLength());
		range->moveEnd(L"character", rr.Length);
		if (injectFailure) throw _com_error(E_FAIL);
		if (rr.Flags & AU::Search::ReplacementFormatStrong) {
			const VARIANT_BOOL applied = range->execCommand(injectCommandFailure ? L"FBE_Test_Unsupported_Command" : L"Bold", VARIANT_FALSE);
			if (checkCommandResult && applied == VARIANT_FALSE) throw _com_error(E_FAIL);
		}
		if (rr.Flags & AU::Search::ReplacementFormatEmphasis) {
			const VARIANT_BOOL applied = range->execCommand(injectCommandFailure ? L"FBE_Test_Unsupported_Command" : L"Italic", VARIANT_FALSE);
			if (checkCommandResult && applied == VARIANT_FALSE) throw _com_error(E_FAIL);
		}
	}
}

// Replacing a range that spans paragraph elements with IHTMLTxtRange::text
// lets MSHTML rewrite block markup. Find may report such a regexp hit, but
// replacement is deliberately refused until a structural replacement engine
// can prove preservation of the surrounding FB2 DOM.
static CString CrossParagraphReplacementError()
{
	return FbeLoadRuntimeStringByKey(L"fbe.replace.cross_paragraph",
		L"Replacing a regular-expression match across paragraphs is not supported because it could change document structure.");
}

void  CFBEView::DoReplace() {
  bool replaced = false;
  std::unique_ptr<FbeDom::MarkupUndoUnitScope> undo;
  _com_error failure(S_OK);
  try {
    MSHTML::IHTMLTxtRangePtr  sel(Document()->selection->createRange());
    if (!(bool)sel)
      return;
	if (CheckReplacementRange(sel, m_fo.fRegexp) == ReplacementPreflightResult::CrossParagraph)
	{
		U::MessageBox(m_hWnd, CrossParagraphReplacementError(),
			FbeLoadRuntimeStringByKey(L"fbe.replace.preview.caption", L"Replace All"), MB_OK | MB_ICONEXCLAMATION);
		return;
	}
    int			      adv=0;

    if (m_fo.hasMatch && m_fo.match) { // use regexp match copy
      std::vector<AU::Search::ReplacementFormattingRun> rl;
      CString rep(PrepareRegexReplacementText(m_fo.replacement, m_fo.match, rl));
      adv=rep.GetLength();
	  const bool zeroLengthNoOp = m_fo.match->Length == 0 && rep.IsEmpty() && rl.empty();
	  if (!zeroLengthNoOp)
	  {
		undo.reset(new FbeDom::MarkupUndoUnitScope(m_mk_srv, L"replace"));
		sel->text=(const wchar_t *)rep;
		replaced = true;
		ApplyReplacementFormatting(sel, rep, rl);
	  }
    } else { // plain text
	  undo.reset(new FbeDom::MarkupUndoUnitScope(m_mk_srv, L"replace"));
      sel->text=(const wchar_t *)m_fo.replacement;
      replaced = true;
      adv=m_fo.replacement.GetLength();
    }
	if (replaced)
	{
	  sel->moveStart(L"character",-adv);
	  sel->select();
	}
  }
  catch (_com_error& e) {
    failure = e;
  }
  catch (const std::bad_alloc&) { failure = _com_error(E_OUTOFMEMORY); }
  catch (const ATL::CAtlException& e) { failure = _com_error(e.m_hr); }
  if (undo) {
    try { undo->Close(); }
    catch (_com_error& e) { failure = e; }
  }
  undo.reset();
  if (replaced)
  {
    // Replacement changes MSHTML text synchronously, while RANGE_SINK may be
    // delivered later. Do not let the next Find reuse pre-replacement offsets.
    m_fo.ClearMatch();
    AdvanceSearchDocumentGeneration();
  }
  if (FAILED(failure.Error())) U::ReportError(failure);
}

bool CFBEView::ConfigureReplaceAllRuntimeTest(const CString& pattern, const CString& replacement, bool regexp)
{
	if (!RuntimeTests::IsScenario(L"replace-all-runtime")) return false;
	m_fo.ClearMatch();
	m_fo.pattern = pattern;
	m_fo.replacement = replacement;
	m_fo.fRegexp = regexp;
	m_fo.flags = 0;
	m_fo.scope = AU::Search::SearchScope::WholeDocument;
	wchar_t mode[64] = {};
	::GetEnvironmentVariableW(L"FBE_NEXT_TEST_REPLACE_MODE", mode, _countof(mode));
	if (wcscmp(mode, L"scope-selection") == 0) m_fo.scope = AU::Search::SearchScope::Selection;
	if (wcscmp(mode, L"scope-section") == 0) m_fo.scope = AU::Search::SearchScope::CurrentSection;
	return true;
}

static bool ReplaceAllRuntimeFault(const wchar_t* fault)
{
	if (!RuntimeTests::IsScenario(L"replace-all-runtime")) return false;
	wchar_t value[64] = {};
	const DWORD length = ::GetEnvironmentVariableW(L"FBE_NEXT_TEST_REPLACE_FAULT", value, _countof(value));
	return length != 0 && length < _countof(value) && wcscmp(value, fault) == 0;
}

int CFBEView::ReplaceAllSearchCore(CString* errorText)
{
	if (errorText != NULL)
		errorText->Empty();
	const std::uint64_t generation = SearchDocumentGeneration();
	const auto previewIsCurrent = [&]() {
		return m_design_search.HasCurrentReplacePreview(generation, FindResultsRevision(),
			m_fo.pattern, m_fo.replacement, m_fo.flags, m_fo.scope,
			m_fo.fRegexp, m_fo.unicodeProperties) && AreFindResultsCurrent();
	};
	if (!previewIsCurrent())
	{
		CString searchError;
		if (!DoFindAll(true, &searchError))
		{
			if (errorText != NULL)
				*errorText = searchError;
			return -1;
		}
		const std::size_t previewCount = m_design_search.Coordinator().GetResults().GetCount();
		if (previewCount == 0)
			return 0;
		m_design_search.SetReplacePreview(generation, FindResultsRevision(),
			m_fo.pattern, m_fo.replacement, m_fo.flags, m_fo.scope,
			m_fo.fRegexp, m_fo.unicodeProperties);
	}

	const std::size_t count = m_design_search.Coordinator().GetResults().GetCount();
	if (count == 0)
		return 0;
	CString preview;
	preview.Format(FbeLoadRuntimeStringByKey(
		L"fbe.replace.preview.message", L"Number of replacements: %Iu. Continue?"), count);
	if (U::MessageBox(m_hWnd, preview, FbeLoadRuntimeStringByKey(
		L"fbe.replace.preview.caption", L"Replace All"), MB_YESNO | MB_ICONQUESTION) != IDYES)
		return -2;
	// The confirmation is modal, so the preview normally stays unchanged. Still
	// validate its complete identity after the user answers: automation, browser
	// events, or another editor command must never apply stale coordinates.
	if (!previewIsCurrent())
	{
		if (errorText != NULL)
			*errorText = FbeLoadRuntimeStringByKey(L"fbe.replace.preview.changed", L"The document changed before Replace All could be applied.");
		return -1;
	}

	// Validate every source coordinate while the document is unchanged. The
	// reverse pass below then preserves all earlier offsets in this snapshot.
	std::vector<MSHTML::IHTMLTxtRangePtr> ranges(count);
	for (std::size_t index = 0; index < count; ++index)
	{
		if (!m_design_search.Coordinator().CreateResultRange(Document(), generation, index, ranges[index]) || !ranges[index])
		{
			if (errorText != NULL)
				*errorText = FbeLoadRuntimeStringByKey(L"fbe.replace.preview.changed", L"The document changed before Replace All could be applied.");
			return -1;
		}
		if (CheckReplacementRange(ranges[index], m_fo.fRegexp) == ReplacementPreflightResult::CrossParagraph)
		{
			if (errorText != NULL)
				*errorText = CrossParagraphReplacementError();
			return -1;
		}
	}

	int replaced = 0;
	bool mutationAttempted = false;
	const bool faultBefore = ReplaceAllRuntimeFault(L"before-first");
	const bool faultAfterOne = ReplaceAllRuntimeFault(L"after-one");
	const bool faultMiddle = ReplaceAllRuntimeFault(L"middle");
	const bool faultFormatting = ReplaceAllRuntimeFault(L"format");
	const bool faultFormattingCommand = ReplaceAllRuntimeFault(L"format-command") || ReplaceAllRuntimeFault(L"format-italic-command");
	const bool faultEnd = ReplaceAllRuntimeFault(L"end");
	struct ControlledMutationScope
	{
		DesignSearchController& search;
		~ControlledMutationScope() { search.SetControlledReplaceAllMutation(false); }
	} controlledMutation{ m_design_search };
	std::unique_ptr<FbeDom::MarkupUndoUnitScope> undo;
	HRESULT failure = S_OK;
	// MSHTML notifies RANGE_SINK for every individual assignment below.  Those
	// notifications are part of this one controlled operation; invalidate once
	// after EndUndoUnit instead of repeatedly clearing the completion status.
	m_design_search.SetControlledReplaceAllMutation(true);
	try
	{
		const AU::Search::SearchTextSnapshot& snapshot = m_design_search.Coordinator().GetSnapshot();
		if (faultBefore) throw _com_error(E_FAIL);
		for (std::size_t index = count; index-- > 0;)
		{
			const AU::Search::SearchResult* result = m_design_search.Coordinator().GetResults().GetAt(index);
			if (result == NULL)
				throw _com_error(E_FAIL);
			CString replacement;
			std::vector<AU::Search::ReplacementFormattingRun> formatting;
			if (m_fo.fRegexp)
			{
				const AU::Search::SearchHit& hit = result->Hit;
				AU::IMatch2 match(CString(snapshot.Text.data() + hit.Start, static_cast<int>(hit.Length)),
					static_cast<int>(hit.Start));
				for (std::size_t capture = 0; capture < hit.Captures.size(); ++capture)
				{
					const AU::Search::SearchCapture& value = hit.Captures[capture];
					match.AddSubMatch(value.Matched
						? CString(snapshot.Text.data() + value.Start, static_cast<int>(value.Length))
						: CString());
				}
				replacement = PrepareRegexReplacementText(m_fo.replacement, &match, formatting);
			}
			else
			{
				replacement = m_fo.replacement;
				NormalizeReplacementNbsp(replacement);
			}
			const bool zeroLengthNoOp = result->Hit.Length == 0 && replacement.IsEmpty() && formatting.empty();
			if (zeroLengthNoOp)
				continue;
			if (!undo)
			{
				undo.reset(new FbeDom::MarkupUndoUnitScope(m_mk_srv, L"replace all"));
			}
			// A COM setter can fail after changing the DOM. Treat even a failed
			// first assignment as potentially partial, not as a clean failure.
			mutationAttempted = true;
			ranges[index]->text = static_cast<LPCWSTR>(replacement);
			if (m_fo.fRegexp)
			{
				ApplyReplacementFormatting(ranges[index], replacement, formatting, faultFormatting, faultFormattingCommand, true);
			}
			++replaced;
			if ((replaced == 1 && faultAfterOne) ||
				(static_cast<std::size_t>(replaced) == (count + 1) / 2 && faultMiddle))
				throw _com_error(E_FAIL);
		}
	}
	catch (const _com_error& error)
	{
		failure = error.Error();
	}
	catch (const std::bad_alloc&)
	{
		failure = E_OUTOFMEMORY;
	}
	catch (const ATL::CAtlException& error)
	{
		failure = error.m_hr;
	}
	if (undo)
	{
		try {
			// Simulate an error immediately before closing; the scope destructor
			// still attempts its normal cleanup, as after a COM exception.
			if (faultEnd) throw _com_error(E_FAIL);
			undo->Close();
		}
		catch (const _com_error& error) { failure = error.Error(); }
	}
	if (FAILED(failure))
	{
		m_design_search.SetControlledReplaceAllMutation(false);
		if (mutationAttempted)
			AdvanceSearchDocumentGeneration();
		if (errorText != NULL)
		{
			*errorText = _com_error(failure).ErrorMessage();
			if (mutationAttempted)
			{
				*errorText += L"\n\n";
				*errorText += FbeLoadRuntimeStringByKey(L"fbe.replace.partial_failure",
					L"Replace All may have partially changed the document. Do not overwrite the original file. Reopen the last saved copy to restore it; unsaved changes will be lost.");
			}
		}
		return -1;
	}
	m_fo.ClearMatch();
	m_design_search.ClearReplacePreview();
	if (replaced != 0)
	{
		m_design_search.SetReplaceAllCompletion(replaced);
		// Do not finalize synchronously: MSHTML may still dispatch RANGE_SINK
		// notifications after this method returns. A one-shot posted message gives
		// them one protected UI turn without a polling timer.
		if (!::PostMessage(m_hWnd, AU::WM_FINALIZE_REPLACE_ALL_COMPLETION, 0, 0))
		{
			BOOL handled = FALSE;
			OnFinalizeReplaceAllCompletion(AU::WM_FINALIZE_REPLACE_ALL_COMPLETION, 0, 0, handled);
		}
	}
	m_design_search.SetControlledReplaceAllMutation(false);
	return replaced;
}

int CFBEView::GlobalReplace(MSHTML::IHTMLElementPtr elem, CString cntTag)
{
	UNREFERENCED_PARAMETER(cntTag); // retained for the public Tools API signature
	if (m_fo.pattern.IsEmpty() || !Document())
		return 0;
	std::unique_ptr<FbeDom::MarkupUndoUnitScope> undo;
	bool mutationApplied = false;
	int replaced = 0;
	_com_error failure(S_OK);
	try
	{
		const std::uint64_t generation = SearchDocumentGeneration();
		AU::Search::SearchQuery query;
		query.Text = static_cast<LPCWSTR>(m_fo.pattern);
		query.Mode = m_fo.fRegexp ? AU::Search::SearchMode::Regex : AU::Search::SearchMode::Literal;
		query.MatchCase = (m_fo.flags & FRF_CASE) != 0;
		query.WholeWord = (m_fo.flags & FRF_WHOLE) != 0;
		query.Direction = AU::Search::SearchDirection::Forward;
		query.UnicodeProperties = m_fo.unicodeProperties;
		query.Multiline = query.Mode == AU::Search::SearchMode::Regex;

		// GlobalReplace is also used by Tools commands with an element scope. Map
		// that DOM range once, then let Search Core filter snapshot hits; never
		// fall back to IHTMLTxtRange::findText for literal replacements.
		if (!m_design_search.Coordinator().Rebuild(Document(), generation, query))
			return 0;
		AU::Search::SearchRange scope;
		if (elem)
		{
			MSHTML::IHTMLTxtRangePtr elementRange(MSHTML::IHTMLBodyElementPtr(Document()->body)->createTextRange());
			if (!elementRange) return 0;
			elementRange->moveToElementText(elem);
			if (!m_design_search.Coordinator().TryGetSearchRange(generation, elementRange, &scope) || scope.Length == 0)
				return 0;
			if (!m_design_search.Coordinator().Rebuild(Document(), generation, query, NULL, &scope))
				return 0;
		}

		const std::size_t count = m_design_search.Coordinator().GetResults().GetCount();
		if (count == 0)
			return 0;
		std::vector<MSHTML::IHTMLTxtRangePtr> ranges(count);
		for (std::size_t index = 0; index < count; ++index)
			if (!m_design_search.Coordinator().CreateResultRange(Document(), generation, index, ranges[index]) || !ranges[index])
				return 0;

		const AU::Search::SearchTextSnapshot& snapshot = m_design_search.Coordinator().GetSnapshot();
		for (std::size_t index = count; index-- > 0;)
		{
			const AU::Search::SearchResult* result = m_design_search.Coordinator().GetResults().GetAt(index);
			if (result == NULL) continue;
			CString replacement;
			std::vector<AU::Search::ReplacementFormattingRun> formatting;
			if (m_fo.fRegexp)
			{
				const AU::Search::SearchHit& hit = result->Hit;
				AU::IMatch2 match(CString(snapshot.Text.data() + hit.Start, static_cast<int>(hit.Length)), static_cast<int>(hit.Start));
				for (std::size_t capture = 0; capture < hit.Captures.size(); ++capture)
				{
					const AU::Search::SearchCapture& value = hit.Captures[capture];
					match.AddSubMatch(value.Matched ? CString(snapshot.Text.data() + value.Start, static_cast<int>(value.Length)) : CString());
				}
				replacement = PrepareRegexReplacementText(m_fo.replacement, &match, formatting);
			}
			else
			{
				replacement = m_fo.replacement;
				NormalizeReplacementNbsp(replacement);
			}
			const bool zeroLengthNoOp = result->Hit.Length == 0 && replacement.IsEmpty() && formatting.empty();
			if (zeroLengthNoOp)
				continue;
			if (!undo)
			{
				undo.reset(new FbeDom::MarkupUndoUnitScope(m_mk_srv, L"replace"));
			}
			ranges[index]->text = static_cast<LPCWSTR>(replacement);
			mutationApplied = true;
			if (m_fo.fRegexp) ApplyReplacementFormatting(ranges[index], replacement, formatting);
			++replaced;
		}
	}
	catch (_com_error& err)
	{
		failure = err;
	}
	catch (const std::bad_alloc&) { failure = _com_error(E_OUTOFMEMORY); }
	catch (const ATL::CAtlException& err) { failure = _com_error(err.m_hr); }
	if (undo)
	{
		try { undo->Close(); }
		catch (_com_error& err) { failure = err; }
	}
	undo.reset();
	if (mutationApplied)
		AdvanceSearchDocumentGeneration();
	if (FAILED(failure.Error()))
	{
		U::ReportError(failure);
		return 0;
	}
	return replaced;
}

int CFBEView::ToolWordsGlobalReplace(	MSHTML::IHTMLElementPtr fbw_body,
										int* pIndex,
										int* globIndex,
										bool find,
										CString cntTag)
{
	if(m_fo.pattern.IsEmpty())
		return 0;

	int nRepl = 0;

	try
	{
		ScopedSearchRegExp re;
		NormalizeSearchPatternNbsp(m_fo.pattern);
		InitSearchRegExp(re.get(), m_fo.flags, m_fo.pattern);
		re->Global = m_fo.flags & FRF_WHOLE ? VARIANT_TRUE : VARIANT_FALSE;
		re->Multiline = VARIANT_TRUE;
		MSHTML::IHTMLElementCollectionPtr paras = MSHTML::IHTMLElement2Ptr(fbw_body)->getElementsByTagName(cntTag.AllocSysString());
		if(!paras->length)
			return 0;

		int iNextElem = pIndex != NULL ? *pIndex : 0;
		CSimpleArray<CFBEView::pElAdjacent> pAdjElems;

		while(iNextElem < paras->length)
		{
			pAdjElems.RemoveAll();

			MSHTML::IHTMLElementPtr currElem(paras->item(iNextElem));
			CString innerText(static_cast<const wchar_t*>(currElem->innerText));
			pAdjElems.Add(pElAdjacent(currElem));

			if(pIndex != NULL)
				*pIndex = iNextElem;

			MSHTML::IHTMLDOMNodePtr currNode(currElem);
			if(MSHTML::IHTMLElementPtr siblElem = currNode->nextSibling)
			{
				int jNextElem = iNextElem + 1;
				for(int i = jNextElem; i < paras->length; ++i)
				{
					MSHTML::IHTMLElementPtr nextElem = paras->item(i);
					if(siblElem == nextElem)
					{
						pAdjElems.Add(pElAdjacent(siblElem));
						innerText += L"\n";
						innerText += siblElem->innerText.GetBSTR();
						iNextElem++;
						siblElem = MSHTML::IHTMLDOMNodePtr(nextElem)->nextSibling;
					}
					else
					{
						break;
					}
				}
			}
			innerText += L"\n";

			if(innerText.IsEmpty())
			{
				iNextElem++;
				continue;
			}

			// Replace
			AU::ReMatches rm(ExecuteSearchRegExp(re.get(), innerText));
			if(rm->Count <= 0)
			{
				iNextElem++;
				continue;
			}

			for(long i = 0; i < rm->Count; ++i)
			{
				AU::ReMatch cur(rm->Item[i]);

				long matchIdx = cur->FirstIndex;
				long matchLen = cur->Length - 1;

				long pAdjLen = 0;
				bool begin = false, end = false;
				int first = 0, last = 0;

				for(int b = 0; b < pAdjElems.GetSize(); ++b)
				{
					int pElemLen = pAdjElems[b].innerText.length() + 1;

					if(!pElemLen)
						continue;

					pAdjLen += pElemLen;

					if(matchIdx < pAdjLen && !begin)
					{
						begin = true;
						first = b;
					}

					if(matchIdx + (matchLen - 1) < pAdjLen)
					{
						end = true;
						last = b;
						break;
					}
				}

				for(int skip = 0; skip < first; ++skip)
					matchIdx -= (pAdjElems[skip].innerText.length() + 1);

				CString newCont;
				for(int index = first; index <= last; ++index)
					newCont += static_cast<const wchar_t*>(pAdjElems[index].innerText);

				MSHTML::IHTMLTxtRangePtr found(Document()->selection->createRange());
				found->moveToElementText(pAdjElems[first].elem);

				// SeNS: fix for issue #148
				int foundOffset = matchIdx + TextOffset(found, cur);
				MoveRangeToRegexMatch(found, cur, foundOffset - cur->FirstIndex);
				found->select();

				if(find)
				{
					if(i == rm->Count - 1)
					{
						(*globIndex) = -1;
						(*pIndex) += (pAdjElems.GetSize());
					}
					else
						(*globIndex)++;

					if(*globIndex > i)
					{
						(*globIndex)--;
						continue;
					}

					return 0;
				}

				found->text = m_fo.replacement.AllocSysString();

				newCont.Delete(matchIdx, matchLen - (last - first));
				newCont.Insert(matchIdx, m_fo.replacement);

				pAdjElems[first].innerText = newCont.AllocSysString();
				//pAdjElems[first].elem->innerText = pAdjElems[first].innerText;

				for(int c = first + 1; c <= last; ++c)
				{
				//	MSHTML::IHTMLDOMNodePtr(pAdjElems[c].elem)->removeNode(VARIANT_TRUE);
					iNextElem--;
				}

				for(int c = first + 1; c < last; ++c)
					pAdjElems.RemoveAt(c);

				if(nRepl >= m_fo.replNum)
					goto stop;

				CString again;
				for(int index = 0; index < pAdjElems.GetSize(); ++index)
				{
					again += static_cast<const wchar_t*>(pAdjElems[index].innerText);
					again += L"\n";
				}
				rm = ExecuteSearchRegExp(re.get(), again);
				i--;

				nRepl++;
			}

			iNextElem++;
		}

stop:
		if(find)
		{
			Document()->selection->empty();
			return -1;
		}
	}
	catch (_com_error& err)
	{
		U::ReportError(err);
	}

	return nRepl;
}

class CViewReplaceDlg : public CReplaceDlgBase {
public:
  CViewReplaceDlg(CFBEView *view) : CReplaceDlgBase(view) { }
  virtual FbeSearchPresets::SearchUiContext SearchContext() const { return FbeSearchPresets::SearchUiContext::Design; }

  virtual void DoFind() {
    if (!m_view->DoSearch())
	{
		U::MessageBox(MB_OK|MB_ICONEXCLAMATION, IDR_MAINFRAME, IDS_SEARCH_END_MSG, static_cast<LPCWSTR>(m_view->m_fo.pattern));
	}
    else {
      SaveString();
      SaveHistory();
      m_selvalid=true;
      MakeClose();
    }
  }
  virtual void DoReplace() {
    if (m_selvalid) { // replace
      m_view->DoReplace();
      m_selvalid=false;
    }
	m_view->m_startMatch = m_view->m_endMatch = 0;
    DoFind();
  }
  virtual void DoReplaceAll() {
    CString error;
    int nRepl=m_view->ReplaceAllSearchCore(&error);
    if (nRepl>0) {
      SaveString();
      SaveHistory();
      U::MessageBox(MB_OK, IDS_REPL_ALL_CAPT, IDS_REPL_DONE_MSG, nRepl);
      MakeClose();
      m_selvalid=false;
	} else if (nRepl == -2) {
		return;
	} else if (!error.IsEmpty())
	{
		U::MessageBox(m_hWnd, error, FbeLoadRuntimeStringByKey(L"fbe.replace.preview.caption", L"Replace All"), MB_OK | MB_ICONEXCLAMATION);
	} else
	{
		U::MessageBox(MB_OK|MB_ICONEXCLAMATION, IDR_MAINFRAME, IDS_SEARCH_END_MSG, static_cast<LPCWSTR>(m_view->m_fo.pattern));
	}
  }
};

void CFBEView::SeedSearchPatternFromSelection()
{
	const CString selection = static_cast<LPCWSTR>(Selection());
	m_fo.pattern = !selection.IsEmpty() && selection.GetLength() <= 120 &&
		selection.Find(L'\r') < 0 && selection.Find(L'\n') < 0 ? selection : CString();
}

LRESULT CFBEView::OnFind(WORD, WORD, HWND, BOOL&)
{
	const bool openingFind = !m_find_dlg || !m_find_dlg->IsValid();
	if (openingFind) SeedSearchPatternFromSelection();
	if(!m_find_dlg)
		m_find_dlg = new CViewFindDlg(this);

	if(!m_find_dlg->IsValid())
		m_find_dlg->ShowDialog(*this); // show modeless
	else
		m_find_dlg->SetFocus();
	return 0;
}

LRESULT CFBEView::OnReplace(WORD, WORD, HWND, BOOL&)
{
	const bool openingReplace = !m_replace_dlg || !m_replace_dlg->IsValid();
	if (openingReplace) SeedSearchPatternFromSelection();
	// Find and Replace share search criteria, including a stable Selection
	// scope.  A reopened Replace dialog may reuse that scope after Find Next
	// moved MSHTML's visible selection to a hit.
	if (openingReplace)
	{
		m_fo.ClearMatch();
		m_design_search.ResetZeroLengthHit();
		m_design_search.ClearReplacePreview();
	}
	if(!m_replace_dlg)
		m_replace_dlg = new CViewReplaceDlg(this);

	if(!m_replace_dlg->IsValid())
		m_replace_dlg->ShowDialog(*this);
	else
		m_replace_dlg->SetFocus();
	return 0;
}

LRESULT  CFBEView::OnFindNext(WORD, WORD, HWND, BOOL&) {
  if (!DoSearch())
  {
	if (!m_last_search_error.IsEmpty() && LastSearchErrorIsRegexp())
		U::MessageBox(m_hWnd, m_last_search_error, FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_find.caption", L"Find"), MB_OK | MB_ICONEXCLAMATION);
	else if (m_last_search_error.IsEmpty())
		U::MessageBox(MB_OK|MB_ICONEXCLAMATION, IDR_MAINFRAME, IDS_SEARCH_FAIL_MSG, static_cast<LPCWSTR>(m_fo.pattern));
  }
  return 0;
}

// binary objects
_variant_t	CFBEView::GetBinary(const wchar_t *id) {
  try {
    CComDispatchDriver    dd(Script());
    _variant_t    ret;
    _variant_t    arg(id);
    if (SUCCEEDED(dd.Invoke1(L"GetBinary",&arg,&ret)))
      return ret;
  }
  catch (_com_error&) { }
  return _variant_t();
}

// change notifications
void	CFBEView::EditorChanged(int id) {
  switch (id) {
  case FWD_SINK:
    break;
  case BACK_SINK:
    break;
  case RANGE_SINK:
	m_startMatch = m_endMatch = 0;
	// A controlled Replace All owns the invalidation and publishes its status
	// only after the complete Undo unit. Ordinary edits still invalidate here.
	if (!m_design_search.ControlledReplaceAllMutation() && !m_design_search.ReplaceAllCompletionPending())
		AdvanceSearchDocumentGeneration();
    if (!m_ignore_changes)
      ::SendMessage(m_frame,WM_COMMAND,MAKELONG(0,IDN_ED_CHANGED),(LPARAM)m_hWnd);
    break;
  }
}

// DWebBrowserEvents2
void  CFBEView::OnDocumentComplete(IDispatch *pDisp,VARIANT *vtUrl) {
  CComPtr<IUnknown> eventBrowser;
  CComPtr<IUnknown> topLevelBrowser;
  if (pDisp) pDisp->QueryInterface(IID_IUnknown, reinterpret_cast<void**>(&eventBrowser));
  if (m_browser) m_browser->QueryInterface(IID_IUnknown, reinterpret_cast<void**>(&topLevelBrowser));
  if (!eventBrowser || !topLevelBrowser || eventBrowser != topLevelBrowser)
  {
    StartupTrace::Event(L"webbrowser", L"WB141", L"DocumentComplete ignored for frame");
    return;
  }
  CString url = (vtUrl && V_VT(vtUrl) == VT_BSTR) ? StartupTrace::RedactPath(V_BSTR(vtUrl)) : CString(L"-");
  CString readyState(L"(unknown)");
  try
  {
    MSHTML::IHTMLDocument2Ptr document = m_browser ? m_browser->Document : NULL;
    if (document) readyState = (const wchar_t*)_bstr_t(document->readyState);
  }
  catch (_com_error&) { }
  const ULONGLONG elapsed = m_navigation_started ? ::GetTickCount64() - m_navigation_started : 0;
  CString details; details.Format(L"url=%s; ready-state=%s; navigate-elapsed=%llu", (LPCWSTR)url, (LPCWSTR)readyState, elapsed);
  StartupTrace::Event(L"webbrowser", L"WB140", details);
  m_last_browser_event=L"DocumentComplete";
  m_complete=true;
}

bool CFBEView::Init()
{
	m_pendingHtmlPaste.reset();
	m_activeHtmlPaste.reset();
    m_htmlPasteCallbackFailure=S_OK;
	ResetPendingDrop();
  StartupTrace::Event(L"webbrowser", L"WB160", L"CFBEView::Init begin");
	// A full MSHTML rebuild invalidates every DOM-scoped navigation origin.
	ClearLinkNavigationHistory();
  if (!m_browser)
  {
    StartupTrace::Error(L"webbrowser", L"WB200", L"IWebBrowser2 unavailable");
    return false;
  }

  CComPtr<IDispatch> documentDispatch;
  HRESULT hr = m_browser->get_Document(&documentDispatch);
  StartupTrace::HResult(L"webbrowser", L"WB200", hr, L"IWebBrowser2::get_Document");
  if (FAILED(hr) || !documentDispatch) return false;

  CComPtr<MSHTML::IHTMLDocument2> document;
  hr = documentDispatch->QueryInterface(&document);
  StartupTrace::HResult(L"webbrowser", L"WB201", hr, L"QueryInterface(IHTMLDocument2)");
  if (FAILED(hr) || !document) return false;
  m_hdoc = document.p;
	// A full hosted-document reload replaces every source coordinate even when
	// MSHTML happens to retain its internal version number. A newly created view
	// has no DOM-scoped search state and its results pane is not ready yet.
	if (m_initialized)
	{
		AdvanceSearchDocumentGeneration(false);
		ResetSearchScope();
	}

  bool autoUrlDetectDisabled = false;
  try
  {
    autoUrlDetectDisabled = document->execCommand(L"AutoUrlDetect", VARIANT_FALSE, _variant_t(VARIANT_FALSE)) == VARIANT_TRUE;
  }
  catch (const _com_error& error)
  {
    StartupTrace::HResult(L"webbrowser", L"WB205", error.Error(), L"Disable AutoUrlDetect via execCommand");
  }
  if (!autoUrlDetectDisabled)
  {
    IOleCommandTargetPtr commandTarget(m_browser);
    VARIANT disabled;
    ::VariantInit(&disabled);
    disabled.vt = VT_BOOL;
    disabled.boolVal = VARIANT_FALSE;
    autoUrlDetectDisabled = commandTarget && SUCCEEDED(commandTarget->Exec(
      &CGID_MSHTML, IDM_AUTOURLDETECT_MODE, OLECMDEXECOPT_DONTPROMPTUSER, &disabled, NULL));
  }
  if (!autoUrlDetectDisabled)
  {
    StartupTrace::Warning(L"webbrowser", L"WB205", L"AutoUrlDetect was not disabled");
  }

  CComPtr<MSHTML::IMarkupServices2> markupServices;
  hr = document->QueryInterface(&markupServices);
  StartupTrace::HResult(L"webbrowser", L"WB210", hr, L"QueryInterface(IMarkupServices2)");
  if (FAILED(hr) || !markupServices) return false;
  m_mk_srv = markupServices.p;

  CComPtr<MSHTML::IMarkupContainer2> markupContainer;
  hr = document->QueryInterface(&markupContainer);
  StartupTrace::HResult(L"webbrowser", L"WB220", hr, L"QueryInterface(IMarkupContainer2)");
  if (FAILED(hr) || !markupContainer) return false;
  m_mkc = markupContainer.p;

  CComPtr<MSHTML::IHTMLElement> body;
  hr = document->get_body(&body);
  StartupTrace::HResult(L"webbrowser", L"WB225", hr, L"IHTMLDocument2::get_body");
  if (FAILED(hr) || !body) return false;

	// A document rebuild can invoke Init again while the old element is still
	// alive. Always detach that connection point before replacing its target.
	if (m_scroll_event_element)
	{
		ScrollEvents::DispEventUnadvise(m_scroll_event_element, &DIID_FBEHTMLElementEvents2);
		m_scroll_event_element = NULL;
	}

  DocumentEvents::DispEventUnadvise(document, &DIID_HTMLDocumentEvents2);
  hr = DocumentEvents::DispEventAdvise(document, &DIID_HTMLDocumentEvents2);
  StartupTrace::HResult(L"webbrowser", L"WB230", hr, L"DocumentEvents::DispEventAdvise");
  if (FAILED(hr)) return false;

	CComPtr<MSHTML::IHTMLDocument3> document3;
	document->QueryInterface(&document3);
	if (document3)
		m_scroll_event_element = document3->documentElement;
	else
		m_scroll_event_element = NULL;
	if (!m_scroll_event_element)
		m_scroll_event_element = body.p;
	hr = ScrollEvents::DispEventAdvise(m_scroll_event_element, &DIID_FBEHTMLElementEvents2);
	StartupTrace::HResult(L"webbrowser", L"WB235", hr, L"ScrollEvents::DispEventAdvise");
	if (FAILED(hr))
		m_scroll_event_element = NULL; // Highlight All degrades safely.

  TextEvents::DispEventUnadvise(body, &DIID_HTMLTextContainerEvents2);
  hr = TextEvents::DispEventAdvise(body, &DIID_HTMLTextContainerEvents2);
  StartupTrace::HResult(L"webbrowser", L"WB240", hr, L"TextEvents::DispEventAdvise");
  if (FAILED(hr)) return false;

  hr = m_mkc->RegisterForDirtyRange((RangeSink*)this, &m_dirtyRangeCookie);
  StartupTrace::HResult(L"webbrowser", L"WB250", hr, L"RegisterForDirtyRange");
  if (FAILED(hr)) return false;

  IDispatchPtr helper = CreateHelper();
  if (!helper)
  {
    StartupTrace::Error(L"webbrowser", L"WB260", L"CreateHelper returned null");
    return false;
  }
  StartupTrace::Event(L"webbrowser", L"WB260", L"CreateHelper completed");
	if (IsSecondSetExternalFaultEnabled())
	{
		hr = E_FAIL;
		StartupTrace::HResult(L"fault", L"FI011", hr, L"second SetExternalDispatch injected failure");
	}
	else
		hr = SetExternalDispatch(helper);
  StartupTrace::HResult(L"webbrowser", L"WB270", hr, L"SetExternalDispatch #2");
  if (FAILED(hr)) return false;

  MSHTML::IHTMLElement2Ptr body2(body.p);
  MSHTML::IHTMLDOMNodePtr bodyNode(body.p);
  if (!body2 || !bodyNode)
  {
    StartupTrace::Error(L"webbrowser", L"WB275", L"body does not expose required interfaces");
    return false;
  }
  StartupTrace::Event(L"webbrowser", L"WB280", L"FixupParagraphs");
	FbeVisualDom::FixupParagraphs(body2);
  if (m_normalize)
  {
    StartupTrace::Event(L"webbrowser", L"WB290", L"Normalize");
    Normalize(bodyNode);
  }

  if (!m_normalize) {
    MSHTML::IHTMLElementCollectionPtr all(document->all);
    MSHTML::IHTMLInputElementPtr ii(all->item(L"diID"));
    if ((bool)ii && ii->value.length()==0) {
      UUID uuid;
      unsigned char *str;
      if (UuidCreate(&uuid)==RPC_S_OK && UuidToStringA(&uuid,&str)==RPC_S_OK) {
        CString us(str);
        RpcStringFreeA(&str);
        us.MakeUpper();
        ii->value=(const wchar_t *)us;
      }
    }
    ii=all->item(L"diVersion");
    if ((bool)ii && ii->value.length()==0) ii->value=L"1.0";
    ii=all->item(L"diDate");
    MSHTML::IHTMLInputElementPtr jj(all->item(L"diDateVal"));
    if ((bool)ii && (bool)jj && ii->value.length()==0 && jj->value.length()==0) {
      time_t tt;
      time(&tt);
      char buffer[128];
      strftime(buffer,sizeof(buffer),"%Y-%m-%d",localtime(&tt));
      ii->value=buffer;
      jj->value=buffer;
    }
    ii=all->item(L"diProgs");
    if ((bool)ii && ii->value.length()==0) ii->value=L"FB Tools";
  }

  hr = m_browser->put_RegisterAsDropTarget(VARIANT_FALSE);
  StartupTrace::HResult(L"webbrowser", L"WB295", hr, L"put_RegisterAsDropTarget");
  if (FAILED(hr)) return false;
  m_initialized=true;
  StartupTrace::Event(L"webbrowser", L"WB299", L"CFBEView::Init completed");
  return true;
}
void CFBEView::OnNavigateError(IDispatch* pDisp, VARIANT* vtUrl, VARIANT* vtFrame, VARIANT* vtStatusCode, VARIANT_BOOL* fCancel)
{
  CComPtr<IUnknown> eventBrowser;
  CComPtr<IUnknown> topLevelBrowser;
  if (pDisp) pDisp->QueryInterface(IID_IUnknown, reinterpret_cast<void**>(&eventBrowser));
  if (m_browser) m_browser->QueryInterface(IID_IUnknown, reinterpret_cast<void**>(&topLevelBrowser));
  const bool topLevel = eventBrowser && topLevelBrowser && eventBrowser == topLevelBrowser;
  CString url = (vtUrl && V_VT(vtUrl) == VT_BSTR) ? StartupTrace::RedactPath(V_BSTR(vtUrl)) : CString(L"-");
  CString frame = (vtFrame && V_VT(vtFrame) == VT_BSTR) ? StartupTrace::SanitizeLogText(V_BSTR(vtFrame), 64) : CString(L"-");
  long status = 0;
  if (vtStatusCode && (V_VT(vtStatusCode) == VT_I4 || V_VT(vtStatusCode) == VT_INT)) status = V_I4(vtStatusCode);
  const int cancelled = fCancel && *fCancel == VARIANT_TRUE ? 1 : 0;
  const ULONGLONG elapsed = m_navigation_started ? ::GetTickCount64() - m_navigation_started : 0;
  CString details;
  details.Format(L"url=%s; frame=%s; status=%ld; top-level=%d; cancel=%d; navigate-elapsed=%llu", (LPCWSTR)url, (LPCWSTR)frame, status, topLevel ? 1 : 0, cancelled, elapsed);
  if (!topLevel)
  {
    StartupTrace::Event(L"webbrowser", L"WB136", details);
    return;
  }
  m_navigation_failed = true;
  m_navigation_status = status;
  m_last_browser_event = L"NavigateError";
  StartupTrace::Warning(L"webbrowser", L"WB135", details);
}
void  CFBEView::OnBeforeNavigate(IDispatch */* unused: pDisp */,VARIANT *vtUrl,VARIANT */* unused: vtFlags */,
				 VARIANT */* unused: vtTargetFrame */,VARIANT */* unused: vtPostData */,
				 VARIANT */* unused: vtHeaders */,VARIANT_BOOL *fCancel)
{
  m_last_browser_event=L"BeforeNavigate";
  if (!m_initialized)
    return;

  if (vtUrl && V_VT(vtUrl)==VT_BSTR) {
    m_nav_url=V_BSTR(vtUrl);

    if (m_nav_url.Left(13)==_T("fbw-internal:"))
      return;

	// changed by SeNS: possible fix for issue #87
	// tested on Windows Vista Ultimate
    ::PostMessage(m_frame,WM_COMMAND,MAKELONG(0,IDN_NAVIGATE),(LPARAM)m_hWnd);
  }

  // disable navigating away
  *fCancel=VARIANT_TRUE;
}

// HTMLDocumentEvents
void  CFBEView::OnSelChange(IDispatch */* unused: evt */) {
	ReportCaretFocusState(L"design-selection", m_hWnd, false);
  if (!m_ignore_changes)
    ::SendMessage(m_frame,WM_COMMAND,MAKELONG(0,IDN_SEL_CHANGE),(LPARAM)m_hWnd);
  if (m_cur_sel)
    m_cur_sel.Release();
}

VARIANT_BOOL  CFBEView::OnContextMenu(IDispatch *evt)
{
	MSHTML::IHTMLEventObjPtr oe(evt);
	oe->cancelBubble = VARIANT_TRUE;
	oe->returnValue = VARIANT_FALSE;
	if(!m_normalize)
	{
		MSHTML::IHTMLElementPtr elem(oe->srcElement);
		if(!(bool)elem)
			return VARIANT_TRUE;
		if(U::scmp(elem->tagName,L"INPUT") && U::scmp(elem->tagName, L"TEXTAREA"))
			return VARIANT_TRUE;
	}

	// display custom context menu here
	CMenu menu;
	CString itemName;

	menu.CreatePopupMenu();
	itemName = FbeLoadCString(IDS_HOTKEY_EDIT_UNDO);
	menu.AppendMenu(MF_STRING, ID_EDIT_UNDO, itemName);
	menu.AppendMenu(MF_SEPARATOR);

	itemName = FbeLoadCString(IDS_CTXMENU_CUT);
	menu.AppendMenu(MF_STRING, ID_EDIT_CUT, itemName);

	itemName = FbeLoadCString(IDS_CTXMENU_COPY);
	menu.AppendMenu(MF_STRING, ID_EDIT_COPY, itemName);

	itemName = FbeLoadCString(IDS_CTXMENU_PASTE);
	menu.AppendMenu(MF_STRING, ID_EDIT_PASTE, itemName);
	itemName = FbeLoadCString(IDS_CTXMENU_PASTE_TEXT);
	menu.AppendMenu(MF_STRING, ID_EDIT_PASTE_TEXT, itemName);

	// The table commands must be available where the user edits a cell, not
	// only in the main menu.  The source element may be an inline child, so
	// walk up to its TD/TH ancestor.
	MSHTML::IHTMLElementPtr contextCell(FbeTable::FindTableCell(MSHTML::IHTMLElementPtr(oe->srcElement)));
	if (m_normalize && contextCell)
	{
		// Right-click does not reliably move the MSHTML selection.  Retain the
		// clicked cell so the command modifies the cell the menu was opened on.
		if (m_table_selection_cells.empty())
			m_cur_sel = contextCell;
		menu.AppendMenu(MF_SEPARATOR);
		menu.AppendMenu(MF_STRING, ID_TABLE_INSERT_ROW_ABOVE, GetLocalizedMainMenuText(ID_TABLE_INSERT_ROW_ABOVE, L"Insert row above"));
		menu.AppendMenu(MF_STRING, ID_TABLE_INSERT_ROW_BELOW, GetLocalizedMainMenuText(ID_TABLE_INSERT_ROW_BELOW, L"Insert row below"));
		menu.AppendMenu(MF_STRING, ID_TABLE_DELETE_ROW, GetLocalizedMainMenuText(ID_TABLE_DELETE_ROW, L"Delete row"));
		menu.AppendMenu(MF_SEPARATOR);
		menu.AppendMenu(MF_STRING, ID_TABLE_INSERT_COLUMN_LEFT, GetLocalizedMainMenuText(ID_TABLE_INSERT_COLUMN_LEFT, L"Insert column left"));
		menu.AppendMenu(MF_STRING, ID_TABLE_INSERT_COLUMN_RIGHT, GetLocalizedMainMenuText(ID_TABLE_INSERT_COLUMN_RIGHT, L"Insert column right"));
		menu.AppendMenu(MF_STRING, ID_TABLE_DELETE_COLUMN, GetLocalizedMainMenuText(ID_TABLE_DELETE_COLUMN, L"Delete column"));
		menu.AppendMenu(MF_SEPARATOR);
		menu.AppendMenu(MF_STRING, ID_TABLE_MAKE_HEADER_CELLS, GetLocalizedMainMenuText(ID_TABLE_MAKE_HEADER_CELLS, L"Make header cells"));
		menu.AppendMenu(MF_STRING, ID_TABLE_MAKE_NORMAL_CELLS, GetLocalizedMainMenuText(ID_TABLE_MAKE_NORMAL_CELLS, L"Make normal cells"));
	}

	if(m_normalize)
	{
		menu.AppendMenu(MF_SEPARATOR);
		MSHTML::IHTMLElementPtr cur(SelectionContainer());
		MSHTML::IHTMLElementPtr initial(cur);
		int cmd = ID_SEL_BASE;
		itemName = FbeLoadCString(IDS_CTXMENU_SELECT);

		while((bool)cur && U::scmp(cur->tagName,L"BODY") && U::scmp(cur->id, L"fbw_body"))
		{
			menu.AppendMenu(MF_STRING, cmd, itemName + L" " + GetPath(cur));
			cur = cur->parentElement;
			++cmd;
		}
		if(U::scmp(initial->className, L"image") == 0)
		{
			MSHTML::IHTMLImgElementPtr image = MSHTML::IHTMLDOMNodePtr(initial)->firstChild;
			CString src = image->src.GetBSTR();
			src.Delete(src.Find(L"fbw-internal:"), 13);
			if(src != L"#undefined")
			{
				menu.AppendMenu(MF_SEPARATOR);
				itemName = FbeLoadCString(IDS_CTXMENU_IMG_SAVEAS);
				menu.AppendMenu(MF_STRING, ID_SAVEIMG_AS, itemName);
			}
		}
	}

	AU::TRACKPARAMS tp;
	tp.hMenu = menu;
	tp.uFlags = TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON;
	tp.x = oe->screenX;
	tp.y = oe->screenY;
	::SendMessage(m_frame, AU::WM_TRACKPOPUPMENU, 0, (LPARAM)&tp);

	return VARIANT_TRUE;
}

void CFBEView::OnScroll(IDispatch */* unused: evt */)
{
	// MSHTML emits this only after its hosted document has updated scrollLeft /
	// scrollTop, so popup coordinates are sampled from the final position.
	UpdateSearchHighlightsForScroll();
	::SendMessage(m_frame, AU::WM_BODY_SCROLL, 0, 0);
}

bool CFBEView::DoSearchNative(bool fMore, AU::Search::SearchMode mode, bool fromScopeStart)
{
	try
	{
		if (!Document())
			return false;
		MSHTML::IHTMLTxtRangePtr selection(Document()->selection->createRange());
		if (!fMore && m_is_start)
			selection = m_is_start->duplicate();
		if (!selection)
			return false;

		AU::Search::SearchQuery query;
		query.Text = static_cast<LPCWSTR>(m_fo.pattern);
		query.Mode = mode;
		query.MatchCase = (m_fo.flags & FRF_CASE) != 0;
		query.WholeWord = (m_fo.flags & FRF_WHOLE) != 0;
		query.Direction = (m_fo.flags & FRF_REVERSE)
			? AU::Search::SearchDirection::Backward
			: AU::Search::SearchDirection::Forward;
		query.UnicodeProperties = m_fo.unicodeProperties;
		query.Scope = m_fo.scope;
		// Legacy regexp searched paragraphs independently. The body snapshot
		// joins them with newlines, so PCRE2 multiline preserves ^/$ behaviour.
		query.Multiline = mode == AU::Search::SearchMode::Regex;

		const std::uint64_t generation = SearchDocumentGeneration();
		std::wstring nativeError;
		bool expressionError = false;
		m_last_search_error.Empty();
		m_last_search_error_is_regexp = false;
		if (!CanReuseDocumentSearch(query, generation) && !RebuildDocumentSearch(query, selection, &nativeError, &expressionError))
		{
			if (!nativeError.empty())
				m_last_search_error = nativeError.c_str();
			m_last_search_error_is_regexp = expressionError;
			return false;
		}
		m_design_search.PrepareZeroLengthSearch(query);
		AU::Search::SearchRange selectedRange;
		const bool skipZeroLengthAtOffset = m_design_search.Coordinator().TryGetSearchRange(generation, selection, &selectedRange) &&
			m_design_search.ShouldSkipZeroLength(query, selectedRange);
		bool wrapped = false;
		const AU::Search::SearchHit* hit = fromScopeStart
			? m_design_search.Coordinator().SelectFromOffset(Document(), generation,
				m_design_search.Scope(query.Scope) ? m_design_search.Scope(query.Scope)->Start : 0,
				AU::Search::SearchDirection::Forward, &wrapped)
			: m_design_search.Coordinator().SelectFromRange(Document(), generation, selection, query.Direction, &wrapped, skipZeroLengthAtOffset);
		if (hit == NULL)
			return false;
		m_design_search.RecordHit(query, *hit);
		if (mode != AU::Search::SearchMode::Regex)
			m_fo.ClearMatch();
		if (mode == AU::Search::SearchMode::Regex)
		{
			// Replace's formatting/template implementation still consumes IMatch2.
			// Adapt the native SearchHit here, at the editor boundary, retaining
			// positional empty captures so $1/$2 semantics remain intact.
			const AU::Search::SearchTextSnapshot& snapshot = m_design_search.Coordinator().GetSnapshot();
			m_fo.ClearMatch();
			m_fo.match = new AU::IMatch2(
				CString(snapshot.Text.data() + hit->Start, static_cast<int>(hit->Length)),
				static_cast<int>(hit->Start));
			for (std::size_t capture = 0; capture < hit->Captures.size(); ++capture)
			{
				const AU::Search::SearchCapture& value = hit->Captures[capture];
				m_fo.match->AddSubMatch(value.Matched
					? CString(snapshot.Text.data() + value.Start, static_cast<int>(value.Length))
					: CString());
			}
			m_fo.hasMatch = true;
		}
		MSHTML::IHTMLTxtRangePtr found(Document()->selection->createRange());
		PositionFoundRange(found);
		RefreshSearchHighlights();
		NotifyWrappedSearch(wrapped);
		return true;
	}
	catch (const _com_error&)
	{
		return false;
	}
}

bool CFBEView::CanReuseDocumentSearch(const AU::Search::SearchQuery& query, std::uint64_t generation) const
{
	// Navigation direction only changes which cached hit is selected.  Rebuild
	// only when the document, matching criteria, or captured scope changed.
	if (!m_design_search.Coordinator().GetSession().IsValidFor(generation) ||
		!m_design_search.Coordinator().GetResults().IsValidFor(generation) ||
		!AU::Search::HasSameSearchCriteria(m_design_search.Coordinator().GetSession().GetQuery(), query))
		return false;
	return query.Scope == AU::Search::SearchScope::WholeDocument ||
		m_design_search.HasValidScope(query.Scope);
}

bool CFBEView::DoFindAll(bool showResults, CString* errorText)
{
	try
	{
		m_find_results_completion_status.Empty();
		if (errorText != NULL)
			errorText->Empty();
		if (!Document())
			return false;
		AU::Search::SearchQuery query;
		query.Text = static_cast<LPCWSTR>(m_fo.pattern);
		query.Mode = m_fo.fRegexp ? AU::Search::SearchMode::Regex : AU::Search::SearchMode::Literal;
		query.MatchCase = (m_fo.flags & FRF_CASE) != 0;
		query.WholeWord = (m_fo.flags & FRF_WHOLE) != 0;
		query.Direction = (m_fo.flags & FRF_REVERSE)
			? AU::Search::SearchDirection::Backward
			: AU::Search::SearchDirection::Forward;
		query.UnicodeProperties = m_fo.unicodeProperties;
		query.Scope = m_fo.scope;
		query.Multiline = query.Mode == AU::Search::SearchMode::Regex;
		MSHTML::IHTMLTxtRangePtr selection(Document()->selection->createRange());
		std::wstring nativeError;
		if (!selection || !RebuildDocumentSearch(query, selection, &nativeError))
		{
			if (errorText != NULL)
				*errorText = nativeError.c_str();
			return false;
		}
		if (showResults)
			ShowFindResults();
		else
			::SendMessage(m_frame, AU::WM_REFRESH_FIND_RESULTS_PANE, reinterpret_cast<WPARAM>(this), 0);
		// Typing into Find invokes this path after a short debounce.  Computing a
		// rectangle for every hit is synchronous MSHTML work and can freeze the
		// editor for a common one-character query.  Highlighting remains available
		// through the explicit Find All action.
		if (showResults)
			RefreshSearchHighlights();
		else
			ClearSearchHighlights();
		return true;
	}
	catch (const _com_error&)
	{
		return false;
	}
}

bool CFBEView::HasTextSelection()
{
	try
	{
		if (!Document())
			return false;
		MSHTML::IHTMLTxtRangePtr range(Document()->selection->createRange());
		return range && range->compareEndPoints(L"StartToEnd", range) != 0;
	}
	catch (const _com_error&)
	{
		return false;
	}
}

void CFBEView::ResetSearchScope()
{
	m_design_search.ResetScope();
	try
	{
		m_selection_search_scope.Release();
	}
	catch (const _com_error& error)
	{
		StartupTrace::HResult(L"webbrowser", L"WB207", error.Error(), L"Release stale selection search scope");
		m_selection_search_scope.Detach();
	}
}

bool CFBEView::RebuildDocumentSearch(const AU::Search::SearchQuery& query, MSHTML::IHTMLTxtRangePtr selection, std::wstring* errorText, bool* expressionError)
{
	if (expressionError != NULL)
		*expressionError = false;
	const std::uint64_t generation = SearchDocumentGeneration();
	m_design_search.Coordinator().EnsureSnapshot(Document(), generation);
	if (query.Scope == AU::Search::SearchScope::WholeDocument)
	{
		if (!m_design_search.Coordinator().Rebuild(Document(), generation, query, errorText))
		{
			if (expressionError != NULL)
				*expressionError = query.Mode == AU::Search::SearchMode::Regex;
			return false;
		}
		return true;
	}

	AU::Search::SearchRange range;
	if (const AU::Search::SearchRange* savedScope = m_design_search.Scope(query.Scope))
	{
		range = *savedScope;
	}
	else
	{
		MSHTML::IHTMLTxtRangePtr scopeSelection(selection);
		if (query.Scope == AU::Search::SearchScope::Selection)
		{
			if (m_selection_search_scope)
				scopeSelection = m_selection_search_scope->duplicate();
			if (!scopeSelection || scopeSelection->compareEndPoints(L"StartToEnd", scopeSelection) == 0)
			{
				if (errorText != NULL) *errorText = static_cast<LPCWSTR>(FbeLoadRuntimeStringByKey(L"fbe.search.error.selection_scope_unavailable", L"The selected search scope is no longer available."));
				m_design_search.Coordinator().Invalidate();
				return false;
			}
		}
		else
		{
			MSHTML::IHTMLElementPtr section(SelectionStructSection());
			if (!section)
			{
				if (errorText != NULL) *errorText = static_cast<LPCWSTR>(FbeLoadRuntimeStringByKey(L"fbe.search.error.current_section_unavailable", L"The current section is not available for search."));
				m_design_search.Coordinator().Invalidate();
				return false;
			}
			scopeSelection = MSHTML::IHTMLBodyElementPtr(Document()->body)->createTextRange();
			scopeSelection->moveToElementText(section);
		}
		if (!m_design_search.Coordinator().TryGetSearchRange(generation, scopeSelection, &range) || range.Length == 0)
		{
			if (errorText != NULL) *errorText = static_cast<LPCWSTR>(FbeLoadRuntimeStringByKey(L"fbe.search.error.scope_mapping_failed", L"The selected search scope could not be mapped to the document."));
			m_design_search.Coordinator().Invalidate();
			return false;
		}
		m_design_search.SetScope(range, query.Scope);
		if (query.Scope == AU::Search::SearchScope::Selection)
			m_selection_search_scope = scopeSelection->duplicate();
	}
	if (!m_design_search.Coordinator().Rebuild(Document(), generation, query, errorText, &range))
	{
		if (expressionError != NULL)
			*expressionError = query.Mode == AU::Search::SearchMode::Regex;
		return false;
	}
	return true;
}

LRESULT CFBEView::OnSelectElement(WORD, WORD wID, HWND, BOOL&) {
  int	steps=wID-ID_SEL_BASE;
  try {
    MSHTML::IHTMLElementPtr	  cur(SelectionContainer());

    while ((bool)cur && steps-->0)
      cur=cur->parentElement;

    MSHTML::IHTMLTxtRangePtr	  r(MSHTML::IHTMLBodyElementPtr(Document()->body)->createTextRange());

    r->moveToElementText(cur);

    ++m_ignore_changes;
    r->select();
    --m_ignore_changes;

    m_cur_sel=cur;
    ::SendMessage(m_frame,WM_COMMAND,MAKELONG(0,IDN_SEL_CHANGE),(LPARAM)m_hWnd);
  }
  catch (_com_error& e) {
    U::ReportError(e);
  }

  return 0;
}

static void HideNotePreview(CFBEView& view)
{
	try { CComDispatchDriver(view.Script()).Invoke0(L"HideNotePreview"); }
	catch(const _com_error&) { }
}

void CFBEView::ClearLinkNavigationHistory()
{
	m_link_navigation_state.Reset();
}

bool CFBEView::NavigateInternalLink(MSHTML::IHTMLElementPtr link, const CString& targetId)
{
	MSHTML::IHTMLElementPtr target(FBELinkNavigation::FindTargetElement(Document(), targetId));
	if(!target) return false;
	m_link_navigation_state.targetId = targetId;
	m_link_navigation_state.originUniqueNumber = FBELinkNavigation::GetLinkUniqueNumber(link);
	if(!m_link_navigation_state.HasOrigin()) return false;
	HideNotePreview(*this);
	GoTo(target);
	return true;
}

bool CFBEView::ReturnToLinkNavigationOrigin()
{
	if(!m_link_navigation_state.HasOrigin() || !Document()) return false;
	MSHTML::IHTMLElementPtr origin(FBELinkNavigation::FindOriginLink(Document(),
		m_link_navigation_state.targetId, m_link_navigation_state.originUniqueNumber));
	ClearLinkNavigationHistory();
	if(!origin || !origin->parentElement) return false;
	GoTo(origin);
	return true;
}

VARIANT_BOOL  CFBEView::OnClick(IDispatch *evt)
{
	MSHTML::IHTMLEventObjPtr oe(evt);
	if(!oe) return VARIANT_FALSE;
	MSHTML::IHTMLElementPtr elem(oe->srcElement);

  	m_startMatch = m_endMatch = 0;

	if(!(bool)elem)
		return VARIANT_FALSE;

	MSHTML::IHTMLElementPtr parent_element = elem->parentElement;

	if(!(bool)parent_element)
		return VARIANT_FALSE;

	bstr_t pc = parent_element->className;

	if(!U::scmp(pc, L"image"))
	{
		// make image selected
		IHTMLControlRangePtr r(((MSHTML::IHTMLElement2Ptr)(Document()->body))->createControlRange());
		HRESULT hr = r->add((IHTMLControlElementPtr)elem->parentElement);
		hr = r->select();
		//::SendMessage(m_frame, WM_COMMAND, MAKELONG(IDC_HREF, IDN_WANTFOCUS), (LPARAM)m_hWnd);

		return VARIANT_TRUE;
	}

	MSHTML::IHTMLElementPtr link = FBELinkNavigation::FindNearestLinkElement(
		elem, FBELinkNavigation::GetEditableBody(Document()));
	if(!link) return VARIANT_FALSE;
	CString href(AU::GetAttrCS(link, L"href"));
	CString documentUrl;
	try {
		MSHTML::IHTMLDocument4Ptr document4(Document());
		if(document4) documentUrl = static_cast<LPCWSTR>(document4->URLUnencoded);
	}
	catch(const _com_error&) { }
	const FBELinkNavigation::LinkActivation activation = FBELinkNavigation::DecideLinkActivation(
		static_cast<LPCWSTR>(href), static_cast<LPCWSTR>(documentUrl),
		oe->ctrlKey == VARIANT_TRUE, oe->altKey == VARIANT_TRUE, oe->shiftKey == VARIANT_TRUE);
	if(activation == FBELinkNavigation::LinkActivation::Ignore) return VARIANT_FALSE;
	if(activation == FBELinkNavigation::LinkActivation::Internal)
	{
		CString targetId(FBELinkNavigation::GetInternalLinkTargetId(Document(), link));
		// Accepted modifier clicks consume broken internal links too: MSHTML must not try a
		// browser fragment navigation after the target was known to be absent.
		NavigateInternalLink(link, targetId);
		oe->cancelBubble = VARIANT_TRUE;
		oe->returnValue = VARIANT_FALSE;
		return VARIANT_TRUE;
	}
	if(activation == FBELinkNavigation::LinkActivation::ExternalHttp)
	{
		HideNotePreview(*this);
		::ShellExecuteW(m_hWnd, L"open", static_cast<LPCWSTR>(href), NULL, NULL, SW_SHOWNORMAL);
		oe->cancelBubble = VARIANT_TRUE;
		oe->returnValue = VARIANT_FALSE;
		return VARIANT_TRUE;
	}
	if(activation == FBELinkNavigation::LinkActivation::Blocked)
	{
		oe->cancelBubble = VARIANT_TRUE;
		oe->returnValue = VARIANT_FALSE;
		return VARIANT_TRUE;
	}
	return VARIANT_FALSE;
}

VARIANT_BOOL CFBEView::OnMouseDown(IDispatch* evt)
{
	MSHTML::IHTMLEventObjPtr eventObject(evt);
	if (!eventObject || eventObject->button != 1) return VARIANT_TRUE;
	m_table_selection_dragging = false;
	if (m_table_selection_anchor) m_table_selection_anchor.Release();
	UpdateTableCellHighlights(m_table_selection_cells, std::vector<MSHTML::IHTMLElementPtr>());
	MSHTML::IHTMLElementPtr source(eventObject->srcElement);
	MSHTML::IHTMLElementPtr cell(FbeTable::FindTableCell(source));
	// The document event signature returns VT_BOOL: VARIANT_FALSE cancels the
	// native MSHTML gesture.  Outside tables (and inside one cell) preserve the
	// editor's normal text selection, double-click and Shift-click behaviour.
	if (!cell) return VARIANT_TRUE;
	m_table_selection_anchor = cell;
	m_table_selection_dragging = true;
	return VARIANT_TRUE;
}

VARIANT_BOOL CFBEView::OnMouseMove(IDispatch* evt)
{
	if (!m_table_selection_dragging || !m_table_selection_anchor) return VARIANT_TRUE;
	MSHTML::IHTMLEventObjPtr eventObject(evt);
	if (!eventObject) return VARIANT_TRUE;
	MSHTML::IHTMLElementPtr source(eventObject ? eventObject->srcElement : NULL);
	MSHTML::IHTMLElementPtr cell(FbeTable::FindTableCell(source));
	std::vector<MSHTML::IHTMLElementPtr> cells;
	if (!cell || cell == m_table_selection_anchor || !FbeTable::GetCellRectangle(m_table_selection_anchor, cell, cells) || !SelectTableCellRange(Document(), m_table_selection_anchor, cell)) return VARIANT_TRUE;
	UpdateTableCellHighlights(m_table_selection_cells, cells);
	eventObject->cancelBubble = VARIANT_TRUE;
	eventObject->returnValue = VARIANT_FALSE;
	return VARIANT_FALSE;
}

VARIANT_BOOL CFBEView::OnMouseUp(IDispatch* evt)
{
	if (m_drag_mouse_armed && !m_internal_drag &&
		!PostMessage(AU::WM_FINALIZE_DROP, m_drop_generation)) ResetPendingDrop();
	if (!m_table_selection_dragging) return VARIANT_TRUE;
	MSHTML::IHTMLEventObjPtr eventObject(evt);
	MSHTML::IHTMLElementPtr source(eventObject ? eventObject->srcElement : NULL);
	MSHTML::IHTMLElementPtr cell(FbeTable::FindTableCell(source));
	std::vector<MSHTML::IHTMLElementPtr> cells;
	bool tableSelectionHandled = !m_table_selection_cells.empty();
	if (cell && cell != m_table_selection_anchor && FbeTable::GetCellRectangle(m_table_selection_anchor, cell, cells) && SelectTableCellRange(Document(), m_table_selection_anchor, cell))
	{
		UpdateTableCellHighlights(m_table_selection_cells, cells);
		tableSelectionHandled = true;
	}
	m_table_selection_dragging = false;
	if (!tableSelectionHandled || !eventObject) return VARIANT_TRUE;
	eventObject->cancelBubble = VARIANT_TRUE;
	eventObject->returnValue = VARIANT_FALSE;
	return VARIANT_FALSE;
}

bool CFBEView::MoveTableCell(bool reverse)
{
	try
	{
		if (!HasDoc()) return false;
		MSHTML::IHTMLTxtRangePtr selection(Document()->selection->createRange());
		MSHTML::IHTMLElementPtr cell(FbeTable::FindTableCell(selection ? selection->parentElement() : MSHTML::IHTMLElementPtr()));
		MSHTML::IHTMLElementPtr row(FbeTable::FindTableRow(cell));
		MSHTML::IHTMLElementPtr table(FbeTable::FindTableElement(row));
		if (!cell || !row || !table) return false;

		std::vector<MSHTML::IHTMLElementPtr> cells;
		FbeTable::GetCells(table, cells);
		size_t index = 0;
		while (index < cells.size() && cells[index] != cell) ++index;
		if (index == cells.size()) return false;

		if (!reverse && index + 1 == cells.size())
		{
			Grid grid;
			if (!FbeTable::BuildGrid(table, grid)) return false;
			long rowIndex = 0; while (rowIndex < static_cast<long>(grid.rows.size()) && grid.rows[rowIndex] != row) ++rowIndex;
			if (rowIndex == static_cast<long>(grid.rows.size())) return false;
			FbeDom::MarkupUndoUnitScope undo(m_mk_srv, L"insert table row below");
			if (!FbeTable::InsertRow(Document(), grid, rowIndex, true, cell->tagName)) return false;
			undo.Close();
			NotifyTableStructureChanged(m_frame, m_hWnd);
			FbeTable::GetCells(table, cells);
		}

		size_t targetIndex = index;
		if (reverse) {
			if (targetIndex > 0) --targetIndex;
		} else if (targetIndex + 1 < cells.size()) {
			++targetIndex;
		}
		MSHTML::IHTMLTxtRangePtr range(MSHTML::IHTMLBodyElementPtr(Document()->body)->createTextRange());
		range->moveToElementText(cells[targetIndex]);
		range->collapse(VARIANT_TRUE);
		range->select();
		return true;
	}
	catch (_com_error&) { return false; }
}

VARIANT_BOOL CFBEView::OnKeyPress(IDispatch* evt)
{
	bool intercepted = false;
	try
	{
		MSHTML::IHTMLEventObjPtr event(evt);
		if (!event) return VARIANT_TRUE;
		if (event->keyCode != L' ' || event->ctrlKey == VARIANT_TRUE || event->altKey == VARIANT_TRUE) return VARIANT_TRUE;
		const CString marker(_Settings.GetNBSPChar());
		if (marker.GetLength() != 1 || marker == L"\u00a0") return VARIANT_TRUE;
		MSHTML::IHTMLDocument2Ptr document(Document());
		if (!document || !document->selection) return VARIANT_TRUE;
		const _bstr_t selectionType(document->selection->type);
		if (selectionType.length() && _wcsicmp(static_cast<LPCWSTR>(selectionType), L"Control") == 0) return VARIANT_TRUE;
		MSHTML::IHTMLTxtRangePtr caret(document->selection->createRange());
		if (!caret || caret->compareEndPoints(L"StartToEnd", caret) != 0) return VARIANT_TRUE;
		MSHTML::IHTMLElementPtr parent(caret->parentElement());
		MSHTML::IHTMLElementPtr body(MSHTML::IHTMLDocument3Ptr(document)->getElementById(L"fbw_body"));
		if (!parent || !body || !body->contains(parent)) return VARIANT_TRUE;
		MSHTML::IHTMLTxtRangePtr previous(caret->duplicate());
		if (!previous || previous->moveStart(L"character", -1) != -1 || previous->parentElement() != parent ||
			CString(static_cast<LPCWSTR>(previous->htmlText)) != L" ") return VARIANT_TRUE;

		// MSHTML would replace the first of two spaces with U+00A0.  The
		// subsequent DOM nodeValue conversion invalidates its typing undo unit.
		// Insert the display marker through the editor's undoable text range
		// before MSHTML performs that conversion.
		event->returnValue = VARIANT_FALSE;
		intercepted = true;
		FbeDom::MarkupUndoUnitScope undo(m_mk_srv, L"insert nonbreaking space");
		const CString replacement = marker + L" ";
		previous->text = static_cast<LPCWSTR>(replacement);
		previous->collapse(VARIANT_FALSE);
		previous->select();
		undo.Close();
		return VARIANT_FALSE;
	}
	catch (_com_error& error) {
		if (intercepted) U::ReportError(error);
		return intercepted ? VARIANT_FALSE : VARIANT_TRUE;
	}
	catch (const ATL::CAtlException& error) {
		if (intercepted) U::ReportError(error.m_hr);
		return intercepted ? VARIANT_FALSE : VARIANT_TRUE;
	}
	catch (const std::bad_alloc&) {
		if (intercepted) U::ReportError(E_OUTOFMEMORY);
		return intercepted ? VARIANT_FALSE : VARIANT_TRUE;
	}
}

VARIANT_BOOL  CFBEView::OnKeyDown(IDispatch *evt)
{
	MSHTML::IHTMLEventObjPtr oe(evt);
	if (oe && oe->keyCode == VK_TAB && MoveTableCell(oe->shiftKey == VARIANT_TRUE))
	{
		oe->cancelBubble = VARIANT_TRUE;
		oe->returnValue = VARIANT_FALSE;
		return VARIANT_FALSE;
	}
	if (oe && (oe->keyCode == VK_LEFT || oe->keyCode == VK_UP || oe->keyCode == VK_PRIOR || oe->keyCode == VK_HOME))
		m_startMatch = m_endMatch = 0;
	return VARIANT_TRUE;
}

VARIANT_BOOL CFBEView::OnKeyUp(IDispatch*)
{
	if (m_cur_input && m_cur_input->value != m_cur_val)
	{
		m_form_changed = true;
		m_form_cp = true;
		::PostMessage(m_frame, AU::WM_DESCRIPTION_FORM_CHANGED, 0, 0);
	}
	return VARIANT_TRUE;
}

BOOL CFBEView::OnIdle()
{
    // Posting may fail (for example, a full thread message queue). The WTL
    // idle handler is registered before the browser can emit paste events;
    // it can recover canceled events or replay requests after a reentrant post.
    if (m_pendingHtmlPaste && !m_pendingHtmlPaste->posted && m_htmlPasteCallbackDepth == 0) {
        // A message consumed by a reentrant COM/clipboard pump still owns its
        // accepted requests. Replay them at idle if reposting was unavailable.
        if (!m_pendingHtmlPaste->requests.empty()) {
            BOOL handled=FALSE;
            OnDeferredHtmlPaste(0,m_pendingHtmlPaste->generation,0,handled);
        } else {
            auto pending = std::move(m_pendingHtmlPaste);
            const HRESULT result = pending->Finish(m_ownerDocument);
            if (FAILED(result)) StartupTrace::HResult(L"clipboard", L"CP101", result, L"Canceled paste recovery");
        }
    }
    if(m_htmlPasteCallbackDepth==0 && FAILED(m_htmlPasteCallbackFailure)) {
        const HRESULT failure=m_htmlPasteCallbackFailure;m_htmlPasteCallbackFailure=S_OK;U::ReportError(failure);
    }
    return FALSE;
}

LRESULT CFBEView::OnDeferredHtmlPaste(UINT, WPARAM generation, LPARAM, BOOL&)
{
    if (!m_pendingHtmlPaste || generation != m_pendingHtmlPaste->generation) return 0;
    if (m_htmlPasteCallbackDepth > 0) {
        // Posted messages can be dispatched by STA calls made inside onpaste.
        // The native canceled event has not closed its Undo parent yet.
        m_pendingHtmlPaste->posted=false;
        return 0;
    }
    auto pending = std::move(m_pendingHtmlPaste);
    try {
        CheckError(pending->Finish(m_ownerDocument));
        for (const auto& request : pending->requests) {
            // Repeated Ctrl+V at the same position follows the caret left by
            // the preceding insertion; an explicitly moved selection is kept.
            if (!request->continueAtCaret && request->selection) request->selection->select();
            m_activeHtmlPaste = request;
            BOOL handled = FALSE;
            OnPaste(0, ID_EDIT_PASTE, 0, handled);
            m_activeHtmlPaste.reset();
        }
    } catch (const _com_error& error) {
        const wchar_t* route=pending->requests.empty() ? L"CP401" : (pending->requests.front()->internal ? L"CP402" : L"CP403");
        CString diagnostic;diagnostic.Format(L"stage=CP314; operation-stage=%s",route);
        StartupTrace::HResult(L"clipboard", L"CP202", error.Error(), diagnostic); U::ReportError(error.Error());
    }
    catch (const std::bad_alloc&) { _com_error error(E_OUTOFMEMORY); U::ReportError(error); }
    return 0;
}

VARIANT_BOOL  CFBEView::OnRealPaste(IDispatch* evt)
{
    // Outlive all event COM references as their Release may pump STA messages.
    struct CallbackScope {
        int& depth;
        explicit CallbackScope(int& value):depth(value){++depth;}
        ~CallbackScope(){--depth;}
    } callback(m_htmlPasteCallbackDepth);
	MSHTML::IHTMLEventObjPtr oe(evt);
	oe->cancelBubble = VARIANT_TRUE;
	if(!m_enable_paste)
	{
		// MSHTML can roll back DOM edits made inside the canceled paste event.
		// Run the editor insertion after the event returns.
        oe->returnValue = VARIANT_FALSE;
        try {
            if (m_normalize && m_ownerDocument) {
                if (!m_pendingHtmlPaste) {
                    CComPtr<IOleUndoManager> manager(GetPasteUndoManager(Document()));
                    if (!manager) CheckError(E_NOINTERFACE);
                    m_pendingHtmlPaste = std::make_shared<PendingHtmlPaste>(manager, m_ownerDocument);
                    m_pendingHtmlPaste->generation = ++m_htmlPasteGeneration;
                }
                auto pending = m_pendingHtmlPaste;
                ++pending->events;
                auto request = std::make_shared<DeferredHtmlPaste>(*this);
                if (!pending->requests.empty()) {
                    auto previous = pending->requests.back()->selection;
                    request->continueAtCaret = previous && request->selection &&
                        previous->compareEndPoints(L"StartToStart", request->selection) == 0 &&
                        previous->compareEndPoints(L"EndToEnd", request->selection) == 0;
                }
                pending->requests.push_back(request);
                if (!pending->posted) {
                    const HRESULT injected = FbeUndo::Inject(L"paste-post");
                    ::SetLastError(ERROR_SUCCESS);
                    const bool posted = SUCCEEDED(injected) && PostMessage(AU::WM_FINALIZE_HTML_PASTE, pending->generation, 0);
                    if (!posted) {
                        const DWORD error = ::GetLastError();
                        const HRESULT failure = FAILED(injected) ? injected : HRESULT_FROM_WIN32(error ? error : ERROR_NOT_ENOUGH_QUOTA);
                        // Own the immutable request until the existing idle handler
                        // can finish the canceled native event and replay it once.
                        CString diagnostic;diagnostic.Format(L"stage=CP315; operation-stage=%s",request->internal ? L"CP402" : L"CP403");
                        StartupTrace::HResult(L"clipboard", L"CP100", failure, diagnostic);
                        return VARIANT_TRUE;
                    }
                    pending->posted = true;
                }
            } else if (!PostMessage(WM_COMMAND, MAKELONG(ID_EDIT_PASTE, 0), 0)) {
                StartupTrace::Event(L"clipboard", L"CP100", L"Deferred form paste queue rejected");
            }
        } catch (_com_error& error) { oe->returnValue = VARIANT_FALSE; m_htmlPasteCallbackFailure=error.Error(); StartupTrace::HResult(L"clipboard",L"CP206",error.Error(),L"stage=CP302; operation-stage=CP401"); return VARIANT_TRUE; }
        catch (const std::bad_alloc&) { oe->returnValue = VARIANT_FALSE; m_htmlPasteCallbackFailure=E_OUTOFMEMORY; StartupTrace::HResult(L"clipboard",L"CP206",E_OUTOFMEMORY,L"stage=CP302; operation-stage=CP401"); return VARIANT_TRUE; }
	}
	else
	{
		oe->returnValue = VARIANT_TRUE;
		if (m_cur_input)
			::PostMessage(m_frame, AU::WM_DESCRIPTION_FORM_CHANGED, 0, 0);
	}

	return VARIANT_TRUE;
}

bool  CFBEView::IsFormChanged() {
  if (!m_form_changed && (bool)m_cur_input)
    m_form_changed=m_form_changed || m_cur_input->value != m_cur_val;
  return m_form_changed;
}

bool  CFBEView::IsFormCP() {
  if (!m_form_cp && (bool)m_cur_input)
    m_form_cp=m_form_cp || m_cur_input->value != m_cur_val;
  return m_form_cp;
}

void  CFBEView::ResetFormChanged() {
  m_form_changed=false;
  if (m_cur_input)
    m_cur_val=m_cur_input->value;
}

void  CFBEView::ResetFormCP() {
  m_form_cp=false;
  if (m_cur_input)
    m_cur_val=m_cur_input->value;
}

void  CFBEView::OnFocusIn(IDispatch *evt) {
	ReportCaretFocusState(L"design-focusin", m_hWnd, false);
  // check previous value
  if (m_cur_input) {
    bool cv=m_cur_input->value != m_cur_val;
    m_form_changed=m_form_changed || cv;
    m_form_cp=m_form_cp || cv;
    m_cur_input.Release();
  }

  MSHTML::IHTMLEventObjPtr  oe(evt);
  if (!(bool)oe)
    return;

  MSHTML::IHTMLElementPtr   te(oe->srcElement);
  if (!(bool)te || U::scmp(te->tagName,L"INPUT"))
    return;

  m_cur_input=te;
  if (!(bool)m_cur_input)
    return;

  if (U::scmp(m_cur_input->type,L"text")) {
    m_cur_input.Release();
    return;
  }

  m_cur_val=m_cur_input->value;
}

// find/replace support for scintilla
static int ScintillaPositionAfter(HWND source, int position)
{
	return static_cast<int>(::SendMessage(source, SCI_POSITIONAFTER, position, 0));
}

static int ScintillaPositionBefore(HWND source, int position)
{
	return static_cast<int>(::SendMessage(source, SCI_POSITIONBEFORE, position, 0));
}

void CFBEView::ResetScintillaSearchError(HWND src)
{
	m_last_search_error.Empty();
	m_last_search_error_is_regexp = false;
	::SendMessage(src, SCI_SETSTATUS, SC_STATUS_OK, 0);
	if (!m_fo.fRegexp || m_fo.pattern.IsEmpty()) m_last_reported_source_error.Empty();
}

bool CFBEView::ReportScintillaRegexError(HWND src)
{
	const int status = static_cast<int>(::SendMessage(src, SCI_GETSTATUS, 0, 0));
	if (status == SC_STATUS_OK)
	{
		m_last_reported_source_error.Empty();
		return false;
	}
	const CString format = status == SC_STATUS_WARN_REGEX
		? FbeLoadRuntimeStringByKey(L"fbe.regex.error.source.invalid", L"Code mode (Scintilla/C++11): invalid regular expression.\nExpression: %s\nStatus: %d. Scintilla does not provide an error position or a detailed reason.")
		: FbeLoadRuntimeStringByKey(L"fbe.regex.error.source.status", L"Code mode (Scintilla/C++11): search failed.\nExpression: %s\nScintilla status: %d.");
	m_last_search_error.Format(format, static_cast<LPCWSTR>(m_fo.pattern), status);
	m_last_search_error_is_regexp = true;
	if (m_last_reported_source_error != m_last_search_error)
	{
		m_last_reported_source_error = m_last_search_error;
		U::MessageBox(m_hWnd, m_last_search_error, L"FictionBook Editor", MB_OK | MB_ICONEXCLAMATION);
	}
	return true;
}

bool CFBEView::SciFindNext(HWND src, bool fFwdOnly, bool fBarf, bool skipCurrentZeroLength,
	int zeroLengthGuardStart, int zeroLengthGuardEnd) {
  ResetScintillaSearchError(src);
  if (m_fo.pattern.IsEmpty())
    return true;

  int	    flags=0;
  if (m_fo.flags & FRF_WHOLE)
    flags|=SCFIND_WHOLEWORD;
  if (m_fo.flags & FRF_CASE)
    flags|=SCFIND_MATCHCASE;
  if (m_fo.fRegexp)
    flags|=SCFIND_REGEXP|SCFIND_CXX11REGEX;
  int rev=m_fo.flags & FRF_REVERSE && !fFwdOnly;

  // Source search must not rewrite the user-visible Find field while
  // adapting U+00A0 to the configured document-space character.
  CString searchPattern(m_fo.pattern);
  NormalizeSearchPatternNbsp(searchPattern);

  DWORD   len=::WideCharToMultiByte(CP_UTF8,0, searchPattern,searchPattern.GetLength(), NULL,0,NULL,NULL);
  std::vector<char> tmp(len+1);
  if (!tmp.empty()) 
  {
    ::WideCharToMultiByte(CP_UTF8,0, searchPattern,searchPattern.GetLength(), tmp.data(),len,NULL,NULL);
    tmp[len]='\0';
    int p1=::SendMessage(src,SCI_GETSELECTIONSTART,0,0);
    int p2=::SendMessage(src,SCI_GETSELECTIONEND,0,0);
	if (p2>p1 && !rev) p1=p2;
	const int skippedZeroLengthPosition = p1;
	if (zeroLengthGuardStart < 0 || zeroLengthGuardEnd < 0)
	{
		zeroLengthGuardStart = skippedZeroLengthPosition;
		zeroLengthGuardEnd = skippedZeroLengthPosition;
	}
	const int zeroLengthGuardFirst = min(zeroLengthGuardStart, zeroLengthGuardEnd);
	const int zeroLengthGuardLast = max(zeroLengthGuardStart, zeroLengthGuardEnd);
	auto isProtectedZeroLengthHit = [&]()
	{
		const int hitStart = static_cast<int>(::SendMessage(src, SCI_GETTARGETSTART, 0, 0));
		const int hitEnd = static_cast<int>(::SendMessage(src, SCI_GETTARGETEND, 0, 0));
		return skipCurrentZeroLength && hitStart == hitEnd &&
			hitStart >= zeroLengthGuardFirst && hitStart <= zeroLengthGuardLast;
	};
	bool exhaustedZeroLengthPosition = false;
	if (skipCurrentZeroLength)
	{
		const int guardBoundary = rev ? zeroLengthGuardFirst : zeroLengthGuardLast;
		const int nextPosition = rev ? guardBoundary : ScintillaPositionAfter(src, guardBoundary);
		// SCI_POSITIONAFTER/BEFORE are document-aware (and therefore UTF-8-safe),
		// but cannot advance beyond EOF/BOF. A reverse search starts at the guard
		// boundary itself and filters that logical hit below: starting before it
		// would make Scintilla exclude a distinct zero-length hit at that boundary.
		exhaustedZeroLengthPosition = !rev && nextPosition == guardBoundary;
		if (!exhaustedZeroLengthPosition)
			p1 = nextPosition;
	}
    else if (rev) p1 = ScintillaPositionBefore(src, p1);
    if (p1<0) p1=0;
    p2=rev ? 0 : ::SendMessage(src,SCI_GETLENGTH,0,0);
    int p3=p2==0 ? ::SendMessage(src,SCI_GETLENGTH,0,0) : 0;
    ::SendMessage(src,SCI_SETTARGETSTART,p1,0);
    ::SendMessage(src,SCI_SETTARGETEND,p2,0);
    ::SendMessage(src,SCI_SETSEARCHFLAGS,flags,0);
    // Scintilla keeps regexp syntax errors in its status channel. Reset it
    // before every attempt so a previous error cannot taint a later Find.
    m_last_search_error.Empty();
    m_last_search_error_is_regexp = false;
    ::SendMessage(src, SCI_SETSTATUS, SC_STATUS_OK, 0);
    // this sometimes hangs in reverse search :)
    int ret = -1;
	auto skipProtectedZeroLengthHits = [&](int rangeEnd) -> bool
	{
		if(rev && skipCurrentZeroLength && (ret == -1 || isProtectedZeroLengthHit()))
		{
			if (!AU::Search::FindPreviousSkippingZeroLengthGuard(src, flags, tmp.data(), static_cast<int>(len),
				zeroLengthGuardFirst, zeroLengthGuardLast, rangeEnd, m_fo.fRegexp, ret, nullptr))
			{
				ReportScintillaRegexError(src);
				return false;
			}
			return true;
		}
		while (ret != -1 && isProtectedZeroLengthHit())
		{
			if(rev) break;
			// A replace may leave the old anchor at the same point (empty
			// replacement) or move it across inserted text. Continue past the
			// complete protected interval for both initial and wrap searches.
			const int guardBoundary = zeroLengthGuardLast;
			const int continuation = ScintillaPositionAfter(src, guardBoundary);
			if (continuation == guardBoundary || (!rev && continuation >= rangeEnd) || (rev && continuation <= rangeEnd))
			{
				ret = -1;
				break;
			}
			::SendMessage(src, SCI_SETTARGETSTART, continuation, 0);
			::SendMessage(src, SCI_SETTARGETEND, rangeEnd, 0);
			::SendMessage(src, SCI_SETSEARCHFLAGS, flags, 0);
			::SendMessage(src, SCI_SETSTATUS, SC_STATUS_OK, 0);
			ret = ::SendMessage(src, SCI_SEARCHINTARGET, len, (LPARAM)tmp.data());
			if (ret == -1 && m_fo.fRegexp && ::SendMessage(src, SCI_GETSTATUS, 0, 0) != SC_STATUS_OK)
			{
				ReportScintillaRegexError(src);
				return false;
			}
		}
		return true;
	};
    if (!exhaustedZeroLengthPosition && p1 != p2)
        ret=::SendMessage(src,SCI_SEARCHINTARGET,len,(LPARAM)tmp.data());
    else if (m_fo.fRegexp && p1 == p2 && p1 == p3)
        // Validate the pattern's status on an empty document while retaining
        // the existing no-match behavior for an empty search range.
        ::SendMessage(src, SCI_SEARCHINTARGET, len, (LPARAM)tmp.data());
    if (ret == -1 && m_fo.fRegexp && ::SendMessage(src, SCI_GETSTATUS, 0, 0) != SC_STATUS_OK)
    {
        ReportScintillaRegexError(src);
        return false;
    }
	if (!skipProtectedZeroLengthHits(p2)) return false;
    if (ret==-1) 
	{ // try wrap
		if (p1!=p3) 
		{
			::SendMessage(src,SCI_SETTARGETSTART,p3,0);
			::SendMessage(src,SCI_SETTARGETEND,p1,0);
			::SendMessage(src,SCI_SETSEARCHFLAGS,flags,0);
            ::SendMessage(src, SCI_SETSTATUS, SC_STATUS_OK, 0);
			ret=::SendMessage(src,SCI_SEARCHINTARGET,len,(LPARAM)tmp.data());
            if (ret == -1 && m_fo.fRegexp && ::SendMessage(src, SCI_GETSTATUS, 0, 0) != SC_STATUS_OK)
            {
                ReportScintillaRegexError(src);
                return false;
            }
			if (!skipProtectedZeroLengthHits(p1)) return false;
		}
		if (ret==-1) 
		{
			m_last_reported_source_error.Empty();
			if (fBarf)
			{
				U::MessageBox(MB_OK|MB_ICONEXCLAMATION, IDR_MAINFRAME, IDS_SEARCH_FAIL_MSG, static_cast<LPCWSTR>(m_fo.pattern));
			}
			return false;
		}
		::MessageBeep(MB_ICONASTERISK);
    }
    m_last_reported_source_error.Empty();
    p1=::SendMessage(src,SCI_GETTARGETSTART,0,0);
    p2=::SendMessage(src,SCI_GETTARGETEND,0,0);
    ::SendMessage(src,SCI_SETSELECTIONSTART,p1,0);
    ::SendMessage(src,SCI_SETSELECTIONEND,p2,0);
    ::SendMessage(src,SCI_SCROLLCARET,0,0);
    return true;
  } else
  {
    wchar_t msg[MAX_LOAD_STRING + 1];
	wchar_t cpt[MAX_LOAD_STRING + 1];
	FbeLoadString(_Module.GetResourceInstance(), IDS_OUT_OF_MEM_MSG, msg, MAX_LOAD_STRING);
	FbeLoadString(_Module.GetResourceInstance(), IDR_MAINFRAME, cpt, MAX_LOAD_STRING);
	U::MessageBox(::GetActiveWindow(), msg, cpt, MB_OK|MB_ICONERROR);
  }

  return false;
}

_bstr_t CFBEView::Selection()
{
	try
	{
		MSHTML::IHTMLTxtRangePtr rng(Document()->selection->createRange());
		if(!(bool)rng)
			return _bstr_t();

		MSHTML::IHTMLTxtRangePtr dup(rng->duplicate());
		dup->collapse(VARIANT_TRUE);

		MSHTML::IHTMLElementPtr elem(dup->parentElement());
		while ((bool)elem && U::scmp(elem->tagName, L"P") && U::scmp(elem->tagName, L"DIV"))
			elem = elem->parentElement;

		if(elem)
		{
			dup->moveToElementText(elem);
			if(rng->compareEndPoints(L"EndToEnd", dup) > 0)
				rng->setEndPoint(L"EndToEnd", dup);
		}

		return rng->text;
	}
	catch (_com_error& err)
	{
		U::ReportError(err);
	}

	return _bstr_t();
}

// Modification by Pilgrim
static bool IsTable(MSHTML::IHTMLDOMNode *node) {
	MSHTML::IHTMLElementPtr   elem(node);
	return U::scmp(elem->className,L"table")==0;
}

static bool IsTR(MSHTML::IHTMLDOMNode *node) {
	MSHTML::IHTMLElementPtr   elem(node);
	return U::scmp(elem->className,L"tr")==0;
}

static bool IsTH(MSHTML::IHTMLDOMNode *node) {
	MSHTML::IHTMLElementPtr   elem(node);
	return U::scmp(elem->className,L"th")==0;
}

static bool IsTD(MSHTML::IHTMLDOMNode *node) {
	MSHTML::IHTMLElementPtr   elem(node);
	return U::scmp(elem->className,L"td")==0;
}

bool CFBEView::GoToFootnote(bool fCheck)
{
	const FBEReferenceNavigation::Resolution result =
		FBEReferenceNavigation::FindFootnoteTarget(Document(), !fCheck);
	if (!result.CanNavigate()) return false;
	if (fCheck) return true;
	if (!result.HasTarget()) return false;
	GoTo(result.element);
	result.scrollElement->scrollIntoView(VARIANT_TRUE);
	return true;
}
bool CFBEView::GoToReference(bool fCheck)
{
	const FBEReferenceNavigation::Resolution result =
		FBEReferenceNavigation::FindReferenceTarget(Document(), !fCheck);
	if (!result.CanNavigate()) return false;
	if (fCheck) return true;
	if (result.status == FBEReferenceNavigation::ResolutionStatus::NoReferences)
	{
		wchar_t cpt[MAX_LOAD_STRING + 1];
		wchar_t msg[MAX_LOAD_STRING + 1];
		FbeLoadString(_Module.GetResourceInstance(), IDR_MAINFRAME, cpt, MAX_LOAD_STRING);
		FbeLoadString(_Module.GetResourceInstance(), IDS_GOTO_REF_FAIL_MSG, msg, MAX_LOAD_STRING);
		U::MessageBox(::GetActiveWindow(), msg, cpt, MB_OK|MB_ICONINFORMATION);
		return false;
	}
	if (!result.HasTarget()) return false;
	GoTo(result.element);
	MSHTML::IHTMLTxtRangePtr range(MSHTML::IHTMLBodyElementPtr(Document()->body)->createTextRange());
	range->moveToElementText(result.element);
	range->collapse(VARIANT_TRUE);
	CString text(static_cast<LPCWSTR>(result.element->innerText));
	range->move(L"character", text.GetLength());
	range->select();
	MSHTML::IHTMLRectPtr rect = MSHTML::IHTMLElement2Ptr(result.element)->getBoundingClientRect();
	MSHTML::IHTMLWindow2Ptr window(MSHTML::IHTMLDocument2Ptr(Document())->parentWindow);
	if (rect && window)
	{
		if (rect->bottom - rect->top <= _Settings.GetViewHeight())
			window->scrollBy(0, (rect->top + rect->bottom - _Settings.GetViewHeight()) / 2);
		else
			window->scrollBy(0, rect->top);
	}
	// Preserve the historic action return value; command routing ignores it.
	return false;
}

LRESULT CFBEView::OnEditInsertTable(WORD /* unused: wNotifyCode */, WORD /* unused: wID */, HWND /* unused: hWndCtl */)
{
	CTableDlg dlg;
	if(dlg.DoModal()==IDOK) {
		int nRows = dlg.m_nRows;
		int nColumns = dlg.m_nColumns;
		bool bTitle = dlg.m_bTitle;
		InsertTable(false,bTitle,nRows,nColumns);
	}
	return 0;
}

static void NotifyTableStructureChanged(HWND frame, HWND view)
{
	::SendMessage(frame, WM_COMMAND, static_cast<WPARAM>(MAKELONG(0, IDN_SEL_CHANGE)), reinterpret_cast<LPARAM>(view));
	::SendMessage(frame, WM_COMMAND, static_cast<WPARAM>(MAKELONG(0, IDN_TREE_RESTORE)), 0);
}

LRESULT CFBEView::OnTableInsertRowAbove(WORD, WORD, HWND, BOOL&)
{
	try
	{
		MSHTML::IHTMLElementPtr cell(SelectionStructTableCon());
		MSHTML::IHTMLElementPtr row(FbeTable::FindTableRow(cell));
		MSHTML::IHTMLElementPtr table(FbeTable::FindTableElement(row));
		Grid grid;
		if (!cell || !row || !FbeTable::BuildGrid(table, grid)) return 0;
		long rowIndex = 0; while (rowIndex < static_cast<long>(grid.rows.size()) && grid.rows[rowIndex] != row) ++rowIndex;
		if (rowIndex == static_cast<long>(grid.rows.size())) return 0;
		FbeDom::MarkupUndoUnitScope undo(m_mk_srv, L"insert table row above");
		const bool changed = FbeTable::InsertRow(Document(), grid, rowIndex, false, cell->tagName);
		undo.Close();
		if (changed) NotifyTableStructureChanged(m_frame, m_hWnd);
	}
	catch (_com_error& error) { U::ReportError(error); }
	return 0;
}

LRESULT CFBEView::OnTableInsertRowBelow(WORD, WORD, HWND, BOOL&)
{
	try
	{
		MSHTML::IHTMLElementPtr cell(SelectionStructTableCon());
		MSHTML::IHTMLElementPtr row(FbeTable::FindTableRow(cell));
		MSHTML::IHTMLElementPtr table(FbeTable::FindTableElement(row));
		Grid grid;
		if (!cell || !row || !FbeTable::BuildGrid(table, grid)) return 0;
		long rowIndex = 0; while (rowIndex < static_cast<long>(grid.rows.size()) && grid.rows[rowIndex] != row) ++rowIndex;
		if (rowIndex == static_cast<long>(grid.rows.size())) return 0;
		FbeDom::MarkupUndoUnitScope undo(m_mk_srv, L"insert table row below");
		const bool changed = FbeTable::InsertRow(Document(), grid, rowIndex, true, cell->tagName);
		undo.Close();
		if (changed) NotifyTableStructureChanged(m_frame, m_hWnd);
	}
	catch (_com_error& error) { U::ReportError(error); }
	return 0;
}

LRESULT CFBEView::OnTableDeleteRow(WORD, WORD, HWND, BOOL&)
{
	try
	{
		MSHTML::IHTMLElementPtr row(FbeTable::FindTableRow(SelectionStructTableCon()));
		Grid grid;
		if (!row || !row->parentElement || !FbeTable::BuildGrid(FbeTable::FindTableElement(row), grid)) return 0;
		long rowIndex = 0; while (rowIndex < static_cast<long>(grid.rows.size()) && grid.rows[rowIndex] != row) ++rowIndex;
		if (rowIndex == static_cast<long>(grid.rows.size())) return 0;
		FbeDom::MarkupUndoUnitScope undo(m_mk_srv, L"delete table row");
		const bool changed = FbeTable::DeleteRow(grid, rowIndex);
		undo.Close();
		if (changed) NotifyTableStructureChanged(m_frame, m_hWnd);
	}
	catch (_com_error& error) { U::ReportError(error); }
	return 0;
}

LRESULT CFBEView::OnTableInsertColumnLeft(WORD, WORD, HWND, BOOL&)
{
	try { MSHTML::IHTMLElementPtr cell(SelectionStructTableCon()), row(FbeTable::FindTableRow(cell)), table(FbeTable::FindTableElement(row)); Grid grid; long index = FbeTable::BuildGrid(table, grid) ? FbeTable::FindCell(grid, cell) : -1; if (index >= 0) { FbeDom::MarkupUndoUnitScope undo(m_mk_srv, L"insert table column left"); const bool changed = FbeTable::InsertColumn(Document(), grid, index, true, cell->tagName); undo.Close(); if (changed) NotifyTableStructureChanged(m_frame, m_hWnd); } }
	catch (_com_error& error) { U::ReportError(error); }
	return 0;
}

LRESULT CFBEView::OnTableInsertColumnRight(WORD, WORD, HWND, BOOL&)
{
	try { MSHTML::IHTMLElementPtr cell(SelectionStructTableCon()), row(FbeTable::FindTableRow(cell)), table(FbeTable::FindTableElement(row)); Grid grid; long index = FbeTable::BuildGrid(table, grid) ? FbeTable::FindCell(grid, cell) : -1; if (index >= 0) { FbeDom::MarkupUndoUnitScope undo(m_mk_srv, L"insert table column right"); const bool changed = FbeTable::InsertColumn(Document(), grid, index, false, cell->tagName); undo.Close(); if (changed) NotifyTableStructureChanged(m_frame, m_hWnd); } }
	catch (_com_error& error) { U::ReportError(error); }
	return 0;
}

bool CFBEView::DeleteTableLogicalColumnForTest(long column)
{
	try {
		MSHTML::IHTMLElementPtr body(Document() ? MSHTML::IHTMLElementPtr(Document()->body) : MSHTML::IHTMLElementPtr());
		MSHTML::IHTMLElementCollectionPtr tables(body ? MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(L"TABLE") : MSHTML::IHTMLElementCollectionPtr());
		MSHTML::IHTMLElementPtr table(tables && tables->length ? MSHTML::IHTMLElementPtr(tables->item(_variant_t(0L), _variant_t())) : MSHTML::IHTMLElementPtr());
		Grid grid;
		if (!FbeTable::BuildGrid(table, grid)) return false;
		FbeDom::MarkupUndoUnitScope undo(m_mk_srv, L"delete table column");
		const bool changed = FbeTable::DeleteColumn(grid, column);
		undo.Close();
		if (!changed) return false;
		NotifyTableStructureChanged(m_frame, m_hWnd);
		return true;
	}
	catch (_com_error&) { return false; }
}

LRESULT CFBEView::OnTableDeleteColumn(WORD, WORD, HWND, BOOL&)
{
	try
	{
		MSHTML::IHTMLElementPtr selectedCell(SelectionStructTableCon());
		MSHTML::IHTMLElementPtr selectedRow(FbeTable::FindTableRow(selectedCell));
		MSHTML::IHTMLElementPtr table(FbeTable::FindTableElement(selectedRow));
		Grid grid;
		if (!selectedCell || !selectedRow || !FbeTable::BuildGrid(table, grid)) return 0;
		const long selectedIndex = FbeTable::FindCell(grid, selectedCell);
		if (selectedIndex < 0) return 0;
		FbeDom::MarkupUndoUnitScope undo(m_mk_srv, L"delete table column");
		const bool changed = FbeTable::DeleteColumn(grid, grid.cells[selectedIndex].startColumn);
		undo.Close();
		if (changed) NotifyTableStructureChanged(m_frame, m_hWnd);
	}
	catch (_com_error& error) { U::ReportError(error); }
	return 0;
}

LRESULT CFBEView::OnTableToggleHeaderCell(WORD, WORD, HWND, BOOL&)
{
	try
	{
		MSHTML::IHTMLElementPtr cell(SelectionStructTableCon());
		if (!cell || !FbeTable::FindTableElement(cell)) return 0;
		if (FbeTable::ToggleHeaderCell(Document(), cell)) NotifyTableStructureChanged(m_frame, m_hWnd);
	}
	catch (_com_error& error) { U::ReportError(error); }
	return 0;
}

LRESULT CFBEView::OnEditInsImage(WORD, WORD cmdID, HWND, BOOL&)
{
	// added by SeNS
	bool bInline = (cmdID != ID_EDIT_INS_IMAGE);
	
	if(_Settings.GetInsImageAsking())
	{
		CAddImageDlg imgDialog;
		imgDialog.DoModal(*this);
	}

	if(!_Settings.GetIsInsClearImage())
	{
		const std::vector<ImageImportFileType> imageTypes = ImageImportFileTypes();
		std::vector<COMDLG_FILTERSPEC> filters;
		filters.reserve(imageTypes.size());
		for (const ImageImportFileType& type : imageTypes)
			filters.push_back({ type.displayName.GetString(), type.wildcard.GetString() });

		wchar_t dlgTitle[MAX_LOAD_STRING + 1];
		const CString localizedTitle = FbeLoadRuntimeStringByKey(L"fbe.image.choose_file", L"Choose image");
		wcsncpy_s(dlgTitle, _countof(dlgTitle), localizedTitle.GetString(), _TRUNCATE);
		ModernFileDialog::Request request;
		request.fileMustExist = true; request.pathMustExist = true; request.title = dlgTitle;
		request.okButtonLabel = FbeLoadRuntimeStringByKey(L"fbe.image.open_button", L"Open").GetString();
		request.filters = filters.data(); request.filterCount = static_cast<UINT>(filters.size()); request.filterIndex = 1;
		const ModernFileDialog::Result dialogResult = ModernFileDialog::Show(m_hWnd, request);
		if (dialogResult.outcome == ModernFileDialog::Outcome::Failed)
			StartupTrace::HResult(L"file-dialog", L"FD104", dialogResult.error, L"Insert image dialog");
		if(dialogResult.outcome == ModernFileDialog::Outcome::Accepted)
		{
			AddImage(dialogResult.paths.front().c_str(), bInline);
		}
	}
	else
	{
		// added by SeNS
		try {
			if (bInline)
			{
				MSHTML::IHTMLDOMNodePtr node(Call(L"InsInlineImage"));
			}
			else
			{
				MSHTML::IHTMLDOMNodePtr node(Call(L"InsImage"));
				if (node && !FbeVisualDom::IsDirectSectionImage(node))
					FbeVisualDom::BubbleUp(node,L"DIV");
			}
		}
		catch (_com_error&) { }
	}

	return 0;
}

bool  CFBEView::InsertTable(bool fCheck, bool bTitle, int nrows, int ncolumns) {
	try {
		// * create selection range
		MSHTML::IHTMLTxtRangePtr	rng(Document()->selection->createRange());
		if (!(bool)rng)
			return false;

		// * get its parent element
		MSHTML::IHTMLElementPtr	pe(GetHP(rng->parentElement()));
		if (!(bool)pe)
			return false;

		// * get parents for start and end ranges and ensure they are the same as pe
		MSHTML::IHTMLTxtRangePtr	tr(rng->duplicate());
		tr->collapse(VARIANT_TRUE);
		if (GetHP(tr->parentElement())!=pe)
			return false;
#if 0
		tr=rng->duplicate();
		tr->collapse(VARIANT_FALSE);
		if (GetHP(tr->parentElement())!=pe)
			return false;
#endif

		// * check if it possible to insert a table there
		_bstr_t   cls(pe->className);
		if (U::scmp(cls,L"section") && U::scmp(cls,L"epigraph") &&
			U::scmp(cls,L"annotation") && U::scmp(cls,L"history") && U::scmp(cls,L"cite"))
			return false;

		// * ok, all checks passed
		if (fCheck)
			return true;

		// at this point we are ready to create a table
		// Structural editor operations use direct DOM insertion inside a markup
		// undo unit.  Find the current paragraph as an insertion anchor.
		MSHTML::IHTMLElementPtr anchor(rng->parentElement());
		while (anchor && U::scmp(anchor->tagName, L"P") != 0)
			anchor = anchor->parentElement;
		if (!anchor || anchor->parentElement != pe)
			return false;

		// * create an undo unit
		m_mk_srv->BeginUndoUnit(L"insert table");

		MSHTML::IHTMLElementPtr te(Document()->createElement(L"TABLE"));
		te->className = L"table";
		// MSHTML only materializes rows/cells added through the DOM when they
		// are placed in a table section.  HTML parsing creates TBODY for us,
		// but direct DOM insertion must do it explicitly.
		MSHTML::IHTMLElementPtr tbody(Document()->createElement(L"TBODY"));
		MSHTML::IHTMLElement2Ptr(te)->insertAdjacentElement(L"beforeEnd", tbody);
		nrows = max(1, nrows);
		ncolumns = max(1, ncolumns);
		const int totalRows = nrows + (bTitle ? 1 : 0);
		MSHTML::IHTMLElementPtr firstCell;
		for (int row = 0; row < totalRows; ++row)
		{
			MSHTML::IHTMLElementPtr tre(Document()->createElement(L"TR"));
			tre->className = L"tr";
			const wchar_t* cellType = bTitle && row == 0 ? L"TH" : L"TD";
			for (int column = 0; column < ncolumns; ++column) {
				MSHTML::IHTMLElementPtr cell(FbeTable::CreateCell(Document(), cellType));
				if (!firstCell) firstCell = cell;
				MSHTML::IHTMLElement2Ptr(tre)->insertAdjacentElement(L"beforeEnd", cell);
			}
			MSHTML::IHTMLElement2Ptr(tbody)->insertAdjacentElement(L"beforeEnd", tre);
		}

		// Unlike pasteHTML/InsertHTML, direct insertion is supported by this
		// MSHTML host and is captured by the surrounding undo unit.
		MSHTML::IHTMLElement2Ptr(anchor)->insertAdjacentElement(L"afterEnd", te);

		// * ensure we have good html
		FbeVisualDom::RelocateParagraphs(MSHTML::IHTMLDOMNodePtr(pe));
		FbeVisualDom::FixupParagraphs(pe);

		// * close undo unit
		m_mk_srv->EndUndoUnit();
		if (firstCell) {
			MSHTML::IHTMLTxtRangePtr cellRange(MSHTML::IHTMLBodyElementPtr(Document()->body)->createTextRange());
			cellRange->moveToElementText(firstCell);
			cellRange->collapse(VARIANT_TRUE);
			cellRange->select();
		}
		// Refresh command state after the modal table dialog. Without this the
		// Undo button can remain disabled even though MSHTML has an undo unit.
		::SendMessage(m_frame, WM_COMMAND, MAKELONG(0, IDN_SEL_CHANGE), reinterpret_cast<LPARAM>(m_hWnd));
		return true;
	}
	catch (_com_error& e) {
		U::ReportError(e);
	}
	return false;
}

long CFBEView::InsertCode()
{	
	if(bCall(L"IsCode", SelectionStructCode()))
	{
		HRESULT hr;
		BeginUndoUnit(L"insert code");
		hr = m_mk_srv->RemoveElement(SelectionStructCode());
		EndUndoUnit();
		return hr == S_OK ? 0 : -1;
	}
	else
	{
		bool undoStarted = false;
		try
		{
			BeginUndoUnit(L"insert code");
			undoStarted = true;

			int offset = -1;
			MSHTML::IHTMLTxtRangePtr rng(Document()->selection->createRange());
			if (!(bool)rng)
			{
				undoStarted = false;
				EndUndoUnit();
				return -1;
			}

			CString rngHTML((wchar_t*)rng->htmlText);

			// empty selection case - select current word
			if(rngHTML.IsEmpty())
			{
				// select word
				rng->moveStart(L"word",-1);
				CString txt(static_cast<const wchar_t*>(rng->text));
				offset = txt.GetLength();
				rng->expand(L"word");
				rngHTML.SetString(rng->htmlText);
			}

			if (!rngHTML.IsEmpty() && iswspace(rngHTML[rngHTML.GetLength()-1])) 
			{
				rng->moveEnd(L"character",-1);
				rngHTML.SetString(rng->htmlText);
				if (offset > rngHTML.GetLength()) offset--;
			}

			// save selection
			MSHTML::IMarkupPointerPtr selBegin, selEnd;
			m_mk_srv->CreateMarkupPointer(&selBegin);
			m_mk_srv->CreateMarkupPointer(&selEnd);
			m_mk_srv->MovePointersToRange(rng, selBegin, selEnd);

			if(rngHTML.Find(L"<P") != -1)
			{
				MSHTML::IHTMLElementPtr spanElem = Document()->createElement(L"<SPAN class=code>");
				MSHTML::IHTMLElementPtr selElem = rng->parentElement();
				
				MSHTML::IHTMLTxtRangePtr rngStart = rng->duplicate();
				MSHTML::IHTMLTxtRangePtr rngEnd = rng->duplicate();
				rngStart->collapse(VARIANT_TRUE);
				rngEnd->collapse(VARIANT_FALSE);

				MSHTML::IHTMLElementPtr elBegin = rngStart->parentElement(), elEnd = rngEnd->parentElement();
				while(U::scmp(elBegin->tagName, L"P")) elBegin = elBegin->parentElement;
				while(U::scmp(elEnd->tagName, L"P")) elEnd = elEnd->parentElement;

				MSHTML::IHTMLDOMNodePtr bNode = elBegin, eNode = elEnd;
				while(bNode)
				{
					CString elBeginHTML(static_cast<const wchar_t*>(elBegin->innerHTML));

					if(U::scmp(elBegin->tagName, L"P") == 0 && elBeginHTML.Find(L"<SPAN") < 0)
					{
						spanElem->innerHTML = elBegin->innerHTML;
						if(!(elBeginHTML.Find(L"<SPAN class=code>") == 0 && 
							elBeginHTML.Find(L"</SPAN>") == elBeginHTML.GetLength() - 7))
						{
							elBegin->innerHTML = spanElem->outerHTML;
						}
					}
					// remove code tag
					else
					{
						elBeginHTML.Replace (L"<SPAN class=code>", L" ");
						elBeginHTML.Replace (L"</SPAN>", L" ");
						elBegin->innerHTML = elBeginHTML.AllocSysString();
					}

					if(bNode == eNode) 
						break;

					bNode = bNode->nextSibling;
					elBegin = bNode;
				}
				// expand selection to the last paragraph
				rng->moveToElementText(elBegin);
				m_mk_srv->MovePointersToRange(rng, NULL, selEnd); 
			}
			else if(rngHTML.Find(L"<SPAN class=code>") != -1 && rngHTML.Find(L"</SPAN>") != -1)
			{
					rngHTML.Replace (L"<SPAN class=code>", L" ");
					rngHTML.Replace (L"</SPAN>", L" ");
					rng->pasteHTML(rngHTML.AllocSysString());
			}
			else
			{
				if (!rngHTML.IsEmpty() && iswspace(rngHTML[0]))
				{
					rng->moveStart(L"character",1);
					rngHTML.SetString(rng->htmlText);
				}
				if (!rngHTML.IsEmpty() && iswspace(rngHTML[rngHTML.GetLength()-1]))
				{
					rng->moveEnd(L"character",-1);
					rngHTML.SetString(rng->htmlText);
				}			
				rngHTML = L"<SPAN class=code>" + rngHTML + L"</SPAN>";
				rng->pasteHTML(rngHTML.AllocSysString());
			}

			// restore selection
			if (offset >= 0)
			{
				rng->move(L"word", -1);
				rng->move(L"character", offset);
				rng->select();
			}
			else
			{
				m_mk_srv->MoveRangeToPointers(selBegin, selEnd, rng);
				rng->select();
			}

			undoStarted = false;
			EndUndoUnit();
		}
		catch (_com_error& e)
		{
			if (undoStarted)
				EndUndoUnit();
			U::ReportError(e);
			return -1;
		}

		return 0;
	}
}

bool CFBEView::CloseFindDialog(CFindDlgBase* dlg)
{
	if(!dlg || !dlg->IsValid())
		return false;

	ClearSearchHighlights();
	dlg->DestroyWindow();
	return true;
}

bool CFBEView::CloseFindDialog(CReplaceDlgBase* dlg)
{
	if(!dlg || !dlg->IsValid())
		return false;

	dlg->DestroyWindow();
	return true;
}

bool CFBEView::IsFindDialogOpen() const
{
	return m_find_dlg != NULL && m_find_dlg->IsValid();
}

bool CFBEView::IsReplaceDialogOpen() const
{
	return m_replace_dlg != NULL && m_replace_dlg->IsValid();
}

void CFBEView::SyncSearchOptionsToOpenDialogs(FRBase* source)
{
	if (m_find_dlg != NULL && m_find_dlg->IsValid() && m_find_dlg != source)
		m_find_dlg->SyncSearchOptionsFromView();
	if (m_replace_dlg != NULL && m_replace_dlg->IsValid() && m_replace_dlg != source)
		m_replace_dlg->SyncSearchOptionsFromView();
}

void CFBEView::RefreshOpenSearchPresetPanels(FRBase* /* source */)
{
	if (m_find_dlg != NULL && m_find_dlg->IsValid() && m_find_dlg->m_templatesExpanded)
		m_find_dlg->RefreshPresetPanel();
	if (m_replace_dlg != NULL && m_replace_dlg->IsValid() && m_replace_dlg->m_templatesExpanded)
		m_replace_dlg->RefreshPresetPanel();
}

LRESULT CFBEView::OnCode(WORD /* unused: wCode */, WORD /* unused: wID */, HWND /* unused: hWnd */, BOOL& /* unused: bHandled */)
{
	return InsertCode();
}

bool CFBEView::SelectionHasTags(wchar_t* elem)
{
	try
	{
		MSHTML::IHTMLTxtRangePtr range = Document()->selection->createRange();
		if(range)
		{
			CString html(static_cast<const wchar_t*>(range->htmlText));
			if(html.Find(CString(L"<") + elem) != -1)
				return true;
		}
	}
	catch(_com_error& err)
	{
		U::ReportError(err);
		return false;
	}

	return false;
}

void CFBEView::ResetTableGridBuildCountForTest()
{
	FbeTable::GridDiagnostics::ResetBuildCount();
}

long CFBEView::TableGridBuildCountForTest()
{
	return FbeTable::GridDiagnostics::BuildCount();
}

bool CFBEView::SelectTableLogicalRangeForTest(long firstRow, long firstColumn, long lastRow, long lastColumn)
{
	try {
		MSHTML::IHTMLElementPtr body(Document() ? MSHTML::IHTMLElementPtr(Document()->body) : MSHTML::IHTMLElementPtr());
		MSHTML::IHTMLElementCollectionPtr tables(body ? MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(L"TABLE") : MSHTML::IHTMLElementCollectionPtr());
		MSHTML::IHTMLElementPtr table(tables && tables->length ? MSHTML::IHTMLElementPtr(tables->item(_variant_t(0L), _variant_t())) : MSHTML::IHTMLElementPtr());
		Grid grid;
		if (!body || !FbeTable::BuildGrid(table, grid)) return false;
		const long first = grid.At(firstRow, firstColumn), last = grid.At(lastRow, lastColumn);
		if (first < 0 || last < 0) return false;
		MSHTML::IHTMLTxtRangePtr range(MSHTML::IHTMLBodyElementPtr(body)->createTextRange());
		MSHTML::IHTMLTxtRangePtr end(MSHTML::IHTMLBodyElementPtr(body)->createTextRange());
		range->moveToElementText(grid.cells[first].element);
		if (first == last) range->collapse(VARIANT_TRUE);
		else {
			end->moveToElementText(grid.cells[last].element);
			range->setEndPoint(L"EndToEnd", end);
		}
		range->select();
		return true;
	}
	catch (_com_error&) { return false; }
}

CStringA CFBEView::TableStructuralSnapshot()
{
	try {
		MSHTML::IHTMLElementPtr body(Document() ? MSHTML::IHTMLElementPtr(Document()->body) : MSHTML::IHTMLElementPtr());
		MSHTML::IHTMLElementCollectionPtr tables(body ? MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(L"TABLE") : MSHTML::IHTMLElementCollectionPtr());
		MSHTML::IHTMLElementPtr table(tables && tables->length ? MSHTML::IHTMLElementPtr(tables->item(_variant_t(0L), _variant_t())) : MSHTML::IHTMLElementPtr());
		Grid grid;
		return FbeTable::BuildGrid(table, grid) ? FbeTable::BuildStructuralSnapshot(grid) : CStringA("invalid");
	}
	catch (_com_error&) { return CStringA("error"); }
}

static bool MakeSelectedTableCells(CFBEView* view, const wchar_t* targetName)
{
	MSHTML::IHTMLElementPtr currentCell(view->SelectionStructTableCon());
	std::vector<MSHTML::IHTMLElementPtr> cells;
	return FbeTable::GetSelectedCells(view->Document(), currentCell, cells) && FbeTable::ReplaceCells(view->Document(), cells, targetName);
}

LRESULT CFBEView::OnTableMakeHeaderCells(WORD, WORD, HWND, BOOL&)
{
	try { if (MakeSelectedTableCells(this, L"TH")) NotifyTableStructureChanged(m_frame, m_hWnd); }
	catch (_com_error& error) { U::ReportError(error); }
	return 0;
}

LRESULT CFBEView::OnTableMakeNormalCells(WORD, WORD, HWND, BOOL&)
{
	try { if (MakeSelectedTableCells(this, L"TD")) NotifyTableStructureChanged(m_frame, m_hWnd); }
	catch (_com_error& error) { U::ReportError(error); }
	return 0;
}

HRESULT CFBEView::AddImportedBinary(const BYTE* bytes, size_t size, const CString& logicalFileName,
	const CString& mimeType, _variant_t* checkedId)
{
	FbeImage::ImageInsertionResult result;
	const HRESULT hr = FbeImage::AddImportedBinary(Script(), bytes, size, logicalFileName, mimeType, &result);
	if (SUCCEEDED(hr) && checkedId) *checkedId = result.binaryId;
	return hr;
}

// images
HRESULT CFBEView::AddImage(const CString& filename, bool bInline, MSHTML::IHTMLElementPtr* inserted, MSHTML::IHTMLElement* afterImage)
{
	if (inserted) *inserted = nullptr;
	ImageImportOptions options;
	options.outputFormat = static_cast<ImageOutputFormat>(_Settings.GetImageImportFormat());
	options.jpegQuality = static_cast<int>(_Settings.GetImageImportJpegQuality());
	options.keepSupportedImages = _Settings.GetImageImportKeepSupported();
	ImageImportResult imported;
	CString error;
	HRESULT hr = ImportImageForFb2(filename, options, imported, error);
	if (hr == E_ABORT) {
		if (U::MessageBox(m_hWnd, FbeLoadRuntimeStringByKey(L"fbe.image_import.flatten_question", L"This image has transparency. Convert it to JPEG on a white background?"), FbeLoadRuntimeStringByKey(L"fbe.image_import.batch_title", L"Image import"), MB_YESNO | MB_ICONWARNING) != IDYES) return E_ABORT;
		options.flattenTransparentJpeg = true;
		hr = ImportImageForFb2(filename, options, imported, error);
	}
	if (FAILED(hr)) { if (!error.IsEmpty()) U::MessageBox(m_hWnd, filename + L"\n\n" + error, L"FictionBook Editor", MB_OK | MB_ICONERROR); else U::ReportError(hr); return hr; }
	CComPtr<IOleUndoManager> manager;
	CComPtr<IOleParentUndoUnit> parent;
	bool open = false;
	bool domRecorded = false;
	MSHTML::IHTMLDOMNodePtr owner;
	std::vector<PasteChildSnapshot> before;
	MSHTML::IHTMLTxtRangePtr beforeSelection;
	DropSelectionEndpoint beforeStart, beforeEnd;
	CComObject<PasteDomUndoUnit>* domRaw = nullptr;
	CComPtr<IOleUndoUnit> domUndo;
	try
	{
		owner = MSHTML::IHTMLDOMNodePtr(FindNormalizationOwner(SelectionContainer()));
		if (!IsLiveDropScope(owner, Document())) CheckError(E_INVALIDARG);
		before = CapturePasteChildren(owner);
		beforeSelection = Document()->selection->createRange();
		if (beforeSelection) { beforeStart.Capture(beforeSelection, true, Document()); beforeEnd.Capture(beforeSelection, false, Document()); }
		CheckError(CComObject<PasteDomUndoUnit>::CreateInstance(&domRaw));
		domUndo = domRaw;
		OpenDropUndoGroup(manager, parent);
		open = true;
		FbeImage::ImageInsertionResult result;
		hr = FbeImage::InsertImportedImage(Script(), imported.data.data(), imported.data.size(), imported.logicalFileName, imported.mimeType,
			bInline ? FbeImage::ImagePlacement::Inline : FbeImage::ImagePlacement::Block, &result, manager);
		CheckError(hr);

		MSHTML::IHTMLDOMNodePtr node(result.insertedElement);
		{
			PasteUndoRecordingScope placement(manager);
			if (!placement.Blocked()) CheckError(E_UNEXPECTED);
			if (!bInline && node && !FbeVisualDom::IsDirectSectionImage(node)) FbeVisualDom::BubbleUp(node, L"DIV");
			if (afterImage && MSHTML::IHTMLDOMNodePtr(afterImage)->nextSibling != node)
				MSHTML::IHTMLElement2Ptr(afterImage)->insertAdjacentElement(L"afterEnd", MSHTML::IHTMLElementPtr(result.insertedElement));
			CheckError(placement.Close());
		}
		MSHTML::IHTMLTxtRangePtr afterSelection(Document()->selection->createRange());
		domRaw->Initialize(owner, before, CapturePasteChildren(owner, MSHTML::IHTMLDOMNodePtr(), false),
			beforeSelection, afterSelection, nullptr, false, false, -1);
		// The OLE parent restores savepoints only after BOTH HTML and binary
		// have been restored. A DOM-only child must not mark a partial state clean.
		if (!domRaw->Changed() || !domRaw->PrepareChangedClones()) CheckError(E_UNEXPECTED);
		CheckError(manager->Add(domUndo));
		domRecorded = true;
		CheckError(CloseDropUndoGroup(manager, parent, true));
		open = false;
		if (inserted) *inserted = result.insertedElement;
	}
	catch (const _com_error& failure) { hr = failure.Error(); }
	catch (const std::bad_alloc&) { hr = E_OUTOFMEMORY; }
	if (open)
	{
		if (!domRecorded && owner) {
			try {
				// No image DOM unit was committed. Restore the preflight snapshot
				// under a blocking parent before rolling back the owned binary.
				PasteUndoRecordingScope recording(manager);
				if (!recording.Blocked()) CheckError(E_UNEXPECTED);
				domRaw->Initialize(owner, before, CapturePasteChildren(owner, MSHTML::IHTMLDOMNodePtr(), false),
					MSHTML::IHTMLTxtRangePtr(), MSHTML::IHTMLTxtRangePtr(), nullptr, false, false, -1);
				if (domRaw->Changed()) CheckError(domRaw->Restore(true));
				CheckError(recording.Close());
			} catch (const _com_error& failure) { hr = failure.Error(); }
		}
		const HRESULT rolledBack = RollbackDropUndoGroup(manager, parent);
		if (FAILED(rolledBack)) hr = rolledBack;
		try {
			MSHTML::IHTMLTxtRangePtr start(beforeStart.Restore(Document())), end(beforeEnd.Restore(Document()));
			if (start && end) { start->setEndPoint(L"EndToStart", end); start->select(); }
		} catch (const _com_error&) {}
	}
	if (FAILED(hr)) U::ReportError(hr);
	return hr;
}
