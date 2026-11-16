	if (IsFbeTestScenario(L"malformed-longline-runtime"))
	{
		wchar_t inputPath[32768] = {}, dumpPath[32768] = {};
		const DWORD inputLength = ::GetEnvironmentVariableW(L"FBE_NEXT_TEST_MALFORMED_SOURCE_PATH", inputPath, _countof(inputPath));
		const DWORD dumpLength = ::GetEnvironmentVariableW(L"FBE_NEXT_TEST_SOURCE_DUMP", dumpPath, _countof(dumpPath));
		FB::Doc* const original = m_doc;
		const DocumentLocation originalLocation = m_document_session.Location();
		m_doc->MarkSavePoint(); m_source.SendMessage(SCI_SETSAVEPOINT);
		DWORD written = 0;
		const char* started = "stage=load-start\n";
		output.Write(started, static_cast<DWORD>(strlen(started)), &written); output.Flush();
		try {
			const FILE_OP_STATUS result = inputLength && inputLength < _countof(inputPath) ? LoadFile(inputPath) : FAIL;
			const bool fallback = result == OK && m_doc == original && FB::Doc::m_active_doc == m_doc &&
				m_bad_xml && m_bad_filename == inputPath && m_editor_view_state.Current() == SOURCE &&
				m_document_session.Location().storagePath == originalLocation.storagePath;
			bool dumped = false;
			if (fallback && dumpLength && dumpLength < _countof(dumpPath)) {
				const std::string bytes = FbeScripts::SourceBytes(m_source);
				CAtlFile dump; CheckError(dump.Create(dumpPath, GENERIC_WRITE, FILE_SHARE_READ, CREATE_NEW));
				CheckError(dump.Write(bytes.data(), static_cast<DWORD>(bytes.size())));
				dumped = true;
			}
			CStringA report; report.Format("fallback=%d\ndumped=%d\n", fallback, dumped);
			output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
			::PostQuitMessage(fallback && dumped ? 0 : 1);
		} catch (const _com_error& error) {
			CStringA report; report.Format("exception=0x%08lX\n", static_cast<unsigned long>(error.Error()));
			output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
			::PostQuitMessage(1);
		}
		return 0;
	}
