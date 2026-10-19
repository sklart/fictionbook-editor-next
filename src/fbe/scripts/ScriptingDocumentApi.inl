        try {
            if (!m_doc) CheckError(E_UNEXPECTED);
            const DISPID method = request->scriptingMethod;
            const auto& args = request->arguments;
            const bool source = IsSourceActive();
            const CString view = source ? L"source" : (m_editor_view_state.Current() == DESC ? L"description" : L"design");
            auto document = m_doc->m_body.Document();
            CComPtr<IUnknown> identity; CheckError(document->QueryInterface(IID_PPV_ARGS(&identity)));
            MSHTML::IHTMLDocument3Ptr document3(document);
            MSHTML::IHTMLElementPtr root(document3->getElementById(L"fbw_body"));
            CComPtr<IUnknown> rootIdentity;
            if (root) CheckError(root->QueryInterface(IID_PPV_ARGS(&rootIdentity)));
            auto& resources = FbeScripts::DocumentResources();
            if (method == 119) {
                if (resources.bookmarks.size() >= 128) CheckError(E_OUTOFMEMORY);
                FbeScripts::SelectionBookmark mark; mark.token = FbeScripts::ResourceToken(); mark.view = view;
                mark.document = identity; mark.root = rootIdentity; mark.revision = m_doc->m_body.GetVersionNumber();
                if (source) {
                    mark.anchor = m_source.SendMessage(SCI_GETANCHOR); mark.caret = m_source.SendMessage(SCI_GETCURRENTPOS);
                    mark.source = FbeScripts::SourceBytes(m_source);
                    size_t total = mark.source.size();
                    for (const auto& saved : resources.bookmarks) total += saved.source.size();
                    if (total > 64 * 1024 * 1024) CheckError(E_OUTOFMEMORY);
                } else {
                    IDispatchPtr selected(document->selection->createRange());
                    MSHTML::IHTMLTxtRangePtr text(selected);
                    if (text) mark.range = text->duplicate();
                    else { MSHTML::IHTMLControlRangePtr controls(selected); if (!controls) CheckError(E_NOTIMPL); mark.controls = controls; }
                }
                request->returnValue = mark.token; resources.bookmarks.push_back(std::move(mark));
            } else if (method == 120 || method == 121) {
                auto found = std::find_if(resources.bookmarks.begin(), resources.bookmarks.end(), [&](const auto& mark) { return mark.token == args[0].bstrVal; });
                bool valid = found != resources.bookmarks.end();
                if (valid && method == 120) {
                    valid = found->document == identity && found->root == rootIdentity && found->view == view &&
                        found->revision == m_doc->m_body.GetVersionNumber();
                    if (valid && source) valid = found->source == FbeScripts::SourceBytes(m_source);
                    if (valid) {
                        if (source) m_source.SendMessage(SCI_SETSEL, found->anchor, found->caret);
                        else if (found->range) found->range->select(); else found->controls->select();
                    } else resources.bookmarks.erase(found);
                } else if (valid) resources.bookmarks.erase(found);
                request->returnValue = valid;
            } else if (method >= 122 && method <= 124) {
                CString text, html, type; long count = 0;
                if (source) {
                    const auto bytes = FbeScripts::SourceBytes(m_source);
                    const auto start = m_source.SendMessage(SCI_GETSELECTIONSTART), end = m_source.SendMessage(SCI_GETSELECTIONEND);
                    text = FbeScripts::FromUtf8(bytes.substr(static_cast<size_t>(start), static_cast<size_t>(end - start)));
                    html = FbeScripts::EscapeHtml(text); type = text.IsEmpty() ? L"none" : L"text";
                } else {
                    type = static_cast<LPCWSTR>(document->selection->type); type.MakeLower();
                    IDispatchPtr selected(document->selection->createRange()); MSHTML::IHTMLTxtRangePtr range(selected);
                    if (range) { text = static_cast<LPCWSTR>(range->text); html = static_cast<LPCWSTR>(range->htmlText); }
                    else {
                        MSHTML::IHTMLControlRangePtr controls(selected);
                        if (controls) { count = controls->length; for (long index = 0; index < count; ++index) html += static_cast<LPCWSTR>(controls->item(index)->outerHTML); }
                    }
                }
                if (method == 122 || method == 123) {
                    request->returnValue.vt = VT_BSTR;
                    request->returnValue.bstrVal = (method == 122 ? text : html).AllocSysString();
                    if (!request->returnValue.bstrVal) CheckError(E_OUTOFMEMORY);
                }
                else {
                    CString json; json.Format(L"{\"schemaVersion\":1,\"view\":\"%s\",\"type\":\"%s\",\"textLength\":%d,\"controlCount\":%ld}", static_cast<LPCWSTR>(view), static_cast<LPCWSTR>(type), text.GetLength(), count);
                    request->returnValue = json;
                }
            } else if (method == 125) {
                bool suspendRedraw = false;
                const std::wstring options(args[1].bstrVal, ::SysStringLen(args[1].bstrVal)); size_t cursor = 0;
                using namespace FbeRuntimeLocalization;
                JsonSkipWhitespace(options, cursor);
                if (cursor >= options.size() || options[cursor++] != L'{') CheckError(E_INVALIDARG);
                JsonSkipWhitespace(options, cursor);
                if (cursor < options.size() && options[cursor] != L'}') {
                    std::wstring key; if (!JsonParseString(options, cursor, key) || key != L"suspendRedraw") CheckError(E_INVALIDARG);
                    JsonSkipWhitespace(options, cursor); if (cursor >= options.size() || options[cursor++] != L':') CheckError(E_INVALIDARG);
                    JsonSkipWhitespace(options, cursor);
                    if (options.compare(cursor, 4, L"true") == 0) { suspendRedraw = true; cursor += 4; }
                    else if (options.compare(cursor, 5, L"false") == 0) cursor += 5; else CheckError(E_INVALIDARG);
                    JsonSkipWhitespace(options, cursor);
                }
                if (cursor >= options.size() || options[cursor++] != L'}') CheckError(E_INVALIDARG);
                JsonSkipWhitespace(options, cursor); if (cursor != options.size() || resources.updates.size() >= 64) CheckError(E_INVALIDARG);
                auto update = std::make_unique<FbeScripts::DocumentUpdate>(m_doc->m_body.MarkupServices(),
                    source ? m_source.m_hWnd : m_doc->m_body.m_hWnd, source, args[0].bstrVal, suspendRedraw);
                request->returnValue = update->token; resources.updates.push_back(std::move(update));
            } else if (method == 126) {
                if (resources.updates.empty() || resources.updates.back()->token != args[0].bstrVal) CheckError(E_INVALIDARG);
                resources.updates.back()->Close(); resources.updates.pop_back(); request->returnValue = true;
            } else CheckError(DISP_E_MEMBERNOTFOUND);
            request->succeeded = true;
        } catch (const _com_error& error) { request->error = error.Error(); }
        catch (const std::bad_alloc&) { request->error = E_OUTOFMEMORY; }
        return 1;
