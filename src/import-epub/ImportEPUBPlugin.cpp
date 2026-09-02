#include "stdafx.h"
#include "MsxmlImport.h"
#include "ImportEPUBPlugin.h"
#include "ImportEPUBGuids.h"
#include "EpubImport.h"
#include "ImportOptionsDialog.h"
#include "resource.h"
#include "RuntimeLocalization.h"
#include "..\\common\\ModernFileDialog.h"

// {3C19F5A2-2EC8-4EC7-B7A9-F4910B4CDD82}
extern const CLSID CLSID_ImportEPUBPlugin =
{ 0x3c19f5a2, 0x2ec8, 0x4ec7, { 0xb7, 0xa9, 0xf4, 0x91, 0x0b, 0x4c, 0xdd, 0x82 } };


class ATL_NO_VTABLE CImportEPUBPlugin :
    public CComObjectRootEx<CComSingleThreadModel>,
    public CComCoClass<CImportEPUBPlugin, &CLSID_ImportEPUBPlugin>,
    public IFBEImportPlugin
{
public:
    CImportEPUBPlugin() = default;

    DECLARE_NO_REGISTRY()

    BEGIN_COM_MAP(CImportEPUBPlugin)
        COM_INTERFACE_ENTRY(IFBEImportPlugin)
    END_COM_MAP()

    // IFBEImportPlugin
    STDMETHOD(Import)(long hWnd, BSTR* filename, IDispatch** document) override;
};

// Register the class in ATL's object map.
//
// The Windows COM registry tells CoCreateInstance which DLL to load, but after
// that COM calls DllGetClassObject(CLSID_ImportEPUBPlugin, ...). ATL can return
// the class factory only for classes that are present in this object map.
// Without this line FBE sees the menu item, the registry keys look correct,
// but CoCreateInstance still fails with a COM activation error.
OBJECT_ENTRY_AUTO(CLSID_ImportEPUBPlugin, CImportEPUBPlugin)

namespace
{
    CStringW LoadPluginString(UINT stringId, LPCWSTR fallback)
    {
        return LoadImportEpubString(stringId, fallback);
    }

    CStringW XmlEscape(const CStringW& text)
    {
        CStringW s(text);
        s.Replace(L"&",  L"&amp;");
        s.Replace(L"<",  L"&lt;");
        s.Replace(L">",  L"&gt;");
        s.Replace(L"\"", L"&quot;");
        s.Replace(L"'",  L"&apos;");
        return s;
    }

    CStringW FileNameOnly(const CStringW& path)
    {
        int slash1 = path.ReverseFind(L'\\');
        int slash2 = path.ReverseFind(L'/');
        int slash = max(slash1, slash2);
        CStringW name = slash >= 0 ? path.Mid(slash + 1) : path;
        int dot = name.ReverseFind(L'.');
        if (dot > 0)
            name = name.Left(dot);
        return name;
    }

    CStringW ChangeExtensionToFb2(const CStringW& path)
    {
        CStringW out(path);
        int slash1 = out.ReverseFind(L'\\');
        int slash2 = out.ReverseFind(L'/');
        int slash = max(slash1, slash2);
        int dot = out.ReverseFind(L'.');
        if (dot > slash)
            out = out.Left(dot);
        out += L".fb2";
        return out;
    }

    enum
    {
        IDC_FILEDLG_SETTINGS_BUTTON = 7001
    };

    // Event sink for the modern Windows file picker.
    //
    // The user asked for a separate import settings button directly in
    // the file selection window. IFileOpenDialog + IFileDialogCustomize is the
    // cleanest way to do this: the button is part of the standard Windows dialog
    // instead of a separate pop-up shown after every file selection.
    class ATL_NO_VTABLE COpenDialogEvents :
        public CComObjectRootEx<CComSingleThreadModel>,
        public IFileDialogEvents,
        public IFileDialogControlEvents
    {
    public:
        BEGIN_COM_MAP(COpenDialogEvents)
            COM_INTERFACE_ENTRY(IFileDialogEvents)
            COM_INTERFACE_ENTRY(IFileDialogControlEvents)
        END_COM_MAP()

        COpenDialogEvents() : m_owner(nullptr), m_options(nullptr)
        {
        }

        void Init(HWND owner, EpubImportOptions* options)
        {
            m_owner = owner;
            m_options = options;
        }

        STDMETHOD(OnFileOk)(IFileDialog*)
        {
            return S_OK;
        }

        STDMETHOD(OnFolderChanging)(IFileDialog*, IShellItem*)
        {
            return S_OK;
        }

        STDMETHOD(OnFolderChange)(IFileDialog*)
        {
            return S_OK;
        }

        STDMETHOD(OnSelectionChange)(IFileDialog*)
        {
            return S_OK;
        }

        STDMETHOD(OnShareViolation)(IFileDialog*, IShellItem*, FDE_SHAREVIOLATION_RESPONSE* response)
        {
            if (response)
                *response = FDESVR_DEFAULT;
            return S_OK;
        }

        STDMETHOD(OnTypeChange)(IFileDialog*)
        {
            return S_OK;
        }

        STDMETHOD(OnOverwrite)(IFileDialog*, IShellItem*, FDE_OVERWRITE_RESPONSE* response)
        {
            if (response)
                *response = FDEOR_DEFAULT;
            return S_OK;
        }

        STDMETHOD(OnItemSelected)(IFileDialogCustomize*, DWORD, DWORD)
        {
            return S_OK;
        }

        STDMETHOD(OnButtonClicked)(IFileDialogCustomize* customize, DWORD controlId)
        {
            if (controlId == IDC_FILEDLG_SETTINGS_BUTTON && m_options)
            {
                HWND owner = m_owner;
                CComPtr<IOleWindow> fileDialogWindow;
                HWND dialogOwner = nullptr;
                if (customize &&
                    SUCCEEDED(customize->QueryInterface(IID_PPV_ARGS(&fileDialogWindow))) &&
                    fileDialogWindow &&
                    SUCCEEDED(fileDialogWindow->GetWindow(&dialogOwner)) &&
                    dialogOwner)
                {
                    owner = dialogOwner;
                }

                // Cancel in the settings dialog only closes that settings dialog.
                // It does not cancel file selection. This is less surprising than
                // closing the whole import operation from a secondary options popup.
                EpubImportOptions edited = *m_options;
                if (ShowImportOptionsDialog(owner, edited))
                {
                    *m_options = edited;
                    SaveImportOptions(*m_options);
                }

                // Some Windows builds reset the standard OK button text after
                // a nested modal dialog is opened from a custom file-dialog
                // control. Restore the standard open-style caption immediately.
                if (customize)
                {
                    CComPtr<IFileDialog> fileDialog;
                    if (SUCCEEDED(customize->QueryInterface(IID_PPV_ARGS(&fileDialog))) && fileDialog)
                        fileDialog->SetOkButtonLabel(
                            LoadPluginString(IDS_IMPORT_PLUGIN_FILEDLG_IMPORT_BUTTON, L"Import"));
                }
            }
            return S_OK;
        }

        STDMETHOD(OnCheckButtonToggled)(IFileDialogCustomize*, DWORD, BOOL)
        {
            return S_OK;
        }

        STDMETHOD(OnControlActivating)(IFileDialogCustomize*, DWORD)
        {
            return S_OK;
        }

    private:
        HWND m_owner;
        EpubImportOptions* m_options;
    };

    bool SelectEpubFileLegacy(HWND owner, CStringW& outPath, EpubImportOptions& options)
    {
        wchar_t buffer[MAX_PATH] = L"";

        OPENFILENAMEW ofn = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = owner;
        const CString epubFilter = LoadPluginString(IDS_IMPORT_PLUGIN_FILEDLG_FILTER_EPUB, L"EPUB files (*.epub)");
        const CString allFilter = LoadPluginString(IDS_IMPORT_PLUGIN_FILEDLG_FILTER_ALL, L"All files (*.*)");
        CString filter = epubFilter; filter.AppendChar(L'\0'); filter += L"*.epub"; filter.AppendChar(L'\0');
        filter += allFilter; filter.AppendChar(L'\0'); filter += L"*.*"; filter.AppendChar(L'\0'); filter.AppendChar(L'\0');
        ofn.lpstrFilter = filter;
        ofn.lpstrFile = buffer;
        ofn.nMaxFile = _countof(buffer);
        ofn.lpstrDefExt = L"epub";
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;

        if (!GetOpenFileNameW(&ofn))
            return false;

        // Fallback for very old systems where IFileOpenDialog is unavailable:
        // the settings dialog is still available, but it cannot be embedded into
        // the legacy GetOpenFileName dialog.
        EpubImportOptions edited = options;
        if (ShowImportOptionsDialog(owner, edited))
        {
            options = edited;
            SaveImportOptions(options);
        }

        outPath = buffer;
        return true;
    }

    bool SelectEpubFile(HWND owner, CStringW& outPath, EpubImportOptions& options)
    {
        outPath.Empty();

        const std::wstring epubFilter = LoadPluginString(IDS_IMPORT_PLUGIN_FILEDLG_FILTER_EPUB, L"EPUB files (*.epub)").GetString();
        const std::wstring allFilter = LoadPluginString(IDS_IMPORT_PLUGIN_FILEDLG_FILTER_ALL, L"All files (*.*)").GetString();
        const COMDLG_FILTERSPEC filters[] = { { epubFilter.c_str(), L"*.epub" }, { allFilter.c_str(), L"*.*" } };
        CComObject<COpenDialogEvents>* rawEvents = nullptr;
        HRESULT hr = CComObject<COpenDialogEvents>::CreateInstance(&rawEvents);
        if (FAILED(hr) || !rawEvents)
            return SelectEpubFileLegacy(owner, outPath, options);

        rawEvents->AddRef();
        rawEvents->Init(owner, &options);

        CComPtr<IFileDialogEvents> events;
        hr = rawEvents->QueryInterface(IID_PPV_ARGS(&events));
        rawEvents->Release();
        if (FAILED(hr))
            return SelectEpubFileLegacy(owner, outPath, options);

        ModernFileDialog::Request request;
        request.fileMustExist = true;
        request.pathMustExist = true;
        request.title = LoadPluginString(IDS_IMPORT_PLUGIN_FILEDLG_TITLE, L"Import EPUB").GetString();
        request.okButtonLabel = LoadPluginString(IDS_IMPORT_PLUGIN_FILEDLG_IMPORT_BUTTON, L"Import").GetString();
        request.fileNameLabel = LoadPluginString(IDS_IMPORT_PLUGIN_FILEDLG_FILE_LABEL, L"EPUB file:").GetString();
        request.defaultExtension = L"epub";
        request.filters = filters;
        request.filterCount = _countof(filters);
        request.filterIndex = 1;
        request.events = events;
        request.customize = [](IFileDialogCustomize* customize) {
            return customize->AddPushButton(IDC_FILEDLG_SETTINGS_BUTTON,
                LoadPluginString(IDS_IMPORT_PLUGIN_FILEDLG_SETTINGS_BUTTON, L"Import settings..."));
        };
        const ModernFileDialog::Result result = ModernFileDialog::Show(owner, request);
        if (result.outcome == ModernFileDialog::Outcome::Cancelled)
            return false;
        if (result.outcome != ModernFileDialog::Outcome::Accepted)
            return SelectEpubFileLegacy(owner, outPath, options);
        outPath = result.paths.front().c_str();
        return !outPath.IsEmpty();
    }

    CStringW BuildDiagnosticFb2Xml(const CStringW& sourcePath, const CStringW& errorText)
    {
        const CStringW bookTitle = FileNameOnly(sourcePath);

        CStringW xml;
        xml += L"<?xml version=\"1.0\"?>\r\n";
        xml += L"<FictionBook xmlns=\"http://www.gribuser.ru/xml/fictionbook/2.0\" ";
        xml += L"xmlns:l=\"http://www.w3.org/1999/xlink\">\r\n";
        xml += L"  <description>\r\n";
        xml += L"    <title-info>\r\n";
        xml += L"      <genre>prose_contemporary</genre>\r\n";
        xml += L"      <author><first-name></first-name><last-name></last-name></author>\r\n";
        xml += L"      <book-title>" + XmlEscape(bookTitle) + L"</book-title>\r\n";
        xml += L"      <annotation>\r\n";
        xml += L"        <p>EPUB could not be imported automatically.</p>\r\n";
        xml += L"        <p>Diagnostic information is shown below.</p>\r\n";
        xml += L"      </annotation>\r\n";
        xml += L"      <lang>ru</lang>\r\n";
        xml += L"    </title-info>\r\n";
        xml += L"    <document-info>\r\n";
        xml += L"      <author><nickname>ImportEPUB skeleton</nickname></author>\r\n";
        xml += L"      <program-used>FBE ImportEPUB skeleton</program-used>\r\n";
        xml += L"      <date></date>\r\n";
        xml += L"      <id>00000000-0000-0000-0000-000000000000</id>\r\n";
        xml += L"      <version>1.0</version>\r\n";
        xml += L"    </document-info>\r\n";
        xml += L"  </description>\r\n";
        xml += L"  <body>\r\n";
        xml += L"    <section>\r\n";
        xml += L"      <title><p>EPUB import</p></title>\r\n";
        xml += L"      <p>The COM plugin is connected correctly, but parsing the selected EPUB failed.</p>\r\n";
        xml += L"      <p>Source file: " + XmlEscape(sourcePath) + L"</p>\r\n";
        xml += L"      <p>Error: " + XmlEscape(errorText) + L"</p>\r\n";
        xml += L"    </section>\r\n";
        xml += L"  </body>\r\n";
        xml += L"</FictionBook>\r\n";
        return xml;
    }

    HRESULT CreateFb2Dom(const CStringW& fb2Xml, IDispatch** document)
    {
        if (!document)
            return E_POINTER;
        *document = nullptr;

        CComPtr<MSXML2::IXMLDOMDocument2> dom;
        // Use the same ProgID style as FBE itself (Msxml2.DOMDocument.6.0).
        // This also gives clearer diagnostics if the MSXML6 32-bit COM class is
        // missing or damaged on the target system.
        HRESULT hr = dom.CoCreateInstance(L"Msxml2.DOMDocument.6.0");
        if (FAILED(hr))
        {
            CStringW msg;
            msg.Format(
                LoadPluginString(
                    IDS_IMPORT_PLUGIN_ERROR_MSXML_CREATE,
                    L"ImportEPUB: failed to create MSXML DOMDocument.6.0.\nHRESULT: 0x%08X"),
                static_cast<unsigned int>(hr));
            ::MessageBoxW(nullptr, msg, L"ImportEPUB", MB_OK | MB_ICONERROR);
            return hr;
        }

        dom->put_async(VARIANT_FALSE);
        dom->put_validateOnParse(VARIANT_FALSE);
        dom->put_resolveExternals(VARIANT_FALSE);
        dom->put_preserveWhiteSpace(VARIANT_TRUE);

        CComBSTR xmlForMsxml(static_cast<LPCWSTR>(fb2Xml));
        VARIANT_BOOL loaded = VARIANT_FALSE;
        HRESULT loadHr = dom->loadXML(xmlForMsxml, &loaded);
        if (FAILED(loadHr) || loaded != VARIANT_TRUE)
        {
            ::MessageBoxW(
                nullptr,
                LoadPluginString(
                    IDS_IMPORT_PLUGIN_ERROR_MSXML_LOAD,
                    L"ImportEPUB: MSXML could not load the generated test FB2 XML."),
                L"ImportEPUB",
                MB_OK | MB_ICONERROR);
            return E_FAIL;
        }

        CComPtr<IDispatch> dispatch;
        hr = dom.QueryInterface(&dispatch);
        if (FAILED(hr))
            return hr;

        *document = dispatch.Detach();
        return S_OK;
    }
}

STDMETHODIMP CImportEPUBPlugin::Import(long hWnd, BSTR* filename, IDispatch** document)
{
    InitImportEpubRuntimeStrings();

    if (!filename || !document)
        return E_POINTER;

    *filename = nullptr;
    *document = nullptr;

    CStringW stage = LoadPluginString(IDS_IMPORT_PLUGIN_STAGE_PREPARE, L"preparing EPUB import");

    try
    {
        CStringW epubPath;

        // The original FBE import interface passes HWND as "long".
        // This is safe for the classic Win32 FBE build, but in a hypothetical
        // x64 host the handle would be truncated before it reaches the plugin.
        // Therefore the x64 build deliberately opens the dialog without an owner
        // window instead of trying to reconstruct a broken HWND value.
#if defined(_WIN64)
        UNREFERENCED_PARAMETER(hWnd);
        HWND ownerWindow = nullptr;
#else
        HWND ownerWindow = reinterpret_cast<HWND>(static_cast<LONG_PTR>(hWnd));
#endif

        EpubImportOptions options;
        stage = LoadPluginString(IDS_IMPORT_PLUGIN_STAGE_READ_SETTINGS, L"reading ImportEPUB settings");
        LoadImportOptions(options);
        stage = LoadPluginString(IDS_IMPORT_PLUGIN_STAGE_SELECT_FILE, L"selecting EPUB file");
        if (!SelectEpubFile(ownerWindow, epubPath, options))
            return S_FALSE;

        const CStringW fb2Path = ChangeExtensionToFb2(epubPath);

        CStringW fb2Xml;
        CStringW importError;
        stage = LoadPluginString(IDS_IMPORT_PLUGIN_STAGE_CONVERT, L"converting EPUB to FB2");
        if (!BuildFb2XmlFromEpub(epubPath, options, fb2Xml, importError))
        {
            CStringW msg;
            msg.Format(
                LoadPluginString(
                    IDS_IMPORT_PLUGIN_WARNING_PARTIAL_IMPORT,
                    L"EPUB could not be fully imported yet.\n\n%s\n\nA diagnostic FB2 document will be opened."),
                importError.GetString());
            ::MessageBoxW(ownerWindow, msg, L"ImportEPUB", MB_OK | MB_ICONWARNING);
            fb2Xml = BuildDiagnosticFb2Xml(epubPath, importError);
        }

        stage = LoadPluginString(IDS_IMPORT_PLUGIN_STAGE_CREATE_DOM, L"creating FB2 DOM document");
        HRESULT hr = CreateFb2Dom(fb2Xml, document);
        if (FAILED(hr))
            return hr;

        stage = LoadPluginString(IDS_IMPORT_PLUGIN_STAGE_RETURN_RESULT, L"returning import result to FBE");
        *filename = ::SysAllocString(fb2Path);
        if (!*filename)
        {
            if (*document)
            {
                (*document)->Release();
                *document = nullptr;
            }
            return E_OUTOFMEMORY;
        }

        return S_OK;
    }
    catch (const _com_error& e)
    {
        CStringW message;
        message.Format(
            LoadPluginString(
                IDS_IMPORT_PLUGIN_ERROR_COM,
                L"ImportEPUB stopped the import because of a COM error.\n\nStage: %s\nHRESULT: 0x%08X"),
            stage.GetString(),
            static_cast<unsigned int>(e.Error()));
        ::MessageBoxW(nullptr, message, L"ImportEPUB", MB_OK | MB_ICONERROR);
        return S_FALSE;
    }
    catch (...)
    {
        CStringW message;
        message.Format(
            LoadPluginString(
                IDS_IMPORT_PLUGIN_ERROR_UNEXPECTED,
                L"ImportEPUB stopped the import because of an unexpected error.\n\nStage: %s"),
            stage.GetString());
        ::MessageBoxW(nullptr, message, L"ImportEPUB", MB_OK | MB_ICONERROR);
        return S_FALSE;
    }
}
