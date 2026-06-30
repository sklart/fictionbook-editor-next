#include "stdafx.h"
#include "Utils.h"
#include "AboutBox.h"
#include "../version.h"

namespace
{
	const wchar_t* const FBE_RELEASE_DOWNLOAD_PREFIX =
		FBE_GITHUB_RELEASE_DOWNLOAD_PREFIX;

	bool IsHttpsUrl(const CString& url)
	{
		return url.Left(8).CompareNoCase(L"https://") == 0;
	}

	bool IsTrustedUpdateUrl(const CString& url, const CString& version)
	{
		CString path(url);
		const int queryPosition = path.FindOneOf(L"?#");
		if (queryPosition >= 0)
			path = path.Left(queryPosition);

		CString expectedUrl;
		expectedUrl.Format(
			L"%sv%s/FictionBookEditorNext-%s-win32-setup.exe",
			FBE_RELEASE_DOWNLOAD_PREFIX,
			static_cast<const wchar_t*>(version),
			static_cast<const wchar_t*>(version));
		return path.CompareNoCase(expectedUrl) == 0;
	}

	int GetMaximumDownloadSize(const CString& url)
	{
		CString path(url);
		const int queryPosition = path.FindOneOf(L"?#");
		if (queryPosition >= 0)
			path = path.Left(queryPosition);

		const CString extension(ATLPath::FindExtension(path));
		return extension.CompareNoCase(L".xml") == 0
			? 1024 * 1024
			: 256 * 1024 * 1024;
	}

	bool IsSHA256(const CString& value)
	{
		if (value.GetLength() != 64)
			return false;

		for (int i = 0; i < value.GetLength(); ++i)
		{
			const wchar_t ch = value[i];
			if (!((ch >= L'0' && ch <= L'9') ||
				(ch >= L'a' && ch <= L'f') ||
				(ch >= L'A' && ch <= L'F')))
			{
				return false;
			}
		}
		return true;
	}

	bool IsVersion(const CString& value)
	{
		int position = 0;
		for (int part = 0; part < 3; ++part)
		{
			const CString component = value.Tokenize(L".", position);
			if (component.IsEmpty())
				return false;
			for (int i = 0; i < component.GetLength(); ++i)
			{
				if (component[i] < L'0' || component[i] > L'9')
					return false;
			}
		}
		return position == -1;
	}

	bool GetUniqueNodeText(
		const MSXML2::IXMLDOMElementPtr& root,
		const wchar_t* name,
		CString& value)
	{
		MSXML2::IXMLDOMNodeListPtr nodes = root->selectNodes(name);
		if (!nodes || nodes->length != 1)
			return false;

		MSXML2::IXMLDOMNodePtr node = nodes->item[0];
		value = static_cast<const wchar_t*>(node->text);
		value.Trim();
		return !value.IsEmpty();
	}

	CString CalculateFileSHA256(const CString& filename)
	{
		ifstream input(filename.GetString(), ios::in | ios::binary);
		if (!input)
			return CString();

		input.seekg(0, ios::end);
		const std::streamoff fileLength = input.tellg();
		if (fileLength <= 0 || fileLength > INT_MAX)
			return CString();

		const int length = static_cast<int>(fileLength);
		std::vector<char> data(length);
		input.seekg(0, ios::beg);
		input.read(data.data(), length);
		if (!input)
			return CString();

		return FCCrypt::Get_SHA256(data.data(), length);
	}

	bool IsNewerVersion(const CString& availableVersion)
	{
		int availablePosition = 0;
		int currentPosition = 0;
		CString currentVersion(FBE_VERSION_WSTRING);

		for (int part = 0; part < 4; ++part)
		{
			CString availablePart = availableVersion.Tokenize(L".", availablePosition);
			CString currentPart = currentVersion.Tokenize(L".", currentPosition);
			const int available = availablePart.IsEmpty() ? 0 : _wtoi(availablePart);
			const int current = currentPart.IsEmpty() ? 0 : _wtoi(currentPart);

			if (available != current)
				return available > current;
		}

		return false;
	}
}

LRESULT CAboutDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&)
{
	m_bAllowResize = false;

	SetIcon(LoadIcon(_Module.GetResourceInstance(),MAKEINTRESOURCE(IDR_MAINFRAME)));

	CString stamp(build_timestamp);
	::SetWindowText(GetDlgItem(IDC_BUILDSTAMP), stamp);

	CString bname(build_name);
	::SetWindowText(GetDlgItem(IDC_STATIC_AB_APPNAMEVER), bname);

	m_Contributors = GetDlgItem(IDC_CONTRIBS);
	HRSRC hres = ::FindResource(NULL, L"ABOUT_FILE", L"ABOUT_FILE");
	HGLOBAL hbytes = ::LoadResource(NULL, hres);
	CA2CT contribs((char*)::LockResource(hbytes), 65001);  // UTF-8
	CString s(contribs);
	s.Replace(L"\r\n", L"\n");
	s.Replace(L"\r", L"\n");
	s.Replace(L"\n", L"\r\n");
	s.TrimRight();
	m_Contributors.SetWindowText(s);

	// create OpenGL logo window
	m_glLogo.SubclassWindow(GetDlgItem(IDC_AB_BANNER));
	if (m_glLogo.OpenGLError())
	{
		m_glLogo.UnsubclassWindow(TRUE);
		GetDlgItem(IDC_AB_BANNER).ShowWindow(SW_HIDE);
		GetDlgItem(IDC_AB_STATIC_BANNER).ShowWindow(SW_SHOW);
	}
	else GetDlgItem(IDC_AB_BANNER).ShowWindow(SW_SHOW);

	// setup automatic updates engine
	m_UpdateButton = GetDlgItem(IDC_UPDATE);
	m_UpdateButton.ShowWindow(SW_HIDE);

	m_AnimIdx = 0;
	m_UpdatePict.SubclassWindow(GetDlgItem(IDC_PIC_UPDATE));
	m_UpdatePict.m_transparentColor = RGB(0,0,0);
	for (int i=0; i<ANIM_SIZE; i++)
		m_AnimBitmaps[i].LoadBitmap(IDB_UPD_CHECK1+i);
	m_UpdatePict.SetBitmap(m_AnimBitmaps[0]);

	m_StatusBitmaps[0].LoadBitmap(IDB_UPD_OK);
	m_StatusBitmaps[1].LoadBitmap(IDB_UPD_UPDATE);
	m_StatusBitmaps[2].LoadBitmap(IDB_UPD_ERR);

	// load localized messages
	m_sCheckingUpdate.LoadString(IDS_UPDATE_CHECK);
	m_sConnecting.LoadString(IDS_UPDATE_CONNECTING);
	m_sCantConnect.LoadString(IDS_UPDATE_CANTCONNECT);
	m_sDownloadedFrom.LoadString(IDS_UPDATE_DOWNLOADEDFROM);
	m_sDownloaded.LoadString(IDS_UPDATE_DOWNLOADED);
	m_sDownloadCompleted.LoadString(IDS_UPDATE_DOWNLOADCOMPLETE);
	m_sDownloadReady.LoadString(IDS_UPDATE_DOWNLOADREADY);
	m_sDownloadError.LoadString(IDS_UPDATE_DOWNLOADERROR);
	m_sError404.LoadString(IDS_UPDATE_404ERROR);
	m_sError403.LoadString(IDS_UPDATE_403ERROR);
	m_sError407.LoadString(IDS_UPDATE_407ERROR);
	m_sNotSupportRange.LoadString(IDS_UPDATE_NOTSUPPORTEDRANGE);
	m_sDownloadErrorStatus.LoadString(IDS_UPDATE_DOWNLOADERRORSTATUS);
	m_sIncorrectChecksum.LoadString(IDS_UPDATE_INCORRECTMD5);
	m_sNewVersionAvailable.LoadString(IDS_UPDATE_NEWVERSIONAVAILABLE);
	m_sHaveLatestVersion.LoadString(IDS_UPDATE_HAVELATESTVERSION);
	m_sLogoCaption.LoadString(IDS_ABOUT_LOGOCAPTION);

	// check FBE update
	CheckUpdate();

	return 0;
}

LRESULT CAboutDlg::OnCloseCmd(WORD, WORD wID, HWND, BOOL&)
{
	DeleteAllDownload();
	EndDialog(wID);
	return 0;
}

LRESULT CAboutDlg::OnCtlColor(UINT, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	HWND hwndEdit = (HWND) lParam;
	if (hwndEdit == GetDlgItem(IDC_CONTRIBS))
	{
		HDC hdc = (HDC)wParam;
		::SetBkColor(hdc, RGB(255,255,255));
		return (LRESULT) ::GetStockObject(WHITE_BRUSH);
	}
	return 0;
}

LRESULT CAboutDlg::OnNMClickSyslinkAbLinks(int /*idCtrl*/, LPNMHDR pNMHDR, BOOL&)
{
	const PNMLINK link = reinterpret_cast<PNMLINK>(pNMHDR);
	const CString url(link->item.szUrl);

	if (IsHttpsUrl(url))
		ShellExecute(m_hWnd, L"open", url, NULL, NULL, SW_SHOWNORMAL);

	return 0;
}

void CAboutDlg::CheckUpdate()
{
    DeleteAllDownload();
    SetDlgItemText(IDC_TEXT_STATUS, m_sCheckingUpdate);
    
	HTTP_SEND_HEADER ht = PrepareHeader(FBE_UPDATE_MANIFEST_URL);
	
	m_UpdateReady = false;
	m_UpdateURL.Empty();
	m_UpdateSHA256.Empty();
	m_DownloadedSHA256.Empty();
	m_TotalDownloadSize = 0;

	// clear stringstream
	m_file.str("");

	int   nTaskID = AddDownload(ht);
    m_monitor.reset (new CDownloadMonitor(m_hWnd, nTaskID));
}

LRESULT CAboutDlg::OnUpdate(WORD, WORD wID, HWND, BOOL&)
{
	if (!m_UpdateURL.IsEmpty())
	{
		m_UpdateButton.ShowWindow(SW_HIDE);
		DeleteAllDownload();

		SetDlgItemText(IDC_TEXT_STATUS, m_sConnecting);
		
		HTTP_SEND_HEADER ht = PrepareHeader(m_UpdateURL);

		// clear stringstream
		m_file.str("");

		m_TotalDownloadSize = 0;
		int   nTaskID = AddDownload(ht);
		m_monitor.reset (new CDownloadMonitor(m_hWnd, nTaskID));
	}
	return 0L;
}

void CAboutDlg::OnAfterDownloadConnected (FCHttpDownload* pTask)
{
    const HTTP_RESPONSE_INFO   & resp = pTask->GetResponseInfo();

    const int maximumSize = GetMaximumDownloadSize(pTask->GetURL());
    if (resp.m_status_code == 0 ||
        !IsHttpsUrl(resp.m_effective_url) ||
        resp.m_content_length > maximumSize)
    {
        SetDlgItemText (IDC_TEXT_STATUS, m_sCantConnect);
        DeleteDownload (pTask->GetTaskID());
		m_UpdatePict.SetBitmap(m_StatusBitmaps[2]);
        return;
    }

#ifdef DOWNLOAD_STATISTIC
    // total length
    CString   totalLength(L"Unknow");
    int   nTotal = resp.m_content_length;
    if (nTotal)
    {
        totalLength.Format(L"%d Kb", (int)nTotal/1024.0);
    }
#endif
}

bool CAboutDlg::AcceptReceivedData (FCHttpDownload* pTask)
{
    BYTE *p;
    int n;

    pTask->PopReceived(p, n);
    if (p)
    {
		const int maximumSize = GetMaximumDownloadSize(pTask->GetURL());
		if (n < 0 || m_TotalDownloadSize > maximumSize - n)
		{
			delete[] p;
			return false;
		}
		m_file.write ((const char*) p, n);
        delete[] p;
		m_TotalDownloadSize += n;
    }
	return true;
}

LRESULT CAboutDlg::OnUpdateProgressUI (UINT, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    if (!m_monitor.get())
        return 0;

    FCHttpDownload *p = FindDownload((int)wParam);
    if (!p)
    {
        m_monitor.reset();
        return 0;
    }

	if (m_AnimIdx >= ANIM_SIZE) m_AnimIdx = 0;
	m_UpdatePict.SetBitmap(m_AnimBitmaps[m_AnimIdx++]);

    if (!AcceptReceivedData(p))
	{
		const int taskID = p->GetTaskID();
		DeleteDownload(taskID);
		m_monitor.reset();
		m_UpdatePict.SetBitmap(m_StatusBitmaps[2]);
		SetDlgItemText(IDC_TEXT_STATUS, m_sDownloadError);
		return 0;
	}

	// show percent only for update download
	if (m_UpdateReady)
	{
		CString currPercent;
		// download
		int nDownload = p->GetDownloadByte();
		int nTotal = p->GetResponseInfo().m_content_length;
		if (nTotal)
		{
			int   nPercent = (int)(100 * (INT64)nDownload / nTotal);
			currPercent.Format(m_sDownloadedFrom, (int)ceil(nDownload/1024.0), (int)ceil(nTotal/1024.0), nPercent);
		}
		else
		{
			currPercent.Format(m_sDownloaded, (int)ceil(nDownload/1024.0));
		}
		SetDlgItemText (IDC_TEXT_STATUS, currPercent);
	}
	
#ifdef DOWNLOAD_STATISTIC
    // current speed
    currSpeed.Format(L"%d Kb / S", (int)ceil(p->GetCurrentSpeed()/1024.0));
    // average speed
    avgSpeed.Format(L"%d Kb / S", (int)ceil(p->GetAverageSpeed()/1024.0));
#endif
    return 0;
}

void CAboutDlg::FinishUpdateStatus (FCHttpDownload* pTask)
{
	bool bStatus = false;
    const HTTP_RESPONSE_INFO   & resp = pTask->GetResponseInfo();
	const std::streamoff downloadPosition = m_file.tellp();
	const int nDownload = downloadPosition <= 0 ? 0 :
		(downloadPosition > INT_MAX ? INT_MAX : static_cast<int>(downloadPosition));

    CString s = m_sDownloadError;
    switch (resp.m_status_code)
    {
         case HTTP_STATUS_OK :
         case HTTP_STATUS_PARTIAL_CONTENT :
             if (resp.m_content_length > 0)
             {
				 if (resp.m_content_length == nDownload) bStatus = true;
             }
             else
             {
                 if (resp.m_final_read_result) bStatus = true;
             }

             // range request
             if (pTask->GetSendHeader().m_start && (resp.m_status_code == HTTP_STATUS_OK))
             {
                 s += m_sNotSupportRange;
             }
             break;

        case HTTP_STATUS_NOT_FOUND :
            s = m_sError404;
            break;

        case HTTP_STATUS_FORBIDDEN :
            s = m_sError403; 
            break;

        case HTTP_STATUS_PROXY_AUTH_REQ :
            s = m_sError407;
            break;

        default :
			s.Format(m_sDownloadErrorStatus, resp.m_status_code);
            break;
    }

    // Calculate the checksum before writing or executing the downloaded file.
	if (bStatus)
	{
		const string data = m_file.str();
		m_DownloadedSHA256 = data.empty() || data.size() > INT_MAX
			? CString()
			: FCCrypt::Get_SHA256(data.data(), static_cast<int>(data.size()));
	}
	else 
	{
		m_UpdatePict.SetBitmap(m_StatusBitmaps[2]);
		SetDlgItemText (IDC_TEXT_STATUS, s);
	}
}

void CAboutDlg::OnAfterDownloadFinish (FCHttpDownload* pTask)
{
	BOOL b;
    OnUpdateProgressUI (0, (WPARAM)pTask->GetTaskID(), 0, b);

    FinishUpdateStatus (pTask);

	// process XML update file
	if (pTask->GetURL().CompareNoCase(FBE_UPDATE_MANIFEST_URL) == 0)
	{
		bool manifestHandled = false;
		try
		{
			MSXML2::IXMLDOMDocument2Ptr xmlDoc(U::CreateDocument(false));
			xmlDoc->put_async(VARIANT_FALSE);
			xmlDoc->put_validateOnParse(VARIANT_FALSE);
			xmlDoc->put_resolveExternals(VARIANT_FALSE);
			xmlDoc->setProperty(L"ProhibitDTD", _variant_t(VARIANT_TRUE));

			_bstr_t str(m_file.str().c_str());
			if (xmlDoc->loadXML(str) == VARIANT_TRUE && !xmlDoc->doctype)
			{
				MSXML2::IXMLDOMElementPtr root = xmlDoc->GetdocumentElement();
				CString availableVersion;
				CString updateURL;
				CString updateSHA256;
				if (root &&
					CString(static_cast<const wchar_t*>(root->nodeName)) == L"FBE" &&
					GetUniqueNodeText(root, L"Version", availableVersion) &&
					GetUniqueNodeText(root, L"DownloadUrl", updateURL) &&
					GetUniqueNodeText(root, L"SHA256", updateSHA256) &&
					IsVersion(availableVersion))
				{
					if (IsNewerVersion(availableVersion))
					{
						CString path = updateURL;
						const int queryPosition = path.FindOneOf(L"?#");
						if (queryPosition >= 0)
							path = path.Left(queryPosition);
						const CString extension(ATLPath::FindExtension(path));

						if (IsTrustedUpdateUrl(updateURL, availableVersion) &&
							extension.CompareNoCase(L".exe") == 0 &&
							IsSHA256(updateSHA256))
						{
							m_UpdateReady = true;
							m_UpdateURL = updateURL;
							m_UpdateSHA256 = updateSHA256;
							SetDlgItemText(IDC_TEXT_STATUS, m_sNewVersionAvailable);
							m_UpdatePict.SetBitmap(m_StatusBitmaps[1]);
							m_UpdateButton.ShowWindow(SW_SHOW);
							manifestHandled = true;
						}
						else
						{
							SetDlgItemText(IDC_TEXT_STATUS, m_sDownloadError);
							m_UpdatePict.SetBitmap(m_StatusBitmaps[2]);
						}
					}
					else 
					{
						SetDlgItemText (IDC_TEXT_STATUS, m_sHaveLatestVersion);
						m_UpdatePict.SetBitmap(m_StatusBitmaps[0]);
						manifestHandled = true;
					}
				}
			}
		}
		catch (const _com_error&)
		{
		}
		if (!manifestHandled)
		{
			SetDlgItemText(IDC_TEXT_STATUS, m_sDownloadError);
			m_UpdatePict.SetBitmap(m_StatusBitmaps[2]);
		}
	}
	// else process downloaded executable
	else
	{
		if (IsSHA256(m_UpdateSHA256) &&
			m_DownloadedSHA256.CompareNoCase(m_UpdateSHA256) == 0)
		try
		{
			SetDlgItemText (IDC_TEXT_STATUS, m_sDownloadCompleted);
			m_UpdatePict.SetBitmap(m_StatusBitmaps[0]);
			DeleteAllDownload();

			CString filename;
			if (!SaveVerifiedUpdate(m_file.str(), filename))
				throw E_FAIL;
			SetDlgItemText (IDC_TEXT_STATUS, m_sDownloadReady);

			// run new installation
			RunUpdate(filename);
		}
		catch (...)
		{
		}
		// wrong checksum
		else
		{
			m_UpdatePict.SetBitmap(m_StatusBitmaps[2]);
			SetDlgItemText (IDC_TEXT_STATUS, m_sIncorrectChecksum);
		}
	}
}

HTTP_SEND_HEADER CAboutDlg::PrepareHeader(const CString& url)
{
	HTTP_SEND_HEADER ht;
    ht.m_url = url;
    ht.m_user_agent = L"FictionBookEditorNext/" FBE_VERSION_WSTRING;
    ht.m_start = 0;
    ht.m_header.Empty();
    ht.m_open_flag = INTERNET_FLAG_RELOAD |
        INTERNET_FLAG_NO_CACHE_WRITE |
        INTERNET_FLAG_PRAGMA_NOCACHE |
        INTERNET_FLAG_NO_UI |
        INTERNET_FLAG_NO_COOKIES |
        INTERNET_FLAG_NO_AUTH |
        INTERNET_FLAG_KEEP_CONNECTION |
        INTERNET_FLAG_SECURE;
/*  ht.m_proxy_ip = DlgSetProxy::s_task.m_proxy_ip;
    ht.m_proxy_port = DlgSetProxy::s_task.m_proxy_port;
    ht.m_proxy_username = DlgSetProxy::s_task.m_proxy_username;
    ht.m_proxy_password = DlgSetProxy::s_task.m_proxy_password; */
	return ht;
}

LRESULT CAboutDlg::OnGetMinMaxInfo(UINT, WPARAM, LPARAM lParam, BOOL&)
{
	if (!m_bAllowResize)
	{
		RECT rect;
		GetWindowRect(&rect);
		LPMINMAXINFO pMMI = (LPMINMAXINFO)lParam;
		pMMI->ptMaxTrackSize.x = rect.right - rect.left;
		pMMI->ptMaxTrackSize.y = rect.bottom - rect.top;
		pMMI->ptMinTrackSize.x = rect.right - rect.left;
		pMMI->ptMinTrackSize.y = rect.bottom - rect.top;
	}
	return TRUE;
}

LRESULT CAboutDlg::OnSize(UINT, WPARAM, LPARAM, BOOL&)
{
	if (m_bAllowResize)
	{
		RECT rect;
		GetClientRect(&rect);
		m_glLogo.SetWindowPos(0, &rect, 0);
	}
	return FALSE;
}

LRESULT CAboutDlg::OnResizeOpenGLWindow(UINT, WPARAM, LPARAM, BOOL&)
{
	CButton btn = GetDlgItem(IDOK);
	// switch glLogo to full client area
	if (btn.IsWindowVisible())
	{
		// hide controls
		btn.ShowWindow(SW_HIDE);
		btn.EnableWindow(FALSE);

		m_SaveBtnState = GetDlgItem(IDC_UPDATE).IsWindowVisible();
		if (m_SaveBtnState)
		{
			GetDlgItem(IDC_UPDATE).ShowWindow(SW_HIDE);
			GetDlgItem(IDC_UPDATE).EnableWindow(FALSE);
		}

		GetDlgItem(IDC_PIC_UPDATE).ShowWindow(SW_HIDE);
		GetDlgItem(IDC_TEXT_STATUS).ShowWindow(SW_HIDE);
		GetDlgItem(IDC_CONTRIBS).ShowWindow(SW_HIDE);
		GetDlgItem(IDC_STATIC_AB_APPICON).ShowWindow(SW_HIDE);
		GetDlgItem(IDC_STATIC_AB_APPNAMEVER).ShowWindow(SW_HIDE);
		GetDlgItem(IDC_BUILDSTAMP).ShowWindow(SW_HIDE);
		GetDlgItem(IDC_SYSLINK_AB_LINKS).ShowWindow(SW_HIDE);
		GetDlgItem(IDC_STATIC_BUILD).ShowWindow(SW_HIDE);

		// save dialog size & position
		GetWindowRect(&m_SaveRect);

		// save control size & position
		m_glLogo.GetWindowRect(&m_LogoRect);
		ScreenToClient(&m_LogoRect);

		RECT rect;
		GetClientRect(&rect);
		m_glLogo.SetWindowPos(0, &rect, 0);
		m_glLogo.SetFocus();

		GetWindowText (m_AboutCaption);
		SetWindowText (m_sLogoCaption);
		ModifyStyle(0, WS_MAXIMIZEBOX, SWP_FRAMECHANGED);
		UpdateWindow();

		m_bAllowResize = true;
	}
	else
	{
		// restore dialog size and position
		SetWindowPos(0, &m_SaveRect, 0);

		m_glLogo.SetWindowPos(0, &m_LogoRect, 0);

		btn.ShowWindow(SW_SHOW);
		btn.EnableWindow(TRUE);

		if (m_SaveBtnState)
		{
			GetDlgItem(IDC_UPDATE).ShowWindow(SW_SHOW);
			GetDlgItem(IDC_UPDATE).EnableWindow(TRUE);
		}

		GetDlgItem(IDC_PIC_UPDATE).ShowWindow(SW_SHOW);
		GetDlgItem(IDC_TEXT_STATUS).ShowWindow(SW_SHOW);
		GetDlgItem(IDC_CONTRIBS).ShowWindow(SW_SHOW);
		GetDlgItem(IDC_STATIC_AB_APPICON).ShowWindow(SW_SHOW);
		GetDlgItem(IDC_STATIC_AB_APPNAMEVER).ShowWindow(SW_SHOW);
		GetDlgItem(IDC_BUILDSTAMP).ShowWindow(SW_SHOW);
		GetDlgItem(IDC_SYSLINK_AB_LINKS).ShowWindow(SW_SHOW);
		GetDlgItem(IDC_STATIC_BUILD).ShowWindow(SW_SHOW);

		SetWindowText (m_AboutCaption);
		ModifyStyle(WS_MAXIMIZEBOX, 0, SWP_FRAMECHANGED);

		m_bAllowResize = false;
	}
	return TRUE;
}

bool CAboutDlg::SaveVerifiedUpdate(const std::string& data, CString& filename)
{
	wchar_t tempDirectory[MAX_PATH];
	const DWORD tempLength = ::GetTempPath(_countof(tempDirectory), tempDirectory);
	if (tempLength == 0 || tempLength >= _countof(tempDirectory))
		return false;

	GUID updateId;
	if (FAILED(::CoCreateGuid(&updateId)))
		return false;

	wchar_t updateIdText[40];
	if (::StringFromGUID2(updateId, updateIdText, _countof(updateIdText)) == 0)
		return false;

	filename.Format(L"%sFBE-update-%s.exe", tempDirectory, updateIdText);
	HANDLE file = ::CreateFile(
		filename,
		GENERIC_WRITE,
		0,
		NULL,
		CREATE_NEW,
		FILE_ATTRIBUTE_TEMPORARY,
		NULL);
	if (file == INVALID_HANDLE_VALUE)
	{
		filename.Empty();
		return false;
	}

	bool saved = true;
	size_t position = 0;
	while (position < data.size())
	{
		const DWORD chunkSize = static_cast<DWORD>(
			min<size_t>(data.size() - position, MAXDWORD));
		DWORD written = 0;
		if (!::WriteFile(file, data.data() + position, chunkSize, &written, NULL) ||
			written != chunkSize)
		{
			saved = false;
			break;
		}
		position += written;
	}

	if (saved && !::FlushFileBuffers(file))
		saved = false;
	::CloseHandle(file);

	if (saved)
	{
		const CString fileSHA256 = CalculateFileSHA256(filename);
		saved = !fileSHA256.IsEmpty() &&
			fileSHA256.CompareNoCase(m_UpdateSHA256) == 0;
	}

	if (!saved)
	{
		::DeleteFile(filename);
		filename.Empty();
	}
	return saved;
}

void CAboutDlg::RunUpdate(const CString& filename)
{
	if (U::MessageBox(MB_YESNO | MB_ICONEXCLAMATION, IDR_MAINFRAME,
		IDS_UPDATE_CLOSE, filename) != IDYES)
	{
		::DeleteFile(filename);
		return;
	}

	const CString fileSHA256 = CalculateFileSHA256(filename);
	if (fileSHA256.IsEmpty() ||
		fileSHA256.CompareNoCase(m_UpdateSHA256) != 0)
	{
		::DeleteFile(filename);
		m_UpdatePict.SetBitmap(m_StatusBitmaps[2]);
		SetDlgItemText(IDC_TEXT_STATUS, m_sIncorrectChecksum);
		return;
	}

	HINSTANCE hInst = ShellExecute(0, L"open", filename, 0, 0, SW_SHOW);
	if ((INT_PTR)hInst > 32)
		::PostMessage(GetParent().m_hWnd, WM_CLOSE, 0, 0);
	else
		::DeleteFile(filename);
}
