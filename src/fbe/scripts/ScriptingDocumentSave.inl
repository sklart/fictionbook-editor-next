        if (request->scriptingMethod >= 166 && request->scriptingMethod <= 168) {
            try {
                FbeScripts::DocumentSave::CheckContext(m_doc);
                static thread_local bool saving = false;
                if (saving) CheckError(E_UNEXPECTED);
                struct Guard {bool& flag;Guard(bool& value):flag(value){flag=true;}~Guard(){flag=false;}} guard(saving);
                const DISPID method = request->scriptingMethod;
                FbeScripts::DocumentSave::Options options;
                CString path;
                if (method != 167) {
                    path = FbeScripts::DocumentSave::Path(request->arguments[0].bstrVal);
                    if (FbeScripts::DocumentSave::SameFile(path, m_document_session.Location().storagePath) ||
                        FbeScripts::DocumentSave::SameFile(path, m_doc->m_filename)) CheckError(E_ACCESSDENIED);
                    if (method == 168) options = FbeScripts::DocumentSave::Parse(request->arguments[1].bstrVal);
                    else options.overwrite = request->arguments[1].boolVal != VARIANT_FALSE;
                }
                const FictionBookFileType target = options.format == L"current" ? m_doc->GetDocumentFileType() :
                    (options.format == L"fb2" ? FictionBookFileType::Fb2 : FictionBookFileType::Fbd);
                if (method != 167 && DetectFictionBookFileType(path) != target) CheckError(E_INVALIDARG);
                CString encoding = options.encoding == L"utf-8" ? L"utf-8" : m_doc->m_encoding;
                CString message;
                MSXML2::IXMLDOMDocument2Ptr snapshot;
                if (IsSourceActive()) {
                    SourceDocumentText source;
                    if (SourceDocumentTransfer::ReadSourceText(m_source, source) != SourceTransitionResult::Success) CheckError(E_FAIL);
                    if (std::find(source.utf8.begin(), source.utf8.end()-1, '\0') != source.utf8.end()-1) CheckError(E_INVALIDARG);
                    if (!source.utf8.empty() && !::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, source.utf8.data(),
                        static_cast<int>(source.utf8.size()-1), NULL, 0)) CheckError(E_INVALIDARG);
                    snapshot = U::CreateDocument(false);
                    snapshot->setProperty(L"ProhibitDTD", _variant_t(true));
                    snapshot->resolveExternals = VARIANT_FALSE;
                    if (snapshot->loadXML(_bstr_t(source.text)) != VARIANT_TRUE) CheckError(E_INVALIDARG);
                } else snapshot = m_doc->CreateScriptingSnapshot(encoding, message);
                if (!snapshot) CheckError(E_FAIL);
                // Only detached XML is changed, including an explicit declaration.
                const auto first = snapshot->firstChild;
                if (first && first->nodeType == MSXML2::NODE_PROCESSING_INSTRUCTION && !wcscmp(first->nodeName, L"xml")) snapshot->removeChild(first);
                const auto declaration = snapshot->createProcessingInstruction(L"xml", _bstr_t(L"version=\"1.0\" encoding=\"" + encoding + L"\""));
                snapshot->insertBefore(declaration, _variant_t(static_cast<IDispatch*>(snapshot->firstChild.GetInterfacePtr())));
                const _bstr_t xml = snapshot->xml;
                bool valid = false;int line = 0, column = 0;
                CheckError(m_doc->ValidateForScripting(xml, valid, line, column, message, target));
                if (!valid) CheckError(E_INVALIDARG);
                if (method == 167) {
                    if (xml.length() > 4U * 1024 * 1024) CheckError(HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW));
                    request->returnValue = static_cast<LPCWSTR>(xml);
                } else {
                    DocumentSaveController::WriteSnapshot(FbeScripts::DocumentSave::Bytes(snapshot), path, options.overwrite);
                    request->returnValue = true;
                }
                request->succeeded = true;
            } catch (const _com_error& error) {request->error = error.Error();}
            catch (const std::bad_alloc&) {request->error = E_OUTOFMEMORY;}
            catch (const CAtlException& error) {request->error = error.m_hr;}
            return 1;
        }
