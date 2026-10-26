#ifdef FBE_BINARY_STORE_DIAGNOSTICS
namespace {
void C4Write(CAtlFile& output,const CStringA& text) { DWORD written=0; CheckError(output.Write(text,static_cast<DWORD>(text.GetLength()),&written)); }
void C4PayloadFile(const CString& path,const CComVariant& value,BYTE tail,bool binary=false) {
    FbeScripts::Binary::Bytes bytes(value.parray); const ULONG size=value.parray->rgsabound[0].cElements;
    CAtlFile file; CheckError(file.Create(path,GENERIC_WRITE,FILE_SHARE_READ,CREATE_ALWAYS));
    if(binary){BYTE zero[8]={};const ULONG prefix=size<8?size:8;CheckError(file.Write(zero,prefix));CheckError(file.Write(bytes.data+prefix,size-prefix));}else CheckError(file.Write(bytes.data,size));CheckError(file.Write(&tail,1));
}
}
__declspec(noinline) LRESULT CMainFrame::RunBinaryStoreC4(CAtlFile& output) {
    bool passed=false;CStringA details="start";
    auto require=[&](bool ok,const char* message){if(!ok){details=message;CheckError(E_FAIL);}};
    auto emit=[&](const char* stage){
        const auto s=m_doc->m_binaryStore.payloads.DiagnosticStatistics();
        const auto current=m_doc->m_binaryStore.DiagnosticCurrentPayloads();
        const auto c=FbeBinary::Diagnostics::Get();
        PROCESS_MEMORY_COUNTERS_EX memory={};memory.cb=sizeof(memory);require(::GetProcessMemoryInfo(::GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),sizeof(memory))!=FALSE,"memory counters");
        CStringA row;row.Format("stage=%s;binary_count=%llu;current_entries=%llu;unique_current_objects=%llu;canonical_bytes=%llu;history_entries=%llu;unique_history_objects=%llu;history_referenced_bytes=%llu;history_only_bytes=%llu;unique_payload_objects=%llu;unique_payload_bytes=%llu;unique_underlying_blocks=%llu;canonical_underlying_blocks=%llu;not_in_lease_history_bytes=%llu;current_document_bytes=%llu;current_document_blocks=%llu;noncurrent_owner_bytes=%llu;current_document_only_bytes=%llu;shared_references=%llu;metadata_entries=%llu;metadata_estimated_bytes=%llu;facade_materializations=%llu;temporary_get_bytes_total=%llu;put_materializations=%llu;put_copy_bytes_total=%llu;same_value_puts=%llu;behavior_attachments=%llu;attach_calls=%llu;adopt_calls=%llu;assign_calls=%llu;dom_enumerations=%llu;dom_enumerated_entries=%llu;reconcile_calls=%llu;metadata_invalidations=%llu;native_image_decoded_bytes_total=%llu;max_single_native_bitmap_bytes=%llu;native_image_decode_calls=%llu;private_bytes=%llu;working_set=%llu;peak_private_bytes=%llu;peak_working_set=%llu\r\n",stage,
            static_cast<unsigned long long>(m_doc->m_binaryStore.Count()),s.currentEntries,s.uniqueCurrentObjects,s.canonicalBytes,s.historyEntries,s.uniqueHistoryObjects,s.historyReferencedBytes,s.historyOnlyBytes,s.uniqueObjects,s.uniqueBytes,s.uniquePayloadBlocks,s.canonicalPayloadBlocks,s.uniqueBytes-s.historyReferencedBytes,current.canonicalBytes,current.uniquePayloadBlocks,s.uniqueBytes-current.canonicalBytes,current.canonicalBytes-current.historyReferencedBytes,s.sharedReferences,
            static_cast<unsigned long long>(m_doc->m_binaryStore.Count()),m_doc->m_binaryStore.DiagnosticEstimatedBytes(),
            c.facadeGets,c.facadeGetBytes,c.facadePuts,c.putCopyBytes,c.sameValuePuts,c.behaviorAttachments,c.attachCalls,c.adoptCalls,c.assignCalls,c.enumerationCalls,c.enumeratedEntries,c.reconcileCalls,c.metadataInvalidations,c.imageDecodedBitmapBytesTotal,c.maxSingleDecodedBitmapBytes,c.imageDecodeCalls,
            static_cast<unsigned long long>(memory.PrivateUsage),static_cast<unsigned long long>(memory.WorkingSetSize),static_cast<unsigned long long>(memory.PeakPagefileUsage),static_cast<unsigned long long>(memory.PeakWorkingSetSize));C4Write(output,row);
        const char* phases[]={"document_load","dom_creation","script_load","binary_discovery","store_population","behavior_attachment","reconcile","metadata_index","image_decode","serialization","save"};
        for(unsigned i=0;i<static_cast<unsigned>(FbeBinary::Diagnostics::Phase::Count);++i){row.Format("profile_stage=%s;phase=%s;calls=%llu;milliseconds=%.6f\r\n",stage,phases[i],c.phaseCalls[i],c.phaseMs[i]);C4Write(output,row);}
    };
    auto gc=[&](){CheckError(m_doc->m_body.Document()->parentWindow->execScript(_bstr_t(L"CollectGarbage();"),L"JScript"));};
    try {
        const CString original=m_doc->m_filename;
        wchar_t rootBuffer[32768]={};::GetEnvironmentVariableW(L"FBE_NEXT_SCRIPT_API_ROOT",rootBuffer,_countof(rootBuffer));const CString root(rootBuffer);
        emit("open_no_facade_access");
        {CStringA row;row.Format("observation=object_sizes;variant=%llu;shared_ptr=%llu;payload_state_including_diagnostic_cookie_map=%llu;metadata=%llu;behavior=%llu\r\n",static_cast<unsigned long long>(sizeof(CComVariant)),static_cast<unsigned long long>(sizeof(std::shared_ptr<const CComVariant>)),static_cast<unsigned long long>(sizeof(FbeBinary::BinaryPayloadStore::State)),static_cast<unsigned long long>(sizeof(FbeBinary::BinaryMetadata)),static_cast<unsigned long long>(FbeBinary::BinaryPayloadStore::DiagnosticBehaviorObjectSize()));C4Write(output,row);}
        {CComVariant phases; CComDispatchDriver script(m_doc->m_body.Document()->Script); const HRESULT hr=script.Invoke0(L"_c4_profile_json",&phases); if(SUCCEEDED(hr)&&phases.vt==VT_BSTR){CStringA row("js_profile=");row+=CStringA(phases.bstrVal);row+="\r\n";C4Write(output,row);}}
        auto objects=FbeScripts::Binary::Objects(m_doc->m_body.Document());require(!objects.empty(),"missing binaries");
        emit("after_dom_catalog_enumeration");
        const auto initial=m_doc->m_binaryStore.payloads.DiagnosticStatistics();
        require(initial.historyEntries==0,"unexpected history after Open");
        CStringA row;
        {CComVariant value;CComDispatchDriver driver(objects[0].element);CheckError(driver.GetPropertyByName(L"base64data",&value));require(value.vt==(VT_ARRAY|VT_UI1),"legacy variant type");require(value.parray!=m_doc->m_binaryStore.payloads.Snapshot(objects[0].element)->parray,"facade leaked canonical array");}
        emit("after_one_facade_read_released");
        for(unsigned pass=0;pass<5;++pass)for(const auto& object:objects){CComVariant value;CComDispatchDriver driver(object.element);CheckError(driver.GetPropertyByName(L"base64data",&value));}
        emit("after_five_full_facade_reads_released");
        // Exercise the actual JScript binding separately from native IDispatch.
        CheckError(m_doc->m_body.Document()->parentWindow->execScript(
            L"function _c4_read_first_only(){var nodes=document.all.binobj.getElementsByTagName('DIV');var data=nodes[0].base64data;return typeof data!='undefined'&&data!==null;} function _c4_read_all(){var nodes=document.all.binobj.getElementsByTagName('DIV');for(var pass=0;pass<5;pass++)for(var i=0;i<nodes.length;i++){var data=nodes[i].base64data;}}",L"JScript"));
        {CComDispatchDriver script(m_doc->m_body.Document()->Script);CComVariant result;CheckError(script.Invoke0(L"_c4_read_first_only",&result));}
        emit("after_js_one_read_returned");
        {CComDispatchDriver script(m_doc->m_body.Document()->Script);CheckError(script.Invoke0(L"_c4_read_all"));}
        emit("after_js_five_full_reads_returned");gc();emit("after_js_reads_gc");
        {
            CComPtr<IDispatch> retained(objects[0].element);const std::weak_ptr<const CComVariant> weak=m_doc->m_binaryStore.payloads.Snapshot(retained);
            require(m_doc->m_binaryStore.payloads.DiagnosticDetach(retained)==S_OK,"detach same DOM after actual JS reads");
            CComDispatchDriver script(m_doc->m_body.Document()->Script);CComVariant result;const auto getsBefore=FbeBinary::Diagnostics::Get().facadeGets;const HRESULT hr=script.Invoke0(L"_c4_read_first_only",&result);const auto getterCalls=FbeBinary::Diagnostics::Get().facadeGets-getsBefore;
            const bool survived=SUCCEEDED(hr)&&result.vt==VT_BOOL&&result.boolVal==VARIANT_TRUE;
            row.Format("observation=same_dom_js_binding_after_detach;js_hr=0x%08lX;usable_payload_survived=%u;canonical_expired=%u;getter_calls=%llu\r\n",static_cast<unsigned long>(hr),survived?1u:0u,weak.expired()?1u:0u,getterCalls);C4Write(output,row);
            require(!survived||getterCalls>0,"JScript binding cached a payload without calling its getter");
        }
        objects.clear();m_doc->MarkSavePoint();require(LoadFile(original)==OK,"reload after JS facade cache probe");
        objects=FbeScripts::Binary::Objects(m_doc->m_body.Document());
        unsigned long long backingAttributes=0;
        for(const auto& object:objects){MSHTML::IHTMLAttributeCollectionPtr attrs=MSHTML::IHTMLDOMNodePtr(object.element)->attributes;for(long i=0;i<attrs->length;++i){CComVariant index(i);MSHTML::IHTMLDOMAttributePtr attr(attrs->item(&index));if(_wcsicmp(static_cast<const wchar_t*>(attr->nodeName),L"base64data")==0)++backingAttributes;}}
        row.Format("observation=dom_attribute_inventory;base64data_attributes=%llu\r\n",backingAttributes);C4Write(output,row);
        emit("after_dom_attribute_inventory");
        require(m_doc->m_binaryStore.payloads.DiagnosticStatistics().uniqueBytes==initial.uniqueBytes,"reads created canonical copies");
        gc();emit("after_js_gc");
        {const auto payload=m_doc->m_binaryStore.payloads.Snapshot(objects[0].element);C4PayloadFile(root+L"\\c4-a.png",*payload,0);C4PayloadFile(root+L"\\c4-b.png",*payload,1);C4PayloadFile(root+L"\\c4-a.bin",*payload,0,true);C4PayloadFile(root+L"\\c4-b.bin",*payload,1,true);}
        objects.clear();
        wchar_t profileOnly[4]={};const bool only=::GetEnvironmentVariableW(L"FBE_C4_PROFILE_ONLY",profileOnly,_countof(profileOnly))==1&&profileOnly[0]==L'1';
        if(!only){
            _Settings.SetScriptsFolder(root);FbeScripts::ScriptExecutionScope context(root+L"\\c4-probe.js");
            auto edit=[&](unsigned index,unsigned sequence){auto entries=FbeScripts::Binary::Objects(m_doc->m_body.Document());require(index<entries.size(),"edit index");CString replacement=root+(sequence%2?L"\\c4-b":L"\\c4-a");replacement+=entries[index].mime.Left(6)==L"image/"?L".png":L".bin";details.Format("edit index=%u sequence=%u",index,sequence);CComVariant result;FbeScripts::Binary::Mutate(m_doc,150,{CComVariant(entries[index].id),CComVariant(replacement),CComVariant(entries[index].mime)},result);};
            edit(0,0);emit("one_binary_edit");
            for(unsigned i=1;i<5;++i)edit(0,i);emit("five_edits_same_binary");
            for(unsigned i=0;i<5;++i)CheckError(static_cast<HRESULT>(m_doc->m_body.ExecCommand(IDM_UNDO)));emit("undo_all_same_binary");
            for(unsigned i=0;i<5;++i)CheckError(static_cast<HRESULT>(m_doc->m_body.ExecCommand(IDM_REDO)));emit("redo_all_same_binary");
            m_doc->MarkSavePoint();require(LoadFile(original)==OK,"reload for different binaries");gc();emit("reopened_before_different_edits");
            for(unsigned i=0;i<5;++i)edit(i,i);emit("five_edits_different_binaries");
            for(unsigned i=0;i<5;++i)CheckError(static_cast<HRESULT>(m_doc->m_body.ExecCommand(IDM_UNDO)));emit("undo_all_different_binaries");
            for(unsigned i=0;i<5;++i)CheckError(static_cast<HRESULT>(m_doc->m_body.ExecCommand(IDM_REDO)));emit("redo_all_different_binaries");
        }
        m_doc->MarkSavePoint();require(LoadFile(original)==OK,"reload before serialization");gc();emit("before_serialization");
        {
            const auto xml=m_doc->CreateDOM(L"utf-8");emit("serialization_dom_live");
            const _bstr_t serialized=xml->xml;
            row.Format("observation=serialization_bstr;utf16_bytes=%llu\r\n",static_cast<unsigned long long>(serialized.length())*sizeof(wchar_t));C4Write(output,row);emit("serialization_bstr_live");
        }
        gc();emit("after_serialization_released");
        ShowView(SOURCE);emit("source_view_live");
        row.Format("observation=scintilla_source;utf8_bytes=%llu\r\n",static_cast<unsigned long long>(::SendMessage(m_source,SCI_GETLENGTH,0,0)));C4Write(output,row);
        ::SendMessage(m_source,SCI_DOCUMENTEND,0,0);::SendMessage(m_source,WM_CHAR,L' ',0);
        ShowView(BODY);emit("body_after_source_apply");gc();emit("body_after_source_apply_gc");
        {CComPtr<IOleUndoManager> manager;IServiceProviderPtr services(m_doc->m_body.Document());CheckError(services->QueryService(SID_SOleUndoManager,IID_IOleUndoManager,reinterpret_cast<void**>(&manager)));CheckError(manager->DiscardFrom(nullptr));}
        gc();emit("after_source_undo_discard_gc");
        m_doc->MarkSavePoint();require(LoadFile(original)==OK,"reload before detach probe");
        {auto entries=FbeScripts::Binary::Objects(m_doc->m_body.Document());CComPtr<IDispatch> retained(entries[0].element);const std::weak_ptr<const CComVariant> weak=m_doc->m_binaryStore.payloads.Snapshot(retained);
            const HRESULT detached=m_doc->m_binaryStore.payloads.DiagnosticDetach(retained);require(detached==S_OK,"remove behavior diagnostic");
            CComVariant value;CComDispatchDriver raw(retained);const HRESULT hr=raw.GetPropertyByName(L"base64data",&value);
            row.Format("observation=behavior_removed_dom_retained;get_hr=0x%08lX;variant_type=%u;canonical_expired=%u\r\n",static_cast<unsigned long>(hr),static_cast<unsigned>(value.vt),weak.expired()?1u:0u);C4Write(output,row);
            require(FAILED(hr)||value.vt==VT_EMPTY||value.vt==VT_NULL,"DOM retained decoded payload after behavior removal");
        }
        passed=true;details="diagnostic observations complete";
    }catch(const _com_error& error){CStringA hr;hr.Format(" hr=0x%08lX",static_cast<unsigned long>(error.Error()));details+=hr;}catch(...){details="native exception";}
    CStringA result;result.Format("result=%s\r\ndetails=%s\r\n",passed?"PASS":"FAIL",details.GetString());C4Write(output,result);output.Close();::PostQuitMessage(passed?0:1);return 0;
}
#endif
