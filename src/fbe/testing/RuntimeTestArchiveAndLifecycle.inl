	CAtlFile output;
	if (FAILED(output.Create(AU::_ARGS.source_memory_benchmark_path, GENERIC_WRITE, FILE_SHARE_READ, CREATE_ALWAYS)))
		return 0;
#include "RuntimeTestFileClose.inl"
#include "RuntimeTestNewDocument.inl"
#include "RuntimeTestUiDefects.inl"
#include "RuntimeTestNbspInput.inl"
#include "RuntimeTestReplaceAll.inl"
if(IsFbeTestScenario(L"issue64-runtime")) return RunIssue64Runtime(output);
	if (IsFbeTestScenario(L"search-history-find"))
	{
		auto stage = [&](const char* value) { DWORD bytes = 0; output.Write(value, static_cast<DWORD>(strlen(value)), &bytes); output.Flush(); };
		stage("stage=start\n");
		BOOL handled = FALSE;
		ShowView(BODY); m_doc->m_body.SetFocus();
		stage("stage=body\n");
		m_doc->m_body.OnFind(0, ID_EDIT_FIND, m_doc->m_body, handled);
		stage("stage=find-open\n");
		HWND find = m_doc->m_body.m_find_dlg ? m_doc->m_body.m_find_dlg->m_hWnd : NULL;
		static int dismissed = 0; dismissed = 0;
		const UINT_PTR timer = ::SetTimer(NULL, 0, 30, [](HWND, UINT, UINT_PTR, DWORD) {
			::EnumThreadWindows(::GetCurrentThreadId(), [](HWND window, LPARAM) -> BOOL {
				wchar_t name[64] = {}; ::GetClassNameW(window, name, _countof(name));
				if ((wcscmp(name, L"#32770") == 0 || wcscmp(name, L"FBEThemedMessageDialog") == 0) && !::GetDlgItem(window, IDC_TEXT)) {
					const int button = ::GetDlgItem(window, IDOK) ? IDOK : ::GetDlgItem(window, IDCANCEL) ? IDCANCEL : 0;
					if (button) { ++dismissed; ::PostMessageW(window, WM_COMMAND, button, 0); }
				}
				return TRUE;
			}, 0);
		});
		bool found = false, missing = false, switched = false;
		if (find && timer) {
			::CheckDlgButton(find, IDC_REGEXP, BST_UNCHECKED);
			::SetWindowTextW(::GetDlgItem(find, IDC_TEXT), L"blank.fb2");
			stage("stage=before-found\n");
			::SendMessageW(find, WM_COMMAND, IDC_FIND_FROM_START, 0);
			stage("stage=after-found\n");
			found = dismissed == 0 && ::SendMessageW(::GetDlgItem(find, IDC_TEXT), CB_FINDSTRINGEXACT, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(L"blank.fb2")) == 0;
			::SetWindowTextW(::GetDlgItem(find, IDC_TEXT), L"ABSENT_HISTORY_PATTERN_8472");
			stage("stage=before-missing\n");
			::SendMessageW(find, WM_COMMAND, IDC_FIND_FROM_START, 0);
			stage("stage=after-missing\n");
			missing = dismissed > 0;
			::SendMessageW(find, WM_CLOSE, 0, 0);
			m_doc->m_body.OnReplace(0, ID_EDIT_REPLACE, m_doc->m_body, handled);
			HWND replace = m_doc->m_body.m_replace_dlg ? m_doc->m_body.m_replace_dlg->m_hWnd : NULL;
			HWND combo = replace ? ::GetDlgItem(replace, IDC_TEXT) : NULL;
			switched = combo && ::SendMessageW(combo, CB_FINDSTRINGEXACT, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(L"ABSENT_HISTORY_PATTERN_8472")) == 0 &&
				::SendMessageW(combo, CB_FINDSTRINGEXACT, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(L"blank.fb2")) == 1;
			if (replace) ::SendMessageW(replace, WM_CLOSE, 0, 0);
		}
		if (timer) ::KillTimer(NULL, timer);
		CStringA report; report.Format("found=%d\nmissing=%d\nswitched=%d\n", found, missing, switched);
		DWORD written = 0; output.Write(report, report.GetLength(), &written); output.Close();
		::PostQuitMessage(found && missing && switched ? 0 : 1); return 0;
	}
	if (IsFbeTestScenario(L"search-history-corrupt"))
	{
		const CString path = U::GetSettingsDir() + L"SearchHistory.dat";
		CAtlFile beforeFile;
		char before[8] = {}; DWORD read = 0;
		const bool beforeOk = SUCCEEDED(beforeFile.Create(path, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING)) &&
			SUCCEEDED(beforeFile.Read(before, sizeof(before), read)) && read == sizeof(before);
		beforeFile.Close();
		BOOL handled = FALSE;
		m_doc->m_body.OnReplace(0, ID_EDIT_REPLACE, m_doc->m_body, handled);
		HWND dialog = m_doc->m_body.m_replace_dlg ? m_doc->m_body.m_replace_dlg->m_hWnd : NULL;
		const bool empty = dialog && ::SendMessageW(::GetDlgItem(dialog, IDC_TEXT), CB_GETCOUNT, 0, 0) == 0;
		if (dialog) {
			::SetWindowTextW(::GetDlgItem(dialog, IDC_TEXT), L"new history entry");
			::SendMessageW(dialog, WM_COMMAND, IDCANCEL, 0);
		}
		CAtlFile afterFile; char after[8] = {}; read = 0;
		const bool preserved = beforeOk && SUCCEEDED(afterFile.Create(path, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING)) &&
			SUCCEEDED(afterFile.Read(after, sizeof(after), read)) && read == sizeof(after) && memcmp(before, after, sizeof(before)) == 0;
		CStringA report; report.Format("opened=%d\nempty=%d\npreserved=%d\n", dialog != NULL, empty, preserved);
		DWORD written = 0; output.Write(report, report.GetLength(), &written); output.Close();
		::PostQuitMessage(dialog && empty && preserved ? 0 : 1); return 0;
	}
	if (IsFbeTestScenario(L"search-history-write") || IsFbeTestScenario(L"search-history-read"))
	{
		const bool write = IsFbeTestScenario(L"search-history-write");
		bool opened = false, first = false, ordered = false, deduplicated = false;
		bool closeButton = !write, escape = !write;
		BOOL handled = FALSE;
		auto openDialog = [&]() -> HWND {
			m_doc->m_body.OnReplace(0, ID_EDIT_REPLACE, m_doc->m_body, handled);
			return m_doc->m_body.m_replace_dlg ? m_doc->m_body.m_replace_dlg->m_hWnd : NULL;
		};
		auto hasEntry = [](HWND dialog, int id, LPCWSTR value, int index) {
			HWND combo = ::GetDlgItem(dialog, id);
			return combo && ::SendMessageW(combo, CB_FINDSTRINGEXACT, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(value)) == index;
		};
		HWND dialog = openDialog(); opened = dialog != NULL;
		if (dialog && write)
		{
			::SetWindowTextW(::GetDlgItem(dialog, IDC_TEXT), L"шаблон\\d+");
			::SetWindowTextW(::GetDlgItem(dialog, IDC_REPLACE), L"замена<&>");
			HWND close = ::GetDlgItem(dialog, IDCANCEL);
			if (close) ::SendMessageW(close, BM_CLICK, 0, 0);
			closeButton = close && !::IsWindow(dialog);
			dialog = openDialog();
			first = dialog && hasEntry(dialog, IDC_TEXT, L"шаблон\\d+", 0) && hasEntry(dialog, IDC_REPLACE, L"замена<&>", 0);
			if (dialog) {
				::SetWindowTextW(::GetDlgItem(dialog, IDC_TEXT), L"второй");
				::SendMessageW(dialog, WM_CLOSE, 0, 0);
				dialog = openDialog();
			}
			ordered = dialog && hasEntry(dialog, IDC_TEXT, L"второй", 0) && hasEntry(dialog, IDC_TEXT, L"шаблон\\d+", 1) &&
				hasEntry(dialog, IDC_REPLACE, L"замена<&>", 0);
			if (dialog) {
				::SetWindowTextW(::GetDlgItem(dialog, IDC_TEXT), L"шаблон\\d+");
				::SetWindowTextW(::GetDlgItem(dialog, IDC_REPLACE), L"");
				MSG key = {}; key.hwnd = ::GetDlgItem(dialog, IDC_TEXT); key.message = WM_KEYDOWN; key.wParam = VK_ESCAPE;
				const bool dispatched = ::IsDialogMessageW(dialog, &key) != FALSE;
				escape = dispatched && !::IsWindow(dialog);
				dialog = openDialog();
			}
			deduplicated = dialog && hasEntry(dialog, IDC_TEXT, L"шаблон\\d+", 0) && hasEntry(dialog, IDC_TEXT, L"второй", 1) &&
				::SendMessageW(::GetDlgItem(dialog, IDC_TEXT), CB_GETCOUNT, 0, 0) == 2 &&
				::SendMessageW(::GetDlgItem(dialog, IDC_REPLACE), CB_GETCOUNT, 0, 0) == 1;
		}
		else if (dialog)
		{
			first = hasEntry(dialog, IDC_TEXT, L"шаблон\\d+", 0) && hasEntry(dialog, IDC_TEXT, L"второй", 1);
			ordered = hasEntry(dialog, IDC_REPLACE, L"замена<&>", 0);
			deduplicated = ::SendMessageW(::GetDlgItem(dialog, IDC_TEXT), CB_GETCOUNT, 0, 0) == 2 &&
				::SendMessageW(::GetDlgItem(dialog, IDC_REPLACE), CB_GETCOUNT, 0, 0) == 1;
		}
		if (dialog) ::SendMessageW(dialog, WM_CLOSE, 0, 0);
		CStringA report; report.Format("opened=%d\nfirst=%d\nordered=%d\ndeduplicated=%d\nclose-button=%d\nescape=%d\n", opened, first, ordered, deduplicated, closeButton, escape);
		DWORD written = 0; output.Write(report, report.GetLength(), &written); output.Flush(); output.Close();
		::PostQuitMessage(opened && first && ordered && deduplicated && closeButton && escape ? 0 : 1); return 0;
	}
	if (IsFbeTestScenario(L"script-card-block-runtime"))
	{
		const BYTE ascii[] = "\xEF\xBB\xBF/*<FBE-SCRIPT-CARD version=\"1\">\r\n{}\r\n</FBE-SCRIPT-CARD>*/\r\nABC";
		ScriptCardBlock::Span span;
		const bool asciiSpan = ScriptCardBlock::Find(ascii, sizeof(ascii) - 1, span) == ScriptCardBlock::Status::Present &&
			span.begin == 3 && span.end < sizeof(ascii) - 1;
		const bool asciiHash = ScriptCardBlock::Sha256WithoutCard(ascii, sizeof(ascii) - 1) ==
			L"850e8455109dc0670eaecf5b638e036046b87d83db5d0bbc6b455a027d5a627a";
		const BYTE plain[] = { 'A', 'B', 'C' };
		const bool plainHash = ScriptCardBlock::Sha256WithoutCard(plain, sizeof(plain)) ==
			L"b5d4045c3f466fa91fe2cc6abe79232a1a57cdf104f7a26e716e0a1e2789df78";
		std::vector<BYTE> utf16 = { 0xFF, 0xFE };
		const wchar_t text[] = L"/*<FBE-NEXT-CARD version=\"1\">\n{}\n</FBE-NEXT-CARD>*/\nABC";
		const BYTE* encoded = reinterpret_cast<const BYTE*>(text);
		utf16.insert(utf16.end(), encoded, encoded + (wcslen(text) * sizeof(wchar_t)));
		const bool utf16Hash = ScriptCardBlock::Find(utf16.data(), utf16.size(), span) == ScriptCardBlock::Status::Present &&
			ScriptCardBlock::Sha256WithoutCard(utf16.data(), utf16.size()) ==
			L"c85e33093b36131b8fee0a7df1b83e94e21e78576831e2df7e5965f1c764f748";
		const BYTE invalid[] = "/*<FBE-NEXT-CARD version=\"1\">\n{}";
		const bool malformed = ScriptCardBlock::Find(invalid, sizeof(invalid) - 1, span) == ScriptCardBlock::Status::Malformed;
		const BYTE misplaced[] = "// header\n/*<FBE-NEXT-CARD version=\"1\">\n{}\n</FBE-NEXT-CARD>*/\nABC";
		const bool rejectedMisplaced = ScriptCardBlock::Find(misplaced, sizeof(misplaced) - 1, span) == ScriptCardBlock::Status::Malformed;
		const BYTE duplicate[] = "/*<FBE-SCRIPT-CARD version=\"1\">\n{}\n</FBE-SCRIPT-CARD>*/\n/*<FBE-NEXT-CARD version=\"1\">\n{}\n</FBE-NEXT-CARD>*/\nABC";
		const bool rejectedDuplicate = ScriptCardBlock::Find(duplicate, sizeof(duplicate) - 1, span) == ScriptCardBlock::Status::Malformed;
		const char json[] = "{\"schema\":1,\"title\":\"Example\",\"version\":\"1\",\"authors\":[{\"name\":\"Author\",\"role\":\"author\"}],"
			"\"summary\":\"Summary\",\"description\":\"Description\",\"scope\":\"document\",\"modifiesDocument\":null,"
			"\"requiresSelection\":false,\"usage\":[\"Open\"],\"warnings\":[],\"dependencies\":[],"
			"\"sourceRevision\":{\"algorithm\":\"sha256-exclude-fbe-card-v1\",\"sha256\":\"b5d4045c3f466fa91fe2cc6abe79232a1a57cdf104f7a26e716e0a1e2789df78\",\"reviewStatus\":\"generated\"}}";
		CStringA cardText("/*<FBE-SCRIPT-CARD version=\"1\">\n");
		cardText += json; cardText += "\n</FBE-SCRIPT-CARD>*/\nABC";
		auto parseText = [](const CStringA& value) {
			return ScriptCards::Parse(reinterpret_cast<const BYTE*>(static_cast<LPCSTR>(value)), static_cast<size_t>(value.GetLength()));
		};
		const ScriptCards::Card current = parseText(cardText);
		const bool parsedCurrent = current.state == ScriptCards::State::Current && current.title == L"Example" &&
			current.authors.size() == 1 && current.authors[0].name == L"Author" && current.modifiesDocument == -1;
		const ScriptCards::Card changed = parseText(cardText + " ");
		const bool parsedChanged = changed.state == ScriptCards::State::Changed;
		CStringA uncheckedText(cardText);
		uncheckedText.Replace("\"b5d4045c3f466fa91fe2cc6abe79232a1a57cdf104f7a26e716e0a1e2789df78\"", "null");
		const ScriptCards::Card unchecked = parseText(uncheckedText);
		const bool parsedUnchecked = unchecked.state == ScriptCards::State::Unchecked && !unchecked.currentSha256.IsEmpty();
		CStringA broken(cardText); broken.Replace("\"schema\":1", "\"schema\":2");
		const bool parsedInvalid = parseText(broken).state == ScriptCards::State::Invalid;
		const bool parsedMissing = parseText(CStringA("ABC")).state == ScriptCards::State::Missing;
		CStringA withDependency(cardText);
		withDependency.Replace("\"dependencies\":[]", "\"dependencies\":[{\"path\":\"HTML/helper.html\",\"sha256\":null,\"optional\":false}],\"dependenciesComplete\":false");
		const ScriptCards::Card dependency = parseText(withDependency);
		const bool parsedDependency = dependency.state == ScriptCards::State::Current && dependency.dependencies.size() == 1 &&
			dependency.dependencies[0].path == L"HTML/helper.html" && !dependency.dependencies[0].optional && !dependency.dependenciesComplete;
		CStringA escapingDependency(withDependency);
		escapingDependency.Replace("HTML/helper.html", "../outside.html");
		const bool rejectedDependency = parseText(escapingDependency).state == ScriptCards::State::Invalid;
		CString dependencyRoot(AU::_ARGS.source_memory_benchmark_path);
		const int lastSlash = dependencyRoot.ReverseFind(L'\\');
		dependencyRoot = lastSlash >= 0 ? dependencyRoot.Left(lastSlash) : CString();
		const CString dependencyPath = dependencyRoot + L"\\script-card-dependency.bin";
		CAtlFile dependencyFile;
		const bool openedDependency = !dependencyRoot.IsEmpty() &&
			SUCCEEDED(dependencyFile.Create(dependencyPath, GENERIC_WRITE, FILE_SHARE_READ, CREATE_ALWAYS));
		DWORD dependencyWritten = 0;
		const bool wroteDependency = openedDependency && SUCCEEDED(dependencyFile.Write("old", 3, &dependencyWritten)) && dependencyWritten == 3;
		dependencyFile.Close();
		CStringA checkedDependency(cardText);
		checkedDependency.Replace("\"dependencies\":[]",
			"\"dependencies\":[{\"path\":\"script-card-dependency.bin\",\"sha256\":\"cba06b5736faf67e54b07b561eae94395e774c517a7d910a54369e1263ccfbd4\",\"optional\":false}],\"dependenciesComplete\":true");
		ScriptCards::Card checked = parseText(checkedDependency);
		if(wroteDependency) ScriptCards::VerifyDependencies(checked, dependencyRoot);
		const bool dependencyCurrent = checked.dependencyState == ScriptCards::Card::DependenciesState::Current &&
			checked.dependencies.size() == 1 && checked.dependencies[0].state == ScriptCards::Dependency::State::Current;
		::DeleteFileW(dependencyPath);
		ScriptCards::VerifyDependencies(checked, dependencyRoot);
		const bool dependencyMissing = checked.dependencyState == ScriptCards::Card::DependenciesState::Changed &&
			checked.dependencies[0].state == ScriptCards::Dependency::State::Missing;
		const bool passed = asciiSpan && asciiHash && plainHash && utf16Hash && malformed && rejectedMisplaced && rejectedDuplicate &&
			parsedCurrent && parsedChanged && parsedUnchecked && parsedInvalid && parsedMissing && parsedDependency && rejectedDependency &&
			dependencyCurrent && dependencyMissing;
		CStringA report; report.Format("ascii-span=%d\nascii-hash=%d\nplain-hash=%d\nutf16-hash=%d\nmalformed=%d\nrejected-misplaced=%d\nrejected-duplicate=%d\nparsed-current=%d\nparsed-changed=%d\nparsed-unchecked=%d\nparsed-invalid=%d\nparsed-missing=%d\nparsed-dependency=%d\nrejected-dependency=%d\ndependency-current=%d\ndependency-missing=%d\nresult=%s\n",
			asciiSpan, asciiHash, plainHash, utf16Hash, malformed, rejectedMisplaced, rejectedDuplicate, parsedCurrent, parsedChanged,
			parsedUnchecked, parsedInvalid, parsedMissing, parsedDependency, rejectedDependency,
			dependencyCurrent, dependencyMissing, passed ? "pass" : "fail");
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Close();
		::PostQuitMessage(passed ? 0 : 1); return 0;
	}
	if (IsFbeTestScenario(L"xml-source-theme-v1-runtime"))
	{
		static const wchar_t* const tokenNames[XML_SRC_STYLE_TOKEN_COUNT] = {
			L"editor.background", L"editor.foreground", L"editor.selection.background", L"editor.selection.foreground",
			L"editor.currentLine.background", L"editor.caret", L"editor.lineNumber", L"editor.lineNumber.active",
			L"editor.matchingTag.background", L"editor.matchingTag.border", L"xml.text", L"xml.tag.name",
			L"xml.tag.delimiter", L"xml.attribute.name", L"xml.attribute.value", L"xml.namespace", L"xml.comment",
			L"xml.entity", L"xml.cdata", L"xml.processingInstruction", L"xml.doctype", L"xml.error", L"xml.warning"
		};
		auto isOptional = [](int token) -> bool
		{
			return token == XML_SRC_STYLE_LINE_NUMBER_ACTIVE || token == XML_SRC_STYLE_MATCHING_TAG_BACKGROUND ||
				token == XML_SRC_STYLE_MATCHING_TAG_BORDER || token == XML_SRC_STYLE_XML_NAMESPACE ||
				token == XML_SRC_STYLE_XML_COMMENT || token == XML_SRC_STYLE_XML_WARNING;
		};
		auto makeTheme = [&](bool omitOptional, const wchar_t* omittedToken, const wchar_t* overriddenToken, const wchar_t* overriddenValue) -> CString
		{
			CString json(L"{\n  \"format\": \"FictionBookEditorNext.CodeTheme\",\n  \"formatVersion\": 1,\n  \"id\": \"xml-theme-v1-test\",\n  \"name\": \"Тема XML\",\n  \"isDark\": false,\n  \"baseThemeId\": \"fbe-light\",\n  \"author\": \"Тест\",\n  \"colors\": {\n");
			bool first = true;
			for (int token = 0; token < XML_SRC_STYLE_TOKEN_COUNT; ++token)
			{
				if ((omitOptional && isOptional(token)) || (omittedToken != NULL && wcscmp(tokenNames[token], omittedToken) == 0)) continue;
				if (!first) json += L",\n";
				first = false;
				const bool overridden = overriddenToken != NULL && wcscmp(tokenNames[token], overriddenToken) == 0;
				CString color;
				color.Format(L"\"#%02X%02X%02X\"", 0x10 + token, 0x40 + token, 0x80 + token);
				json.AppendFormat(L"    \"%s\": %s", tokenNames[token], overridden ? overriddenValue : static_cast<LPCWSTR>(color));
			}
			json += L"\n  }\n}\n";
			return json;
		};
		auto writeUtf8 = [](const CString& path, const CString& text) -> bool
		{
			const int byteCount = ::WideCharToMultiByte(CP_UTF8, 0, text, text.GetLength(), NULL, 0, NULL, NULL);
			if (byteCount <= 0) return false;
			std::vector<char> bytes(static_cast<size_t>(byteCount));
			if (::WideCharToMultiByte(CP_UTF8, 0, text, text.GetLength(), bytes.data(), byteCount, NULL, NULL) != byteCount) return false;
			HANDLE file = ::CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			if (file == INVALID_HANDLE_VALUE) return false;
			DWORD written = 0;
			const bool saved = ::WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, NULL) != FALSE && written == bytes.size();
			::CloseHandle(file);
			return saved;
		};
		auto readStrictUtf8 = [](const CString& path, CString& text) -> bool
		{
			HANDLE file = ::CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			if (file == INVALID_HANDLE_VALUE) return false;
			LARGE_INTEGER size = {};
			const bool validSize = ::GetFileSizeEx(file, &size) != FALSE && size.QuadPart > 0 && size.QuadPart <= INT_MAX;
			std::vector<char> bytes(validSize ? static_cast<size_t>(size.QuadPart) : 0);
			DWORD read = 0;
			const bool loaded = validSize && ::ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &read, NULL) != FALSE && read == bytes.size();
			::CloseHandle(file);
			if (!loaded) return false;
			const int length = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), static_cast<int>(bytes.size()), NULL, 0);
			if (length <= 0) return false;
			wchar_t* const buffer = text.GetBuffer(length);
			const int decoded = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), static_cast<int>(bytes.size()), buffer, length);
			text.ReleaseBuffer(decoded > 0 ? decoded : 0);
			return decoded == length;
		};
		const CString sourcePath = AU::_ARGS.source_memory_benchmark_path + L".source.fbetheme";
		const CString exportedPath = AU::_ARGS.source_memory_benchmark_path + L".exported.fbetheme";
		CString error;
		XmlSourceThemeImport imported = {};
		const bool sourceWritten = writeUtf8(sourcePath, makeTheme(true, NULL, NULL, NULL));
		const bool importedMinimal = sourceWritten && XmlSourceThemes::LoadImportTheme(sourcePath, imported, error);
		const bool fallbacks = importedMinimal &&
			imported.colors[XML_SRC_STYLE_LINE_NUMBER_ACTIVE] == imported.colors[XML_SRC_STYLE_LINE_NUMBER] &&
			imported.colors[XML_SRC_STYLE_MATCHING_TAG_BACKGROUND] == imported.colors[XML_SRC_STYLE_SELECTION_BACKGROUND] &&
			imported.colors[XML_SRC_STYLE_MATCHING_TAG_BORDER] == imported.colors[XML_SRC_STYLE_XML_TAG_NAME] &&
			imported.colors[XML_SRC_STYLE_XML_NAMESPACE] == imported.colors[XML_SRC_STYLE_XML_ATTRIBUTE_NAME] &&
			imported.colors[XML_SRC_STYLE_XML_COMMENT] == imported.colors[XML_SRC_STYLE_XML_TEXT] &&
			imported.colors[XML_SRC_STYLE_XML_WARNING] == imported.colors[XML_SRC_STYLE_XML_ERROR];
		const wchar_t* const optionalTokens[] = {
			L"editor.lineNumber.active", L"editor.matchingTag.background", L"editor.matchingTag.border",
			L"xml.namespace", L"xml.comment", L"xml.warning"
		};
		bool optionalRolesAccepted = true;
		for (size_t index = 0; index < _countof(optionalTokens); ++index)
		{
			XmlSourceThemeImport optionalImport = {};
			optionalRolesAccepted = optionalRolesAccepted && writeUtf8(sourcePath, makeTheme(false, optionalTokens[index], NULL, NULL)) &&
				XmlSourceThemes::LoadImportTheme(sourcePath, optionalImport, error);
		}
		const bool exported = importedMinimal && XmlSourceThemes::ExportThemeFile(imported.info.id, imported.info.name, imported.colors, exportedPath, error, &imported.metadata);
		CString exportedText;
		const bool utf8Export = exported && readStrictUtf8(exportedPath, exportedText) &&
			exportedText.Find(L"\"Тема XML\"") >= 0 && exportedText.Find(L"\"Тест\"") >= 0;
		XmlSourceThemeImport roundTrip = {};
		const bool reimported = utf8Export && XmlSourceThemes::LoadImportTheme(exportedPath, roundTrip, error);
		bool palettePreserved = reimported;
		for (int token = 0; token < XML_SRC_STYLE_TOKEN_COUNT && palettePreserved; ++token)
			palettePreserved = imported.colors[token] == roundTrip.colors[token];
		const bool metadataPreserved = reimported && roundTrip.metadata.baseThemeId == L"fbe-light" && roundTrip.metadata.author == L"Тест";
		const wchar_t* const invalidTokens[] = { L"xml.tag.name", L"editor.background" };
		bool missingRequiredRejected = true;
		for (size_t index = 0; index < _countof(invalidTokens); ++index)
		{
			XmlSourceThemeImport rejected = {};
			missingRequiredRejected = missingRequiredRejected && writeUtf8(sourcePath, makeTheme(false, invalidTokens[index], NULL, NULL)) && !XmlSourceThemes::LoadImportTheme(sourcePath, rejected, error);
		}
		const wchar_t* const invalidValues[] = { L"\"#FFF\"", L"\"red\"", L"\"\"", L"123" };
		bool invalidValuesRejected = true;
		for (size_t index = 0; index < _countof(invalidValues); ++index)
		{
			XmlSourceThemeImport rejected = {};
			invalidValuesRejected = invalidValuesRejected && writeUtf8(sourcePath, makeTheme(false, NULL, L"xml.tag.name", invalidValues[index])) && !XmlSourceThemes::LoadImportTheme(sourcePath, rejected, error);
		}
		CString stagedId;
		XmlSourceThemes::BeginSettingsEdit();
		bool themeTransaction = importedMinimal && XmlSourceThemes::ImportThemeFile(imported, stagedId, error) && XmlSourceThemes::IsUserTheme(stagedId);
		XmlSourceThemes::EndSettingsEdit();
		themeTransaction = themeTransaction && !XmlSourceThemes::IsUserTheme(stagedId);
		XmlSourceThemes::BeginSettingsEdit();
		themeTransaction = themeTransaction && XmlSourceThemes::ImportThemeFile(imported, stagedId, error);
		{ SettingsFileTransaction files; themeTransaction = themeTransaction && XmlSourceThemes::PrepareSettingsEdit(files, error) && files.Commit(); }
		XmlSourceThemes::EndSettingsEdit();
		themeTransaction = themeTransaction && XmlSourceThemes::IsUserTheme(stagedId);
		XmlSourceThemes::BeginSettingsEdit();
		themeTransaction = themeTransaction && XmlSourceThemes::DeleteUserTheme(stagedId, error) && !XmlSourceThemes::IsUserTheme(stagedId);
		XmlSourceThemes::EndSettingsEdit();
		themeTransaction = themeTransaction && XmlSourceThemes::IsUserTheme(stagedId);
		XmlSourceThemes::BeginSettingsEdit();
		themeTransaction = themeTransaction && XmlSourceThemes::DeleteUserTheme(stagedId, error);
		{ SettingsFileTransaction files; themeTransaction = themeTransaction && XmlSourceThemes::PrepareSettingsEdit(files, error) && files.Commit(); }
		XmlSourceThemes::EndSettingsEdit();
		themeTransaction = themeTransaction && !XmlSourceThemes::IsUserTheme(stagedId);
		const bool noSelfReference = reimported && roundTrip.metadata.baseThemeId.CompareNoCase(roundTrip.info.id) != 0;
		HIGHCONTRAST highContrast = {};
		highContrast.cbSize = sizeof(highContrast);
		const bool highContrastEnabled = ::SystemParametersInfo(SPI_GETHIGHCONTRAST, sizeof(highContrast), &highContrast, 0) && (highContrast.dwFlags & HCF_HIGHCONTRASTON) != 0;
		const CString originalTheme = _Settings.GetXmlSrcThemeId();
		auto applyThemeAndReadIndicators = [&](const wchar_t* id, COLORREF& background, COLORREF& border, COLORREF& errorColor, COLORREF& warning) -> bool
		{
			_Settings.SetXmlSrcThemeId(id, false);
			ApplyXmlSourceEditorChanges(false);
			background = static_cast<COLORREF>(m_source.Send(SCI_INDICGETFORE, EDITOR_INDICATOR_TAG_MATCH_BACKGROUND));
			border = static_cast<COLORREF>(m_source.Send(SCI_INDICGETFORE, EDITOR_INDICATOR_TAG_MATCH));
			errorColor = static_cast<COLORREF>(m_source.Send(SCI_INDICGETFORE, EDITOR_INDICATOR_XML_TAG_MISMATCHED));
			warning = static_cast<COLORREF>(m_source.Send(SCI_INDICGETFORE, EDITOR_INDICATOR_XML_TAG_MISSING_OPENING));
			DWORD expectedBackground = 0, expectedBorder = 0;
			const bool expectedColors = highContrastEnabled ||
				(XmlSourceThemes::GetThemeColor(id, XML_SRC_STYLE_MATCHING_TAG_BACKGROUND, expectedBackground) &&
				 XmlSourceThemes::GetThemeColor(id, XML_SRC_STYLE_MATCHING_TAG_BORDER, expectedBorder));
			if (highContrastEnabled) { expectedBackground = ::GetSysColor(COLOR_HIGHLIGHT); expectedBorder = ::GetSysColor(COLOR_HIGHLIGHTTEXT); }
			return expectedColors && background == expectedBackground && border == expectedBorder &&
				m_source.Send(SCI_INDICGETSTYLE, EDITOR_INDICATOR_TAG_MATCH_BACKGROUND) == INDIC_FULLBOX &&
				m_source.Send(SCI_INDICGETALPHA, EDITOR_INDICATOR_TAG_MATCH_BACKGROUND) == 72 &&
				m_source.Send(SCI_INDICGETUNDER, EDITOR_INDICATOR_TAG_MATCH_BACKGROUND) != 0 &&
				m_source.Send(SCI_INDICGETSTYLE, EDITOR_INDICATOR_TAG_MATCH) == INDIC_ROUNDBOX &&
				m_source.Send(SCI_INDICGETALPHA, EDITOR_INDICATOR_TAG_MATCH) == 0 &&
				m_source.Send(SCI_INDICGETOUTLINEALPHA, EDITOR_INDICATOR_TAG_MATCH) == 255 &&
				m_source.Send(SCI_INDICGETUNDER, EDITOR_INDICATOR_TAG_MATCH) == 0;
		};
		COLORREF lightBackground = 0, lightBorder = 0, lightError = 0, lightWarning = 0;
		COLORREF darkBackground = 0, darkBorder = 0, darkError = 0, darkWarning = 0;
		const bool lightApplied = applyThemeAndReadIndicators(L"fbe-light", lightBackground, lightBorder, lightError, lightWarning);
		const bool darkApplied = applyThemeAndReadIndicators(L"fbe-dark", darkBackground, darkBorder, darkError, darkWarning);
		const bool themeSwitchApplied = lightApplied && darkApplied && (highContrastEnabled ||
			(lightBackground != darkBackground && lightBorder != darkBorder && lightError != darkError && lightWarning != darkWarning));
		const bool highContrastSystemColors = !highContrastEnabled ||
			(darkBackground == ::GetSysColor(COLOR_HIGHLIGHT) && darkBorder == ::GetSysColor(COLOR_HIGHLIGHTTEXT) &&
			darkError == ::GetSysColor(COLOR_HOTLIGHT) && darkWarning == ::GetSysColor(COLOR_HOTLIGHT));
		_Settings.SetXmlSrcThemeId(originalTheme, false);
		ApplyXmlSourceEditorChanges(false);
		const bool passed = themeTransaction && importedMinimal && fallbacks && optionalRolesAccepted && exported && utf8Export && reimported && palettePreserved && metadataPreserved && missingRequiredRejected && invalidValuesRejected && noSelfReference && themeSwitchApplied && highContrastSystemColors;
		CStringA report;
		report.Format("import=%d\nfallbacks=%d\noptional_roles=%d\nexport=%d\nutf8_export=%d\nreimport=%d\npalette=%d\nmetadata=%d\nmissing_required=%d\ninvalid_values=%d\nno_self_reference=%d\ntheme_switch=%d\nhigh_contrast_system_colors=%d\nresult=%s\n",
			importedMinimal, fallbacks, optionalRolesAccepted, exported, utf8Export, reimported, palettePreserved, metadataPreserved, missingRequiredRejected, invalidValuesRejected, noSelfReference, themeSwitchApplied, highContrastSystemColors, passed ? "pass" : "fail");
		DWORD written = 0;
		output.Write(report, static_cast<DWORD>(report.GetLength()), &written);
		output.Close();
		::DeleteFileW(sourcePath); ::DeleteFileW(exportedPath);
		::PostQuitMessage(passed ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"archive-open-runtime"))
	{
		const bool archiveSource = m_document_session.Location().IsArchive();
		const bool fb2 = m_doc->GetDocumentFileType() == FictionBookFileType::Fb2;
		const bool fbd = m_doc->GetDocumentFileType() == FictionBookFileType::Fbd;
		const bool htmlReady = m_doc->m_body.Document() != NULL;
		const bool rar = m_document_session.Location().containerKind == DocumentContainerKind::Rar;
		// Runtime reports are consumed as UTF-8 by the PowerShell regressions.
		// Do not let the runner's ANSI code page convert a Unicode ZIP entry:
		// on an English Windows image that conversion can throw before the
		// archive-open diagnostic is written.
		const CStringA entryUtf8(CW2A(m_document_session.Location().entryPath, CP_UTF8));
		CStringA report;
		report.Format("archive=%d\nfb2=%d\nfbd=%d\nmshtml=%d\nrar=%d\nentry=%s\n", archiveSource, fb2, fbd, htmlReady, rar,
			static_cast<LPCSTR>(entryUtf8));
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Close();
		::PostQuitMessage(archiveSource && (fb2 || fbd) && htmlReady ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"archive-runtime") || IsFbeTestScenario(L"archive-rar-save-runtime"))
	{
		const bool archiveSource = m_document_session.Location().IsArchive();
		const bool fb2 = m_doc->GetDocumentFileType() == FictionBookFileType::Fb2;
		const bool htmlReady = m_doc->m_body.Document() != NULL;
		ShowView(SOURCE);
		const sptr_t sourceLength = m_source.SendMessage(SCI_GETLENGTH);
		std::vector<char> source(static_cast<size_t>(sourceLength) + 1);
		m_source.SendMessage(SCI_GETTEXT, sourceLength + 1, reinterpret_cast<LPARAM>(source.data()));
		const char* markerText = "ARCHIVE_RUNTIME_BEFORE";
		char* marker = strstr(source.data(), markerText);
		if (!marker)
		{
			// The checked-in RAR5 fixture is an existing valid FB2 regression
			// document.  Its stable author field is the edit target for Save As.
			markerText = "FBE Test";
			marker = strstr(source.data(), markerText);
		}
		const bool markerFound = marker != NULL;
		if (markerFound) { const size_t offset = static_cast<size_t>(marker - source.data()); m_source.SendMessage(SCI_SETSEL, offset, offset + strlen(markerText)); m_source.SendMessage(SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>("ARCHIVE_RUNTIME_AFTER")); }
		int sourceLine = 0, sourceColumn = 0;
		const bool sourceCommitted = markerFound && m_doc->SetXMLAndValidate(m_source, false, sourceLine, sourceColumn);
		ShowView(BODY);
		const bool bodyActive = !IsSourceActive();
		const bool readOnlyArchive = m_document_session.Location().containerKind == DocumentContainerKind::Rar;
		const bool saveAs = IsFbeTestScenario(L"archive-rar-save-runtime");
		const bool shouldSave = !readOnlyArchive || saveAs;
		const bool saved = sourceCommitted && bodyActive && markerFound && (!shouldSave || SaveFile(false) == OK);
		CStringA report;
		report.Format("archive=%d\nfb2=%d\nfbd=%d\nmshtml=%d\nrar=%d\nsave_as=%d\nsource_committed=%d\nbody_active=%d\nentry=%S\nsaved=%d\n", archiveSource, fb2,
			m_doc->GetDocumentFileType() == FictionBookFileType::Fbd, htmlReady, readOnlyArchive, saveAs,
			sourceCommitted, bodyActive, static_cast<LPCWSTR>(m_document_session.Location().entryPath), saved);
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(saved ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"phase-b-cache-runtime"))
	{
		if (!StartupTrace::Enabled() || !m_doc || !m_doc->m_body.Document())
		{
			output.Close(); ::PostQuitMessage(1); return 0;
		}
		// Validate twice in the same apartment.  The first pass may legitimately
		// use a schema loaded earlier in this lifecycle; only the second pass is
		// required not to add another schema load.
		ShowView(SOURCE);
		int line = 0, column = 0;
		const bool firstValidation = m_doc->SetXMLAndValidate(m_source, true, line, column);
		const ULONGLONG schemasAfterFirst = StartupTrace::XsdSchemaLoadCount();
		const bool secondValidation = m_doc->SetXMLAndValidate(m_source, true, line, column);
		const ULONGLONG schemasAfterSecond = StartupTrace::XsdSchemaLoadCount();

		const ULONGLONG serializationsBefore = StartupTrace::SourceSerializationCount();
		ShowView(BODY); ShowView(SOURCE);
		const ULONGLONG serializationsAfterFirst = StartupTrace::SourceSerializationCount();
		ShowView(BODY); ShowView(SOURCE);
		const ULONGLONG serializationsAfterSecond = StartupTrace::SourceSerializationCount();

		ShowView(BODY);
		MSHTML::IHTMLBodyElementPtr body(m_doc->m_body.Document()->body);
		MSHTML::IHTMLElementCollectionPtr paragraphs(body ? MSHTML::IHTMLElementCollectionPtr(MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(L"P")) : MSHTML::IHTMLElementCollectionPtr());
		MSHTML::IHTMLElementPtr paragraph(paragraphs && paragraphs->length ? MSHTML::IHTMLElementPtr(paragraphs->item(_variant_t(0L), _variant_t())) : MSHTML::IHTMLElementPtr());
		bool edited = false;
		if (paragraph)
		{
			CString text(static_cast<LPCWSTR>(_bstr_t(paragraph->innerText)));
			text += L" phase-b-cache-edit";
			paragraph->innerText = _bstr_t(static_cast<LPCWSTR>(text));
			BOOL handled = FALSE;
			OnEdChange(0, 0, NULL, handled);
			edited = true;
		}
		ShowView(SOURCE);
		const ULONGLONG serializationsAfterEdit = StartupTrace::SourceSerializationCount();
		const bool xsdSecondUnchanged = schemasAfterSecond == schemasAfterFirst;
		const bool sourceSecondUnchanged = serializationsAfterSecond == serializationsAfterFirst;
		const bool sourceInvalidatedAfterEdit = edited && serializationsAfterEdit > serializationsAfterSecond;
		CStringA report;
		report.Format("first_validation=%d\nxsd_after_first=%I64u\nsecond_validation=%d\nxsd_after_second=%I64u\nxsd_second_equals_first=%d\nsource_before=%I64u\nsource_after_first=%I64u\nsource_after_second=%I64u\nunchanged_second_source=%d\nsource_after_edit=%I64u\nedit_invalidates_source=%d\n",
			firstValidation, schemasAfterFirst, secondValidation, schemasAfterSecond, xsdSecondUnchanged,
			serializationsBefore, serializationsAfterFirst, serializationsAfterSecond, sourceSecondUnchanged,
			serializationsAfterEdit, sourceInvalidatedAfterEdit);
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(firstValidation && secondValidation && xsdSecondUnchanged && sourceSecondUnchanged && sourceInvalidatedAfterEdit && written == static_cast<DWORD>(report.GetLength()) ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"empty-image-placeholder-runtime"))
	{
		auto bodyPlaceholderState = [&]()
		{
			MSHTML::IHTMLDocument2Ptr document(m_doc ? m_doc->m_body.Document() : NULL);
			MSHTML::IHTMLBodyElementPtr body(document ? document->body : NULL);
			MSHTML::IHTMLElementPtr bodyElement(body);
			const CString html(bodyElement ? static_cast<LPCWSTR>(_bstr_t(bodyElement->innerHTML)) : L"");
			return html.Find(L"href=\"#undefined\"") >= 0 &&
				html.Find(L"fbw-internal:#undefined") >= 0 &&
				html.Find(L"fbw-internal:#\"") < 0;
		};
		ShowView(BODY);
		const bool bodyOpened = bodyPlaceholderState();
		ShowView(SOURCE);
		const sptr_t length = m_source.SendMessage(SCI_GETLENGTH);
		std::vector<char> source(static_cast<size_t>(length) + 1);
		m_source.SendMessage(SCI_GETTEXT, length + 1, reinterpret_cast<LPARAM>(source.data()));
		const bool sourcePreserved = strstr(source.data(), "#undefined") != NULL && strstr(source.data(), "fbw-internal:#") == NULL;
		ShowView(BODY);
		const bool bodyAfterSource = bodyPlaceholderState();
		const CString filename(m_doc->m_filename);
		const bool saved = !filename.IsEmpty() && m_doc->Save();
		const bool reopened = saved && LoadFile(filename) == OK;
		const bool bodyAfterReopen = reopened && bodyPlaceholderState();
		CStringA report;
		report.Format("body_open=%d\nsource=%d\nbody_after_source=%d\nsaved=%d\nreopened=%d\nbody_after_reopen=%d\n",
			bodyOpened, sourcePreserved, bodyAfterSource, saved, reopened, bodyAfterReopen);
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(bodyOpened && sourcePreserved && bodyAfterSource && saved && reopened && bodyAfterReopen ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"generated-id-reopen-runtime"))
	{
		wchar_t expected[128] = {};
		const DWORD expectedLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_EXPECTED_ID", expected, _countof(expected));
		ShowView(DESC);
		MSHTML::IHTMLDocument2Ptr document(m_doc->m_body.Document());
		MSHTML::IHTMLDocument3Ptr document3(document);
		MSHTML::IHTMLInputTextElementPtr id(document3 ? document3->getElementById(L"diID") : NULL);
		const bool descriptionCurrent = expectedLength > 0 && expectedLength < _countof(expected) && id && CString(static_cast<LPCWSTR>(id->value)) == expected;
		ShowView(SOURCE);
		const sptr_t length = m_source.SendMessage(SCI_GETLENGTH);
		std::vector<char> xml(static_cast<size_t>(length) + 1);
		m_source.SendMessage(SCI_GETTEXT, length + 1, reinterpret_cast<LPARAM>(xml.data()));
		const CStringA expectedUtf8(CW2A(expected, CP_UTF8));
		const bool sourceCurrent = descriptionCurrent && strstr(xml.data(), expectedUtf8) != NULL;
		CStringA report; report.Format("description-current=%d\nsource-current=%d\n", descriptionCurrent, sourceCurrent);
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(descriptionCurrent && sourceCurrent ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"description-custom-undo-probe"))
	{
		class ProbeUnit : public IOleUndoUnit {
			ULONG refs = 1;
			MSHTML::IHTMLInputTextElementPtr input;
			CString original, generated;
			bool restoreOriginal = true;
		public:
			int calls = 0; bool receivedManager = false;
			ProbeUnit(MSHTML::IHTMLInputTextElementPtr field, const CString& before, const CString& after)
				: input(field), original(before), generated(after) {}
			STDMETHOD(QueryInterface)(REFIID iid, void** result) override {
				if (!result) return E_POINTER;
				*result = NULL;
				if (iid != IID_IUnknown && iid != IID_IOleUndoUnit) return E_NOINTERFACE;
				*result = static_cast<IOleUndoUnit*>(this); AddRef(); return S_OK;
			}
			STDMETHOD_(ULONG, AddRef)() override { return ++refs; }
			STDMETHOD_(ULONG, Release)() override { const ULONG remaining = --refs; if (!remaining) delete this; return remaining; }
			STDMETHOD(Do)(IOleUndoManager* manager) override {
				++calls; receivedManager = manager != NULL;
				try {
					input->value = static_cast<LPCWSTR>(restoreOriginal ? original : generated);
					if (manager) { const HRESULT result = manager->Add(this); if (FAILED(result)) return result; }
					restoreOriginal = !restoreOriginal; return S_OK;
				} catch (const _com_error& error) { return error.Error(); }
			}
			STDMETHOD(GetDescription)(BSTR* value) override { if (!value) return E_POINTER; *value = SysAllocString(L"ID probe"); return *value ? S_OK : E_OUTOFMEMORY; }
			STDMETHOD(GetUnitType)(CLSID* clsid, LONG* id) override { if (!clsid || !id) return E_POINTER; *clsid = GUID_NULL; *id = 1; return S_OK; }
			STDMETHOD(OnNextAdd)() override { return S_OK; }
		};
		ShowView(DESC);
		MSHTML::IHTMLDocument2Ptr document(m_doc->m_body.Document());
		MSHTML::IHTMLDocument3Ptr document3(document);
		MSHTML::IHTMLInputTextElementPtr field(document3 ? document3->getElementById(L"diID") : NULL);
		IServiceProviderPtr provider(document); CComPtr<IOleUndoManager> manager;
		const HRESULT service = provider ? provider->QueryService(SID_SOleUndoManager, IID_IOleUndoManager, reinterpret_cast<void**>(&manager)) : E_NOINTERFACE;
		const CString original = field ? CString(static_cast<LPCWSTR>(field->value)) : CString();
		const CString generated(L"probe-generated-id");
		ProbeUnit* unit = field ? new ProbeUnit(field, original, generated) : NULL;
		const HRESULT added = manager && unit ? manager->Add(unit) : E_NOINTERFACE;
		if (SUCCEEDED(added)) field->value = static_cast<LPCWSTR>(generated);
		OLECMD undo = { IDM_UNDO, 0 }; m_doc->m_body.QueryStatus(&undo, 1);
		SendMessage(WM_COMMAND, ID_EDIT_UNDO, 0);
		const bool restored = field && CString(static_cast<LPCWSTR>(field->value)) == original;
		OLECMD redo = { IDM_REDO, 0 }; m_doc->m_body.QueryStatus(&redo, 1);
		SendMessage(WM_COMMAND, ID_EDIT_REDO, 0);
		const bool redone = field && CString(static_cast<LPCWSTR>(field->value)) == generated;
		CStringA report; report.Format("service=0x%08lX\nadd=0x%08lX\nundo-enabled=%d\nundo-restored=%d\nredo-enabled=%d\nredo-restored=%d\ncalls=%d\nmanager-received=%d\n",
			static_cast<unsigned long>(service), static_cast<unsigned long>(added), (undo.cmdf & OLECMDF_ENABLED) != 0, restored,
			(redo.cmdf & OLECMDF_ENABLED) != 0, redone, unit ? unit->calls : 0, unit && unit->receivedManager);
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		if (unit) unit->Release();
		::PostQuitMessage(SUCCEEDED(service) && SUCCEEDED(added) && restored && redone ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"description-input-undo-probe"))
	{
		wchar_t requestedLayout[16] = {};
		const DWORD layoutLength = ::GetEnvironmentVariableW(L"FBE_NEXT_TEST_DESCRIPTION_LAYOUT", requestedLayout, _countof(requestedLayout));
		const bool layoutApplied = layoutLength && layoutLength < _countof(requestedLayout) && _Settings.GetDescriptionLayout() == requestedLayout;
		wchar_t requestedMode[16] = {};
		const DWORD modeLength = ::GetEnvironmentVariableW(L"FBE_NEXT_TEST_DESCRIPTION_MODE", requestedMode, _countof(requestedMode));
		const bool modeApplied = modeLength && modeLength < _countof(requestedMode) &&
			((wcscmp(requestedMode, L"portable") == 0 && DeploymentContext::CurrentMode() == DeploymentContext::Mode::Portable) ||
			 (wcscmp(requestedMode, L"installed") == 0 && DeploymentContext::CurrentMode() == DeploymentContext::Mode::Installed));
		const bool cleanImmediately = !DocChanged();
		const ULONGLONG pumpUntil = ::GetTickCount64() + 100;
		while (::GetTickCount64() < pumpUntil) {
			MSG message = {};
			while (::PeekMessageW(&message, NULL, 0, 0, PM_REMOVE)) { ::TranslateMessage(&message); ::DispatchMessageW(&message); }
			::Sleep(1);
		}
		const bool cleanBeforeDescription = !DocChanged();
		const auto initialSavepoint = m_doc->CaptureUndoSavepoints();
		const long initialVersion = m_doc->m_body.GetVersionNumber();
		MSHTML::IHTMLDocument2Ptr initialDocument(m_doc->m_body.Document());
		MSHTML::IHTMLElementPtr initialBody(initialDocument ? initialDocument->all->item(L"fbw_body") : NULL);
		const CString initialBodyHtml(initialBody ? static_cast<LPCWSTR>(initialBody->innerHTML) : L"");
		const auto initialXml = m_doc->CreateDOMForAnalysis();
		auto initialText = [&](const wchar_t* path) -> CString {
			const MSXML2::IXMLDOMNodePtr node(initialXml ? initialXml->selectSingleNode(path) : NULL);
			return node ? CString(static_cast<LPCWSTR>(node->text)) : CString();
		};
		CString binaryText(initialText(L"/fb:FictionBook/fb:binary"));
		MSXML2::IXMLDOMElementPtr binaryElement(initialXml ? initialXml->selectSingleNode(L"/fb:FictionBook/fb:binary") : NULL);
		binaryText.Remove(L' '); binaryText.Remove(L'\r'); binaryText.Remove(L'\n'); binaryText.Remove(L'\t');
		const bool initialTitle = initialText(L"/fb:FictionBook/fb:description/fb:title-info/fb:book-title") == L"Original title";
		const bool initialAuthor = initialText(L"/fb:FictionBook/fb:description/fb:title-info/fb:author/fb:nickname") == L"Tester";
		const bool initialId = initialText(L"/fb:FictionBook/fb:description/fb:document-info/fb:id") == L"original-id";
		const bool initialBodyText = initialText(L"/fb:FictionBook/fb:body/fb:section/fb:p") == L"Body";
		const bool initialBinary = binaryElement && binaryText == L"AQIDBA==" &&
			CString(static_cast<LPCWSTR>(_bstr_t(binaryElement->getAttribute(L"id")))) == L"attachment" &&
			CString(static_cast<LPCWSTR>(_bstr_t(binaryElement->getAttribute(L"content-type")))) == L"application/octet-stream";
		const bool initialContentPreserved = initialTitle && initialAuthor && initialId && initialBodyText && initialBinary;
		ShowView(DESC);
		const bool initiallyClean = !DocChanged();
		const bool formInitiallyChanged = m_doc->m_body.IsFormChanged();
		MSHTML::IHTMLDocument2Ptr document(m_doc->m_body.Document());
		MSHTML::IHTMLDocument3Ptr document3(document);
		MSHTML::IHTMLInputTextElementPtr title(document3 ? document3->getElementById(L"tiTitle") : NULL);
		CString before = title ? CString(static_cast<LPCWSTR>(title->value)) : CString();
		auto snapshotXml = [&]() -> CString {
			const auto snapshot = m_doc->CreateDOMForAnalysis();
			return snapshot ? CString(static_cast<LPCWSTR>(snapshot->xml)) : CString();
		};
		const CString snapshotBefore = snapshotXml();
		if (title) { MSHTML::IHTMLElement2Ptr element(title); if (element) element->focus(); title->select(); }
		const HWND focused = ::GetFocus();
		if (focused) ::SendMessageW(focused, WM_CHAR, L'X', 1);
		const CString edited = title ? CString(static_cast<LPCWSTR>(title->value)) : CString();
		const CString snapshotEdited = snapshotXml();
		OLECMD status = { IDM_UNDO, 0 }; m_doc->m_body.QueryStatus(&status, 1);
		const bool dirtyAfterEdit = DocChanged();
		IServiceProviderPtr provider(document); CComPtr<IOleUndoManager> manager;
		const HRESULT service = provider ? provider->QueryService(SID_SOleUndoManager,IID_IOleUndoManager,reinterpret_cast<void**>(&manager)) : E_NOINTERFACE;
		CComPtr<IEnumOleUndoUnits> units; const HRESULT enumerated = manager ? manager->EnumUndoable(&units) : E_NOINTERFACE;
		int unitCount = 0; CComPtr<IOleUndoUnit> unit;
		if (units) while (units->Next(1,&unit,NULL) == S_OK) { ++unitCount; unit.Release(); }
		SendMessage(WM_COMMAND,ID_EDIT_UNDO,0);
		const CString afterUndo = title ? CString(static_cast<LPCWSTR>(title->value)) : CString();
		const CString snapshotUndo = snapshotXml();
		const bool dirtyAfterUndo = DocChanged();
		OLECMD redo = { IDM_REDO, 0 }; m_doc->m_body.QueryStatus(&redo, 1);
		SendMessage(WM_COMMAND,ID_EDIT_REDO,0);
		const CString afterRedo = title ? CString(static_cast<LPCWSTR>(title->value)) : CString();
		const bool saved = m_doc->Save();
		const bool cleanAfterSave = saved && !DocChanged();
		if (title) { MSHTML::IHTMLElement2Ptr element(title); if (element) element->focus(); title->select(); }
		const HWND savedFocus = ::GetFocus();
		if (savedFocus) ::SendMessageW(savedFocus, WM_CHAR, L'Z', 1);
		const CString editedAfterSave = title ? CString(static_cast<LPCWSTR>(title->value)) : CString();
		const bool dirtyAfterSavedEdit = DocChanged();
		SendMessage(WM_COMMAND, ID_EDIT_UNDO, 0);
		const CString afterSavedUndo = title ? CString(static_cast<LPCWSTR>(title->value)) : CString();
		const bool dirtyAfterSavedUndo = DocChanged();
		MSHTML::IHTMLInputTextElementPtr keywords(document3 ? document3->getElementById(L"tiKwd") : NULL);
		const CString originalKeywords = keywords ? CString(static_cast<LPCWSTR>(keywords->value)) : CString();
		if (keywords) keywords->value = L"independent keyword edit";
		if (title) { MSHTML::IHTMLElement2Ptr element(title); if (element) element->focus(); title->select(); }
		if (::GetFocus()) ::SendMessageW(::GetFocus(), WM_CHAR, L'Q', 1);
		const bool fieldEditDirty = DocChanged();
		m_doc->ReconcileDescriptionSavepoint();
		const bool fieldReconcileDirty = DocChanged();
		SendMessage(WM_COMMAND, ID_EDIT_UNDO, 0);
		const bool fieldUndoRestored = title && CString(static_cast<LPCWSTR>(title->value)) == afterRedo;
		const bool fieldUndoDirty = DocChanged();
		const bool keywordRetained = keywords && CString(static_cast<LPCWSTR>(keywords->value)) == L"independent keyword edit";
		const bool independentFieldPreserved = fieldEditDirty && fieldReconcileDirty && fieldUndoRestored && fieldUndoDirty && keywordRetained;
		if (keywords) keywords->value = static_cast<LPCWSTR>(originalKeywords);
		const bool savedBeforeLanguage = m_doc->Save() && !DocChanged();
		MSHTML::IHTMLSelectElementPtr guardLanguage(document3 ? document3->getElementById(L"tiLang") : NULL);
		const CString originalLanguage = guardLanguage ? CString(static_cast<LPCWSTR>(guardLanguage->value)) : CString();
		if (guardLanguage) guardLanguage->value = originalLanguage == L"ru" ? L"en" : L"ru";
		const CString changedLanguage = guardLanguage ? CString(static_cast<LPCWSTR>(guardLanguage->value)) : CString();
		if (title) { MSHTML::IHTMLElement2Ptr element(title); if (element) element->focus(); title->select(); }
		if (::GetFocus()) ::SendMessageW(::GetFocus(), WM_CHAR, L'L', 1);
		const bool languageEditDirty = DocChanged();
		m_doc->ReconcileDescriptionSavepoint();
		const bool languageReconcileDirty = DocChanged();
		SendMessage(WM_COMMAND, ID_EDIT_UNDO, 0);
		const bool independentLanguagePreserved = savedBeforeLanguage && guardLanguage && changedLanguage != originalLanguage && languageEditDirty && languageReconcileDirty &&
			title && CString(static_cast<LPCWSTR>(title->value)) == afterRedo && CString(static_cast<LPCWSTR>(guardLanguage->value)) == changedLanguage && DocChanged();
		if (guardLanguage) guardLanguage->value = static_cast<LPCWSTR>(originalLanguage);
		const bool savedBeforeBinary = m_doc->Save() && !DocChanged();
		MSHTML::IHTMLElementPtr binaryContainer(document3 ? document3->getElementById(L"binobj") : NULL);
		MSHTML::IHTMLElementCollectionPtr binaryNodes(binaryContainer ? MSHTML::IHTMLElement2Ptr(binaryContainer)->getElementsByTagName(L"DIV") : NULL);
		IDispatchPtr binaryDispatch;
		std::shared_ptr<const CComVariant> originalPayload;
		for (long index = 0; binaryNodes && index < binaryNodes->length && !originalPayload; ++index) {
			IDispatchPtr candidate(binaryNodes->item(_variant_t(index), _variant_t()));
			originalPayload = m_doc->m_binaryStore.payloads.Snapshot(candidate);
			if (originalPayload) binaryDispatch = candidate;
		}
		CComVariant changedPayload;
		changedPayload.vt = VT_ARRAY | VT_UI1; changedPayload.parray = ::SafeArrayCreateVector(VT_UI1, 0, 4);
		if (changedPayload.parray) { for (LONG index = 0; index < 4; ++index) { BYTE value = static_cast<BYTE>(index + 5); ::SafeArrayPutElement(changedPayload.parray, &index, &value); } }
		const HRESULT binaryChange = originalPayload && changedPayload.parray ? m_doc->m_binaryStore.payloads.Assign(binaryDispatch, std::make_shared<CComVariant>(changedPayload)) : E_FAIL;
		if (title) { MSHTML::IHTMLElement2Ptr element(title); if (element) element->focus(); title->select(); }
		if (::GetFocus()) ::SendMessageW(::GetFocus(), WM_CHAR, L'B', 1);
		const bool binaryEditDirty = DocChanged();
		m_doc->ReconcileDescriptionSavepoint();
		const bool binaryReconcileDirty = DocChanged();
		SendMessage(WM_COMMAND, ID_EDIT_UNDO, 0);
		const bool binaryUndoRestored = title && CString(static_cast<LPCWSTR>(title->value)) == afterRedo;
		const bool binaryUndoDirty = DocChanged();
		const bool independentBinaryPreserved = savedBeforeBinary && SUCCEEDED(binaryChange) && binaryEditDirty && binaryReconcileDirty && binaryUndoRestored && binaryUndoDirty;
		if (originalPayload) m_doc->m_binaryStore.payloads.Assign(binaryDispatch, originalPayload);
		const bool savedBeforeBody = m_doc->Save() && !DocChanged();
		MSHTML::IHTMLElementPtr editorBody(document3 ? document3->getElementById(L"fbw_body") : NULL);
		MSHTML::IHTMLElementCollectionPtr paragraphs(editorBody ? MSHTML::IHTMLElement2Ptr(editorBody)->getElementsByTagName(L"P") : NULL);
		MSHTML::IHTMLElementPtr bodyParagraph(paragraphs && paragraphs->length ? paragraphs->item(_variant_t(0L), _variant_t()) : NULL);
		const CString oldBodyText = bodyParagraph ? CString(static_cast<LPCWSTR>(bodyParagraph->innerText)) : CString();
		if (bodyParagraph) bodyParagraph->innerText = static_cast<LPCWSTR>(oldBodyText + L" pending body edit");
		if (title) { MSHTML::IHTMLElement2Ptr element(title); if (element) element->focus(); title->select(); }
		if (::GetFocus()) ::SendMessageW(::GetFocus(), WM_CHAR, L'W', 1);
		const bool bodyEditDirty = DocChanged();
		m_doc->ReconcileDescriptionSavepoint();
		const bool bodyReconcileDirty = DocChanged();
		SendMessage(WM_COMMAND, ID_EDIT_UNDO, 0);
		const bool independentBodyPreserved = savedBeforeBody && bodyParagraph && title && bodyEditDirty && bodyReconcileDirty &&
			CString(static_cast<LPCWSTR>(title->value)) == afterRedo &&
			CString(static_cast<LPCWSTR>(bodyParagraph->innerText)) == oldBodyText + L" pending body edit" && DocChanged();
		const CString beforeLanguage = snapshotXml();
		MSHTML::IHTMLSelectElementPtr language(document3 ? document3->getElementById(L"tiLang") : NULL);
		const CString languageBefore = language ? CString(static_cast<LPCWSTR>(language->value)) : CString();
		if (language) language->value = languageBefore == L"ru" ? L"en" : L"ru";
		const CString languageAfter = language ? CString(static_cast<LPCWSTR>(language->value)) : CString();
		const CString afterLanguage = snapshotXml();
		CStringA report; report.Format("input-focused=%d\ninput-edited=%d\ninput-dirty=%d\nsnapshot-before-present=%d\nsnapshot-changed=%d\nsnapshot-undo-restored=%d\nlanguage-changed=%d\nsnapshot-language-changed=%d\nundo-enabled=%d\nundo-units=%d\nundo-restored=%d\nundo-dirty=%d\nredo-enabled=%d\nredo-restored=%d\nservice=0x%08lX\nenumerated=0x%08lX\n",
			focused != NULL, edited != before, dirtyAfterEdit, !snapshotBefore.IsEmpty(), snapshotEdited != snapshotBefore, snapshotUndo == snapshotBefore, language && languageBefore != languageAfter, beforeLanguage != afterLanguage, (status.cmdf & OLECMDF_ENABLED) != 0, unitCount, afterUndo == before,
			dirtyAfterUndo, (redo.cmdf & OLECMDF_ENABLED) != 0, afterRedo == edited, static_cast<unsigned long>(service), static_cast<unsigned long>(enumerated));
		report.AppendFormat("clean-immediately=%d\nclean-before-description=%d\ninitially-clean=%d\ninitial-form-changed=%d\nsaved=%d\nsaved-clean=%d\nsaved-edit-changed=%d\nsaved-edit-dirty=%d\nsaved-undo-restored=%d\nsaved-undo-dirty=%d\n", cleanImmediately, cleanBeforeDescription, initiallyClean, formInitiallyChanged, saved, cleanAfterSave, editedAfterSave != afterRedo, dirtyAfterSavedEdit, afterSavedUndo == afterRedo, dirtyAfterSavedUndo);
		report.AppendFormat("initial-saved-version=%ld\ninitial-current-version=%ld\ninitial-body-equal=%d\ninitial-body-snapshot=%d\n", initialSavepoint.savedVersion, initialVersion, initialSavepoint.hasSaved && initialBodyHtml == initialSavepoint.savedHtml, initialSavepoint.hasSaved);
		report.AppendFormat("initial-content-preserved=%d\n", initialContentPreserved);
		report.AppendFormat("mode-applied=%d\nlayout-applied=%d\nindependent-language-preserved=%d\n", modeApplied, layoutApplied, independentLanguagePreserved);
		report.AppendFormat("independent-field-preserved=%d\nindependent-binary-preserved=%d\nindependent-body-preserved=%d\n", independentFieldPreserved, independentBinaryPreserved, independentBodyPreserved);
		report.AppendFormat("field-undo-restored=%d\nfield-undo-dirty=%d\nkeyword-retained=%d\nsaved-before-binary=%d\nbinary-change=0x%08lX\nbinary-undo-restored=%d\nbinary-undo-dirty=%d\n", fieldUndoRestored, fieldUndoDirty, keywordRetained, savedBeforeBinary, static_cast<unsigned long>(binaryChange), binaryUndoRestored, binaryUndoDirty);
		report.AppendFormat("initial-title=%d\ninitial-author=%d\ninitial-id=%d\ninitial-body-text=%d\ninitial-binary=%d\n", initialTitle, initialAuthor, initialId, initialBodyText, initialBinary);
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(modeApplied && layoutApplied && initialContentPreserved && independentLanguagePreserved && focused && edited != before && dirtyAfterEdit && (status.cmdf & OLECMDF_ENABLED) && unitCount > 0 &&
			afterUndo == before && (redo.cmdf & OLECMDF_ENABLED) && afterRedo == edited && saved && cleanAfterSave && editedAfterSave != afterRedo && dirtyAfterSavedEdit && afterSavedUndo == afterRedo && independentFieldPreserved && independentBinaryPreserved && independentBodyPreserved && SUCCEEDED(service) && SUCCEEDED(enumerated) ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"generated-id-runtime"))
	{
		ShowView(SOURCE);
		ShowView(DESC);
		m_doc->MarkSavePoint();
		MSHTML::IHTMLDocument2Ptr document(m_doc->m_body.Document());
		MSHTML::IHTMLDocument3Ptr document3(document);
		MSHTML::IHTMLInputTextElementPtr id(document3 ? document3->getElementById(L"diID") : NULL);
		MSHTML::IHTMLWindow2Ptr window(document ? document->parentWindow : NULL);
		const CString before = id ? CString(static_cast<LPCWSTR>(id->value)) : CString();
		const bool initiallyClean = !DocChanged();
		auto canUndo = [&]() { OLECMD command = { IDM_UNDO, 0 }; m_doc->m_body.QueryStatus(&command, 1); return (command.cmdf & OLECMDF_ENABLED) != 0; };
		auto undoUnits = [&]() {
			IServiceProviderPtr provider(document);
			CComPtr<IOleUndoManager> manager;
			if (!provider || FAILED(provider->QueryService(SID_SOleUndoManager, IID_IOleUndoManager, reinterpret_cast<void**>(&manager))) || !manager) return -1;
			CComPtr<IEnumOleUndoUnits> units;
			if (FAILED(manager->EnumUndoable(&units)) || !units) return -1;
			int count = 0; CComPtr<IOleUndoUnit> unit;
			while (units->Next(1, &unit, NULL) == S_OK) { ++count; unit.Release(); }
			return count;
		};
		const bool undoBefore = canUndo();
		const int undoUnitsBefore = undoUnits();
		::SetEnvironmentVariableW(L"FBE_NEXT_UNDO_FAILURE", L"description-id-add");
		const HRESULT failedAdd = m_doc->m_body.SetGeneratedDocumentId(CComBSTR(L"injected-id"));
		::SetEnvironmentVariableW(L"FBE_NEXT_UNDO_FAILURE", NULL);
		const bool failureRolledBack = FAILED(failedAdd) && id && CString(static_cast<LPCWSTR>(id->value)) == before &&
			!DocChanged() && undoUnits() == undoUnitsBefore && canUndo() == undoBefore;
		const HRESULT cancelAction = window ? static_cast<HRESULT>(window->execScript(_bstr_t(L"window.AskYesNo=function(){return false;}; NewDocumentID('');"), _bstr_t(L"JScript"))) : E_NOINTERFACE;
		const bool canceledClean = SUCCEEDED(cancelAction) && id && CString(static_cast<LPCWSTR>(id->value)) == before && !DocChanged();
		const HRESULT action = window ? static_cast<HRESULT>(window->execScript(_bstr_t(L"window.AskYesNo=function(){return true;}; NewDocumentID('');"), _bstr_t(L"JScript"))) : E_NOINTERFACE;
		const CString after = id ? CString(static_cast<LPCWSTR>(id->value)) : CString();
		const bool generated = SUCCEEDED(action) && !after.IsEmpty() && after != before;
		const bool undoAfter = canUndo();
		const int undoUnitsAfter = undoUnits();
		const bool dirty = DocChanged();
		SendMessage(WM_COMMAND, ID_EDIT_UNDO, 0);
		const bool undoRestored = id && CString(static_cast<LPCWSTR>(id->value)) == before;
		ShowView(SOURCE);
		const sptr_t undoLength = m_source.SendMessage(SCI_GETLENGTH);
		std::vector<char> undoXml(static_cast<size_t>(undoLength) + 1);
		m_source.SendMessage(SCI_GETTEXT, undoLength + 1, reinterpret_cast<LPARAM>(undoXml.data()));
		const CStringA originalId(CW2A(before, CP_UTF8));
		const bool undoSourceCurrent = undoRestored && strstr(undoXml.data(), originalId) != NULL;
		ShowView(DESC);
		document = m_doc->m_body.Document(); document3 = document;
		id = document3 ? document3->getElementById(L"diID") : NULL;
		if (id) MSHTML::IHTMLElement2Ptr(id)->focus();
		OLECMD redoStatus = { IDM_REDO, 0 }; m_doc->m_body.QueryStatus(&redoStatus, 1);
		SendMessage(WM_COMMAND, ID_EDIT_REDO, 0);
		const bool redoRestored = id && CString(static_cast<LPCWSTR>(id->value)) == after;
		const HRESULT secondAction = window ? static_cast<HRESULT>(window->execScript(_bstr_t(L"NewDocumentID('');"), _bstr_t(L"JScript"))) : E_NOINTERFACE;
		const CString second = id ? CString(static_cast<LPCWSTR>(id->value)) : CString();
		const bool secondGenerated = SUCCEEDED(secondAction) && !second.IsEmpty() && second != after;
		::SetEnvironmentVariableW(L"FBE_NEXT_UNDO_FAILURE", L"description-id-undo-add");
		SendMessage(WM_COMMAND, ID_EDIT_UNDO, 0);
		::SetEnvironmentVariableW(L"FBE_NEXT_UNDO_FAILURE", NULL);
		const bool undoFailureValuePreserved = id && CString(static_cast<LPCWSTR>(id->value)) == second;
		const bool undoFailureHistoryPreserved = canUndo();
		SendMessage(WM_COMMAND, ID_EDIT_UNDO, 0);
		const bool secondUndone = id && CString(static_cast<LPCWSTR>(id->value)) == after;
		SendMessage(WM_COMMAND, ID_EDIT_REDO, 0);
		const bool secondRedone = id && CString(static_cast<LPCWSTR>(id->value)) == second;
		SendMessage(WM_COMMAND, ID_EDIT_UNDO, 0);
		const bool firstRetained = id && CString(static_cast<LPCWSTR>(id->value)) == after;
		ShowView(SOURCE);
		const sptr_t length = m_source.SendMessage(SCI_GETLENGTH);
		std::vector<char> xml(static_cast<size_t>(length) + 1);
		m_source.SendMessage(SCI_GETTEXT, length + 1, reinterpret_cast<LPARAM>(xml.data()));
		const CStringA newId(CW2A(after, CP_UTF8));
		const bool sourceCurrent = generated && strstr(xml.data(), newId) != NULL;
		const bool saved = sourceCurrent && SaveFile(false) == OK;
		const bool cleanAfterSave = saved && !DocChanged();
		MSXML2::IXMLDOMDocument2Ptr reopened;
		VARIANT_BOOL loaded = VARIANT_FALSE;
		const bool reopenedOk = saved && SUCCEEDED(reopened.CreateInstance(__uuidof(MSXML2::DOMDocument60))) &&
			SUCCEEDED(reopened->raw_load(CComVariant(m_doc->m_filename), &loaded)) && loaded == VARIANT_TRUE;
		const CString reopenedXml = reopenedOk ? CString(static_cast<LPCWSTR>(reopened->xml)) : CString();
		const bool persisted = reopenedOk && reopenedXml.Find(after) >= 0;
		CStringA report; report.Format("initially-clean=%d\ncanceled-clean=%d\ngenerated=%d\ndirty=%d\nsource-current=%d\nsaved=%d\nclean-after-save=%d\npersisted=%d\n", initiallyClean, canceledClean, generated, dirty, sourceCurrent, saved, cleanAfterSave, persisted);
		report.AppendFormat("undo-before=%d\nundo-after=%d\nundo-units-before=%d\nundo-units-after=%d\nundo-restored=%d\nundo-source-current=%d\nredo-enabled=%d\nredo-restored=%d\n", undoBefore, undoAfter, undoUnitsBefore, undoUnitsAfter, undoRestored, undoSourceCurrent, (redoStatus.cmdf & OLECMDF_ENABLED) != 0, redoRestored);
		report.AppendFormat("second-generated=%d\nsecond-undone=%d\nsecond-redone=%d\nfirst-retained=%d\n", secondGenerated, secondUndone, secondRedone, firstRetained);
		report.AppendFormat("add-failure-rolled-back=%d\n", failureRolledBack);
		report.AppendFormat("undo-failure-value-preserved=%d\nundo-failure-history-preserved=%d\n", undoFailureValuePreserved, undoFailureHistoryPreserved);
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(initiallyClean && failureRolledBack && canceledClean && generated && dirty && undoAfter && undoRestored && undoSourceCurrent && (redoStatus.cmdf & OLECMDF_ENABLED) && redoRestored && secondGenerated && undoFailureValuePreserved && undoFailureHistoryPreserved && secondUndone && secondRedone && firstRetained && sourceCurrent && saved && cleanAfterSave && persisted ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"body-source-transition-runtime"))
	{
		FB::Doc* const originalDocument = m_doc;
		ShowView(BODY);
		RECT browserHost = {}; ::GetClientRect(m_doc->m_body, &browserHost);
		::MoveWindow(m_doc->m_body, 0, 0, browserHost.right + 17, browserHost.bottom + 13, FALSE);
		HWND browserChild = ::GetWindow(m_doc->m_body, GW_CHILD);
		RECT childBounds = {}, resizedClient = {};
		::GetClientRect(m_doc->m_body, &resizedClient);
		if (browserChild) { ::GetWindowRect(browserChild, &childBounds); ::MapWindowPoints(NULL, m_doc->m_body, reinterpret_cast<POINT*>(&childBounds), 2); }
		const bool bodyBrowserFits = browserChild && ::EqualRect(&childBounds, &resizedClient);
		m_view.FitPane(m_doc->m_body);
		const wchar_t* const unicodeMarker = L"\x0413\x0420\x0410\x041D\x042C_UNICODE";
		// Exercise the actual MSHTML selection -> Scintilla selection -> MSHTML
		// selection path before the legacy source-edit checks below.  The fixture
		// deliberately includes Unicode, repeated text and inline boundaries: a
		// source position alone cannot prove that these ranges survived a view
		// transition.
		auto selectBodyRange = [&](const wchar_t* elementId, const wchar_t* beginMarker, const wchar_t* endMarker, bool collapsed)
		{
			MSHTML::IHTMLDocument2Ptr document(m_doc->m_body.Document());
			MSHTML::IHTMLDocument3Ptr document3(document);
			MSHTML::IHTMLBodyElementPtr body(document ? document->body : NULL);
			MSHTML::IHTMLElementPtr element(document3 ? document3->getElementById(elementId) : NULL);
			if (!body || !element) return false;
			MSHTML::IHTMLTxtRangePtr begin(body->createTextRange());
			MSHTML::IHTMLTxtRangePtr end(body->createTextRange());
			if (!begin || !end) return false;
			begin->moveToElementText(element); end->moveToElementText(element);
			const bool wholeElement = beginMarker == NULL || *beginMarker == L'\0';
			if (!wholeElement && !begin->findText(beginMarker, 0, 0)) return false;
			if (collapsed)
				begin->collapse(VARIANT_TRUE);
			else
			{
				if (!wholeElement && (!end->findText(endMarker, 0, 0) || FAILED(begin->setEndPoint(L"EndToEnd", end)))) return false;
			}
			begin->select();
			return true;
		};
		auto selectedBodyText = [&]()
		{
			MSHTML::IHTMLDocument2Ptr document(m_doc->m_body.Document());
			MSHTML::IHTMLSelectionObjectPtr selection(document ? document->selection : NULL);
			MSHTML::IHTMLTxtRangePtr range(selection ? selection->createRange() : NULL);
			return range ? CString(static_cast<LPCWSTR>(range->text)) : CString();
		};
		CStringA selectionRoundTripDiagnostics;
		auto roundTripBodySelection = [&](const wchar_t* elementId, const wchar_t* beginMarker, const wchar_t* endMarker, const wchar_t* expectedText, bool collapsed)
		{
			const bool selected = selectBodyRange(elementId, beginMarker, endMarker, collapsed);
			if (!selected) { selectionRoundTripDiagnostics.AppendFormat("%S=select;", elementId); return false; }
			const CString before(selectedBodyText());
			ShowView(SOURCE);
			const sptr_t sourceStart = m_source.SendMessage(SCI_GETSELECTIONSTART);
			const sptr_t sourceEnd = m_source.SendMessage(SCI_GETSELECTIONEND);
			const bool sourceSelection = IsSourceActive() && m_editor_selection_state.BodySource().bodyToSourceTransferred &&
				sourceStart >= 0 && sourceEnd >= sourceStart && (collapsed ? sourceStart == sourceEnd : sourceEnd > sourceStart);
			const sptr_t sourceLength = m_source.SendMessage(SCI_GETLENGTH);
			std::vector<char> source(static_cast<size_t>(sourceLength) + 1);
			m_source.SendMessage(SCI_GETTEXT, sourceLength + 1, reinterpret_cast<LPARAM>(source.data()));
			const CStringA expectedUtf8(CW2A(expectedText, CP_UTF8));
			const bool sourceContainsExpected = collapsed || (sourceSelection && CStringA(source.data() + sourceStart,
				static_cast<int>(sourceEnd - sourceStart)).Find(expectedUtf8) >= 0);
			ShowView(BODY);
			const CString after(selectedBodyText());
			// IHTMLTxtRange::text omits inline markup content for an outer paragraph
			// range.  Its UTF-8 source range above is the authoritative check for
			// this case; the visual comparison still proves that the same range came
			// back after the round trip.
			const bool outerInlineRange = wcscmp(elementId, L"body-source-inline") == 0;
			const bool beforeMatches = collapsed ? before.IsEmpty() : (outerInlineRange ? !before.IsEmpty() : before.Find(expectedText) >= 0);
			const bool afterMatches = collapsed ? after.IsEmpty() : (outerInlineRange ? !after.IsEmpty() : after.Find(expectedText) >= 0);
			const bool exactRoundTrip = before == after;
			selectionRoundTripDiagnostics.AppendFormat("%S=%d/%d/%d/%d/%d/%d;", elementId, sourceSelection, sourceContainsExpected, beforeMatches, afterMatches, exactRoundTrip, !IsSourceActive());
			return sourceSelection && sourceContainsExpected && !IsSourceActive() && beforeMatches && afterMatches && exactRoundTrip;
		};
		const bool titleSelection = roundTripBodySelection(L"body-source-title", L"TITLE_BEGIN", L"TITLE_END", L"TITLE_BEGIN title selection TITLE_END", false);
		const bool annotationSelection = roundTripBodySelection(L"body-source-annotation", L"ANNOTATION_BEGIN", L"ANNOTATION_END", L"ANNOTATION_BEGIN annotation selection ANNOTATION_END", false);
		const bool titleCaretSelection = roundTripBodySelection(L"body-source-title", L"TITLE_BEGIN", L"TITLE_BEGIN", L"", true);

		const bool unicodeInlineSelection = roundTripBodySelection(L"body-source-unicode", L"UNICODE_BEGIN", L"UNICODE_END", unicodeMarker, false);
		const bool inlineSelection = roundTripBodySelection(L"body-source-inline", L"PREFIX", L"SUFFIX", L"PREFIX", false);
		const bool repeatedTextSelection = roundTripBodySelection(L"body-source-repeat", L"REPEAT_TOKEN", L"REPEAT_TOKEN_REPEAT_END", L"REPEAT_TOKEN middle REPEAT_TOKEN_REPEAT_END", false);
		const bool tagBoundarySelection = roundTripBodySelection(L"body-source-boundary", L"BOUNDARY_BEGIN", L"BOUNDARY_END", L"BOUNDARY_BEGIN", false);
		const bool paragraphPairSelection = roundTripBodySelection(L"body-source-pair", L"P_TO_P_BEGIN", L"P_TO_P_END", L"P_TO_P_BEGIN first paragraph", false);
		const bool nestedSectionSelection = roundTripBodySelection(L"body-source-nested", L"NESTED_BEGIN", L"NESTED_END", L"NESTED_BEGIN nested section NESTED_END", false);
		const bool collapsedCaretSelection = roundTripBodySelection(L"body-source-caret", L"CARET_UNICODE", L"CARET_UNICODE", L"", true);
		selectBodyRange(L"body-source-unicode", L"UNICODE_BEGIN", L"UNICODE_END", false);
		ShowView(SOURCE);
		const bool bodyToSourceTransfersSelection = m_editor_selection_state.BodySource().bodyToSourceTransferred;
		// Exercise a real character message immediately after transferring a
		// selection, then undo it before the remaining round-trip checks.
		const sptr_t inputStart = m_source.SendMessage(SCI_GETSELECTIONSTART);
		const sptr_t inputEnd = m_source.SendMessage(SCI_GETSELECTIONEND);
		const std::string sourceBeforeTyping = FbeScripts::SourceBytes(m_source);
		const bool sourceFocusedAfterSelection = ::GetFocus() == m_source.m_hWnd;
		::SendMessageW(m_source, WM_CHAR, L'X', 1);
		std::string expectedTyping = sourceBeforeTyping;
		if (inputStart >= 0 && inputEnd > inputStart && static_cast<size_t>(inputEnd) <= expectedTyping.size())
			expectedTyping.replace(static_cast<size_t>(inputStart), static_cast<size_t>(inputEnd - inputStart), "X");
		const bool sourceTypingAfterSelection = inputEnd > inputStart && FbeScripts::SourceBytes(m_source) == expectedTyping;
		m_source.SendMessage(SCI_UNDO);
		const bool sourceTypingUndone = FbeScripts::SourceBytes(m_source) == sourceBeforeTyping;
		const bool validInputRange = inputStart >= 0 && inputEnd > inputStart && static_cast<size_t>(inputEnd) <= sourceBeforeTyping.size();
		std::string expectedDeleted = sourceBeforeTyping;
		if (validInputRange) expectedDeleted.erase(static_cast<size_t>(inputStart), static_cast<size_t>(inputEnd - inputStart));
		m_source.SendMessage(SCI_SETSEL, inputStart, inputEnd);
		::SendMessageW(m_source, WM_KEYDOWN, VK_BACK, 1);
		const bool sourceBackspace = validInputRange && FbeScripts::SourceBytes(m_source) == expectedDeleted;
		m_source.SendMessage(SCI_UNDO);
		const bool sourceBackspaceUndo = FbeScripts::SourceBytes(m_source) == sourceBeforeTyping;
		m_source.SendMessage(SCI_SETSEL, inputStart, inputEnd);
		::SendMessageW(m_source, WM_KEYDOWN, VK_DELETE, 1);
		const bool sourceDelete = validInputRange && FbeScripts::SourceBytes(m_source) == expectedDeleted;
		m_source.SendMessage(SCI_UNDO);
		const bool sourceDeleteUndo = FbeScripts::SourceBytes(m_source) == sourceBeforeTyping;
		m_source.SendMessage(SCI_REDO);
		const bool sourceDeleteRedo = FbeScripts::SourceBytes(m_source) == expectedDeleted;
		m_source.SendMessage(SCI_UNDO);
		const bool sourceDeleteRestored = FbeScripts::SourceBytes(m_source) == sourceBeforeTyping;
		m_source.SendMessage(SCI_SETSEL, inputStart, inputEnd);
		::SendMessageW(m_source, WM_KEYDOWN, VK_LEFT, 1);
		const bool sourceArrowCollapsesSelection = validInputRange && m_source.SendMessage(SCI_GETSELECTIONSTART) == inputStart &&
			m_source.SendMessage(SCI_GETSELECTIONEND) == inputStart && FbeScripts::SourceBytes(m_source) == sourceBeforeTyping;
		wchar_t clipboardTest[4] = {};
		const bool exerciseClipboard = ::GetEnvironmentVariableW(L"FBE_NEXT_TEST_SOURCE_CLIPBOARD", clipboardTest, _countof(clipboardTest)) == 1 && clipboardTest[0] == L'1';
		bool sourceCopyPaste = true, sourceCopyPasteUndo = true, sourceCopyPasteRedo = true;
		if (exerciseClipboard && validInputRange) {
			m_source.SendMessage(SCI_SETSEL, inputStart, inputEnd);
			::SendMessageW(m_source, WM_COPY, 0, 0);
			m_source.SendMessage(SCI_SETSEL, inputEnd, inputEnd);
			::SendMessageW(m_source, WM_PASTE, 0, 0);
			std::string expectedPaste = sourceBeforeTyping;
			expectedPaste.insert(static_cast<size_t>(inputEnd), sourceBeforeTyping.substr(static_cast<size_t>(inputStart), static_cast<size_t>(inputEnd - inputStart)));
			sourceCopyPaste = FbeScripts::SourceBytes(m_source) == expectedPaste;
			m_source.SendMessage(SCI_UNDO);
			sourceCopyPasteUndo = FbeScripts::SourceBytes(m_source) == sourceBeforeTyping;
			m_source.SendMessage(SCI_REDO);
			sourceCopyPasteRedo = FbeScripts::SourceBytes(m_source) == expectedPaste;
			m_source.SendMessage(SCI_UNDO);
			sourceCopyPasteUndo = sourceCopyPasteUndo && FbeScripts::SourceBytes(m_source) == sourceBeforeTyping;
		} else if (exerciseClipboard) sourceCopyPaste = false;
		m_source.SendMessage(SCI_SETSEL, inputStart, inputEnd);
		// Preserve the slow fallback when a target is hidden by folding. An
		// already visible target must not require expanding the document.
		m_source.SendMessage(SCI_COLOURISE, 0, -1);
		const sptr_t foldSelectionStart = m_source.SendMessage(SCI_GETSELECTIONSTART);
		const sptr_t foldSelectionEnd = m_source.SendMessage(SCI_GETSELECTIONEND);
		const int foldTarget = static_cast<int>(m_source.SendMessage(SCI_LINEFROMPOSITION, foldSelectionStart));
		int foldParent = static_cast<int>(m_source.SendMessage(SCI_GETFOLDPARENT, foldTarget));
		// A portable profile may disable the XML lexer. Construct the same
		// native fold state explicitly so the fallback test has a hidden line.
		if (foldParent < 0 && foldTarget > 0) {
			foldParent = foldTarget - 1;
			m_source.SendMessage(SCI_SETFOLDLEVEL, foldParent, SC_FOLDLEVELBASE | SC_FOLDLEVELHEADERFLAG);
			m_source.SendMessage(SCI_SETFOLDLEVEL, foldTarget, SC_FOLDLEVELBASE + 1);
		}
		if (foldParent >= 0) {
			m_source.SendMessage(SCI_SETFOLDEXPANDED, foldParent, 0);
			m_source.SendMessage(SCI_HIDELINES, foldParent + 1, foldTarget);
		}
		const bool targetWasFolded = foldParent >= 0 && !m_source.SendMessage(SCI_GETLINEVISIBLE, foldTarget);
		// Re-present the cached Source selection through the production host,
		// keeping text and fold state intact rather than reloading the buffer.
		ShowView(SOURCE);
		const sptr_t foldRestoredStart = m_source.SendMessage(SCI_GETSELECTIONSTART);
		const sptr_t foldRestoredEnd = m_source.SendMessage(SCI_GETSELECTIONEND);
		const bool foldedSelectionRestored = targetWasFolded &&
			m_source.SendMessage(SCI_GETLINEVISIBLE, foldTarget) && m_source.SendMessage(SCI_GETFOLDEXPANDED, foldParent) &&
			foldRestoredStart == foldSelectionStart && foldRestoredEnd == foldSelectionEnd;

		ShowView(DESC);
		ShowView(SOURCE);
		const bool descriptionToSourceSkipsSelection = IsSourceActive() &&
			!m_editor_selection_state.BodySource().bodyToSourceTransferred;
		ShowView(SOURCE);
		const bool sourceActive = IsSourceActive();
		const sptr_t initialLength = m_source.SendMessage(SCI_GETLENGTH);
		std::vector<char> initialSource(static_cast<size_t>(initialLength) + 1);
		m_source.SendMessage(SCI_GETTEXT, initialLength + 1, reinterpret_cast<LPARAM>(initialSource.data()));
		const char* const originalMarker = "BODY_SOURCE_ORIGINAL";
		const bool sourceCurrent = strstr(initialSource.data(), originalMarker) != NULL;
		ShowView(BODY);
		const bool bodyWithoutChange = !IsSourceActive() && m_doc == originalDocument && m_doc->m_body.Document() != NULL;
		ShowView(SOURCE);
		char* marker = strstr(initialSource.data(), originalMarker);
		const bool markerFound = marker != NULL;
		if(markerFound)
		{
			const sptr_t position = static_cast<sptr_t>(marker - initialSource.data());
			m_source.SendMessage(SCI_SETSEL, position, position + strlen(originalMarker));
			m_source.SendMessage(SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>("BODY_SOURCE_EDITED"));
		}
		ShowView(BODY);
		const bool validEditApplied = markerFound && !IsSourceActive() && m_doc == originalDocument && m_doc->m_body.Document() != NULL;
		ShowView(SOURCE);
		const sptr_t editedLength = m_source.SendMessage(SCI_GETLENGTH);
		std::vector<char> editedSource(static_cast<size_t>(editedLength) + 1);
		m_source.SendMessage(SCI_GETTEXT, editedLength + 1, reinterpret_cast<LPARAM>(editedSource.data()));
		const bool editedSourceCurrent = strstr(editedSource.data(), "BODY_SOURCE_EDITED") != NULL;
		const auto sourceCheckpoint = m_doc->CaptureUndoSavepoints();
		const bool sourceCheckpointCurrent = sourceCheckpoint.hasRecovery && sourceCheckpoint.recoveryClean &&
			sourceCheckpoint.recoveryHtml.Find(L"BODY_SOURCE_EDITED") >= 0;
		m_doc->MarkDocCPIfChanged();
		const auto unchangedCheckpoint = m_doc->CaptureUndoSavepoints();
		const bool checkpointReused = unchangedCheckpoint.hasRecovery && unchangedCheckpoint.recoveryClean &&
			unchangedCheckpoint.recoveryHtml.GetString() == sourceCheckpoint.recoveryHtml.GetString();
		m_doc->MarkDocCPIfChanged(true);
		const auto forcedCheckpoint = m_doc->CaptureUndoSavepoints();
		const bool checkpointForced = forcedCheckpoint.hasRecovery && forcedCheckpoint.recoveryClean &&
			forcedCheckpoint.recoveryHtml == unchangedCheckpoint.recoveryHtml &&
			forcedCheckpoint.recoveryHtml.GetString() != unchangedCheckpoint.recoveryHtml.GetString();
		bool cycles = true;
		for(int cycle = 0; cycle < 3; ++cycle) { ShowView(BODY); cycles = cycles && !IsSourceActive(); ShowView(SOURCE); cycles = cycles && IsSourceActive(); }
		ShowView(BODY);
		MSHTML::IHTMLDocument2Ptr checkpointDocument(m_doc->m_body.Document());
		MSHTML::IHTMLElementPtr editable(checkpointDocument->all->item(L"fbw_body"));
		MSHTML::IHTMLElementCollectionPtr paragraphs(MSHTML::IHTMLElement2Ptr(editable)->getElementsByTagName(L"P"));
		MSHTML::IHTMLElementPtr lastParagraph(paragraphs->item(paragraphs->length - 1));
		MSHTML::IHTMLTxtRangePtr editRange(MSHTML::IHTMLBodyElementPtr(checkpointDocument->body)->createTextRange());
		editRange->moveToElementText(lastParagraph); editRange->collapse(VARIANT_FALSE); editRange->pasteHTML(L" checkpoint body edit");
		ShowView(SOURCE);
		const auto bodyCheckpoint = m_doc->CaptureUndoSavepoints();
		const bool bodyCheckpointCurrent = bodyCheckpoint.hasRecovery && bodyCheckpoint.recoveryClean &&
			bodyCheckpoint.recoveryHtml.Find(L"checkpoint body edit") >= 0 && bodyCheckpoint.recoveryHtml != unchangedCheckpoint.recoveryHtml;
		const sptr_t preservedSelectionStart = m_source.SendMessage(SCI_GETSELECTIONSTART);
		const sptr_t preservedSelectionEnd = m_source.SendMessage(SCI_GETSELECTIONEND);
		m_source.SendMessage(SCI_SELECTALL);
		m_source.SendMessage(SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>("<FictionBook><broken>"));
		const bool invalidRejected = !SourceToHTML();
		const bool invalidPreserved = invalidRejected && IsSourceActive() && m_doc == originalDocument &&
			m_source.SendMessage(SCI_GETLENGTH) > 0 && m_source.SendMessage(SCI_GETSELECTIONSTART) >= 0 && m_source.SendMessage(SCI_GETSELECTIONEND) >= 0;
		CStringA report;
		report.Format("source_active=%d\nsource_current=%d\nbody_without_change=%d\nvalid_edit=%d\nedited_source=%d\ncycles=%d\ninvalid_rejected=%d\ninvalid_preserved=%d\nselection_saved=%d\ntitle_selection=%d\ntitle_caret_selection=%d\nannotation_selection=%d\nunicode_inline_selection=%d\ninline_selection=%d\nrepeated_text_selection=%d\ntag_boundary_selection=%d\nparagraph_pair_selection=%d\nnested_section_selection=%d\ncollapsed_caret_selection=%d\nbody_to_source_selection=%d\ndescription_to_source_no_selection=%d\nselection_diagnostics=%s\n", sourceActive, sourceCurrent, bodyWithoutChange, validEditApplied, editedSourceCurrent, cycles, invalidRejected, invalidPreserved, preservedSelectionStart >= 0 && preservedSelectionEnd >= 0, titleSelection, titleCaretSelection, annotationSelection, unicodeInlineSelection, inlineSelection, repeatedTextSelection, tagBoundarySelection, paragraphPairSelection, nestedSectionSelection, collapsedCaretSelection, bodyToSourceTransfersSelection, descriptionToSourceSkipsSelection, static_cast<LPCSTR>(selectionRoundTripDiagnostics));
		report.AppendFormat("source_checkpoint_current=%d\ncheckpoint_reused=%d\ncheckpoint_forced=%d\nbody_checkpoint_current=%d\n", sourceCheckpointCurrent, checkpointReused, checkpointForced, bodyCheckpointCurrent);
		report.AppendFormat("body_browser_fits=%d\nfolded_selection_restored=%d\n", bodyBrowserFits ? 1 : 0, foldedSelectionRestored ? 1 : 0);
		report.AppendFormat("source_focus_after_selection=%d\nsource_typing_after_selection=%d\nsource_typing_undo=%d\n", sourceFocusedAfterSelection, sourceTypingAfterSelection, sourceTypingUndone);
		report.AppendFormat("source_backspace=%d\nsource_backspace_undo=%d\nsource_delete=%d\nsource_delete_undo=%d\nsource_delete_redo=%d\nsource_delete_restored=%d\nsource_arrow_collapse=%d\n", sourceBackspace, sourceBackspaceUndo, sourceDelete, sourceDeleteUndo, sourceDeleteRedo, sourceDeleteRestored, sourceArrowCollapsesSelection);
		report.AppendFormat("source_clipboard_exercised=%d\nsource_copy_paste=%d\nsource_copy_paste_undo=%d\nsource_copy_paste_redo=%d\n", exerciseClipboard, sourceCopyPaste, sourceCopyPasteUndo, sourceCopyPasteRedo);
		report.AppendFormat("fold_setup=%d/%d/%d;selection=%Id/%Id->%Id/%Id\n", foldParent, foldTarget, targetWasFolded ? 1 : 0, foldSelectionStart, foldSelectionEnd, foldRestoredStart, foldRestoredEnd);
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(sourceActive && sourceCurrent && bodyWithoutChange && validEditApplied && editedSourceCurrent && cycles && invalidRejected && invalidPreserved && titleSelection && titleCaretSelection && annotationSelection && unicodeInlineSelection && inlineSelection && repeatedTextSelection && tagBoundarySelection && paragraphPairSelection && nestedSectionSelection && collapsedCaretSelection && bodyToSourceTransfersSelection && descriptionToSourceSkipsSelection && sourceCheckpointCurrent && checkpointReused && checkpointForced && bodyCheckpointCurrent && bodyBrowserFits && foldedSelectionRestored && sourceFocusedAfterSelection && sourceTypingAfterSelection && sourceTypingUndone && sourceBackspace && sourceBackspaceUndo && sourceDelete && sourceDeleteUndo && sourceDeleteRedo && sourceDeleteRestored && sourceArrowCollapsesSelection && sourceCopyPaste && sourceCopyPasteUndo && sourceCopyPasteRedo ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"source-xml-declaration-encoding-runtime"))
	{
		wchar_t requestedEncoding[32] = {};
		const DWORD requestedLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_TARGET_ENCODING", requestedEncoding, _countof(requestedEncoding));
		const CString targetEncoding(requestedLength && requestedLength < _countof(requestedEncoding) ? requestedEncoding : L"");
		// The regression exercises the normal "keep opened encoding" path, not a
		// user's optional default-save-encoding override.
		_Settings.SetKeepEncoding(true);
		ShowView(SOURCE);
		const sptr_t sourceLength = m_source.SendMessage(SCI_GETLENGTH);
		std::vector<char> source(static_cast<size_t>(sourceLength) + 1);
		m_source.SendMessage(SCI_GETTEXT, sourceLength + 1, reinterpret_cast<LPARAM>(source.data()));
		const char* const encodingPrefix = "encoding=\"";
		char* const encodingStart = strstr(source.data(), encodingPrefix);
		char* const valueStart = encodingStart ? encodingStart + strlen(encodingPrefix) : NULL;
		char* const valueEnd = valueStart ? strchr(valueStart, '\"') : NULL;
		const CStringA targetUtf8(CW2A(targetEncoding, CP_UTF8));
		const bool sourceEdited = valueStart != NULL && valueEnd != NULL && !targetEncoding.IsEmpty();
		if (sourceEdited)
		{
			const sptr_t start = static_cast<sptr_t>(valueStart - source.data());
			const sptr_t end = static_cast<sptr_t>(valueEnd - source.data());
			m_source.SendMessage(SCI_SETSEL, start, end);
			m_source.SendMessage(SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>(targetUtf8.GetString()));
		}
		ShowView(BODY);
		const bool sourceApplied = sourceEdited && !IsSourceActive() &&
			m_doc->m_encoding.CompareNoCase(targetEncoding) == 0;
		DocumentSaveController saveController;
		const DocumentSaveResult saveResult = sourceApplied
			? saveController.SaveCurrent(*m_doc, m_document_session, m_document_session.Location())
			: DocumentSaveResult();
		const bool saved = saveResult.Succeeded();
		if(saved) CommitSuccessfulSave();
		CStringA report;
		report.Format("source_edited=%d\nbody=%d\nsaved=%d\n", sourceEdited, sourceApplied, saved);
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(sourceApplied && saved ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"source-xml-declaration-reopen-runtime"))
	{
		wchar_t requestedEncoding[32] = {};
		const DWORD requestedLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_TARGET_ENCODING", requestedEncoding, _countof(requestedEncoding));
		const CString targetEncoding(requestedLength && requestedLength < _countof(requestedEncoding) ? requestedEncoding : L"");
		_Settings.SetKeepEncoding(true);
		ShowView(SOURCE);
		const sptr_t sourceLength = m_source.SendMessage(SCI_GETLENGTH);
		std::vector<char> source(static_cast<size_t>(sourceLength) + 1);
		m_source.SendMessage(SCI_GETTEXT, sourceLength + 1, reinterpret_cast<LPARAM>(source.data()));
		const CStringA targetUtf8(CW2A(targetEncoding, CP_UTF8));
		CStringA expectedDeclaration;
		expectedDeclaration.Format("encoding=\"%s\"", static_cast<LPCSTR>(targetUtf8));
		const bool reopened = m_doc->m_body.Document() != NULL && m_doc->m_encoding.CompareNoCase(targetEncoding) == 0;
		const bool sourceDeclaration = reopened && IsSourceActive() && strstr(source.data(), expectedDeclaration) != NULL;
		CStringA report;
		report.Format("reopened=%d\nsource_declaration=%d\n", reopened, sourceDeclaration);
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(reopened && sourceDeclaration ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"caret-design-runtime"))
	{
		ShowView(BODY);
		m_doc->m_body.SetFocus();
		MSHTML::IHTMLDocument3Ptr document(m_doc->m_body.Document());
		MSHTML::IHTMLElementPtr body = document ? document->getElementById(L"fbw_body") : NULL;
		MSHTML::IHTMLDocument2Ptr initialDocument(m_doc->m_body.Document());
		MSHTML::IHTMLBodyElementPtr initialBody(initialDocument ? initialDocument->body : NULL);
		MSHTML::IHTMLTxtRangePtr initialRange(initialBody ? initialBody->createTextRange() : NULL);
		if (initialRange && body) {
			initialRange->moveToElementText(body);
			initialRange->collapse(VARIANT_TRUE);
			initialRange->move(L"character", 1);
			initialRange->select();
		}
		const bool selectionPrepared = initialRange && initialRange->compareEndPoints(L"StartToEnd", initialRange) == 0;
		const _bstr_t before = body ? body->outerHTML : _bstr_t();
		MSHTML::IDisplayServicesPtr display(m_doc->m_body.Document());
		MSHTML::IHTMLCaretPtr caret;
		if(display) display->GetCaret(&caret);
		if(caret) caret->Show(0);
		MSHTML::tagPOINT beforePoint = {}, afterPoint = {};
		const bool located = caret && SUCCEEDED(caret->GetLocation(&beforePoint, true));
		BOOL handled = FALSE;
		OnViewShowTextCaret(0, ID_VIEW_SHOW_TEXT_CARET, NULL, handled);
		const HWND marker = ::FindWindowExW(NULL, NULL, L"FBE.Next.CaretMarker", NULL);
		const bool shown = marker != NULL;
		OnViewShowTextCaret(0, ID_VIEW_SHOW_TEXT_CARET, NULL, handled);
		const bool reused = marker != NULL && m_caret_marker == marker &&
			::FindWindowExW(NULL, marker, L"FBE.Next.CaretMarker", NULL) == NULL;
		const bool unchanged = body && before == _bstr_t(body->outerHTML) && located &&
			SUCCEEDED(caret->GetLocation(&afterPoint, true)) &&
			beforePoint.x == afterPoint.x && beforePoint.y == afterPoint.y;
		::Sleep(1250);
		MSG pending = {};
		while(::PeekMessageW(&pending, NULL, 0, 0, PM_REMOVE)) { ::TranslateMessage(&pending); ::DispatchMessageW(&pending); }
		const bool removed = ::FindWindowExW(NULL, NULL, L"FBE.Next.CaretMarker", NULL) == NULL;
		OnViewShowTextCaret(0, ID_VIEW_SHOW_TEXT_CARET, NULL, handled);
		const bool shownBeforeFocus = m_caret_marker != NULL;
		const HWND focusedWindow = ::GetFocus();
		if(focusedWindow) ::SendMessage(focusedWindow, WM_KILLFOCUS, 0, 0);
		const bool focusRemoved = shownBeforeFocus && m_caret_marker == NULL;
		OnViewShowTextCaret(0, ID_VIEW_SHOW_TEXT_CARET, NULL, handled);
		const bool shownBeforeScroll = m_caret_marker != NULL;
		::SendMessage(m_hWnd, AU::WM_BODY_SCROLL, 0, 0);
		const bool scrollRemoved = shownBeforeScroll && m_caret_marker == NULL;
		OnViewShowTextCaret(0, ID_VIEW_SHOW_TEXT_CARET, NULL, handled);
		const bool shownBeforeActivation = m_caret_marker != NULL;
		BOOL activationHandled = FALSE;
		OnCaretMarkerActivation(WM_ACTIVATEAPP, FALSE, 0, activationHandled);
		const bool activationRemoved = shownBeforeActivation && m_caret_marker == NULL;
		OnViewShowTextCaret(0, ID_VIEW_SHOW_TEXT_CARET, NULL, handled);
		const bool shownBeforeSwitch = m_caret_marker != NULL;
		ShowView(SOURCE);
		const bool switchRemoved = shownBeforeSwitch && m_caret_marker == NULL;
		m_source.SetFocus();
		OnViewShowTextCaret(0, ID_VIEW_SHOW_TEXT_CARET, NULL, handled);
		const bool sourceIgnored = ::FindWindowExW(NULL, NULL, L"FBE.Next.CaretMarker", NULL) == NULL;
		const bool sourceUnmodified = m_source.SendMessage(SCI_GETMODIFY) == 0;
		ShowView(BODY);
		::SetForegroundWindow(m_hWnd);
		m_doc->m_body.SetFocus();
		const bool focusRestored = ::GetFocus() == m_view.GetActiveWnd() ||
			::IsChild(m_view.GetActiveWnd(), ::GetFocus());
		// MSHTML restores caret geometry asynchronously after the view switch.
		// Wait for the actual feature preconditions without forcing caret visibility.
		bool returnCaretReady = false;
		const auto caretDeadline = ::GetTickCount64() + 2000;
		do {
			MSG message = {};
			while (::PeekMessage(&message, NULL, 0, 0, PM_REMOVE)) { ::TranslateMessage(&message); ::DispatchMessage(&message); }
			MSHTML::IDisplayServicesPtr returnedDisplay(m_doc->m_body.Document());
			MSHTML::IHTMLCaretPtr returnedCaret;
			long visible = 0; MSHTML::tagPOINT location = {};
			if (returnedDisplay) returnedDisplay->GetCaret(&returnedCaret);
			returnCaretReady = ::GetForegroundWindow() == m_hWnd && returnedCaret &&
				SUCCEEDED(returnedCaret->IsVisible(&visible)) && visible != 0 &&
				SUCCEEDED(returnedCaret->GetLocation(&location, true));
			if (!returnCaretReady) ::MsgWaitForMultipleObjects(0, NULL, FALSE, 10, QS_ALLINPUT);
		} while (!returnCaretReady && ::GetTickCount64() < caretDeadline);
		OnViewShowTextCaret(0, ID_VIEW_SHOW_TEXT_CARET, NULL, handled);
		const bool shownAfterReturn = ::FindWindowExW(NULL, NULL, L"FBE.Next.CaretMarker", NULL) != NULL;
		CStringA report;
		report.Format("located=%d\nshown=%d\nreused=%d\nunchanged=%d\nremoved=%d\nfocus_removed=%d\nscroll_removed=%d\nactivation_removed=%d\nswitch_removed=%d\nsource_ignored=%d\nfocus_restored=%d\nshown_after_return=%d\n",
			located, shown, reused, unchanged, removed, focusRemoved, scrollRemoved, activationRemoved, switchRemoved,
			sourceIgnored, focusRestored, shownAfterReturn);
		report.AppendFormat("selection_prepared=%d\nsource_unmodified=%d\nreturn_caret_ready=%d\n", selectionPrepared, sourceUnmodified, returnCaretReady);
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(selectionPrepared && sourceUnmodified && returnCaretReady && located && shown && reused && unchanged && removed && focusRemoved && scrollRemoved &&
			activationRemoved && switchRemoved && sourceIgnored && focusRestored && shownAfterReturn ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"settings-residual-runtime"))
	{
		SendMessage(WM_COMMAND, MAKEWPARAM(ID_TOOLS_OPTIONS, 0), 0);
		const int result = static_cast<int>(reinterpret_cast<INT_PTR>(::GetPropW(m_hWnd, L"FBE_SETTINGS_RESIDUAL_RESULT")));
		::RemovePropW(m_hWnd, L"FBE_SETTINGS_RESIDUAL_RESULT");
		CStringA report;
		report.Format("single_error=%d\nnbsp_rollback=%d\nnbsp_retry=%d\n", !!(result & 1), !!(result & 2), !!(result & 4));
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(result == 7 ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"settings-dialog-runtime") || IsFbeTestScenario(L"settings-dialog-runtime-verify"))
	{
		const bool verifyOnly = IsFbeTestScenario(L"settings-dialog-runtime-verify");
		auto sourceText = [&]() {
			const sptr_t length = m_source.SendMessage(SCI_GETLENGTH);
			std::vector<char> text(static_cast<size_t>(length) + 1);
			m_source.SendMessage(SCI_GETTEXT, length + 1, reinterpret_cast<LPARAM>(text.data()));
			return text;
		};
		const std::vector<char> originalText = sourceText();
		const sptr_t originalPosition = m_source.SendMessage(SCI_GETCURRENTPOS);
		const sptr_t originalAnchor = m_source.SendMessage(SCI_GETANCHOR);
		const sptr_t originalUndo = m_source.SendMessage(SCI_CANUNDO);
		if (!verifyOnly)
			SendMessage(WM_COMMAND, MAKEWPARAM(ID_TOOLS_OPTIONS, 0), 0);
		const bool sourceUnchanged = originalText == sourceText() &&
			originalPosition == m_source.SendMessage(SCI_GETCURRENTPOS) &&
			originalAnchor == m_source.SendMessage(SCI_GETANCHOR) &&
			originalUndo == m_source.SendMessage(SCI_CANUNDO);
		const bool layoutPassed = verifyOnly ||
			::GetPropW(m_hWnd, L"FBE_SETTINGS_LAYOUT_PASSED") == reinterpret_cast<HANDLE>(1);
		::RemovePropW(m_hWnd, L"FBE_SETTINGS_LAYOUT_PASSED");
		const bool initialSystem = ::GetPropW(m_hWnd, L"FBE_CARET_INITIAL_SYSTEM") == reinterpret_cast<HANDLE>(1);
		::RemovePropW(m_hWnd, L"FBE_CARET_INITIAL_SYSTEM");
		const bool followupPassed = verifyOnly || ::GetPropW(m_hWnd, L"FBE_SETTINGS_FOLLOWUP_PASSED") == reinterpret_cast<HANDLE>(1);
		::RemovePropW(m_hWnd, L"FBE_SETTINGS_FOLLOWUP_PASSED");
		const bool backupPersisted = _Settings.GetCreateBackupFile();
		const bool nbspPersisted = _Settings.GetNBSPChar() == L"\x25AB";
		const bool wrapPersisted = _Settings.XmlSrcWrap();
		const bool yoPersisted = _Settings.GetCheckRussianYoUsage();
		const bool persisted = backupPersisted && nbspPersisted && wrapPersisted && yoPersisted;
		const bool applied = verifyOnly || (m_source.SendMessage(SCI_GETWRAPMODE) != SC_WRAP_NONE);
		const CaretPreferences caret = _Settings.GetCaretPreferences();
		const bool caretPersisted = caret.mode == TextCaretMode::Custom && caret.width == 4 &&
			caret.color == TextCaretColor::White && caret.blink == TextCaretBlink::Solid;
		const bool caretApplied = m_source.SendMessage(SCI_GETCARETWIDTH) ==
			UiMetrics::ScaleForDpi(4, UiMetrics::DpiForWindow(m_source.m_hWnd)) &&
			m_source.SendMessage(SCI_GETCARETPERIOD) == 0 &&
			m_source.SendMessage(SCI_GETCARETFORE) == static_cast<sptr_t>(ThemeManager::IsHighContrast() ?
				::GetSysColor(COLOR_WINDOWTEXT) : RGB(255, 255, 255));
		CStringA report;
		report.Format("followup=%d\nsettings_dialog=%d\nlayout=%d\npersisted=%d\nbackup=%d\nnbsp=%d\nwrap=%d\nyo_usage=%d\napplied=%d\ncaret_persisted=%d\ncaret_applied=%d\ncaret_initial_system=%d\nsource_unchanged=%d\n", followupPassed, !verifyOnly, layoutPassed, persisted, backupPersisted, nbspPersisted, wrapPersisted, yoPersisted, applied, caretPersisted, caretApplied, initialSystem, sourceUnchanged);
        report.AppendFormat("followup_phase=%u\n",
            static_cast<unsigned int>(reinterpret_cast<UINT_PTR>(::GetPropW(m_hWnd, L"FBE_SETTINGS_FOLLOWUP_PHASE"))));
        ::RemovePropW(m_hWnd, L"FBE_SETTINGS_FOLLOWUP_PHASE");
        report.AppendFormat("followup_theme_steps=%u\nfollowup_theme_error=%lu\n",
            static_cast<unsigned int>(reinterpret_cast<UINT_PTR>(::GetPropW(m_hWnd, L"FBE_SETTINGS_FOLLOWUP_THEME_STEPS"))),
            static_cast<unsigned long>(reinterpret_cast<UINT_PTR>(::GetPropW(m_hWnd, L"FBE_SETTINGS_FOLLOWUP_THEME_ERROR"))));
        ::RemovePropW(m_hWnd, L"FBE_SETTINGS_FOLLOWUP_THEME_STEPS");
        ::RemovePropW(m_hWnd, L"FBE_SETTINGS_FOLLOWUP_THEME_ERROR");
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(followupPassed && persisted && applied && layoutPassed && caretPersisted && caretApplied && sourceUnchanged ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"spell-dialog-layout-runtime"))
	{
		struct SpellLocale { DWORD language; LPCWSTR name; LPCWSTR ignoreAll; };
		const SpellLocale locales[] = {
			{ FBE_INTERFACE_LANGUAGE_RUSSIAN, L"ru-RU", L"Пропустить все" },
			{ FBE_INTERFACE_LANGUAGE_ENGLISH, L"en-US", L"Ignore all" },
			{ FBE_INTERFACE_LANGUAGE_GERMAN, L"de-DE", L"Alle ignorieren" },
			{ FBE_INTERFACE_LANGUAGE_UKRAINIAN, L"uk-UA", L"Пропустити все" }
		};
		const DWORD originalLanguage = _Settings.GetInterfaceLanguageID();
		const RECT originalDialogRect = dialogRect;
		dialogRect.left = -30000; dialogRect.top = -30000;
		CSpeller fallbackSpeller(U::GetProgDir() + L"Dict\\");
		CStringA report;
		bool passed = true;
		for(const SpellLocale& locale : locales)
		{
			_Settings.SetInterfaceLanguage(locale.language);
			FbePublishRuntimeLocaleName(locale.name); FbeResetRuntimeLocalization();
			CSpellDialog dialog(m_Speller ? m_Speller : &fallbackSpeller);
			const HWND window = dialog.ShowDialog(m_hWnd);
			bool valid = window != NULL;
			bool localized = false, textFits = true, geometry = false, workArea = false;
			CStringA clipped;
			if(valid)
			{
				wchar_t caption[128] = {};
				::GetDlgItemTextW(window, IDC_SPELL_IGNOREALL, caption, _countof(caption));
				localized = wcscmp(caption, locale.ignoreAll) == 0;
				auto bounds = [window](int id) {
					RECT result = {};
					::GetWindowRect(::GetDlgItem(window, id), &result);
					return result;
				};
				auto fits = [window, &bounds](int id) {
					HWND control = ::GetDlgItem(window, id);
					wchar_t text[512] = {};
					::GetWindowTextW(control, text, _countof(text));
					HDC dc = ::GetDC(control);
					if(!dc) return false;
					HFONT font = reinterpret_cast<HFONT>(::SendMessage(control, WM_GETFONT, 0, 0));
					HFONT previous = font ? static_cast<HFONT>(::SelectObject(dc, font)) : NULL;
					RECT measured = { 0, 0, 10000, 0 };
					::DrawTextW(dc, text, -1, &measured, DT_CALCRECT | DT_SINGLELINE);
					if(previous) ::SelectObject(dc, previous);
					::ReleaseDC(control, dc);
					const RECT actual = bounds(id);
					return measured.right - measured.left + 4 <= actual.right - actual.left;
				};
				for(int id : { IDC_SPELL_NOT_IN_DICTIONARY, IDC_SPELL_CHANGE_TO, IDC_SPELL_SUGGESTIONS,
					IDC_SPELL_IGNORE, IDC_SPELL_IGNOREALL, IDC_SPELL_CHANGE, IDC_SPELL_CHANGEALL,
					IDC_SPELL_ADD, IDC_SPELL_SUGGEST, IDC_SPELL_UNDO, IDCANCEL })
					if(!fits(id)) { textFits = false; clipped.AppendFormat("%d,", id); }
				const RECT list = bounds(IDC_SPELL_SUGG_LIST);
				const RECT first = bounds(IDC_SPELL_IGNORE);
				const RECT second = bounds(IDC_SPELL_IGNOREALL);
				const RECT dialogBounds = [&]() { RECT result = {}; ::GetWindowRect(window, &result); return result; }();
				MONITORINFO monitor = { sizeof(monitor) };
				geometry = list.right <= first.left && first.right <= second.left;
				workArea = ::GetMonitorInfoW(::MonitorFromRect(&dialogBounds, MONITOR_DEFAULTTONEAREST), &monitor) &&
					dialogBounds.left >= monitor.rcWork.left && dialogBounds.top >= monitor.rcWork.top &&
					dialogBounds.right <= monitor.rcWork.right && dialogBounds.bottom <= monitor.rcWork.bottom;
				valid = localized && textFits && geometry && workArea;
				dialog.DestroyWindow();
			}
			report.AppendFormat("%S=%d localized=%d text=%d geometry=%d work=%d clipped=%s\n",
				locale.name, valid, localized, textFits, geometry, workArea, static_cast<LPCSTR>(clipped));
			passed = passed && valid;
		}
		_Settings.SetInterfaceLanguage(originalLanguage);
		dialogRect = originalDialogRect;
		FbePublishRuntimeLocaleName(_Settings.GetInterfaceLocaleName()); FbeResetRuntimeLocalization();
		report.AppendFormat("result=%s\n", passed ? "pass" : "fail");
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(passed ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"backup-settings-save-runtime") || IsFbeTestScenario(L"backup-settings-save-runtime-verify"))
	{
		const bool verifyOnly = IsFbeTestScenario(L"backup-settings-save-runtime-verify");
		if (!verifyOnly)
			SendMessage(WM_COMMAND, MAKEWPARAM(ID_TOOLS_OPTIONS, 0), 0);
		const bool backupDisabled = !_Settings.GetCreateBackupFile();
		bool sourceEdited = false;
		bool sourceApplied = false;
		FILE_OP_STATUS saveStatus = FAIL;
		int saveAttempts = 0;
		bool saved = false;
		if (!verifyOnly && backupDisabled)
		{
			ShowView(SOURCE);
			const sptr_t length = m_source.SendMessage(SCI_GETLENGTH);
			std::vector<char> source(static_cast<size_t>(length) + 1);
			m_source.SendMessage(SCI_GETTEXT, length + 1, reinterpret_cast<LPARAM>(source.data()));
			const char* markerText = "BACKUP_RUNTIME_BEFORE";
			const char* replacementText = "BACKUP_RUNTIME_AFTER";
			const char* marker = strstr(source.data(), markerText);
			if (!marker)
			{
				markerText = "BACKUP_RUNTIME_AFTER";
				replacementText = "BACKUP_RUNTIME_FINAL";
				marker = strstr(source.data(), markerText);
			}
			if (marker)
			{
				const sptr_t start = static_cast<sptr_t>(marker - source.data());
				m_source.SendMessage(SCI_SETSEL, start, start + strlen(markerText));
				m_source.SendMessage(SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>(replacementText));
				sourceEdited = true;
			}
			ShowView(BODY);
			sourceApplied = sourceEdited && !IsSourceActive();
			if (sourceApplied)
			{
				for (; saveAttempts < 30;)
				{
					++saveAttempts;
					saveStatus = SaveFile(false);
					if (saveStatus == OK || m_doc->GetLastSaveError() != HRESULT_FROM_WIN32(ERROR_UNABLE_TO_REMOVE_REPLACED)) break;
					::Sleep(10);
				}
			}
			saved = saveStatus == OK;
		}
		CStringA report;
		report.Format("settings_dialog=%d\nbackup_disabled=%d\nsource_edited=%d\nsource_applied=%d\nsaved=%d\nsave_attempts=%d\nsave_status=%d\nsave_error=0x%08lX\n", !verifyOnly, backupDisabled, sourceEdited, sourceApplied, saved, saveAttempts, saveStatus, static_cast<unsigned long>(m_doc->GetLastSaveError()));
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(backupDisabled && (verifyOnly || (sourceEdited && sourceApplied && saved)) ? 0 : 1);
		return 0;
	}
#include "RuntimeTestWatchdog.inl"
	if (IsFbeTestScenario(L"diagnostic-startup-runtime"))
    {
        bool unread=false, completed=false; unsigned int heartbeat=0;
        const ULONGLONG start=::GetTickCount64();
        do {
            completed=m_diagnostic_crash_discovery.Poll(unread);
            MSG message={}; while(::PeekMessage(&message,NULL,0,0,PM_REMOVE)) { ::TranslateMessage(&message); ::DispatchMessage(&message); ++heartbeat; }
            if(!completed) ::Sleep(1);
        } while(!completed && ::GetTickCount64()-start<10000);
        BOOL handled=TRUE; OnTimer(WM_TIMER,DIAGNOSTIC_DISCOVERY_TIMER_ID,0,handled);
        const auto& metrics=m_diagnostic_crash_discovery.Metrics();
        const bool passed=completed && unread && g_hasPreviousDiagnosticCrash && metrics.metadataReads==0 && metrics.crashHeaders==512 &&
            m_diagnostic_crash_discovery.ThreadId()!=::GetCurrentThreadId() && m_diagnostic_crash_discovery.LaunchMilliseconds()<500;
        CStringA report; report.Format("startup_background=%d\ncrash_headers=%llu\nmetadata_reads=%llu\nlaunch_ms=%llu\nscan_ms=%llu\nheartbeat=%u\n",passed,metrics.crashHeaders,metrics.metadataReads,m_diagnostic_crash_discovery.LaunchMilliseconds(),m_diagnostic_crash_discovery.ScanMilliseconds(),heartbeat);
        DWORD written=0; output.Write(report,static_cast<DWORD>(report.GetLength()),&written); output.Flush(); output.Close();
        ::PostQuitMessage(passed ? 0 : 1); return 0;
    }
    if (IsFbeTestScenario(L"diagnostic-crash-retention")) {
        // Controlled real process termination; never reachable without both
        // the batch runtime scenario and FBE_NEXT_TEST_MODE guard.
        StartupTrace::HResult(L"clipboard",L"CP297",E_UNEXPECTED,L"stage=CP314; operation-stage=CP402");
        ::SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
        ::RaiseException(EXCEPTION_ACCESS_VIOLATION,EXCEPTION_NONCONTINUABLE,0,NULL);
        ::PostQuitMessage(1);return 0;
    }
    if (IsFbeTestScenario(L"diagnostic-trace-preference")) {
        wchar_t expected[2]={},toggle[2]={};
        ::GetEnvironmentVariableW(L"FBE_NEXT_TEST_TRACE_EXPECTED",expected,_countof(expected));
        ::GetEnvironmentVariableW(L"FBE_NEXT_TEST_TRACE_TOGGLE",toggle,_countof(toggle));
        DiagnosticDialog dialog(m_diagnostic_commands,&m_ui_watchdog);
        const bool passed=dialog.RunTracePreferenceTest(m_hWnd,expected[0]==L'1',toggle[0]==L'1');
        const CStringA report=dialog.TracePreferenceDetails();DWORD written=0;
        output.Write(report,static_cast<DWORD>(report.GetLength()),&written);output.Close();::PostQuitMessage(passed ? 0 : 1);return 0;
    }
	if (IsFbeTestScenario(L"diagnostic-dialog-runtime"))
	{
		DiagnosticDialog dialog(m_diagnostic_commands,&m_ui_watchdog);
		const bool passed=dialog.RunRuntimeTest(m_hWnd);
		CStringA report=passed ? "layout_dpi_theme=1\nresponsive_cancel_close=1\n" : "layout_dpi_theme=0\nresponsive_cancel_close=0\n";
		report+=dialog.RuntimeTestDetails();
		DWORD written=0; output.Write(report,static_cast<DWORD>(report.GetLength()),&written); output.Flush(); output.Close();
		::PostQuitMessage(passed ? 0 : 1); return 0;
	}
	if (IsFbeTestScenario(L"diagnostic-operations-runtime"))
	{
		bool modes = true;
		for (const auto target : { DESC, SOURCE, BODY, SOURCE, DESC, BODY }) {
			ShowView(target);
			const CString expected = target == BODY ? L"body" : (target == DESC ? L"description" : L"source");
			modes = modes && m_editor_view_state.Current() == target && StartupTrace::ActiveEditorModeName() == expected;
		}
		int line = 0, column = 0;
		const bool validated = m_doc->Validate(line, column);
		const CString filename(m_doc->m_filename), encoding(m_doc->m_encoding);
		const DocumentLocation location = m_document_session.Location();
		const FILE_OP_STATUS saved = SaveFile(true);
		const bool failedSafely = saved == FAIL && CString(m_doc->m_filename) == filename && CString(m_doc->m_encoding) == encoding && m_document_session.Location().storagePath == location.storagePath;
		CStringA report; report.Format("modes=%d\nvalidated=%d\nsave_failed_safely=%d\n", modes, validated, failedSafely);
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(modes && validated && failedSafely ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"editor-view-lifecycle-runtime"))
	{
		FB::Doc* const originalDocument = m_doc;
		const bool containerWasShown = (m_view.GetStyle() & WS_VISIBLE) != 0;
		m_view.ShowWindow(SW_HIDE);
		const bool hiddenBodyActive = m_view.GetActiveWnd() == m_doc->m_body;
		m_view.HideActiveWnd();
		const bool noHiddenPaneActive = m_view.GetActiveWnd() == NULL;
		m_view.ActivateWnd(m_doc->m_body);
		const bool hiddenContainer = hiddenBodyActive && noHiddenPaneActive &&
			m_view.GetActiveWnd() == m_doc->m_body && (m_view.GetStyle() & WS_VISIBLE) == 0;
		if (containerWasShown) m_view.ShowWindow(SW_SHOW);
		const bool originalTreePreference = _Settings.ViewDocumentTree();
		BOOL treeCommandHandled = FALSE;
		if (!originalTreePreference) OnViewTree(0, 0, NULL, treeCommandHandled);
		const auto treeShown = [&]() { return (m_document_tree.GetStyle() & WS_VISIBLE) != 0; };
		const bool treeVisibleInitially = treeShown() && m_splitter.GetSinglePaneMode() == SPLIT_PANE_NONE;
		const auto isBodyHostActive = [&]() { return m_view.GetActiveWnd() == m_doc->m_body; };
		const auto descriptionModeEnabled = [&]()
		{
			MSHTML::IHTMLDocument3Ptr document(m_doc->m_body.Document());
			MSHTML::IHTMLElementPtr description = document ? document->getElementById(L"fbw_desc") : MSHTML::IHTMLElementPtr();
			return description && description->style && U::scmp(description->style->display, L"block") == 0;
		};
		ShowView(DESC);
		const bool bodyToDescription = m_editor_view_state.Current() == DESC && m_editor_view_state.Previous() == BODY && isBodyHostActive() && descriptionModeEnabled();
		const bool treeHiddenInDescription = !treeShown() && m_splitter.GetSinglePaneMode() == SPLIT_PANE_RIGHT && _Settings.ViewDocumentTree();
		ShowView(BODY);
		const bool descriptionToBody = m_editor_view_state.Current() == BODY && m_editor_view_state.Previous() == DESC && isBodyHostActive() && !descriptionModeEnabled();
		const bool treeRestoredInBody = treeShown() && m_splitter.GetSinglePaneMode() == SPLIT_PANE_NONE;
		ShowView(SOURCE);
		const bool bodyToSource = m_editor_view_state.Current() == SOURCE && IsSourceActive() && m_editor_view_state.Previous() == BODY;
		ShowView(BODY);
		const bool sourceToBody = m_editor_view_state.Current() == BODY && !IsSourceActive() && m_editor_view_state.Previous() == SOURCE && isBodyHostActive();
		ShowView(DESC);
		ShowView(SOURCE);
		const bool descriptionToSource = m_editor_view_state.Current() == SOURCE && IsSourceActive() && m_editor_view_state.Previous() == DESC;
		ShowView(DESC);
		const bool sourceToDescription = m_editor_view_state.Current() == DESC && !IsSourceActive() && m_editor_view_state.Previous() == SOURCE && isBodyHostActive();
		ShowView(SOURCE);
		ShowView(BODY);
		ShowView(DESC);
		const bool sourceBodyDescription = m_editor_view_state.Current() == DESC && m_editor_view_state.Previous() == BODY && isBodyHostActive();
		ShowView(SOURCE);
		ShowView(DESC);
		ShowView(BODY);
		const bool sourceDescriptionBody = m_editor_view_state.Current() == BODY && m_editor_view_state.Previous() == DESC && isBodyHostActive();
		bool cycles = true;
		for(int cycle = 0; cycle < 3; ++cycle)
		{
			ShowView(DESC); cycles = cycles && m_editor_view_state.Current() == DESC && isBodyHostActive();
			ShowView(SOURCE); cycles = cycles && IsSourceActive();
			ShowView(BODY); cycles = cycles && m_editor_view_state.Current() == BODY && isBodyHostActive();
		}
		const bool documentPreserved = m_doc == originalDocument && m_doc->m_body.Document() != NULL;
		OnViewTree(0, 0, NULL, treeCommandHandled);
		ShowView(DESC);
		ShowView(BODY);
		const bool hiddenTreePreferencePreserved = !_Settings.ViewDocumentTree() && !treeShown() && m_splitter.GetSinglePaneMode() == SPLIT_PANE_RIGHT;
		if (originalTreePreference) OnViewTree(0, 0, NULL, treeCommandHandled);
		CStringA report;
		report.Format("hidden_container=%d\nbody_desc=%d\ndesc_body=%d\nbody_source=%d\nsource_body=%d\ndesc_source=%d\nsource_desc=%d\nsource_body_desc=%d\nsource_desc_body=%d\ncycles=%d\ndocument=%d\ntree_initial=%d\ntree_desc_hidden=%d\ntree_body_restored=%d\ntree_hidden_preference=%d\n", hiddenContainer, bodyToDescription, descriptionToBody, bodyToSource, sourceToBody, descriptionToSource, sourceToDescription, sourceBodyDescription, sourceDescriptionBody, cycles, documentPreserved, treeVisibleInitially, treeHiddenInDescription, treeRestoredInBody, hiddenTreePreferencePreserved);
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(hiddenContainer && bodyToDescription && descriptionToBody && bodyToSource && sourceToBody && descriptionToSource && sourceToDescription && sourceBodyDescription && sourceDescriptionBody && cycles && documentPreserved && treeVisibleInitially && treeHiddenInDescription && treeRestoredInBody && hiddenTreePreferencePreserved ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"archive-two-phase-runtime"))
	{
		wchar_t failedArchive[MAX_PATH] = {};
		const DWORD failedLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_ARCHIVE_FAILURE_PATH", failedArchive, _countof(failedArchive));
		ShowView(SOURCE);
		const sptr_t sourceLength = m_source.SendMessage(SCI_GETLENGTH);
		std::vector<char> source(static_cast<size_t>(sourceLength) + 1);
		m_source.SendMessage(SCI_GETTEXT, sourceLength + 1, reinterpret_cast<LPARAM>(source.data()));
		char* marker = strstr(source.data(), "ARCHIVE_RUNTIME_BEFORE");
		if (marker)
		{
			const size_t offset = static_cast<size_t>(marker - source.data());
			m_source.SendMessage(SCI_SETSEL, static_cast<WPARAM>(offset), static_cast<LPARAM>(offset + strlen("ARCHIVE_RUNTIME_BEFORE")));
			m_source.SendMessage(SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>("ARCHIVE_RUNTIME_UNSAVED"));
		}
		const CString filenameBefore(m_doc->m_filename);
		CString mruBefore;
		for (int index = 0; index < m_recentDocuments.List().m_arrDocs.GetSize(); ++index)
			mruBefore.AppendFormat(L"%d:%s\n", index, static_cast<LPCWSTR>(m_recentDocuments.List().m_arrDocs[index].szDocName));
		const FILE_OP_STATUS result = failedLength && failedLength < _countof(failedArchive) ? LoadFile(failedArchive) : FAIL;
		const bool sourceStillModified = m_source.SendMessage(SCI_GETMODIFY) != 0;
		const bool sameDocument = CString(m_doc->m_filename) == filenameBefore && !m_document_session.Location().IsArchive();
		CString mruAfter;
		for (int index = 0; index < m_recentDocuments.List().m_arrDocs.GetSize(); ++index)
			mruAfter.AppendFormat(L"%d:%s\n", index, static_cast<LPCWSTR>(m_recentDocuments.List().m_arrDocs[index].szDocName));
		const bool mruUnchanged = mruAfter == mruBefore;
		CStringA report;
		report.Format("open_cancelled=%d\nmodified=%d\nsame_document=%d\nmru_unchanged=%d\n", result == CANCELLED, sourceStillModified, sameDocument, mruUnchanged);
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(result == CANCELLED && sourceStillModified && sameDocument && mruUnchanged ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"failed-open-runtime"))
	{
		wchar_t failedPath[MAX_PATH] = {};
		const DWORD failedLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_FAILED_OPEN_PATH", failedPath, _countof(failedPath));
		FB::Doc* const original = m_doc;
		const DocumentLocation originalLocation = m_document_session.Location();
		CString mruBefore;
		for (int index = 0; index < m_recentDocuments.List().m_arrDocs.GetSize(); ++index) mruBefore.AppendFormat(L"%d:%s\n", index, static_cast<LPCWSTR>(m_recentDocuments.List().m_arrDocs[index].szDocName));
		// This probe verifies that a failed open preserves the current document;
		// it is not a Save-prompt scenario.  A freshly initialized MSHTML body
		// may report a transient modification, which would otherwise block the
		// unattended attempt before the missing path is evaluated.
		m_doc->MarkSavePoint(); m_source.SendMessage(SCI_SETSAVEPOINT);
		const FILE_OP_STATUS result = failedLength && failedLength < _countof(failedPath) ? LoadFile(failedPath) : FAIL;
		CString mruAfter;
		for (int index = 0; index < m_recentDocuments.List().m_arrDocs.GetSize(); ++index) mruAfter.AppendFormat(L"%d:%s\n", index, static_cast<LPCWSTR>(m_recentDocuments.List().m_arrDocs[index].szDocName));
		const bool preserved = result == FAIL && m_doc == original && FB::Doc::m_active_doc == m_doc &&
			m_document_session.Location().storagePath == originalLocation.storagePath && mruBefore == mruAfter;
		CStringA report; report.Format("failed=%d\nidentity=%d\nactive=%d\nsession=%d\nmru_unchanged=%d\n", result == FAIL, m_doc == original, FB::Doc::m_active_doc == m_doc, m_document_session.Location().storagePath == originalLocation.storagePath, mruBefore == mruAfter);
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(preserved ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"save-as-failure-runtime"))
	{
		wchar_t destination[MAX_PATH] = {};
		const DWORD length = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_SAVE_PATH", destination, _countof(destination));
		const CString filename(m_doc->m_filename), encoding(m_doc->m_encoding);
		const bool nameValid = m_doc->m_namevalid;
		const FictionBookFileType type = m_doc->GetDocumentFileType();
		const DocumentLocation location = m_document_session.Location();
		const FILE_OP_STATUS result = length && length < _countof(destination) ? SaveFile(true) : FAIL;
		const bool preserved = result == FAIL && CString(m_doc->m_filename) == filename && m_doc->m_namevalid == nameValid &&
			m_doc->GetDocumentFileType() == type && CString(m_doc->m_encoding) == encoding &&
			m_document_session.Location().storagePath == location.storagePath;
		CStringA report; report.Format("failed=%d\nfilename=%d\nnamevalid=%d\ntype=%d\nencoding=%d\nsession=%d\n", result == FAIL,
			CString(m_doc->m_filename) == filename, m_doc->m_namevalid == nameValid, m_doc->GetDocumentFileType() == type,
			CString(m_doc->m_encoding) == encoding, m_document_session.Location().storagePath == location.storagePath);
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Close();
		::PostQuitMessage(preserved ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"save-as-cancel-runtime"))
	{
		const CString filename(m_doc->m_filename), encoding(m_doc->m_encoding);
		const bool nameValid = m_doc->m_namevalid;
		const FictionBookFileType type = m_doc->GetDocumentFileType();
		const DocumentLocation location = m_document_session.Location();
		const FILE_OP_STATUS result = SaveFile(true);
		const bool preserved = result == CANCELLED && CString(m_doc->m_filename) == filename && m_doc->m_namevalid == nameValid &&
			m_doc->GetDocumentFileType() == type && CString(m_doc->m_encoding) == encoding && m_document_session.Location().storagePath == location.storagePath;
		CStringA report; report.Format("cancelled=%d\nfilename=%d\nnamevalid=%d\ntype=%d\nencoding=%d\nsession=%d\n", result == CANCELLED,
			CString(m_doc->m_filename) == filename, m_doc->m_namevalid == nameValid, m_doc->GetDocumentFileType() == type,
			CString(m_doc->m_encoding) == encoding, m_document_session.Location().storagePath == location.storagePath);
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Close();
		::PostQuitMessage(preserved ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"save-as-mru-runtime"))
	{
		const CString source(m_doc->m_filename); CString target; wchar_t path[MAX_PATH] = {};
		const DWORD length = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_SAVE_PATH", path, _countof(path));
		if (length && length < _countof(path)) target = path;
		const FILE_OP_STATUS first = target.IsEmpty() ? FAIL : SaveFile(true);
		const FILE_OP_STATUS repeat = first == OK ? SaveFile(true) : FAIL;
		int targetCount = 0, sourceCount = 0;
		for (int i = 0; i < m_recentDocuments.List().m_arrDocs.GetSize(); ++i) { const CString item(m_recentDocuments.List().m_arrDocs[i].szDocName); if (item.CompareNoCase(target) == 0) ++targetCount; if (item.CompareNoCase(source) == 0) ++sourceCount; }
		CString beforeCancel; for (int i = 0; i < m_recentDocuments.List().m_arrDocs.GetSize(); ++i) beforeCancel.AppendFormat(L"%s\n", static_cast<LPCWSTR>(m_recentDocuments.List().m_arrDocs[i].szDocName));
		::SetEnvironmentVariable(L"FBE_NEXT_TEST_SAVE_PATH", NULL);
		const FILE_OP_STATUS cancelled = SaveFile(true);
		CString afterCancel; for (int i = 0; i < m_recentDocuments.List().m_arrDocs.GetSize(); ++i) afterCancel.AppendFormat(L"%s\n", static_cast<LPCWSTR>(m_recentDocuments.List().m_arrDocs[i].szDocName));
		wchar_t failure[MAX_PATH] = {}; const DWORD failureLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_SAVE_FAIL_PATH", failure, _countof(failure));
		if (failureLength && failureLength < _countof(failure)) ::SetEnvironmentVariable(L"FBE_NEXT_TEST_SAVE_PATH", failure);
		const FILE_OP_STATUS failed = failureLength ? SaveFile(true) : FAIL;
		CString afterFailure; for (int i = 0; i < m_recentDocuments.List().m_arrDocs.GetSize(); ++i) afterFailure.AppendFormat(L"%s\n", static_cast<LPCWSTR>(m_recentDocuments.List().m_arrDocs[i].szDocName));
		CString firstKey; const bool menuFirst = m_recentDocuments.List().GetFromList(ID_FILE_MRU_FIRST, firstKey) && firstKey.CompareNoCase(target) == 0;
		const bool passed = first == OK && repeat == OK && menuFirst && targetCount == 1 && sourceCount == 1 && cancelled == CANCELLED && beforeCancel == afterCancel && failed == FAIL && afterCancel == afterFailure;
		CStringA report; report.Format("saved=%d\nfirst=%d\nunique=%d\nsource=%d\ncancel=%d\nfailed=%d\ncancel_unchanged=%d\nfailed_unchanged=%d\n", first == OK && repeat == OK, menuFirst, targetCount == 1, sourceCount == 1, cancelled == CANCELLED, failed == FAIL, beforeCancel == afterCancel, afterCancel == afterFailure);
		DWORD written = 0; output.Write(report, report.GetLength(), &written); output.Close(); if (!passed) { ::PostQuitMessage(1); return 0; } ::PostMessage(m_hWnd, WM_CLOSE, 0, 0); return 0;
	}
	if (IsFbeTestScenario(L"save-as-mru-restart-runtime"))
	{
		wchar_t expected[MAX_PATH] = {}; const DWORD length = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_SAVE_PATH", expected, _countof(expected));
		const CString target = length && length < _countof(expected) ? CString(expected) : CString();
		CString firstKey; const bool first = m_recentDocuments.List().GetFromList(ID_FILE_MRU_FIRST, firstKey) && firstKey.CompareNoCase(target) == 0;
		CStringA report; report.Format("order=%d\n", first); DWORD written = 0; output.Write(report, report.GetLength(), &written); output.Close(); ::PostQuitMessage(first ? 0 : 1); return 0;
	}
	if (IsFbeTestScenario(L"malformed-source-fallback-runtime"))
	{
		wchar_t malformedPath[MAX_PATH] = {};
		const DWORD malformedLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_MALFORMED_SOURCE_PATH", malformedPath, _countof(malformedPath));
		FB::Doc* const original = m_doc;
		const DocumentLocation originalLocation = m_document_session.Location();
		// This scenario verifies malformed-source fallback, not an unrelated
		// startup document Save prompt caused by MSHTML's transient dirty bit.
		m_doc->MarkSavePoint(); m_source.SendMessage(SCI_SETSAVEPOINT);
		const FILE_OP_STATUS result = malformedLength && malformedLength < _countof(malformedPath) ? LoadFile(malformedPath) : FAIL;
		const bool sourceFallback = result == OK && m_doc == original && FB::Doc::m_active_doc == m_doc &&
			m_bad_xml && m_bad_filename == malformedPath && m_editor_view_state.Current() == SOURCE &&
			m_document_session.Location().storagePath == originalLocation.storagePath;
		CStringA report; report.Format("fallback=%d\nidentity=%d\nactive=%d\nsession=%d\nsource=%d\n", result == OK, m_doc == original,
			FB::Doc::m_active_doc == m_doc, m_document_session.Location().storagePath == originalLocation.storagePath, m_bad_xml && m_bad_filename == malformedPath && m_editor_view_state.Current() == SOURCE);
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(sourceFallback ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"malformed-source-correction-runtime"))
	{
		wchar_t malformedPath[MAX_PATH] = {};
		const DWORD malformedLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_MALFORMED_SOURCE_PATH", malformedPath, _countof(malformedPath));
		// The correction route starts by deliberately opening malformed input;
		// keep the disposable startup document from presenting a Save prompt.
		m_doc->MarkSavePoint(); m_source.SendMessage(SCI_SETSAVEPOINT);
		const FILE_OP_STATUS loadResult = malformedLength && malformedLength < _countof(malformedPath) ? LoadFile(malformedPath) : FAIL;
		const CStringA correctedSource("<?xml version=\"1.0\" encoding=\"utf-8\"?><FictionBook xmlns=\"http://www.gribuser.ru/xml/fictionbook/2.0\"><description><title-info><genre>prose</genre><author><first-name>Runtime</first-name><last-name>Correction</last-name></author><book-title>Corrected</book-title><lang>en</lang></title-info><document-info><author><nickname>FBE Next</nickname></author><program-used>FBE Next</program-used><date value=\"2026-09-14\">14 September 2026</date><id>malformed-correction-test</id><version>1.0</version></document-info></description><body><section><p>CORRECTED_MALFORMED_SOURCE</p></section></body></FictionBook>");
		if (loadResult == OK && m_bad_xml)
		{
			m_source.SendMessage(SCI_CLEARALL);
			m_source.SendMessage(SCI_APPENDTEXT, correctedSource.GetLength(), reinterpret_cast<LPARAM>(correctedSource.GetString()));
			ShowView(BODY);
		}
		MSXML2::IXMLDOMDocument2Ptr correctedDom(m_doc->CreateDOM(m_doc->m_encoding));
		const CString correctedXml(correctedDom ? static_cast<LPCWSTR>(correctedDom->xml) : L"");
		const bool corrected = loadResult == OK && m_editor_view_state.Current() == BODY && !m_bad_xml &&
			m_doc->m_filename == malformedPath && m_doc->m_namevalid &&
			m_document_session.Location().storagePath == malformedPath && correctedXml.Find(L"CORRECTED_MALFORMED_SOURCE") >= 0;
		CStringA report; report.Format("loaded=%d\nbody=%d\nbad_xml_cleared=%d\nfilename=%d\nnamevalid=%d\nsession=%d\ndom=%d\n", loadResult == OK,
			m_editor_view_state.Current() == BODY, !m_bad_xml, m_doc->m_filename == malformedPath, m_doc->m_namevalid,
			m_document_session.Location().storagePath == malformedPath, correctedXml.Find(L"CORRECTED_MALFORMED_SOURCE") >= 0);
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(corrected ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"successful-open-runtime"))
	{
		wchar_t openedPath[MAX_PATH] = {};
		const DWORD openedLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_SUCCESSFUL_OPEN_PATH", openedPath, _countof(openedPath));
		FB::Doc* const original = m_doc;
		// This scenario validates a successful open, not the Save prompt for
		// the disposable startup document created by the test harness.
		m_doc->MarkSavePoint(); m_source.SendMessage(SCI_SETSAVEPOINT);
		const FILE_OP_STATUS result = openedLength && openedLength < _countof(openedPath) ? LoadFile(openedPath) : FAIL;
		const bool opened = result == OK && m_doc != original && FB::Doc::m_active_doc == m_doc &&
			m_document_session.Location().storagePath == openedPath && m_doc->m_filename == openedPath && m_doc->m_body.Document() != NULL;
		CStringA report; report.Format("opened=%d\nidentity_changed=%d\nactive=%d\nsession=%d\nfilename=%d\nvalid=%d\n", result == OK,
			m_doc != original, FB::Doc::m_active_doc == m_doc, m_document_session.Location().storagePath == openedPath,
			m_doc->m_filename == openedPath, m_doc->m_body.Document() != NULL);
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(opened ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"startup-blank-clean-runtime"))
	{
		const bool clean = m_document_session.HasDocument() && m_document_session.Location().storagePath.IsEmpty() && !DocChanged();
		const auto saved = m_doc->CaptureUndoSavepoints();
		MSHTML::IHTMLDocument3Ptr document(m_doc->m_editor.Document());
		MSHTML::IHTMLElementPtr body = document ? document->getElementById(L"fbw_body") : NULL;
		const bool bodyMatches = body && saved.hasSaved && CString(static_cast<LPCWSTR>(body->innerHTML)) == saved.savedHtml;
		ShowView(SOURCE);
		m_source.SendMessage(SCI_GOTOPOS, 0);
		m_source.SendMessage(SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>(" "));
		const bool editDirty = DocChanged() && m_source.SendMessage(SCI_CANUNDO);
		m_source.SendMessage(SCI_UNDO);
		const bool undoClean = !DocChanged();
		m_source.SendMessage(SCI_REDO);
		const bool redoDirty = DocChanged();
		CStringA report; report.Format("startup_clean=%d\nbody_matches=%d\nedit_dirty=%d\nundo_clean=%d\nredo_dirty=%d\n", clean, bodyMatches, editDirty, undoClean, redoDirty);
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(clean && editDirty && undoClean && redoDirty ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"new-document-runtime"))
	{
		FB::Doc* const original = m_doc;
		// New-document lifecycle is tested independently from the startup
		// document's unsaved-changes prompt.
		m_doc->MarkSavePoint(); m_source.SendMessage(SCI_SETSAVEPOINT);
		BOOL handled = FALSE;
		OnFileNew(0, ID_FILE_NEW, NULL, handled);
		const bool created = m_doc != original && FB::Doc::m_active_doc == m_doc &&
			m_document_session.Location().storagePath.IsEmpty() && !m_document_session.Location().IsArchive() && m_doc->m_body.Document() != NULL;
		CStringA report; report.Format("created=%d\nidentity_changed=%d\nactive=%d\nsession_new=%d\nvalid=%d\n", created,
			m_doc != original, FB::Doc::m_active_doc == m_doc, m_document_session.Location().storagePath.IsEmpty() && !m_document_session.Location().IsArchive(), m_doc->m_body.Document() != NULL);
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(created ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"reload-success-runtime") || IsFbeTestScenario(L"reload-failure-runtime"))
	{
		const bool expectSuccess = IsFbeTestScenario(L"reload-success-runtime");
		FB::Doc* const original = m_doc;
		const DocumentLocation originalLocation = m_document_session.Location();
		if (!expectSuccess) ::SetEnvironmentVariable(L"FBE_NEXT_FAULT_INJECT", L"api-load-return-false");
		const bool result = ReloadFile();
		if (!expectSuccess) ::SetEnvironmentVariable(L"FBE_NEXT_FAULT_INJECT", NULL);
		const bool reloadMatches = expectSuccess
			? result && m_doc != original && FB::Doc::m_active_doc == m_doc && m_doc->m_body.Document() != NULL &&
				m_document_session.Location().storagePath == originalLocation.storagePath
			: !result && m_doc == original && FB::Doc::m_active_doc == m_doc &&
				m_document_session.Location().storagePath == originalLocation.storagePath;
		CStringA report; report.Format("reloaded=%d\nidentity=%d\nactive=%d\nsession=%d\nvalid=%d\n", result,
			expectSuccess ? m_doc != original : m_doc == original, FB::Doc::m_active_doc == m_doc,
			m_document_session.Location().storagePath == originalLocation.storagePath, m_doc->m_body.Document() != NULL);
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(reloadMatches ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"archive-mru-runtime"))
	{
		wchar_t secondEntry[MAX_PATH] = {}, occurrenceText[16] = {}, normalEntries[8192] = {};
		const DWORD secondLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_ARCHIVE_MRU_SECOND_ENTRY", secondEntry, _countof(secondEntry));
		::GetEnvironmentVariable(L"FBE_NEXT_TEST_ARCHIVE_MRU_SECOND_OCCURRENCE", occurrenceText, _countof(occurrenceText));
		const DWORD normalLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_ARCHIVE_MRU_NORMAL_ENTRIES", normalEntries, _countof(normalEntries));
		DocumentLocation first = m_document_session.Location(), second = first;
		unsigned int occurrence = 0;
		const bool secondValid = secondLength > 0 && secondLength < _countof(secondEntry) && FbeRecentDocuments::ParseArchiveMruUnsigned(occurrenceText, occurrence);
		second.entryPath = secondEntry; second.entryOccurrence = occurrence; second.documentType = DetectFictionBookFileType(second.entryPath);
		// This scenario exercises MRU routing, not the unsaved-changes prompt.
		// A freshly loaded MSHTML document can carry a transient form-change bit.
		m_doc->MarkSavePoint(); m_source.SendMessage(SCI_SETSAVEPOINT);
		ResolvedOpenDocument secondResolved; FbeArchive::Error secondError;
		const bool secondFound = secondValid && FbeArchiveUi::ResolveOpenRequest(second.storagePath, secondResolved, &second, &secondError);
		const FILE_OP_STATUS secondOpen = secondFound ? LoadFile(second.storagePath, &second) : CANCELLED;
		if (secondOpen == OK) FbeRecentDocuments::RememberArchiveMruRecord(m_recentDocuments.List(), m_document_session.Location());
		if (secondOpen == OK) { m_doc->MarkSavePoint(); m_source.SendMessage(SCI_SETSAVEPOINT); }
		std::vector<FbeRecentDocuments::ArchiveMruRecord> records; FbeRecentDocuments::ReadArchiveMruRecords(records);
		const CString firstKey = FbeRecentDocuments::ArchiveMruKey(first); WORD firstCommand = 0;
		for (int offset = 0; offset < m_recentDocuments.List().m_arrDocs.GetSize() && offset <= ID_FILE_MRU_LAST - ID_FILE_MRU_FIRST; ++offset) { CString key; const WORD candidate = MruCommandId(offset); if (m_recentDocuments.List().GetFromList(candidate, key) && key == firstKey) { firstCommand = candidate; break; } }
		DocumentLocation menuFirst;
		const bool menuLookup = firstCommand != 0 && FbeRecentDocuments::FindArchiveMruRecord(firstKey, menuFirst) && FbeRecentDocuments::SameArchiveMruIdentity(menuFirst, first);
		BOOL handled = FALSE;
		const LRESULT handlerResult = secondOpen == OK && menuLookup ? OnFileOpenMRU(0, firstCommand, NULL, handled) : 1;
		const FILE_OP_STATUS firstOpen = handlerResult == 0 && FbeRecentDocuments::SameArchiveMruIdentity(m_document_session.Location(), first) ? OK : FAIL;
		const bool reopenedFirst = firstOpen == OK && FbeRecentDocuments::SameArchiveMruIdentity(m_document_session.Location(), first);
		bool normalEntriesOpened = normalLength == 0;
		if (normalLength > 0 && normalLength < _countof(normalEntries))
		{
			int position = 0;
			while (position >= 0)
			{
				const CString normal = CString(normalEntries).Tokenize(L"|", position);
				if (normal.IsEmpty()) continue;
				m_doc->MarkSavePoint(); m_source.SendMessage(SCI_SETSAVEPOINT);
				if (LoadFile(normal) != OK) { normalEntriesOpened = false; break; }
				FbeRecentDocuments::RememberNormalMruRecord(m_recentDocuments.List(), normal);
				normalEntriesOpened = true;
			}
		}
		FbeRecentDocuments::ReadArchiveMruRecords(records);
		bool hasFirst = false, hasSecond = false;
		for (size_t index = 0; index < records.size(); ++index) { hasFirst = hasFirst || FbeRecentDocuments::SameArchiveMruIdentity(records[index].location, first); hasSecond = hasSecond || FbeRecentDocuments::SameArchiveMruIdentity(records[index].location, second); }
		DocumentLocation missing = first; missing.entryPath = L"missing.fb2"; missing.entryOccurrence = 0;
		ResolvedOpenDocument ignored; FbeArchive::Error missingError;
		const bool missingRejected = !FbeArchiveUi::ResolveOpenRequest(first.storagePath, ignored, &missing, &missingError) && missingError.code == FbeArchive::ErrorCode::EntryNotFound;
		int menuCount = 0, visibleArchiveCount = 0; bool menuClean = true;
		const HMENU mruMenu = m_recentDocuments.List().GetMenuHandle();
		if (mruMenu != NULL) for (int index = 0; index < ::GetMenuItemCount(mruMenu); ++index)
		{
			MENUITEMINFO item = { sizeof(item) }; item.fMask = MIIM_ID;
			if (!::GetMenuItemInfo(mruMenu, index, TRUE, &item) || item.wID < ID_FILE_MRU_FIRST || item.wID > ID_FILE_MRU_LAST) continue;
			++menuCount; wchar_t caption[512] = {};
			::GetMenuString(mruMenu, item.wID, caption, _countof(caption), MF_BYCOMMAND);
			DocumentLocation captionLocation;
			if (FbeRecentDocuments::ParseArchiveMruKey(caption, captionLocation) || CString(caption).Left(2) == L"&1" || CString(caption).Left(2) == L"&2") menuClean = false;
			CString key; DocumentLocation keyLocation;
			if (m_recentDocuments.List().GetFromList(item.wID, key) && FbeRecentDocuments::ParseArchiveMruKey(key, keyLocation)) ++visibleArchiveCount;
		}
		const CString firstCaption = FbeRecentDocuments::ArchiveMruCaption(firstKey), secondCaption = FbeRecentDocuments::ArchiveMruCaption(FbeRecentDocuments::ArchiveMruKey(second));
		const bool captionsDifferent = firstCaption.Compare(secondCaption) != 0;
		const bool captionsDistinct = visibleArchiveCount < 2 || captionsDifferent;
		FbeRecentDocuments::WritePortableMru(m_recentDocuments.List());
		CStringA report; report.Format("first=%d\nsecond=%d\nmenu_lookup=%d\nsecond_found=%d\nsecond_error=%d\nsecond_open=%d\nfirst_open=%d\nreopened_first=%d\nmissing_entry=%d\narchive_records=%u\nnormal_entries=%d\nmenu_count=%d\nmenu_clean=%d\ncaption_diff=%d\ncaptions_distinct=%d\n", hasFirst, hasSecond, menuLookup, secondFound, static_cast<int>(secondError.code), secondOpen, firstOpen, reopenedFirst, missingRejected, static_cast<unsigned int>(records.size()), normalEntriesOpened, menuCount, menuClean, captionsDifferent, captionsDistinct);
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Close();
		::PostQuitMessage(hasFirst && hasSecond && menuLookup && reopenedFirst && missingRejected && normalEntriesOpened && menuCount > 0 && menuCount <= 10 && menuClean && captionsDistinct ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"archive-mru-restart-runtime"))
	{
		std::vector<CString> order; FbeRecentDocuments::ReadMruOrder(order);
		const int count = m_recentDocuments.List().m_arrDocs.GetSize(); bool exactOrder = count == 10 && order.size() == 10, cleanMenu = true; int menuCount = 0;
		for (int index = 0; index < count && exactOrder; ++index) exactOrder = CString(m_recentDocuments.List().m_arrDocs[index].szDocName) == order[order.size() - 1 - index];
		for (int index = 0; index < count; ++index) { DocumentLocation location; if (FbeRecentDocuments::ParseArchiveMruKey(CString(m_recentDocuments.List().m_arrDocs[index].szDocName), location) && !FbeRecentDocuments::FindArchiveMruRecord(CString(m_recentDocuments.List().m_arrDocs[index].szDocName), location)) cleanMenu = false; }
		std::vector<CString> captions; const HMENU menu = m_recentDocuments.List().GetMenuHandle(); int emptyCaption = 0, rawCaption = 0, numberedCaption = 0, duplicateCaption = 0, disabledCaption = 0; bool minimalFolderContexts = false;
		if (menu == NULL) cleanMenu = false; else for (int index = 0; index < ::GetMenuItemCount(menu); ++index)
		{
			MENUITEMINFO item = { sizeof(item) }; item.fMask = MIIM_ID;
			if (!::GetMenuItemInfo(menu, index, TRUE, &item) || item.wID < ID_FILE_MRU_FIRST || item.wID > ID_FILE_MRU_LAST) continue;
			++menuCount; wchar_t text[512] = {}; ::GetMenuString(menu, index, text, _countof(text), MF_BYPOSITION); CString caption(text), parsedKey;
			item.fMask = MIIM_STATE; ::GetMenuItemInfo(menu, index, TRUE, &item); DocumentLocation parsed; if (caption == m_recentDocuments.List().m_szNoEntries) { ++emptyCaption; } if ((item.fState & (MFS_DISABLED | MFS_GRAYED)) != 0) { cleanMenu = false; ++disabledCaption; } if (FbeRecentDocuments::ParseArchiveMruKey(caption, parsed)) { cleanMenu = false; ++rawCaption; } if (caption.GetLength() > 1 && caption[0] == L'&' && caption[1] >= L'0' && caption[1] <= L'9') { cleanMenu = false; ++numberedCaption; }
			for (size_t previous = 0; previous < captions.size(); ++previous) if (captions[previous].CompareNoCase(caption) == 0) { cleanMenu = false; ++duplicateCaption; }
			captions.push_back(caption);
		}
		for (size_t i = 0; i < captions.size(); ++i) for (size_t j = i + 1; j < captions.size(); ++j)
			if ((captions[i].Find(L"A\\Books\\archive.zip") >= 0 && captions[j].Find(L"B\\Books\\archive.zip") >= 0) || (captions[i].Find(L"B\\Books\\archive.zip") >= 0 && captions[j].Find(L"A\\Books\\archive.zip") >= 0)) minimalFolderContexts = true;
		wchar_t path[MAX_PATH] = {}, entry[MAX_PATH] = {}, occurrenceText[16] = {}; ::GetEnvironmentVariable(L"FBE_NEXT_TEST_ARCHIVE_MRU_REOPEN_PATH", path, _countof(path)); ::GetEnvironmentVariable(L"FBE_NEXT_TEST_ARCHIVE_ENTRY", entry, _countof(entry)); ::GetEnvironmentVariable(L"FBE_NEXT_TEST_ARCHIVE_OCCURRENCE", occurrenceText, _countof(occurrenceText));
		unsigned int occurrence = 0; DocumentLocation target; target.containerKind = DetectDocumentContainerKind(path); target.storagePath = path; target.entryPath = entry; target.entryOccurrence = FbeRecentDocuments::ParseArchiveMruUnsigned(occurrenceText, occurrence) ? occurrence : 0; target.documentType = DetectFictionBookFileType(target.entryPath);
		WORD command = 0; const CString key = FbeRecentDocuments::ArchiveMruKey(target); for (int offset = 0; offset < count && offset <= ID_FILE_MRU_LAST - ID_FILE_MRU_FIRST; ++offset) { CString value; const WORD candidate = MruCommandId(offset); if (m_recentDocuments.List().GetFromList(candidate, value) && value == key) { command = candidate; break; } }
		// The restart probe exercises persisted MRU routing.  Its disposable
		// startup document can still have MSHTML's transient dirty bit set, which
		// must not surface an unattended Save prompt before the real MRU command.
		m_doc->MarkSavePoint(); m_source.SendMessage(SCI_SETSAVEPOINT);
		BOOL handled = FALSE; const bool reopened = command != 0 && OnFileOpenMRU(0, command, NULL, handled) == 0 && FbeRecentDocuments::SameArchiveMruIdentity(m_document_session.Location(), target);
		auto getMruItem = [&](UINT id, CString& caption, UINT& state) -> bool
		{
			const int position = FindMenuPositionByCommand(menu, id);
			if (position < 0) return false;
			wchar_t text[512] = {}; if (::GetMenuString(menu, position, text, _countof(text), MF_BYPOSITION) <= 0) return false;
			MENUITEMINFO item = { sizeof(item) }; item.fMask = MIIM_STATE;
			if (!::GetMenuItemInfo(menu, position, TRUE, &item)) return false;
			caption = text; state = item.fState; return true;
		};
		CString firstBefore, firstRussian; UINT firstBeforeState = 0, firstRussianState = 0;
		CString firstKeyBefore; const bool firstMappedBefore = m_recentDocuments.List().GetFromList(ID_FILE_MRU_FIRST, firstKeyBefore);
		const bool firstVisibleBefore = getMruItem(ID_FILE_MRU_FIRST, firstBefore, firstBeforeState);
		_Settings.SetInterfaceLanguage(FBE_INTERFACE_LANGUAGE_RUSSIAN);
		FbePublishRuntimeLocaleName(_Settings.GetInterfaceLocaleName()); FbeResetRuntimeLocalization(); RefreshLocalizedMainFrameUi();
		const bool russianLocaleSelected = _Settings.GetInterfaceLocaleName() == L"ru-RU";
		const CString russianLocalizedEmpty = FbeLoadRuntimeStringByKey(L"fbe.menu.idr_mainframe.recent.empty", L"No Recent Files");
		CString firstKeyRussian; const bool firstMappedRussian = m_recentDocuments.List().GetFromList(ID_FILE_MRU_FIRST, firstKeyRussian);
		const bool firstVisibleRussian = getMruItem(ID_FILE_MRU_FIRST, firstRussian, firstRussianState);
		const bool nonEmptyMruLocalized = firstMappedBefore && firstMappedRussian && firstKeyBefore == firstKeyRussian && firstVisibleBefore && firstVisibleRussian && firstBefore == firstRussian && firstRussian != m_recentDocuments.List().m_szNoEntries && (firstRussianState & (MFS_DISABLED | MFS_GRAYED)) == 0;
		m_recentDocuments.List().m_arrDocs.RemoveAll();
		RefreshMruEmptyStateText(m_recentDocuments.List()); FbeRecentDocuments::RebuildMruMenu(m_recentDocuments.List());
		CString russianEmpty; UINT russianEmptyState = 0; int russianEmptyCount = 0;
		for (int index = 0; index < ::GetMenuItemCount(menu); ++index) { MENUITEMINFO item = { sizeof(item) }; item.fMask = MIIM_ID; if (::GetMenuItemInfo(menu, index, TRUE, &item) && item.wID >= ID_FILE_MRU_FIRST && item.wID <= ID_FILE_MRU_LAST) ++russianEmptyCount; }
		const bool russianEmptyOk = russianEmptyCount == 1 && !russianLocalizedEmpty.IsEmpty() && getMruItem(ID_FILE_MRU_FIRST, russianEmpty, russianEmptyState) && russianEmpty == russianLocalizedEmpty && (russianEmptyState & (MFS_DISABLED | MFS_GRAYED)) != 0;
		_Settings.SetInterfaceLanguage(FBE_INTERFACE_LANGUAGE_ENGLISH);
		FbePublishRuntimeLocaleName(_Settings.GetInterfaceLocaleName()); FbeResetRuntimeLocalization(); RefreshLocalizedMainFrameUi();
		CString englishEmpty; UINT englishEmptyState = 0; int englishEmptyCount = 0;
		for (int index = 0; index < ::GetMenuItemCount(menu); ++index) { MENUITEMINFO item = { sizeof(item) }; item.fMask = MIIM_ID; if (::GetMenuItemInfo(menu, index, TRUE, &item) && item.wID >= ID_FILE_MRU_FIRST && item.wID <= ID_FILE_MRU_LAST) ++englishEmptyCount; }
		const bool englishEmptyOk = englishEmptyCount == 1 && getMruItem(ID_FILE_MRU_FIRST, englishEmpty, englishEmptyState) && englishEmpty == L"No Recent Files" && (englishEmptyState & (MFS_DISABLED | MFS_GRAYED)) != 0;
		CStringA report; report.Format("count=%d\nmenu_count=%d\norder=%d\nclean=%d\nempty=%d\ndisabled=%d\nraw=%d\nnumbered=%d\nduplicates=%d\nfolders=%d\nreopened=%d\nlocalized_nonempty=%d\nrussian_empty=%d\nrussian_locale=%d\nenglish_empty=%d\n", count, menuCount, exactOrder, cleanMenu, emptyCaption, disabledCaption, rawCaption, numberedCaption, duplicateCaption, minimalFolderContexts, reopened, nonEmptyMruLocalized, russianEmptyOk, russianLocaleSelected, englishEmptyOk); DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Close();
		::PostQuitMessage(count == 10 && menuCount == 10 && exactOrder && cleanMenu && reopened && nonEmptyMruLocalized && russianEmptyOk && englishEmptyOk ? 0 : 1); return 0;
	}
	if (IsFbeTestScenario(L"archive-recovery-create"))
	{
		ShowView(SOURCE);
		const sptr_t length = m_source.SendMessage(SCI_GETLENGTH); std::vector<char> source(static_cast<size_t>(length) + 1);
		m_source.SendMessage(SCI_GETTEXT, length + 1, reinterpret_cast<LPARAM>(source.data()));
		char* marker = strstr(source.data(), "ARCHIVE_RUNTIME_BEFORE");
		if (marker) { const size_t offset = static_cast<size_t>(marker - source.data()); m_source.SendMessage(SCI_SETSEL, offset, offset + strlen("ARCHIVE_RUNTIME_BEFORE")); m_source.SendMessage(SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>("ARCHIVE_RUNTIME_RECOVERY")); }
		else
		{
			char* titleEnd = strstr(source.data(), "</book-title>");
			const size_t offset = titleEnd ? static_cast<size_t>(titleEnd - source.data()) : static_cast<size_t>(length);
			m_source.SendMessage(SCI_SETSEL, offset, offset);
			m_source.SendMessage(SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>(" ARCHIVE_RUNTIME_RECOVERY"));
		}
		const bool saved = SourceToHTML() && SaveRecoveryNow(); CStringA report; report.Format("recovery_created=%d\n", saved); DWORD written = 0; output.Write(report, report.GetLength(), &written); output.Close(); ::PostQuitMessage(saved ? 0 : 1); return 0;
	}
	if (IsFbeTestScenario(L"normal-recovery-create"))
	{
		ShowView(SOURCE); const sptr_t length = m_source.SendMessage(SCI_GETLENGTH); std::vector<char> source(static_cast<size_t>(length) + 1); m_source.SendMessage(SCI_GETTEXT, length + 1, reinterpret_cast<LPARAM>(source.data())); char* marker = strstr(source.data(), "Runtime");
		if (marker) { const size_t offset = static_cast<size_t>(marker - source.data()); m_source.SendMessage(SCI_SETSEL, offset, offset + strlen("Runtime")); m_source.SendMessage(SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>("RECOVERY_NORMAL")); }
		const bool saved = marker != NULL && SourceToHTML() && SaveRecoveryNow(); CStringA report; report.Format("recovery_created=%d\n", saved); DWORD written = 0; output.Write(report, report.GetLength(), &written); output.Close(); ::PostQuitMessage(saved ? 0 : 1); return 0;
	}
	if (IsFbeTestScenario(L"normal-recovery-verify"))
	{
		ShowView(SOURCE); const sptr_t length = m_source.SendMessage(SCI_GETLENGTH); std::vector<char> source(static_cast<size_t>(length) + 1); m_source.SendMessage(SCI_GETTEXT, length + 1, reinterpret_cast<LPARAM>(source.data()));
		const bool payload = strstr(source.data(), "RECOVERY_NORMAL") != NULL; const bool identity = CString(m_doc->m_filename) == L"Untitled.fb2" && !m_doc->m_namevalid && m_document_session.Location().storagePath.IsEmpty();
		const bool dirty = DocChanged(); const bool cleaned = ::GetFileAttributes(m_recovery.SnapshotPath()) == INVALID_FILE_ATTRIBUTES;
		CStringA report; report.Format("payload=%d\nidentity=%d\ndirty=%d\ncleaned=%d\n", payload, identity, dirty, cleaned); DWORD written = 0; output.Write(report, report.GetLength(), &written); output.Close(); ::PostQuitMessage(identity && dirty && cleaned ? 0 : 1); return 0;
	}
	if (IsFbeTestScenario(L"archive-recovery-verify"))
	{
		ShowView(SOURCE);
		const sptr_t length = m_source.SendMessage(SCI_GETLENGTH); std::vector<char> source(static_cast<size_t>(length) + 1);
		m_source.SendMessage(SCI_GETTEXT, length + 1, reinterpret_cast<LPARAM>(source.data()));
		const bool archive = m_document_session.Location().IsArchive(); const bool fbd = m_doc->GetDocumentFileType() == FictionBookFileType::Fbd;
		const bool payload = strstr(source.data(), "ARCHIVE_RUNTIME_RECOVERY") != NULL;
		CStringA report; report.Format("archive=%d\nfbd=%d\nrecovery_payload=%d\n", archive, fbd, payload); DWORD written = 0; output.Write(report, report.GetLength(), &written); output.Close(); ::PostQuitMessage(archive && payload ? 0 : 1); return 0;
	}
	if (IsFbeTestScenario(L"archive-recovery-external-verify"))
	{
		ShowView(SOURCE);
		const sptr_t length = m_source.SendMessage(SCI_GETLENGTH); std::vector<char> source(static_cast<size_t>(length) + 1);
		m_source.SendMessage(SCI_GETTEXT, length + 1, reinterpret_cast<LPARAM>(source.data()));
		char* marker = strstr(source.data(), "ARCHIVE_RUNTIME_RECOVERY");
		if (marker) { const size_t offset = static_cast<size_t>(marker - source.data()); m_source.SendMessage(SCI_SETSEL, offset, offset + strlen("ARCHIVE_RUNTIME_RECOVERY")); m_source.SendMessage(SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>("ARCHIVE_RUNTIME_EXTERNAL")); }
		::SetEnvironmentVariable(L"FBE_NEXT_TEST_ARCHIVE_SAVE_ERROR", NULL);
		const bool blocked = marker != NULL && SourceToHTML() && SaveFile(false) == FAIL;
		wchar_t errorCode[16] = {};
		const bool modifiedExternally = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_ARCHIVE_SAVE_ERROR", errorCode, _countof(errorCode)) > 0 &&
			_wtoi(errorCode) == static_cast<int>(FbeArchive::ErrorCode::ModifiedExternally);
		CStringA report; report.Format("archive=%d\nblocked=%d\nmodified_externally=%d\nerror_code=%S\n", m_document_session.Location().IsArchive(), blocked, modifiedExternally, errorCode); DWORD written = 0; output.Write(report, report.GetLength(), &written); output.Close(); ::PostQuitMessage(blocked && modifiedExternally ? 0 : 1); return 0;
	}
	if (IsFbeTestScenario(L"table-roundtrip"))
	{
		const ULONGLONG start = ::GetTickCount64();
		auto appendTablePhase = [&](const char* phase)
		{
			const sptr_t sourceLength = m_source.SendMessage(SCI_GETLENGTH);
			std::vector<char> source(static_cast<size_t>(sourceLength) + 1);
			m_source.SendMessage(SCI_GETTEXT, sourceLength + 1, reinterpret_cast<LPARAM>(source.data()));
			auto countTag = [&](const char* tag) -> long { long count = 0; for (const char* position = source.data(); (position = strstr(position, tag)) != NULL; ++position) ++count; return count; };
			const FbeSourceDiagnostics::ProcessMemorySnapshot memory = FbeSourceDiagnostics::GetProcessMemorySnapshot();
			CStringA row;
			row.Format("%s\t%I64u\t%I64u\t%I64u\t%ld\t%ld\t%ld\t%ld\r\n", phase,
				::GetTickCount64() - start, static_cast<unsigned __int64>(memory.privateBytes), static_cast<unsigned __int64>(memory.workingSetBytes),
				countTag("<table"), countTag("<tr"), countTag("<td"), countTag("<th"));
			DWORD written = 0; output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Flush();
		};
		CStringA header("phase\telapsed_ms\tprivate_bytes\tworking_set_bytes\ttable_count\ttr_count\ttd_count\tth_count\r\n");
		DWORD written = 0; output.Write(header, static_cast<DWORD>(header.GetLength()), &written); output.Flush();
		appendTablePhase("open-complete");
		for (int cycle = 1; cycle <= 5; ++cycle)
		{
			CStringA phase; phase.Format("source-%d-start", cycle); appendTablePhase(phase);
			ShowView(SOURCE); phase.Format("source-%d-complete", cycle); appendTablePhase(phase);
			phase.Format("body-%d-start", cycle); appendTablePhase(phase);
			ShowView(BODY); phase.Format("body-%d-complete", cycle); appendTablePhase(phase);
		}
		appendTablePhase("save-1-start");
		if (!m_doc->Save())
		{
			appendTablePhase("save-1-failed;phase=save-1;operation=Save;actual_hresult=unavailable;symbolic_hresult=unavailable");
			output.Close(); ::PostQuitMessage(1); return 0;
		}
		appendTablePhase("save-1-complete");
		output.Close(); PostMessage(WM_CLOSE); return 0;
	}
	if (IsFbeTestScenario(L"save-decision-quit-new") ||
		IsFbeTestScenario(L"save-decision-quit-open") ||
		IsFbeTestScenario(L"save-decision-quit-close"))
	{
		// Exercise the real operation and DiscardChanges gate. A real modal
		// dialog receives WM_QUIT, which must remain a non-answer and retain its
		// exit code; the -b report does not bypass the close confirmation here.
		ThemeManager::SetSelectedTheme(INTERFACE_THEME_DARK);
		ShowView(SOURCE);
		const sptr_t length = m_source.SendMessage(SCI_GETLENGTH);
		std::vector<char> source(static_cast<size_t>(length) + 1);
		m_source.SendMessage(SCI_GETTEXT, length + 1, reinterpret_cast<LPARAM>(source.data()));
		char* marker = strstr(source.data(), "SAVE_DECISION_ORIGINAL");
		if(marker != NULL)
		{
			const sptr_t start = static_cast<sptr_t>(marker - source.data());
			m_source.SendMessage(SCI_SETSEL, start, start + strlen("SAVE_DECISION_ORIGINAL"));
			m_source.SendMessage(SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>("SAVE_DECISION_DIRTY"));
		}
		const bool dirtyBefore = marker != NULL && DocChanged();
		const bool savedRecovery = dirtyBefore && SaveRecoveryNow();
		const CString snapshot = m_recovery.SnapshotPath();
		const bool recoveryBefore = savedRecovery && ::GetFileAttributesW(snapshot) != INVALID_FILE_ATTRIBUTES;
		FB::Doc* const original = m_doc;
		const CString filename = m_doc->m_filename;
		const DWORD mainThread = ::GetCurrentThreadId();
		bool found = false, posted = false;
		std::thread driver([&]()
		{
			HWND dialog = NULL;
			for(int retry = 0; retry < 500 && dialog == NULL; ++retry)
			{
				dialog = ::FindWindowW(L"FBEThemedMessageDialog", NULL);
				if(dialog == NULL) ::Sleep(10);
			}
			found = dialog != NULL;
			posted = ::PostThreadMessageW(mainThread, WM_QUIT, 73, 0) != FALSE;
		});
		bool cancelled = false;
		if(IsFbeTestScenario(L"save-decision-quit-new"))
		{
			BOOL handled = FALSE;
			OnFileNew(0, ID_FILE_NEW, NULL, handled);
			cancelled = m_doc == original;
		}
		else if(IsFbeTestScenario(L"save-decision-quit-open"))
		{
			wchar_t path[MAX_PATH] = {};
			const DWORD pathLength = ::GetEnvironmentVariableW(L"FBE_NEXT_TEST_DECISION_OPEN_PATH", path, _countof(path));
			cancelled = pathLength > 0 && pathLength < _countof(path) && LoadFile(path) == CANCELLED;
		}
		else
		{
			BOOL handled = FALSE;
			OnClose(WM_CLOSE, 0, 0, handled);
			cancelled = ::IsWindow(m_hWnd) != FALSE;
		}
		driver.join();
		const bool retained = cancelled && m_doc == original && m_doc->m_filename == filename &&
			DocChanged() && ::GetFileAttributesW(snapshot) != INVALID_FILE_ATTRIBUTES;
		CStringA report; report.Format("dirty_before\t%d\r\nrecovery_before\t%d\r\ndialog_found\t%d\r\nquit_posted\t%d\r\noperation_cancelled\t%d\r\ndocument_retained\t%d\r\nrecovery_retained\t%d\r\n",
			dirtyBefore ? 1 : 0, recoveryBefore ? 1 : 0, found ? 1 : 0, posted ? 1 : 0,
			cancelled ? 1 : 0, retained ? 1 : 0,
			::GetFileAttributesW(snapshot) != INVALID_FILE_ATTRIBUTES ? 1 : 0);
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written);
		output.Close();
		return 0; // The modal dialog reposted WM_QUIT with code 73.
	}
	if (IsFbeTestScenario(L"settings-editor-page-runtime"))
	{
		// Standalone page fixtures have a main-frame parent, not SettingsDlg.
		// Exercise Validate/Commit explicitly; the full-dialog smoke covers OK.
		CStringA report("case\tpassed\r\n");
		auto append = [&](const char* name, bool passed)
		{
			CStringA row; row.Format("%s\t%d\r\n", name, passed ? 1 : 0);
			report += row;
		};
		const CString missingCustom(L"C:\\FBE-missing-background.gif");
		_Settings.SetEditorBackgroundKind(L"custom");
		_Settings.SetEditorBackgroundCustomPath(missingCustom);
		_Settings.SetColorBG(CLR_DEFAULT);
		_Settings.SetColorFG(CLR_DEFAULT);
		{
			CSettingsEditorPage page;
			const HWND window = page.Create(m_hWnd);
			const HWND picker = ::GetDlgItem(window, IDC_EDITOR_BACKGROUND_IMAGE);
			const int count = static_cast<int>(::SendMessageW(picker, CB_GETCOUNT, 0, 0));
			const bool selected = count >= 2 && ::SendMessageW(picker, CB_GETCURSEL, 0, 0) == count - 1;
			if(page.Validate()) page.Commit();
			append("missing-custom-ok", window != NULL && selected &&
				_Settings.GetEditorBackgroundKind() == L"custom" && _Settings.GetEditorBackgroundCustomPath() == missingCustom &&
				_Settings.GetColorBG() == CLR_DEFAULT && _Settings.GetColorFG() == CLR_DEFAULT);
			page.DestroyWindow();
		}
		_Settings.SetEditorBackgroundKind(L"builtin");
		_Settings.SetEditorBackgroundId(L"removed-preset");
		{
			CSettingsEditorPage page;
			const HWND window = page.Create(m_hWnd);
			if(page.Validate()) page.Commit();
			const HWND picker = ::GetDlgItem(window, IDC_EDITOR_BACKGROUND_IMAGE);
			const bool unavailableSelected = ::SendMessageW(picker, CB_GETCURSEL, 0, 0) == ::SendMessageW(picker, CB_GETCOUNT, 0, 0) - 1;
			append("missing-builtin-ok", window != NULL && unavailableSelected && _Settings.GetEditorBackgroundKind() == L"builtin" &&
				_Settings.GetEditorBackgroundId() == L"removed-preset");
			page.DestroyWindow();
		}
		{
			CSettingsEditorPage page;
			const HWND window = page.Create(m_hWnd);
			const HWND picker = ::GetDlgItem(window, IDC_EDITOR_BACKGROUND_IMAGE);
			::SendMessageW(picker, CB_SETCURSEL, 0, 0);
			::SendMessageW(window, WM_COMMAND, MAKEWPARAM(IDC_EDITOR_BACKGROUND_IMAGE, CBN_SELCHANGE), reinterpret_cast<LPARAM>(picker));
			if(page.Validate()) page.Commit();
			append("missing-builtin-select-none", window != NULL && _Settings.GetEditorBackgroundKind() == L"none");
			page.DestroyWindow();
		}
		_Settings.SetEditorBackgroundKind(L"custom");
		_Settings.SetEditorBackgroundCustomPath(missingCustom);
		{
			CSettingsEditorPage page;
			const HWND window = page.Create(m_hWnd);
			const HWND picker = ::GetDlgItem(window, IDC_EDITOR_BACKGROUND_IMAGE);
			::SendMessageW(picker, CB_SETCURSEL, 0, 0);
			::SendMessageW(window, WM_COMMAND, MAKEWPARAM(IDC_EDITOR_BACKGROUND_IMAGE, CBN_SELCHANGE), reinterpret_cast<LPARAM>(picker));
			page.CancelChanges();
			append("custom-cancel", window != NULL && _Settings.GetEditorBackgroundKind() == L"custom" &&
				_Settings.GetEditorBackgroundCustomPath() == missingCustom);
			page.DestroyWindow();
		}
		ThemeManager::SetSelectedTheme(INTERFACE_THEME_LIGHT);
		_Settings.SetEditorBackgroundKind(L"none");
		_Settings.SetColorFG(RGB(230, 20, 20));
		_Settings.SetColorBG(RGB(20, 40, 220));
		{
			CSettingsEditorPage page;
			const HWND window = page.Create(m_hWnd);
			const EditorBackgroundColors defaults = EditorBackgrounds::ResolveBodyColors(
				CLR_DEFAULT, CLR_DEFAULT, L"none", L"", ThemeManager::IsHighContrast());
			const EditorBackgroundColors automaticForeground = EditorBackgrounds::ResolveBodyColors(
				CLR_DEFAULT, RGB(20, 40, 220), L"none", L"", ThemeManager::IsHighContrast());
			const EditorBackgroundColors automaticBackground = EditorBackgrounds::ResolveBodyColors(
				RGB(230, 20, 20), CLR_DEFAULT, L"none", L"", ThemeManager::IsHighContrast());
			append("explicit-colors-automatic-swatches", window != NULL &&
				page.m_foreground.GetColor() == RGB(230, 20, 20) && page.m_background.GetColor() == RGB(20, 40, 220) &&
				page.m_foreground.GetDefaultColor() == automaticForeground.foreground &&
				page.m_background.GetDefaultColor() == automaticBackground.background &&
				page.m_backgroundPreview.m_foreground == RGB(230, 20, 20) &&
				page.m_backgroundPreview.m_background == RGB(20, 40, 220));
			BOOL handled = FALSE;
			page.m_foreground.SetColor(CLR_DEFAULT);
			page.OnPreviewColorChanged(0, NULL, handled);
			append("foreground-automatic-preview", window != NULL &&
				page.m_foreground.GetColor() == CLR_DEFAULT &&
				page.m_foreground.GetDefaultColor() == automaticForeground.foreground &&
				page.m_backgroundPreview.m_foreground == automaticForeground.foreground &&
				page.m_backgroundPreview.m_background == RGB(20, 40, 220));
			page.m_background.SetColor(CLR_DEFAULT);
			page.OnPreviewColorChanged(0, NULL, handled);
			append("background-automatic-preview", window != NULL &&
				page.m_background.GetColor() == CLR_DEFAULT &&
				page.m_background.GetDefaultColor() == defaults.background &&
				page.m_backgroundPreview.m_foreground == defaults.foreground &&
				page.m_backgroundPreview.m_background == defaults.background);
			if(page.Validate()) page.Commit();
			m_doc->ApplyConfChanges();
			MSHTML::IHTMLDocument3Ptr document3(m_doc->m_body.Document());
			MSHTML::IHTMLElementPtr style(document3 ? document3->getElementById(L"fbe-runtime-body-colors") : NULL);
			MSHTML::IHTMLStyleElementPtr styleElement(style);
			MSHTML::IHTMLStyleSheetPtr sheet(styleElement ? styleElement->styleSheet : NULL);
			CString css(sheet ? static_cast<LPCWSTR>(sheet->cssText) : L"");
			css.MakeLower();
			CString expectedForeground, expectedBackground;
			expectedForeground.Format(L"#%02x%02x%02x", GetRValue(defaults.foreground),
				GetGValue(defaults.foreground), GetBValue(defaults.foreground));
			expectedBackground.Format(L"#%02x%02x%02x", GetRValue(defaults.background),
				GetGValue(defaults.background), GetBValue(defaults.background));
			append("automatic-settings-body", _Settings.GetColorFG() == CLR_DEFAULT &&
				_Settings.GetColorBG() == CLR_DEFAULT && css.Find(expectedForeground) >= 0 &&
				css.Find(expectedBackground) >= 0);
			page.DestroyWindow();
		}
		{
			CSettingsEditorPage page;
			const HWND window = page.Create(m_hWnd);
			BOOL handled = FALSE;
			page.m_foreground.SetColor(RGB(230, 20, 20));
			page.m_background.SetColor(RGB(20, 40, 220));
			page.OnPreviewColorChanged(0, NULL, handled);
			const bool preview = page.m_backgroundPreview.m_foreground == RGB(230, 20, 20) &&
				page.m_backgroundPreview.m_background == RGB(20, 40, 220);
			if(page.Validate()) page.Commit();
			append("automatic-to-explicit", window != NULL && preview &&
				_Settings.GetColorFG() == RGB(230, 20, 20) && _Settings.GetColorBG() == RGB(20, 40, 220));
			page.DestroyWindow();
		}
		auto verifyAutomaticPair = [&](const char* name, InterfaceTheme theme, const wchar_t* kind, const wchar_t* id,
			DWORD foreground, DWORD background, COLORREF expectedForeground, COLORREF expectedBackground)
		{
			ThemeManager::SetSelectedTheme(theme);
			ThemeManager::ApplyToAllThreadWindows(::GetCurrentThreadId());
			_Settings.SetEditorBackgroundKind(kind); _Settings.SetEditorBackgroundId(id);
			_Settings.SetColorFG(foreground); _Settings.SetColorBG(background);
			CSettingsEditorPage page;
			const HWND window = page.Create(m_hWnd);
			const bool lowContrast = EditorBackgrounds::HasLowTextContrast(expectedForeground, expectedBackground);
			const CString expectedTitle = FbeLoadRuntimeStringByKey(
				lowContrast ? L"fbe.settings.editor.low_contrast" : L"fbe.settings.editor.preview",
				lowContrast ? L"Preview — low text contrast" : L"Preview");
			const bool previewAndSwatches = window != NULL &&
				page.m_backgroundPreview.m_foreground == expectedForeground && page.m_backgroundPreview.m_background == expectedBackground &&
				(foreground != CLR_DEFAULT || page.m_foreground.GetDefaultColor() == expectedForeground) &&
				(background != CLR_DEFAULT || page.m_background.GetDefaultColor() == expectedBackground) &&
				U::GetWindowText(page.GetDlgItem(IDC_EDITOR_PREVIEW_GROUP)) == expectedTitle;
			if(page.Validate()) page.Commit();
			m_doc->ApplyConfChanges();
			MSHTML::IHTMLDocument3Ptr document3(m_doc->m_body.Document());
			MSHTML::IHTMLElementPtr style(document3 ? document3->getElementById(L"fbe-runtime-body-colors") : NULL);
			MSHTML::IHTMLStyleElementPtr styleElement(style);
			MSHTML::IHTMLStyleSheetPtr sheet(styleElement ? styleElement->styleSheet : NULL);
			CString css(sheet ? static_cast<LPCWSTR>(sheet->cssText) : L""); css.MakeLower();
			CString expectedForegroundCss, expectedBackgroundCss;
			expectedForegroundCss.Format(L"#%02x%02x%02x", GetRValue(expectedForeground), GetGValue(expectedForeground), GetBValue(expectedForeground));
			expectedBackgroundCss.Format(L"#%02x%02x%02x", GetRValue(expectedBackground), GetGValue(expectedBackground), GetBValue(expectedBackground));
			append(name, previewAndSwatches && css.Find(expectedForegroundCss) >= 0 && css.Find(expectedBackgroundCss) >= 0);
			page.DestroyWindow();
		};
		verifyAutomaticPair("dark-white-background-auto-text", INTERFACE_THEME_DARK, L"none", L"", CLR_DEFAULT, RGB(255, 255, 255), RGB(0, 0, 0), RGB(255, 255, 255));
		verifyAutomaticPair("dark-black-text-auto-background", INTERFACE_THEME_DARK, L"none", L"", RGB(0, 0, 0), CLR_DEFAULT, RGB(0, 0, 0), RGB(32, 32, 32));
		verifyAutomaticPair("light-black-background-auto-text", INTERFACE_THEME_LIGHT, L"none", L"", CLR_DEFAULT, RGB(0, 0, 0), RGB(255, 255, 255), RGB(0, 0, 0));
		verifyAutomaticPair("light-white-text-auto-background", INTERFACE_THEME_LIGHT, L"none", L"", RGB(255, 255, 255), CLR_DEFAULT, RGB(255, 255, 255), RGB(255, 255, 255));
		verifyAutomaticPair("explicit-body-colors", INTERFACE_THEME_DARK, L"none", L"", RGB(12, 34, 56), RGB(210, 190, 170), RGB(12, 34, 56), RGB(210, 190, 170));
		verifyAutomaticPair("builtin-background-automatic", INTERFACE_THEME_DARK, L"builtin", L"12_graphite_dark", CLR_DEFAULT, CLR_DEFAULT, RGB(232, 232, 232), RGB(61, 61, 60));
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written);
		output.Close(); PostMessage(WM_CLOSE); return 0;
	}
	if (IsFbeTestScenario(L"settings-editor-alpha-runtime"))
	{
		wchar_t directory[MAX_PATH] = {};
		const DWORD directoryLength = ::GetEnvironmentVariableW(L"FBE_NEXT_TEST_PREVIEW_DIRECTORY", directory, _countof(directory));
		if(directoryLength == 0 || directoryLength >= _countof(directory)) { output.Close(); ::PostQuitMessage(1); return 0; }
		struct AlphaSample { const wchar_t* name; DWORD argb; BYTE alpha; COLORREF color; };
		const AlphaSample samples[] = {
			{ L"transparent", 0x00FF0000, 0, RGB(255, 0, 0) },
			{ L"half", 0x8000FF00, 128, RGB(0, 255, 0) },
			{ L"opaque", 0xFF0000FF, 255, RGB(0, 0, 255) }
		};
		const wchar_t* layouts[] = { L"tile", L"center", L"contain", L"cover" };
		CString paths[_countof(samples)];
		bool imagesSaved = true;
		for(size_t sample = 0; sample < _countof(samples); ++sample)
		{
			paths[sample].Format(L"%s\\alpha-%s.png", directory, samples[sample].name);
			CImage image;
			imagesSaved = imagesSaved && image.Create(1, 1, 32, CImage::createAlphaChannel) != FALSE;
			if(imagesSaved)
			{
				*static_cast<DWORD*>(image.GetPixelAddress(0, 0)) = samples[sample].argb;
				imagesSaved = SUCCEEDED(image.Save(paths[sample]));
			}
		}
		CStringA report("case\tpassed\tactual\texpected\r\n");
		bool allPassed = imagesSaved;
		for(int dark = 0; dark < 2; ++dark)
		{
			ThemeManager::SetSelectedTheme(dark ? INTERFACE_THEME_DARK : INTERFACE_THEME_LIGHT);
			const COLORREF background = dark ? RGB(20, 30, 40) : RGB(240, 230, 220);
			for(size_t layout = 0; layout < _countof(layouts); ++layout)
				for(size_t sample = 0; sample < _countof(samples); ++sample)
				{
					_Settings.SetEditorBackgroundKind(L"custom");
					_Settings.SetEditorBackgroundCustomPath(paths[sample]);
					_Settings.SetEditorBackgroundLayout(layouts[layout]);
					_Settings.SetColorBG(background);
					_Settings.SetColorFG(CLR_DEFAULT);
					CSettingsEditorPage page;
					const HWND window = page.Create(m_hWnd);
					const unsigned int initialLoadCount = page.m_cachedBackgroundLoadCount;
					BOOL handled = FALSE;
					page.m_backgroundLayout.SetCurSel((layout + 1) % _countof(layouts));
					page.OnBackgroundSelectionChanged(0, IDC_EDITOR_BACKGROUND_LAYOUT, NULL, handled);
					page.m_fontSize.SetWindowText(L"14"); page.OnPreviewSettingsChanged(0, 0, NULL, handled);
					page.m_foreground.SetColor(RGB(10, 20, 30)); page.OnPreviewColorChanged(0, NULL, handled);
					const bool cacheReused = initialLoadCount == 1 && page.m_cachedBackgroundLoadCount == initialLoadCount;
					page.m_backgroundLayout.SetCurSel(static_cast<int>(layout));
					page.OnBackgroundSelectionChanged(0, IDC_EDITOR_BACKGROUND_LAYOUT, NULL, handled);
					// Paint the original geometry after exercising a different layout;
					// the cache assertion above is independent of the draw mode.
					page.m_backgroundPreview.m_layout = layouts[layout];
					page.m_backgroundPreview.m_text.Empty();
					RECT rc = {}; page.m_backgroundPreview.GetClientRect(&rc);
					const int width = rc.right - rc.left, height = rc.bottom - rc.top;
					HDC screen = ::GetDC(NULL);
					HDC memory = screen ? ::CreateCompatibleDC(screen) : NULL;
					HBITMAP canvas = screen && width > 0 && height > 0 ? ::CreateCompatibleBitmap(screen, width, height) : NULL;
					COLORREF actual = CLR_INVALID;
					int sampleX = width / 2, sampleY = height / 2;
					if(layout == 1) // A 1px centered image may sit left of the geometric center.
					{
						const UINT dpi = UiMetrics::DpiForWindow(page.m_backgroundPreview);
						const int natural = (std::max)(1, ::MulDiv(1, dpi, 96));
						sampleX = (width - natural) / 2 + natural / 2;
						sampleY = (height - natural) / 2 + natural / 2;
					}
					if(memory != NULL && canvas != NULL)
					{
						HGDIOBJ old = ::SelectObject(memory, canvas);
						page.m_backgroundPreview.PaintPreview(memory, rc);
						actual = ::GetPixel(memory, sampleX, sampleY);
						::SelectObject(memory, old);
					}
					if(canvas != NULL) ::DeleteObject(canvas);
					if(memory != NULL) ::DeleteDC(memory);
					if(screen != NULL) ::ReleaseDC(NULL, screen);
					const BYTE alpha = samples[sample].alpha;
					const COLORREF color = samples[sample].color;
					const COLORREF expected = RGB(
						(GetRValue(color) * alpha + GetRValue(background) * (255 - alpha) + 127) / 255,
						(GetGValue(color) * alpha + GetGValue(background) * (255 - alpha) + 127) / 255,
						(GetBValue(color) * alpha + GetBValue(background) * (255 - alpha) + 127) / 255);
					const bool passed = imagesSaved && window != NULL && cacheReused && page.m_backgroundPreview.m_bitmapHasAlpha && actual != CLR_INVALID &&
						abs(GetRValue(actual) - GetRValue(expected)) <= 3 &&
						abs(GetGValue(actual) - GetGValue(expected)) <= 3 &&
						abs(GetBValue(actual) - GetBValue(expected)) <= 3;
					allPassed = allPassed && passed;
					CStringA row; row.Format("%s-%S-%S\t%d\t%06lx\t%06lx\r\n", dark ? "dark" : "light", layouts[layout],
						samples[sample].name, passed ? 1 : 0, static_cast<unsigned long>(actual), static_cast<unsigned long>(expected));
					report += row;
					if(window != NULL) page.DestroyWindow();
				}
		}
		const CString cachePath = CString(directory) + L"\\alpha-cache-refresh.png";
		auto saveOpaque = [](const CString& path, DWORD argb) {
			CImage image; if(!image.Create(1, 1, 32, CImage::createAlphaChannel)) return false;
			*static_cast<DWORD*>(image.GetPixelAddress(0, 0)) = argb; return SUCCEEDED(image.Save(path));
		};
		bool cacheSaved = saveOpaque(cachePath, 0xFFFF0000);
		WIN32_FILE_ATTRIBUTE_DATA first = {};
		cacheSaved = cacheSaved && ::GetFileAttributesEx(cachePath, GetFileExInfoStandard, &first) != FALSE;
		_Settings.SetEditorBackgroundKind(L"custom"); _Settings.SetEditorBackgroundCustomPath(cachePath);
		_Settings.SetEditorBackgroundLayout(L"tile"); _Settings.SetColorBG(RGB(255, 255, 255)); _Settings.SetColorFG(CLR_DEFAULT);
		CSettingsEditorPage cachePage; const HWND cacheWindow = cachePage.Create(m_hWnd);
		const unsigned int firstLoad = cachePage.m_cachedBackgroundLoadCount;
		BOOL handled = FALSE; cachePage.m_backgroundLayout.SetCurSel(1); cachePage.OnBackgroundSelectionChanged(0, IDC_EDITOR_BACKGROUND_LAYOUT, NULL, handled);
		cachePage.m_fontSize.SetWindowText(L"14"); cachePage.OnPreviewSettingsChanged(0, 0, NULL, handled);
		cachePage.m_foreground.SetColor(RGB(10, 20, 30)); cachePage.OnPreviewColorChanged(0, NULL, handled);
		const bool reused = firstLoad == 1 && cachePage.m_cachedBackgroundLoadCount == firstLoad;
		cacheSaved = cacheSaved && saveOpaque(cachePath, 0xFF0000FF);
		HANDLE file = ::CreateFile(cachePath, FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
		if(file != INVALID_HANDLE_VALUE) {
			ULARGE_INTEGER time = {}; time.LowPart = first.ftLastWriteTime.dwLowDateTime; time.HighPart = first.ftLastWriteTime.dwHighDateTime; time.QuadPart += 20000000ULL;
			FILETIME changed = { time.LowPart, time.HighPart }; cacheSaved = cacheSaved && ::SetFileTime(file, NULL, NULL, &changed) != FALSE; ::CloseHandle(file);
		} else cacheSaved = false;
		cachePage.m_backgroundLayout.SetCurSel(0); cachePage.UpdateBackgroundPreview(); cachePage.m_backgroundPreview.m_text.Empty();
		RECT rect = {}; cachePage.m_backgroundPreview.GetClientRect(&rect); const int width = rect.right - rect.left, height = rect.bottom - rect.top;
		HDC screen = ::GetDC(NULL), memory = screen ? ::CreateCompatibleDC(screen) : NULL;
		HBITMAP canvas = screen && width > 0 && height > 0 ? ::CreateCompatibleBitmap(screen, width, height) : NULL; COLORREF actual = CLR_INVALID;
		if(memory && canvas) { HGDIOBJ old = ::SelectObject(memory, canvas); cachePage.m_backgroundPreview.PaintPreview(memory, rect); actual = ::GetPixel(memory, width / 2, height / 2); ::SelectObject(memory, old); }
		if(canvas) ::DeleteObject(canvas); if(memory) ::DeleteDC(memory); if(screen) ::ReleaseDC(NULL, screen);
		const bool reloaded = cacheWindow && cacheSaved && reused && cachePage.m_cachedBackgroundLoadCount == 2 &&
			actual != CLR_INVALID && abs(GetRValue(actual)) <= 3 && abs(GetGValue(actual)) <= 3 && abs(GetBValue(actual) - 255) <= 3;
		allPassed = allPassed && reloaded; CStringA cacheRow; cacheRow.Format("same-path-reload\t%d\t%06lx\t%06lx\r\n", reloaded ? 1 : 0, static_cast<unsigned long>(actual), static_cast<unsigned long>(RGB(0, 0, 255))); report += cacheRow;
		if(cacheWindow) cachePage.DestroyWindow();
		const CString retryPath = CString(directory) + L"\\alpha-cache-retry.png";
		HANDLE damaged = ::CreateFile(retryPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		DWORD damagedBytes = 0; const char invalidPng[] = "not a png";
		const bool damagedWritten = damaged != INVALID_HANDLE_VALUE && ::WriteFile(damaged, invalidPng, sizeof(invalidPng), &damagedBytes, NULL) != FALSE;
		if(damaged != INVALID_HANDLE_VALUE) ::CloseHandle(damaged);
		_Settings.SetEditorBackgroundCustomPath(retryPath);
		CSettingsEditorPage retryPage; const HWND retryWindow = retryPage.Create(m_hWnd);
		const bool firstLoadFailed = damagedWritten && retryPage.m_cachedBackgroundLoadCount == 1 && retryPage.m_cachedBackgroundBitmap == NULL && !retryPage.m_cachedBackgroundValid;
		const bool repaired = saveOpaque(retryPath, 0xFF0000FF);
		retryPage.UpdateBackgroundPreview();
		retryPage.m_backgroundPreview.m_text.Empty();
		RECT retryRect = {}; retryPage.m_backgroundPreview.GetClientRect(&retryRect);
		const int retryWidth = retryRect.right - retryRect.left, retryHeight = retryRect.bottom - retryRect.top;
		HDC retryScreen = ::GetDC(NULL), retryMemory = retryScreen ? ::CreateCompatibleDC(retryScreen) : NULL;
		HBITMAP retryCanvas = retryScreen && retryWidth > 0 && retryHeight > 0 ? ::CreateCompatibleBitmap(retryScreen, retryWidth, retryHeight) : NULL;
		COLORREF retryPixel = CLR_INVALID;
		if(retryMemory && retryCanvas)
		{
			HGDIOBJ old = ::SelectObject(retryMemory, retryCanvas);
			retryPage.m_backgroundPreview.PaintPreview(retryMemory, retryRect);
			retryPixel = ::GetPixel(retryMemory, retryWidth / 2, retryHeight / 2);
			::SelectObject(retryMemory, old);
		}
		if(retryCanvas) ::DeleteObject(retryCanvas); if(retryMemory) ::DeleteDC(retryMemory); if(retryScreen) ::ReleaseDC(NULL, retryScreen);
		const bool retryLoaded = retryWindow && firstLoadFailed && repaired && retryPage.m_cachedBackgroundLoadCount == 2 &&
			retryPage.m_cachedBackgroundBitmap != NULL && retryPage.m_cachedBackgroundHasAlpha && retryPage.m_cachedBackgroundValid &&
			retryPixel != CLR_INVALID && abs(GetRValue(retryPixel)) <= 3 && abs(GetGValue(retryPixel)) <= 3 && abs(GetBValue(retryPixel) - 255) <= 3;
		allPassed = allPassed && retryLoaded; CStringA retryRow; retryRow.Format("same-path-retry-after-failure\t%d\t%06lx\t%06lx\r\n", retryLoaded ? 1 : 0, static_cast<unsigned long>(retryPixel), static_cast<unsigned long>(RGB(0, 0, 255))); report += retryRow;
		if(retryWindow) retryPage.DestroyWindow();
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written);
		output.Close(); ::PostQuitMessage(allPassed ? 0 : 1); return 0;
	}
	if (IsFbeTestScenario(L"body-theme-runtime"))
	{
		MSHTML::IHTMLDocument2Ptr document(m_doc->m_body.Document());
		MSHTML::IHTMLElementPtr body(document ? document->body : NULL);
		MSHTML::IHTMLElement2Ptr scrollable(body);
		if(!document || !body || !scrollable) { output.Close(); ::PostQuitMessage(1); return 0; }
		_Settings.SetEditorBackgroundKind(L"none");
		_Settings.SetColorFG(CLR_DEFAULT); _Settings.SetColorBG(CLR_DEFAULT);
		ThemeManager::SetSelectedTheme(INTERFACE_THEME_LIGHT);
		ThemeManager::ApplyToAllThreadWindows(::GetCurrentThreadId());
		m_doc->ApplyConfChanges();
		m_doc->MarkSavePoint();
		MSHTML::IHTMLBodyElementPtr bodyElement(body);
		MSHTML::IHTMLTxtRangePtr selection(bodyElement ? bodyElement->createTextRange() : NULL);
		if(selection && selection->findText(L"Theme target", 0, 0)) selection->select();
		MSHTML::IHTMLTxtRangePtr baselineSelection(selection ? selection->duplicate() : NULL);
		scrollable->scrollTop = 160;
		const long baselineScroll = scrollable->scrollTop;
		const CString baselineHtml(static_cast<LPCWSTR>(body->innerHTML));
		const bool baselineDirty = m_doc->DocChanged();
		OLECMD baselineCommands[] = { { IDM_UNDO, 0 }, { IDM_REDO, 0 } };
		m_doc->m_body.QueryStatus(baselineCommands, _countof(baselineCommands));
		CStringA report("phase\tforeground\tbackground\ttable_dark\tno_important\tstate_ok\tconfigured_fg\tconfigured_bg\ttheme_dark\thigh_contrast\tsame_html\tsame_dirty\tsame_selection\tsame_scroll\tsame_undo\r\n");
		auto append = [&](const char* phase)
		{
			MSHTML::IHTMLElement2Ptr themedBody(body);
			MSHTML::IHTMLCurrentStylePtr bodyStyle(themedBody ? themedBody->currentStyle : NULL);
			CString foreground, background;
			if(bodyStyle)
			{
				_variant_t foregroundValue(bodyStyle->color), backgroundValue(bodyStyle->backgroundColor);
				if(foregroundValue.vt == VT_BSTR && foregroundValue.bstrVal) foreground = foregroundValue.bstrVal;
				if(backgroundValue.vt == VT_BSTR && backgroundValue.bstrVal) background = backgroundValue.bstrVal;
			}
			MSHTML::IHTMLDocument3Ptr document3(document);
			MSHTML::IHTMLElementPtr style(document3 ? document3->getElementById(L"fbe-runtime-dark-table-theme") : NULL);
			MSHTML::IHTMLStyleElementPtr styleElement(style);
			MSHTML::IHTMLStyleSheetPtr sheet(styleElement ? styleElement->styleSheet : NULL);
			const CString tableCss(sheet ? static_cast<LPCWSTR>(sheet->cssText) : L"");
			MSHTML::IHTMLTxtRangePtr currentSelection(document->selection->createRange());
			OLECMD commands[] = { { IDM_UNDO, 0 }, { IDM_REDO, 0 } };
			m_doc->m_body.QueryStatus(commands, _countof(commands));
			const bool sameSelection = baselineSelection && currentSelection &&
				baselineSelection->compareEndPoints(L"StartToStart", currentSelection) == 0 &&
				baselineSelection->compareEndPoints(L"EndToEnd", currentSelection) == 0;
			const bool sameHtml = baselineHtml == CString(static_cast<LPCWSTR>(body->innerHTML));
			const bool sameDirty = baselineDirty == m_doc->DocChanged();
			const bool sameScroll = baselineScroll == scrollable->scrollTop;
			const bool sameUndo = baselineCommands[0].cmdf == commands[0].cmdf && baselineCommands[1].cmdf == commands[1].cmdf;
			const bool stateOk = sameHtml && sameDirty && sameSelection && sameScroll && sameUndo;
			CStringA foregroundUtf8(CW2A(foreground, CP_UTF8)), backgroundUtf8(CW2A(background, CP_UTF8));
			CStringA row; row.Format("%s\t%s\t%s\t%d\t%d\t%d\t%lu\t%lu\t%d\t%d\t%d\t%d\t%d\t%d\t%d\r\n", phase,
				foregroundUtf8.GetString(), backgroundUtf8.GetString(), tableCss.IsEmpty() ? 0 : 1,
				tableCss.Find(L"!important") < 0 ? 1 : 0, stateOk ? 1 : 0,
				static_cast<unsigned long>(_Settings.GetColorFG()), static_cast<unsigned long>(_Settings.GetColorBG()),
				ThemeManager::IsDark() ? 1 : 0, ThemeManager::IsHighContrast() ? 1 : 0,
				sameHtml ? 1 : 0, sameDirty ? 1 : 0, sameSelection ? 1 : 0, sameScroll ? 1 : 0, sameUndo ? 1 : 0);
			report += row;
		};
		append("light-auto");
		ThemeManager::SetSelectedTheme(INTERFACE_THEME_DARK);
		ThemeManager::ApplyToAllThreadWindows(::GetCurrentThreadId()); append("dark-auto");
		_Settings.SetColorFG(RGB(0, 0, 0)); _Settings.SetColorBG(RGB(255, 255, 255));
		m_doc->ApplyConfChanges(); append("dark-ui-light-body");
		ThemeManager::SetSelectedTheme(INTERFACE_THEME_LIGHT);
		ThemeManager::ApplyToAllThreadWindows(::GetCurrentThreadId());
		_Settings.SetColorFG(RGB(255, 255, 255)); _Settings.SetColorBG(RGB(20, 20, 20));
		m_doc->ApplyConfChanges(); append("light-ui-dark-body");
		_Settings.SetColorFG(CLR_DEFAULT); _Settings.SetColorBG(CLR_DEFAULT);
		m_doc->ApplyConfChanges(); append("light-auto-again");
		ThemeManager::SetSelectedTheme(INTERFACE_THEME_DARK);
		ThemeManager::ApplyToAllThreadWindows(::GetCurrentThreadId());
		_Settings.SetColorFG(RGB(0, 0, 0)); _Settings.SetColorBG(RGB(255, 255, 255));
		m_doc->ApplyConfChanges(); append("dark-explicit-again");
		// A later palette switch must not accidentally acknowledge a real edit.
		body->insertAdjacentHTML(L"beforeEnd", L"<p>Unsent user edit.</p>");
		const CString editedHtml(static_cast<LPCWSTR>(body->innerHTML));
		const bool dirtyBeforeTheme = m_doc->DocChanged();
		ThemeManager::SetSelectedTheme(INTERFACE_THEME_LIGHT);
		ThemeManager::ApplyToAllThreadWindows(::GetCurrentThreadId());
		if(!dirtyBeforeTheme || !m_doc->DocChanged() ||
			editedHtml != CString(static_cast<LPCWSTR>(body->innerHTML)))
		{
			output.Close(); ::PostQuitMessage(1); return 0;
		}
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written);
		output.Close(); PostMessage(WM_CLOSE); return 0;
	}
	if (IsFbeTestScenario(L"editor-background-runtime"))
	{
		StartupTrace::AppendTestStartupBreadcrumb("scenario-enter");
		CStringA header("phase\timage\tcss_url\trepeat\tposition\tsize\tattachment\tmodified\r\n");
		DWORD written = 0; output.Write(header, static_cast<DWORD>(header.GetLength()), &written);
		auto backgroundPathFromEnvironment = [](const wchar_t* name) -> CString
		{
			wchar_t value[1024] = {};
			const DWORD length = ::GetEnvironmentVariable(name, value, _countof(value));
			return length && length < _countof(value) ? CString(value) : CString();
		};
		auto appendBackgroundPhase = [&](const char* phase)
		{
			MSHTML::IHTMLStylePtr style(m_doc->m_body.Document() ? MSHTML::IHTMLStylePtr(m_doc->m_body.Document()->body->style) : MSHTML::IHTMLStylePtr());
			CString image, repeat, position, attachment, cssText, size(L"auto");
			CString cssUrl;
			if(style) {
				image = static_cast<LPCWSTR>(style->backgroundImage); repeat = static_cast<LPCWSTR>(style->backgroundRepeat);
				position = static_cast<LPCWSTR>(style->backgroundPosition); attachment = static_cast<LPCWSTR>(style->backgroundAttachment);
				cssText = static_cast<LPCWSTR>(style->cssText);
				if(cssText.Find(L"background-size: contain") >= 0) size = L"contain";
				else if(cssText.Find(L"background-size: cover") >= 0) size = L"cover";
				else { _variant_t sizeAttribute(style->getAttribute(L"background-size", 0)); if(sizeAttribute.vt == VT_BSTR && sizeAttribute.bstrVal) size = sizeAttribute.bstrVal; }
			}
			CString path;
			if(_Settings.GetEditorBackgroundKind() == L"builtin") EditorBackgrounds::ResolveBuiltIn(_Settings.GetEditorBackgroundId(), path);
			else if(_Settings.GetEditorBackgroundKind() == L"custom") path = _Settings.GetEditorBackgroundCustomPath();
			if(!path.IsEmpty()) { const CString uri = U::UrlFromPath(path); if(!uri.IsEmpty()) cssUrl.Format(L"url(\"%s\")", static_cast<LPCWSTR>(uri)); }
			CStringA imageA(CW2A(image, CP_UTF8)), repeatA(CW2A(repeat, CP_UTF8)), positionA(CW2A(position, CP_UTF8));
			CStringA cssUrlA(CW2A(cssUrl, CP_UTF8)), sizeA(CW2A(size, CP_UTF8)), attachmentA(CW2A(attachment, CP_UTF8));
			CStringA row; row.Format("%s\t%s\t%s\t%s\t%s\t%s\t%s\t%d\r\n", phase,
				imageA.GetString(), cssUrlA.GetString(), repeatA.GetString(), positionA.GetString(), sizeA.GetString(), attachmentA.GetString(), m_doc->DocChanged() ? 1 : 0);
			output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Flush();
		};
		// The command-line fixture has completed its initial MSHTML load before
		// this scenario runs.  Establish that loaded document as the baseline so
		// every following assertion measures only the UI-only background change.
		m_doc->MarkSavePoint();
		_Settings.SetEditorBackgroundKind(L"none"); _Settings.SetEditorBackgroundId(CString()); _Settings.SetEditorBackgroundCustomPath(CString()); _Settings.SetEditorBackgroundLayout(L"tile");
		StartupTrace::AppendTestStartupBreadcrumb("none-start"); m_doc->ApplyConfChanges(); appendBackgroundPhase("none"); StartupTrace::AppendTestStartupBreadcrumb("none-complete");
		_Settings.SetEditorBackgroundKind(L"builtin"); _Settings.SetEditorBackgroundId(L"01_clean_white"); _Settings.SetEditorBackgroundLayout(L"tile");
		StartupTrace::AppendTestStartupBreadcrumb("builtin-tile-start"); m_doc->ApplyConfChanges(); appendBackgroundPhase("builtin-tile"); StartupTrace::AppendTestStartupBreadcrumb("builtin-tile-complete");
		for(const wchar_t* layout : { L"center", L"contain", L"cover" }) {
			_Settings.SetEditorBackgroundLayout(layout); m_doc->ApplyConfChanges();
			if(wcscmp(layout, L"center") == 0) { appendBackgroundPhase("builtin-center"); StartupTrace::AppendTestStartupBreadcrumb("builtin-center-complete"); }
			else if(wcscmp(layout, L"contain") == 0) { appendBackgroundPhase("builtin-contain"); StartupTrace::AppendTestStartupBreadcrumb("builtin-contain-complete"); }
			else { appendBackgroundPhase("builtin-cover"); StartupTrace::AppendTestStartupBreadcrumb("builtin-cover-complete"); }
		}
		StartupTrace::AppendTestStartupBreadcrumb("source-view-start"); ShowView(SOURCE); StartupTrace::AppendTestStartupBreadcrumb("source-view-complete"); StartupTrace::AppendTestStartupBreadcrumb("body-view-start"); ShowView(BODY); StartupTrace::AppendTestStartupBreadcrumb("body-view-complete");
		// Source-to-Body recreates the MSHTML document in this test harness.  Its
		// freshly loaded version is the baseline before checking that UI-only
		// background changes leave the document clean.
		m_doc->MarkSavePoint();
		m_doc->ApplyConfChanges(); appendBackgroundPhase("builtin-after-view-recreate"); StartupTrace::AppendTestStartupBreadcrumb("view-recreate-complete");
		_Settings.SetEditorBackgroundId(L"unknown-background"); m_doc->ApplyConfChanges(); appendBackgroundPhase("unknown-builtin"); StartupTrace::AppendTestStartupBreadcrumb("unknown-builtin-complete");
		_Settings.SetEditorBackgroundKind(L"custom"); _Settings.SetEditorBackgroundCustomPath(backgroundPathFromEnvironment(L"FBE_NEXT_TEST_BACKGROUND_MISSING_PATH")); m_doc->ApplyConfChanges(); appendBackgroundPhase("missing-custom"); StartupTrace::AppendTestStartupBreadcrumb("missing-custom-complete");
		_Settings.SetEditorBackgroundCustomPath(backgroundPathFromEnvironment(L"FBE_NEXT_TEST_BACKGROUND_PATH")); _Settings.SetEditorBackgroundLayout(L"contain"); m_doc->ApplyConfChanges(); appendBackgroundPhase("custom"); StartupTrace::AppendTestStartupBreadcrumb("custom-complete");
		_Settings.SetEditorBackgroundKind(L"builtin"); _Settings.SetEditorBackgroundId(L"01_clean_white"); _Settings.SetEditorBackgroundLayout(L"tile"); m_doc->ApplyConfChanges(); appendBackgroundPhase("before-save"); StartupTrace::AppendTestStartupBreadcrumb("before-save");
		StartupTrace::AppendTestStartupBreadcrumb("save-start");
		if(!m_doc->Save()) {
			CString saveFailure;
			saveFailure.Format(L"editor background runtime save failed; name-valid=%d", m_doc->m_namevalid ? 1 : 0);
			StartupTrace::HResult(L"test", L"TST202", m_doc->GetLastSaveError(), saveFailure);
			CStringA savePhase;
			savePhase.Format("save-failed-hr-0x%08lX", static_cast<unsigned long>(m_doc->GetLastSaveError()));
			appendBackgroundPhase(savePhase);
			StartupTrace::AppendTestStartupBreadcrumb(m_doc->m_namevalid ? "save-failed" : "save-failed-name-invalid");
			output.Close(); ::PostQuitMessage(1); return 0;
		}
		StartupTrace::AppendTestStartupBreadcrumb("save-complete");
		appendBackgroundPhase("after-save"); output.Flush(); StartupTrace::AppendTestStartupBreadcrumb("after-save"); StartupTrace::AppendTestStartupBreadcrumb("report-flush"); output.Close(); StartupTrace::AppendTestStartupBreadcrumb("report-closed"); StartupTrace::AppendTestStartupBreadcrumb("shutdown-requested"); ::PostQuitMessage(0); StartupTrace::AppendTestStartupBreadcrumb("shutdown-quit-posted"); return 0;
	}
	if (IsFbeTestScenario(L"structural-trace-contract"))
	{
		wchar_t tracePath[MAX_PATH] = {};
		const DWORD traceLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_STRUCTURE_TRACE", tracePath, _countof(tracePath));
		FbeStructure::StructuralTrace trace(traceLength ? tracePath : nullptr, L"cite\ttrace", L"case\r\ntrace");
		trace.Before(L"phase\tone", L"TAB\tCR\rLF\n");
		trace.After(L"phase-two", L"complete");
		trace.Hr(L"failed-write", E_ACCESSDENIED, L"denied");
		CStringA row; row.Format("enabled\twrite_failed\tlast_error\tresult\r\n%d\t%d\t0x%08lX\tpass\r\n", trace.IsEnabled() ? 1 : 0, trace.HasWriteFailure() ? 1 : 0, static_cast<unsigned long>(trace.LastError()));
		DWORD written = 0; output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(0); return 0;
	}
	if (IsFbeTestScenario(L"cite-poem-undo"))
	{
		wchar_t operation[16] = {};
		wchar_t target[16] = {};
		wchar_t selectionMode[16] = {};
		wchar_t tracePath[MAX_PATH] = {};
		wchar_t traceCase[64] = {};
		wchar_t route[16] = {}, citePoemFault[32] = {};
		const DWORD operationLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_STRUCTURE_OPERATION", operation, _countof(operation));
		const DWORD targetLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_STRUCTURE_TARGET", target, _countof(target));
		const DWORD selectionModeLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_STRUCTURE_SELECTION_MODE", selectionMode, _countof(selectionMode));
		const DWORD traceLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_STRUCTURE_TRACE", tracePath, _countof(tracePath));
		const DWORD traceCaseLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_STRUCTURE_CASE", traceCase, _countof(traceCase));
		const DWORD routeLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_STRUCTURE_ROUTE", route, _countof(route));
		const DWORD citePoemFaultLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_CITE_POEM_FAULT", citePoemFault, _countof(citePoemFault));
		const bool cite = operationLength == 4 && wcscmp(operation, L"cite") == 0;
		const bool poem = operationLength == 4 && wcscmp(operation, L"poem") == 0;
		const wchar_t* targetClass = targetLength ? target : L"section";
		const CStringA targetName((CW2A(targetClass)));
		const bool selectCaret = selectionModeLength == 5 && wcscmp(selectionMode, L"caret") == 0;
		const CStringA selectionName(selectCaret ? "caret" : "selected");
		const bool repeat = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_STRUCTURE_REPEAT", nullptr, 0) != 0;
		const bool viaWrapper = routeLength == 7 && wcscmp(route, L"wrapper") == 0;
		const bool viaCommand = routeLength == 7 && wcscmp(route, L"command") == 0;
		CStringA header("operation\ttarget\tselection_mode\tselection_collapsed\tselection_text_utf16\tselection_html_utf16\tselection_parent_utf16\tselection_start_to_first_start\tselection_end_to_first_end\tcheck_allowed\tbefore_equals_undo\tafter_equals_redo\tsequential_cycle\tbefore_paragraphs\tafter_cites\tafter_poems\tafter_stanzas\tpoem_text_utf16\tempty_divs\tempty_paragraphs\tempty_stanzas\tundo_validated\tundo_saved\tfault_recovery\tsaved\tresult\tcheck_status\tapply_status\thresult\tdocument_changed\r\n");
		DWORD written = 0; output.Write(header, static_cast<DWORD>(header.GetLength()), &written);
		auto writeFailure = [&](const char* reason)
		{
			CStringA row; row.Format("%s\t%s\t%s\t0\t-\t-\t-\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\t-\t0\t0\t0\t0\t0\t0\t0\t%s\r\n", cite ? "cite" : poem ? "poem" : "unknown", (LPCSTR)targetName, (LPCSTR)selectionName, reason);
			output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Close(); ::PostQuitMessage(1);
		};
		if (!cite && !poem) { writeFailure("invalid-operation"); return 0; }
		MSHTML::IHTMLElementPtr body(m_doc->m_body.Document() ? MSHTML::IHTMLElementPtr(m_doc->m_body.Document()->body) : MSHTML::IHTMLElementPtr());
		MSHTML::IHTMLElementCollectionPtr divs(body ? MSHTML::IHTMLElementCollectionPtr(MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(L"DIV")) : MSHTML::IHTMLElementCollectionPtr());
		MSHTML::IHTMLElementPtr container;
		for (long index = 0; divs && index < divs->length; ++index) {
			MSHTML::IHTMLElementPtr div(divs->item(_variant_t(index), _variant_t()));
			if (div && U::scmp(div->className, targetClass) == 0) { container = div; break; }
		}
		MSHTML::IHTMLElementCollectionPtr paragraphs(container ? MSHTML::IHTMLElementCollectionPtr(MSHTML::IHTMLElement2Ptr(container)->getElementsByTagName(L"P")) : MSHTML::IHTMLElementCollectionPtr());
		if (!body || !container || !paragraphs || paragraphs->length == 0) { writeFailure("missing-paragraph"); return 0; }
		MSHTML::IHTMLElementPtr first(paragraphs->item(_variant_t(0L), _variant_t()));
		MSHTML::IHTMLElementPtr last(paragraphs->item(_variant_t(paragraphs->length - 1), _variant_t()));
		MSHTML::IHTMLTxtRangePtr range(MSHTML::IHTMLBodyElementPtr(body)->createTextRange());
		MSHTML::IHTMLTxtRangePtr rangeEnd(MSHTML::IHTMLBodyElementPtr(body)->createTextRange());
		if (!first || !last || !range || !rangeEnd) { writeFailure("selection-create"); return 0; }
		m_doc->m_body.SetFocus();
		range->moveToElementText(first); range->collapse(VARIANT_TRUE);
		// Keep the caret inside an empty P.  Moving one character from its start
		// makes MSHTML place the range after the P, in an editor-owned DIV.
		if (!CString((const wchar_t*)first->innerText).IsEmpty())
			range->move(L"character", 1);
		if (!selectCaret) {
			rangeEnd->moveToElementText(last);
			rangeEnd->collapse(VARIANT_FALSE); rangeEnd->move(L"character", -1);
			// Both boundaries must be inside their P elements. MSHTML otherwise
			// reports the enclosing DIV as parentElement(), and
			// The structural editor rejects this selection.
			range->setEndPoint(L"EndToEnd", rangeEnd);
		}
		auto utf16Summary = [](const CString& value) -> CStringA
		{
			if (value.IsEmpty()) return CStringA("-");
			CStringA summary;
			for (int index = 0; index < value.GetLength(); ++index) {
				if (index) summary += ',';
				CStringA codeUnit; codeUnit.Format("%04X", static_cast<unsigned int>(static_cast<unsigned short>(value[index])));
				summary += codeUnit;
			}
			return summary;
		};
		const CString selectionText((const wchar_t*)range->text), selectionHtml((const wchar_t*)range->htmlText);
		const bool selectionCollapsed = range->compareEndPoints(L"StartToEnd", range) == 0;
		MSHTML::IHTMLTxtRangePtr firstRange(MSHTML::IHTMLBodyElementPtr(body)->createTextRange());
		firstRange->moveToElementText(first);
		MSHTML::IHTMLElementPtr selectionParent(range->parentElement());
		CString selectionParentText = selectionParent ? CString((const wchar_t*)selectionParent->tagName) + L":" + CString((const wchar_t*)selectionParent->className) : CString(L"-");
		const long selectionStartToFirstStart = range->compareEndPoints(L"StartToStart", firstRange);
		const long selectionEndToFirstEnd = range->compareEndPoints(L"EndToEnd", firstRange);
		const CStringA selectionTextSummary(utf16Summary(selectionText)), selectionHtmlSummary(utf16Summary(selectionHtml));
		const CStringA selectionParentSummary(utf16Summary(selectionParentText));
		range->select();
		auto countEmpty = [&](const wchar_t* tagName, const wchar_t* className = nullptr) -> long
		{
			long count = 0;
			MSHTML::IHTMLElementCollectionPtr elements(MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(tagName));
			for (long index = 0; elements && index < elements->length; ++index) {
				MSHTML::IHTMLElementPtr element(elements->item(_variant_t(index), _variant_t()));
				MSHTML::IHTMLElementCollectionPtr children(element ? MSHTML::IHTMLElementCollectionPtr(element->children) : MSHTML::IHTMLElementCollectionPtr());
				if (element && (!className || U::scmp(element->className, className) == 0) && (!children || children->length == 0) && CString((const wchar_t*)element->innerHTML).Trim().IsEmpty()) ++count;
			}
			return count;
		};
		const long beforeEmptyDivs = countEmpty(L"DIV"), beforeEmptyParagraphs = countEmpty(L"P"), beforeEmptyStanzas = countEmpty(L"DIV", L"stanza");
		const CString before((const wchar_t*)body->innerHTML);
		const long beforeParagraphs = paragraphs->length;
		const bool dirtyBefore = m_doc->DocChanged();
		FbeStructure::StructuralTrace trace(traceLength ? tracePath : nullptr,
			cite ? L"cite" : L"poem", traceCaseLength ? traceCase : L"runtime");
		FbeStructure::BodyStructuralEditor extracted(m_doc->m_body.Document(), m_doc->m_body.MarkupServices(), trace.IsEnabled() ? &trace : nullptr);
		const FbeStructure::CitePoemFailurePoint failurePoint = citePoemFaultLength
			? (wcscmp(citePoemFault, L"after-insert") == 0 ? FbeStructure::CitePoemFailurePoint::AfterInsert
				: wcscmp(citePoemFault, L"before-selection") == 0 ? FbeStructure::CitePoemFailurePoint::BeforeSelection
				: FbeStructure::CitePoemFailurePoint::BeforeMutation)
			: FbeStructure::CitePoemFailurePoint::None;
		auto apply = [&](bool checkOnly) -> FbeStructure::StructuralOperationResult {
			// The command route deliberately goes through CMainFrame.  The check
			// remains a direct production preflight; WM_COMMAND itself only has the
			// historical bool result boundary and does not expose the rich result.
			if (viaCommand && !checkOnly) {
				const CString commandBefore((const wchar_t*)body->innerHTML);
				m_doc->m_body.SetFocus();
				::SetFocus(m_doc->m_body);
				SendMessage(WM_COMMAND, MAKEWPARAM(cite ? ID_EDIT_INS_CITE : ID_EDIT_INS_POEM, 0), 0);
				return commandBefore != CString((const wchar_t*)body->innerHTML)
					? FbeStructure::StructuralOperationResult::Applied()
					: FbeStructure::StructuralOperationResult::NotApplicable();
			}
			if (viaWrapper)
				return cite ? m_doc->m_body.InsertCiteResult(checkOnly, failurePoint) : m_doc->m_body.InsertPoemResult(checkOnly, failurePoint);
			return cite ? extracted.InsertCite(checkOnly, failurePoint) : extracted.InsertPoem(checkOnly, failurePoint);
		};
		const FbeStructure::StructuralOperationResult checkResult = apply(true);
		MSHTML::IHTMLTxtRangePtr selectionAfterCheck(m_doc->m_body.Document()->selection->createRange());
		const bool checkDomUnchanged = before == CString((const wchar_t*)body->innerHTML);
		const bool checkSelectionUnchanged = selectionAfterCheck &&
			range->compareEndPoints(L"StartToStart", selectionAfterCheck) == 0 &&
			range->compareEndPoints(L"EndToEnd", selectionAfterCheck) == 0;
		const bool checkDirtyUnchanged = dirtyBefore == m_doc->DocChanged();
		const FbeStructure::StructuralOperationResult applyResult = apply(false);
		const bool checkAllowed = checkResult.IsApplied();
		const bool applied = applyResult.IsApplied();
		const CString after((const wchar_t*)body->innerHTML);
		if (citePoemFaultLength) {
			MSHTML::IHTMLTxtRangePtr selectionAfterApply(m_doc->m_body.Document()->selection->createRange());
			const bool selectionUnchanged = selectionAfterApply &&
				range->compareEndPoints(L"StartToStart", selectionAfterApply) == 0 &&
				range->compareEndPoints(L"EndToEnd", selectionAfterApply) == 0;
			const bool observedChanged = before != after;
			const bool dirtyChanged = dirtyBefore != m_doc->DocChanged();
			BOOL handled = FALSE;
			if (observedChanged) m_doc->m_body.OnUndo(0, 0, m_doc->m_body, handled);
			const bool expectedChanged = wcscmp(citePoemFault, L"before-mutation") != 0;
			const bool restored = before == CString((const wchar_t*)body->innerHTML);
			bool recovery = false;
			if (restored) {
				MSHTML::IHTMLElementPtr recoveryContainer;
				MSHTML::IHTMLElementCollectionPtr recoveryDivs(MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(L"DIV"));
				for (long index = 0; recoveryDivs && index < recoveryDivs->length; ++index) {
					MSHTML::IHTMLElementPtr div(recoveryDivs->item(_variant_t(index), _variant_t()));
					if (div && U::scmp(div->className, targetClass) == 0) { recoveryContainer = div; break; }
				}
				MSHTML::IHTMLElementCollectionPtr recoveryParagraphs(recoveryContainer ? MSHTML::IHTMLElementCollectionPtr(MSHTML::IHTMLElement2Ptr(recoveryContainer)->getElementsByTagName(L"P")) : MSHTML::IHTMLElementCollectionPtr());
				MSHTML::IHTMLElementPtr recoveryFirst(recoveryParagraphs && recoveryParagraphs->length ? MSHTML::IHTMLElementPtr(recoveryParagraphs->item(_variant_t(0L), _variant_t())) : MSHTML::IHTMLElementPtr());
				MSHTML::IHTMLTxtRangePtr recoveryRange(MSHTML::IHTMLBodyElementPtr(body)->createTextRange());
				if (recoveryFirst && recoveryRange) {
					recoveryRange->moveToElementText(recoveryFirst); recoveryRange->collapse(VARIANT_TRUE);
					if (!CString((const wchar_t*)recoveryFirst->innerText).IsEmpty()) recoveryRange->move(L"character", 1);
					recoveryRange->select();
					const FbeStructure::StructuralOperationResult recoveryResult = cite
						? m_doc->m_body.InsertCiteResult(false) : m_doc->m_body.InsertPoemResult(false);
					const CString recoveryAfter((const wchar_t*)body->innerHTML);
					BOOL recoveryHandled = FALSE;
					m_doc->m_body.OnUndo(0, 0, m_doc->m_body, recoveryHandled);
					const bool recoveryUndo = before == CString((const wchar_t*)body->innerHTML);
					m_doc->m_body.OnRedo(0, 0, m_doc->m_body, recoveryHandled);
					const bool recoveryRedo = recoveryAfter == CString((const wchar_t*)body->innerHTML);
					m_doc->m_body.OnUndo(0, 0, m_doc->m_body, recoveryHandled);
					recovery = recoveryResult.IsApplied() && recoveryAfter != before && recoveryUndo && recoveryRedo && before == CString((const wchar_t*)body->innerHTML);
				}
			}
			const bool passed = checkResult.IsApplied() && checkDomUnchanged && checkSelectionUnchanged && checkDirtyUnchanged &&
				applyResult.HasTechnicalFailure() && applyResult.error == E_FAIL && observedChanged == expectedChanged &&
				applyResult.documentChanged == observedChanged && recovery && (expectedChanged ? restored : selectionUnchanged && !dirtyChanged && restored);
			CStringA row;
			row.Format("%s\t%s\t%s\t%d\t%s\t%s\t%s\t%ld\t%ld\t%d\t%d\t1\t1\t%ld\t0\t0\t0\t-\t0\t0\t0\t0\t0\t%d\t0\t%s\t%s\t%s\t0x%08lX\t%d\r\n", cite ? "cite" : poem ? "poem" : "unknown", (LPCSTR)targetName, (LPCSTR)selectionName, selectionCollapsed, (LPCSTR)selectionTextSummary, (LPCSTR)selectionHtmlSummary, (LPCSTR)selectionParentSummary, selectionStartToFirstStart, selectionEndToFirstEnd, checkAllowed, restored, beforeParagraphs, recovery ? 1 : 0, passed ? "pass" : "fail", checkResult.IsApplied() ? "applied" : checkResult.HasTechnicalFailure() ? "failed" : "not-applicable", applyResult.IsApplied() ? "applied" : applyResult.HasTechnicalFailure() ? "failed" : "not-applicable", static_cast<unsigned long>(applyResult.error), applyResult.documentChanged ? 1 : 0);
			output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Flush(); output.Close(); ::PostQuitMessage(passed ? 0 : 1); return 0;
		}
		if (!applied || before == after) {
			MSHTML::IHTMLTxtRangePtr selectionAfterApply(m_doc->m_body.Document()->selection->createRange());
			const bool applySelectionUnchanged = selectionAfterApply &&
				range->compareEndPoints(L"StartToStart", selectionAfterApply) == 0 &&
				range->compareEndPoints(L"EndToEnd", selectionAfterApply) == 0;
			const bool applyDomUnchanged = before == after;
			const bool applyDirtyUnchanged = dirtyBefore == m_doc->DocChanged();
			const bool genuineNotApplicable = applyResult.status == FbeStructure::StructuralOperationStatus::NotApplicable &&
				checkDomUnchanged && checkSelectionUnchanged && checkDirtyUnchanged &&
				applyDomUnchanged && applySelectionUnchanged && applyDirtyUnchanged;
			const char* reason = genuineNotApplicable ? "not-applicable" : "operation-failed";
			CStringA row;
			row.Format("%s\t%s\t%s\t%d\t%s\t%s\t%s\t%ld\t%ld\t%d\t0\t0\t0\t%ld\t0\t0\t0\t-\t0\t0\t0\t0\t0\t0\t0\t%s\t%s\t%s\t0x%08lX\t%d\r\n", cite ? "cite" : "poem", (LPCSTR)targetName, (LPCSTR)selectionName, selectionCollapsed, (LPCSTR)selectionTextSummary, (LPCSTR)selectionHtmlSummary, (LPCSTR)selectionParentSummary, selectionStartToFirstStart, selectionEndToFirstEnd, checkAllowed, beforeParagraphs, reason, checkResult.IsApplied() ? "applied" : checkResult.HasTechnicalFailure() ? "failed" : "not-applicable", applyResult.IsApplied() ? "applied" : applyResult.HasTechnicalFailure() ? "failed" : "not-applicable", static_cast<unsigned long>(applyResult.error), applyResult.documentChanged ? 1 : 0);
			output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Close(); ::PostQuitMessage(1); return 0;
		}
		BOOL handled = FALSE;
		auto invokeEditorCommand = [&](WORD commandId)
		{
			if (viaCommand) {
				// Exercise the same CMainFrame routing used by the Edit menu and
				// shortcuts.  Other scenarios intentionally retain the direct view
				// calls to isolate structural-editor Undo units.
				m_doc->m_body.SetFocus();
				::SetFocus(m_doc->m_body);
				SendMessage(WM_COMMAND, MAKEWPARAM(commandId, 0), 0);
			}
			else if (commandId == ID_EDIT_UNDO)
				m_doc->m_body.OnUndo(0, 0, m_doc->m_body, handled);
			else
				m_doc->m_body.OnRedo(0, 0, m_doc->m_body, handled);
		};
		invokeEditorCommand(ID_EDIT_UNDO);
		const CString undo((const wchar_t*)body->innerHTML);
		invokeEditorCommand(ID_EDIT_REDO);
		const CString redo((const wchar_t*)body->innerHTML);
		auto countClass = [&](const wchar_t* className) -> long
		{
			long count = 0;
			MSHTML::IHTMLElementCollectionPtr divs(MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(L"DIV"));
			for (long index = 0; divs && index < divs->length; ++index) {
				MSHTML::IHTMLElementPtr div(divs->item(_variant_t(index), _variant_t()));
				if (div && U::scmp(div->className, className) == 0) ++count;
			}
			return count;
		};
		const long citeCount = countClass(L"cite"), poemCount = countClass(L"poem"), stanzaCount = countClass(L"stanza");
		CString poemText;
		MSHTML::IHTMLElementCollectionPtr poemElements(MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(L"DIV"));
		for (long index = 0; poemElements && index < poemElements->length; ++index) {
			MSHTML::IHTMLElementPtr poemElement(poemElements->item(_variant_t(index), _variant_t()));
			if (poemElement && U::scmp(poemElement->className, L"poem") == 0) { poemText = (const wchar_t*)poemElement->innerText; break; }
		}
		const CStringA poemTextSummary(utf16Summary(poemText));
		const long emptyDivsAfterRedo = countEmpty(L"DIV"), emptyParagraphsAfterRedo = countEmpty(L"P"), emptyStanzasAfterRedo = countEmpty(L"DIV", L"stanza");
		invokeEditorCommand(ID_EDIT_UNDO);
		const CString restored((const wchar_t*)body->innerHTML);
		const long undoEmptyDivs = countEmpty(L"DIV"), undoEmptyParagraphs = countEmpty(L"P"), undoEmptyStanzas = countEmpty(L"DIV", L"stanza");
		const long emptyDivs = (emptyDivsAfterRedo > undoEmptyDivs ? emptyDivsAfterRedo : undoEmptyDivs) - beforeEmptyDivs;
		const long emptyParagraphs = (emptyParagraphsAfterRedo > undoEmptyParagraphs ? emptyParagraphsAfterRedo : undoEmptyParagraphs) - beforeEmptyParagraphs;
		const long emptyStanzas = (emptyStanzasAfterRedo > undoEmptyStanzas ? emptyStanzasAfterRedo : undoEmptyStanzas) - beforeEmptyStanzas;
		const bool undone = before == undo && before == restored;
		const bool redone = after == redo;
		bool sequential = true;
		if (repeat) {
			range->select();
			const bool secondApplied = apply(false).IsApplied();
			const CString secondAfter((const wchar_t*)body->innerHTML);
			invokeEditorCommand(ID_EDIT_UNDO);
			const CString secondUndo((const wchar_t*)body->innerHTML);
			invokeEditorCommand(ID_EDIT_REDO);
			const CString secondRedo((const wchar_t*)body->innerHTML);
			invokeEditorCommand(ID_EDIT_UNDO);
			const CString secondRestored((const wchar_t*)body->innerHTML);
			sequential = secondApplied && before == secondUndo && secondAfter == secondRedo && before == secondRestored;
		}
		const bool structure = cite ? citeCount == 1 && poemCount == 0 : poemCount == 1 && stanzaCount >= 1;
		// MSHTML Save resets its undo/redo history.  Check the one-step Redo before
		// saving, then persist the independently restored Undo state below.
		int validationLine = 0, validationColumn = 0;
		const bool undoValidated = before == restored && m_doc->Validate(validationLine, validationColumn);
		const bool undoSaved = undoValidated && m_doc->Save();
		const bool saved = undoSaved;
		const bool passed = undone && redone && sequential && structure && emptyDivs == 0 && emptyParagraphs == 0 && emptyStanzas == 0 && undoValidated && undoSaved;
		CStringA row;
		row.Format("%s\t%s\t%s\t%d\t%s\t%s\t%s\t%ld\t%ld\t%d\t%d\t%d\t%d\t%ld\t%ld\t%ld\t%ld\t%s\t%ld\t%ld\t%ld\t%d\t%d\t0\t%d\t%s\t%s\t%s\t0x%08lX\t%d\r\n", cite ? "cite" : "poem", (LPCSTR)targetName, (LPCSTR)selectionName, selectionCollapsed, (LPCSTR)selectionTextSummary, (LPCSTR)selectionHtmlSummary, (LPCSTR)selectionParentSummary, selectionStartToFirstStart, selectionEndToFirstEnd, checkAllowed, undone, redone, sequential,
			beforeParagraphs, citeCount, poemCount, stanzaCount, (LPCSTR)poemTextSummary, emptyDivs, emptyParagraphs, emptyStanzas, undoValidated, undoSaved, saved, passed ? "pass" : "fail", checkResult.IsApplied() ? "applied" : checkResult.HasTechnicalFailure() ? "failed" : "not-applicable", applyResult.IsApplied() ? "applied" : applyResult.HasTechnicalFailure() ? "failed" : "not-applicable", static_cast<unsigned long>(applyResult.error), applyResult.documentChanged ? 1 : 0);
		output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Flush(); output.Close(); ::PostQuitMessage(passed ? 0 : 1); return 0;
	}
