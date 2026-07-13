#include "stdafx.h"
#include "MsxmlImport.h"
#include "ImportEPUBPlugin.h"
#include "ImportEPUBGuids.h"
#include "EpubImport.h"
#include "ImportOptionsDialog.h"
#include "resource.h"
#include "RuntimeLocalization.h"

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
                HWND owner = GetActiveWindow();
                if (!owner)
                    owner = m_owner;

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
                            LoadPluginString(IDS_IMPORT_PLUGIN_FILEDLG_OPEN_BUTTON, L"Open..."));
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
        ofn.lpstrFilter = L"EPUB files (*.epub)\0*.epub\0All files (*.*)\0*.*\0";
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

        CComPtr<IFileOpenDialog> dialog;
        HRESULT hr = dialog.CoCreateInstance(CLSID_FileOpenDialog);
        if (FAILED(hr))
            return SelectEpubFileLegacy(owner, outPath, options);

        DWORD existingOptions = 0;
        if (SUCCEEDED(dialog->GetOptions(&existingOptions)))
        {
            dialog->SetOptions(existingOptions | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST | FOS_FORCEFILESYSTEM);
        }

        dialog->SetTitle(LoadPluginString(IDS_IMPORT_PLUGIN_FILEDLG_TITLE, L"Import EPUB"));
        dialog->SetOkButtonLabel(LoadPluginString(IDS_IMPORT_PLUGIN_FILEDLG_OPEN_BUTTON, L"Open..."));
        dialog->SetFileNameLabel(LoadPluginString(IDS_IMPORT_PLUGIN_FILEDLG_FILE_LABEL, L"EPUB file:"));
        dialog->SetDefaultExtension(L"epub");

        const COMDLG_FILTERSPEC filters[] =
        {
            { L"EPUB files (*.epub)", L"*.epub" },
            { L"All files (*.*)", L"*.*" }
        };
        dialog->SetFileTypes(static_cast<UINT>(_countof(filters)), filters);
        dialog->SetFileTypeIndex(1);

        CComPtr<IFileDialogCustomize> customize;
        if (SUCCEEDED(dialog->QueryInterface(IID_PPV_ARGS(&customize))))
        {
            // Keep the import options entry visually compact and consistent with
            // the export plugins: only one button, without an additional group
            // caption or explanatory text in the file picker itself. Detailed
            // explanations remain in tooltips inside the settings dialog.
            //
            // Note: the modern Windows IFileOpenDialog places custom controls in
            // its bottom custom-control area. The button is intentionally added
            // without StartVisualGroup/AddText so it does not create the extra
            // two-line block that was visible above the file name field.
            customize->AddPushButton(
                IDC_FILEDLG_SETTINGS_BUTTON,
                LoadPluginString(IDS_IMPORT_PLUGIN_FILEDLG_SETTINGS_BUTTON, L"Import settings..."));
        }

        CComObject<COpenDialogEvents>* rawEvents = nullptr;
        hr = CComObject<COpenDialogEvents>::CreateInstance(&rawEvents);
        if (FAILED(hr) || !rawEvents)
            return SelectEpubFileLegacy(owner, outPath, options);

        rawEvents->AddRef();
        rawEvents->Init(owner, &options);

        CComPtr<IFileDialogEvents> events;
        hr = rawEvents->QueryInterface(IID_PPV_ARGS(&events));
        rawEvents->Release();
        if (FAILED(hr))
            return SelectEpubFileLegacy(owner, outPath, options);

        DWORD cookie = 0;
        if (SUCCEEDED(dialog->Advise(events, &cookie)))
        {
            hr = dialog->Show(owner);
            dialog->Unadvise(cookie);
        }
        else
        {
            hr = dialog->Show(owner);
        }

        if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED))
            return false;
        if (FAILED(hr))
            return SelectEpubFileLegacy(owner, outPath, options);

        CComPtr<IShellItem> item;
        hr = dialog->GetResult(&item);
        if (FAILED(hr) || !item)
            return false;

        PWSTR filePath = nullptr;
        hr = item->GetDisplayName(SIGDN_FILESYSPATH, &filePath);
        if (FAILED(hr) || !filePath)
            return false;

        outPath = filePath;
        CoTaskMemFree(filePath);
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
