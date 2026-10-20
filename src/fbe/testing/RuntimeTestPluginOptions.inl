if(IsFbeTestScenario(L"plugin-options-tabs-runtime"))
{
    CStringA report;
    bool passed = false;
    wchar_t captures[32768] = {};
    ::GetEnvironmentVariableW(L"FBE_NEXT_TEST_PLUGIN_CAPTURES", captures, _countof(captures));
    try {
        struct RegistryFixture {
            CRegKey key;
            std::wstring path;
            bool overridden = false;
            void Open() {
                if(!DeploymentContext::RegistryPersistenceAllowed()) return;
                GUID id = {}; CheckError(::CoCreateGuid(&id));
                wchar_t text[40] = {}; ::StringFromGUID2(id, text, _countof(text));
                path = L"Software\\FBEPluginTabsTests\\" + std::wstring(text);
                CheckError(HRESULT_FROM_WIN32(key.Create(HKEY_CURRENT_USER, path.c_str())));
                CheckError(HRESULT_FROM_WIN32(::RegOverridePredefKey(HKEY_CURRENT_USER, key)));
                overridden = true;
            }
            ~RegistryFixture() {
                if(overridden) ::RegOverridePredefKey(HKEY_CURRENT_USER, NULL);
                key.Close();
                if(!path.empty()) ::RegDeleteTreeW(HKEY_CURRENT_USER, path.c_str());
            }
        } registryFixture;
        registryFixture.Open();
        ShowWindow(SW_SHOWNORMAL); UpdateWindow();
        const CString xmlBefore(static_cast<LPCWSTR>(m_doc->CreateDOMForAnalysis()->xml));
        const bool dirtyBefore = m_doc->DocChanged();
        ::SetEnvironmentVariableW(L"FBE_NEXT_TEST_PLUGIN_SETTINGS", L"1");
        for(InterfaceTheme theme : {INTERFACE_THEME_LIGHT, INTERFACE_THEME_DARK}) {
            ThemeManager::SetSelectedTheme(theme); ThemeManager::ApplyToWindow(m_hWnd);
            for(LPCWSTR locale : {L"ru-RU", L"de-DE"}) {
                ::SetEnvironmentVariableW(L"FBE_NEXT_UI_LOCALE", locale);
                for(int index : {0, 1, 2, 3}) {
                    if(index < 2 && (theme != INTERFACE_THEME_LIGHT || wcscmp(locale, L"ru-RU") != 0)) continue;
                    CLSID clsid = {};
                    const wchar_t* ids[] = {L"{C3098839-EF69-4DE5-B27D-1E80051CA843}", L"{09B5ABFF-177E-4C03-98D0-9EF4E1C9DB56}", L"{3C19F5A2-2EC8-4EC7-B7A9-F4910B4CDD82}", L"{36FCFB2D-C3D8-4B81-ABC1-5A09CA846515}"};
                    CheckError(::CLSIDFromString(ids[index], &clsid));
                    IUnknownPtr instance; CheckError(m_plugins.Manager().CreateInstance(clsid, instance));
                    CComPtr<IFBEPluginHost> host; CheckError(FbePluginApiV2::CreateHost(m_hWnd, locale, &host));
                    const wchar_t* names[] = {L"html", L"docx", L"import", L"export"};
                    const wchar_t* profiles[] = {L"export-html.json", L"export-docx.json", L"import-epub.json", L"export-epub.json"};
                    const std::wstring name = std::wstring(names[index]) + L"-" + locale + (theme == INTERFACE_THEME_DARK ? L"-dark" : L"-light");
                    const std::wstring profile = DeploymentContext::SettingsDirectory() + L"Plugins\\" + profiles[index];
                    PluginOptionsProbe::Start(index, PluginOptionsProbe::Action::Inspect, captures, name, profile);
                    if(index >= 2) {
                        std::map<std::wstring, CStringW> translations;
                        FbeRuntimeLocalization::LoadRuntimeStringFiles(::GetModuleHandleW(NULL), index == 2 ? L"import-epub.json" : L"export-epub.json", translations);
                        PluginOptionsProbe::expectedTitle = translations.at(index == 2 ? L"import_epub.options.title" : L"export_epub.options.dialog_title").GetString();
                        if(index == 2) PluginOptionsProbe::expectedConfirmText = translations.at(L"import_epub.options.button_ok").GetString();
                        PluginOptionsProbe::expectedDarkTabs = theme == INTERFACE_THEME_DARK;
                    }
                    const UINT_PTR timer = ::SetTimer(NULL, 0, 80, PluginOptionsProbe::Timer); if(!timer) CheckError(E_FAIL);
                    HRESULT result = E_NOINTERFACE;
                    if(index == 2) {
                        CComQIPtr<IFBEImportPlugin2> importer(instance); CComBSTR fileName; CComPtr<IStream> stream;
                        if(importer) result = importer->Import(host, &fileName, &stream);
                    } else {
                        CComQIPtr<IFBEExportPlugin2> exporter(instance); CComPtr<IFBEDocumentSnapshot> snapshot;
                        CheckError(FbePluginApiV2::CreateSnapshot(m_doc->CreateDOMForAnalysis(), L"fixture.fb2", L"utf-8", &snapshot));
                        if(exporter) result = exporter->Export(host, CComBSTR(L"fixture.fb2"), snapshot);
                    }
                    ::KillTimer(NULL, timer);
                    PluginOptionsProbe::Stop();
                    report.AppendFormat("dialog=%s;hresult=0x%08lX;pages=%d;failed_line=%d\n", CStringA(name.c_str()).GetString(),
                        static_cast<unsigned long>(result), PluginOptionsProbe::pages, PluginOptionsProbe::failedLine);
                    if(!PluginOptionsProbe::errorText.empty()) report.AppendFormat("detail=%s\n", CStringA(PluginOptionsProbe::errorText.c_str()).GetString());
                    CheckError(result); if(!PluginOptionsProbe::Passed()) CheckError(E_FAIL);
                }
            }
        }
        if(xmlBefore != CString(static_cast<LPCWSTR>(m_doc->CreateDOMForAnalysis()->xml)) || dirtyBefore != m_doc->DocChanged()) CheckError(E_FAIL);
        report += "all_tabs=pass\nkeyboard=pass\ncontrolled_dpi_96_120_144_192=pass\nlight_dark=pass\nru_de_captions=pass\ndocument_preserved=pass\n";
        passed = true;
    } catch(const _com_error& error) { report.AppendFormat("hresult=0x%08lX\n", static_cast<unsigned long>(error.Error())); }
      catch(...) { report += "exception=1\n"; }
    report.AppendFormat("native_dpi=%u\nhigh_contrast=%s\nresult=%s\n", UiMetrics::DpiForWindow(m_hWnd),
        ThemeManager::IsHighContrast() ? "RUN" : "NOT RUN", passed ? "pass" : "fail");
    DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written);
    output.Close(); ::PostQuitMessage(passed ? 0 : 1); return 0;
}
