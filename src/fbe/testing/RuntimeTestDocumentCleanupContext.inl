        const bool cleanupContext=::GetEnvironmentVariableW(L"FBE_NEXT_TEST_CLEANUP_CONTEXT",nullptr,0)>0;
        bool cleanupCorrect=true,cleanupThrowScript=false,cleanupThrowNative=false,cleanupUnwind=false;
        unsigned cleanupBooks=0,cleanupClosed=0,cleanupScriptFailures=0,cleanupNativeFailures=0,cleanupAttempts=0;
        if(cleanupContext) {
            wchar_t path[32768]={};::GetEnvironmentVariableW(L"FBE_NEXT_TEST_CLEANUP_SCRIPT",path,_countof(path));
            for(int i=0;i<2;++i) {
                auto descriptor=std::make_unique<CElementDescriptor>();
                require(descriptor->Load(path),"real script descriptor loaded");
                _EDMnr.AddElementDescriptor(descriptor.get());cleanupGuard.descriptors.push_back(std::move(descriptor));
            }
            DocumentCleanupRuntimeProbe::OnScript()=[&](LPCWSTR className,bool completed,HRESULT hr) {
                if(wcscmp(className,L"cleanup_context_probe")) return;
                if(!completed) {
                    ++cleanupAttempts;
                    if(cleanupThrowNative&&cleanupAttempts==1) {++cleanupNativeFailures;CheckError(E_ABORT);}
                } else {
                    cleanupCorrect=cleanupCorrect&&FB::Doc::m_active_doc==m_doc;
                    if(FAILED(hr)) {++cleanupScriptFailures;cleanupCorrect=cleanupCorrect&&cleanupThrowScript;}
                }
            };
            DocumentCleanupRuntimeProbe::OnDetach()=[&](FB::Doc* old,FB::Doc* next,bool released) {
                if(cleanupUnwind&&!released) CheckError(E_ABORT);
                const bool open=m_document_session.HasDocument();
                cleanupCorrect=cleanupCorrect&&FB::Doc::m_active_doc==(open?old:nullptr);
                if(!released) {
                    cleanupAttempts=0;
                    // Install independently in both hosts. The executed JScript
                    // records its own document identity, not the C++ caller's.
                    for(FB::Doc* host:{old,next}) {
                        CString identity;identity.Format(L"book-%p",static_cast<void*>(host));
                        CString script;script.Format(
                            L"document.body.setAttribute('data-cleanup-book','%s');"
                            L"document.body.setAttribute('data-cleanup-seen','');"
                            L"document.body.setAttribute('data-cleanup-calls','0');"
                            L"window.fbeNextFaultInjection='%s';"
                            L"if(!window.cleanupOriginal)window.cleanupOriginal=apiCleanUp;"
                            L"apiCleanUp=function(c){if(c!='cleanup_context_probe')return window.cleanupOriginal(c);"
                            L"document.body.setAttribute('data-cleanup-seen',document.body.getAttribute('data-cleanup-book'));"
                            L"document.body.setAttribute('data-cleanup-calls',String(Number(document.body.getAttribute('data-cleanup-calls'))+1));"
                            L"%s return window.cleanupOriginal(c);};",
                            identity.GetString(),cleanupThrowScript?L"cleanup-regression":L"",
                            cleanupThrowScript?L"throw new Error('cleanup regression');":L"");
                        CheckError(host->m_body.Document()->parentWindow->execScript(_bstr_t(script),L"JScript"));
                    }
                } else {
                    auto attribute=[](FB::Doc* host,LPCWSTR name){return CString(static_cast<LPCWSTR>(_bstr_t(host->m_body.Document()->body->getAttribute(_bstr_t(name),0))));};
                    cleanupCorrect=cleanupCorrect&&attribute(next,L"data-cleanup-calls")==L"0";
                    if(open) {
                        ++cleanupBooks;
                        cleanupCorrect=cleanupCorrect&&cleanupAttempts==2&&
                            attribute(old,L"data-cleanup-seen")==attribute(old,L"data-cleanup-book")&&
                            attribute(old,L"data-cleanup-calls")== (cleanupThrowNative?L"1":L"2");
                    } else {++cleanupClosed;cleanupCorrect=cleanupCorrect&&cleanupAttempts==0&&attribute(old,L"data-cleanup-calls")==L"0";}
                    cleanupCorrect=cleanupCorrect&&!NewDocumentRuntimeProbe::References(m_Speller)&&
                        !m_editor_selection_state.BodyRange()&&!m_editor_selection_state.DescriptionRange()&&
                        !m_selection_context.container&&!m_find_results_pane.AttachedView()&&m_document_tree.m_tree.m_tree.GetCount()==0;
                }
            };
        }
