#include "stdafx.h"
#include "Utils.h"
#include "AboutBox.h"
#include "RuntimeLocalization.h"
#include "../common/DeploymentContext.h"
#include "UpdateArtifact.h"
#include "UpdateVersion.h"
#include "UpdateChannel.h"
#include "Settings.h"
#include "../version.h"

extern CSettings _Settings;

namespace
{
	const wchar_t* const FBE_RELEASE_DOWNLOAD_PREFIX =
		FBE_GITHUB_RELEASE_DOWNLOAD_PREFIX;

	bool IsHttpsUrl(const CString& url)
	{
		return url.Left(8).CompareNoCase(L"https://") == 0;
	}

	bool IsTrustedUpdateUrl(const CString& url, const CString& releaseTag, const CString& baseVersion)
	{
		if (!IsHttpsUrl(url) || url.FindOneOf(L"?#") >= 0)
			return false;

		CString expectedUrl;
		const UpdateArtifact artifact = SelectUpdateArtifact(
			DeploymentContext::CurrentMode(),
			DeploymentContext::CurrentCompatibilityTarget(),
			baseVersion);
		expectedUrl.Format(
			L"%s%s/%s",
			FBE_RELEASE_DOWNLOAD_PREFIX,
			static_cast<const wchar_t*>(releaseTag),
			static_cast<const wchar_t*>(artifact.fileName));
		return url.CompareNoCase(expectedUrl) == 0;
	}

	CString GetPortableUpdateUrl(const CString& setupUrl)
	{
		CString url(setupUrl);
		url.Replace(L"-setup.exe", L"-portable.zip");
		return url;
	}

	bool GetUniqueNodeText(const MSXML2::IXMLDOMElementPtr& root, const wchar_t* name, CString& value);

	bool GetUniqueChildElement(
		const MSXML2::IXMLDOMElementPtr& root,
		const wchar_t* name,
		MSXML2::IXMLDOMElementPtr& value)
	{
		value = nullptr;
		if (!root) return false;
		MSXML2::IXMLDOMNodeListPtr nodes = root->childNodes;
		if (!nodes) return false;
		for (long i = 0; i < nodes->length; ++i)
		{
			MSXML2::IXMLDOMNodePtr node = nodes->item[i];
			if (!node || node->nodeType != MSXML2::NODE_ELEMENT) continue;
			CString nodeName = node->baseName.length() > 0 ? static_cast<const wchar_t*>(node->baseName) : static_cast<const wchar_t*>(node->nodeName);
			if (nodeName.CompareNoCase(name) != 0) continue;
			if (value) return false;
			value = node;
		}
		return value != nullptr;
	}

	bool GetProfileArtifact(
		const MSXML2::IXMLDOMElementPtr& root,
		CString& url,
		CString& sha256)
	{
		MSXML2::IXMLDOMElementPtr artifacts;
		const wchar_t* profile = DeploymentContext::CurrentCompatibilityTarget() == DeploymentContext::CompatibilityTarget::Win7 ? L"Win7" : L"Modern";
		MSXML2::IXMLDOMElementPtr profileNode;
		if (!GetUniqueChildElement(root, L"Artifacts", artifacts) || !GetUniqueChildElement(artifacts, profile, profileNode)) return false;
		const UpdateArtifact artifact = SelectUpdateArtifact(
			DeploymentContext::CurrentMode(),
			DeploymentContext::CurrentCompatibilityTarget(),
			FBE_VERSION_WSTRING);
		return GetUniqueNodeText(profileNode, artifact.manifestUrlElement, url) &&
			GetUniqueNodeText(profileNode, artifact.manifestSha256Element, sha256);
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


	bool GetUniqueNodeText(
		const MSXML2::IXMLDOMElementPtr& root,
		const wchar_t* name,
		CString& value)
	{
		MSXML2::IXMLDOMNodeListPtr nodes = root->childNodes;
		if (!nodes)
			return false;

		MSXML2::IXMLDOMNodePtr matchedNode;
		for (long i = 0; i < nodes->length; ++i)
		{
			MSXML2::IXMLDOMNodePtr node = nodes->item[i];
			if (!node || node->nodeType != MSXML2::NODE_ELEMENT)
				continue;

			CString nodeName;
			if (node->baseName.length() > 0)
				nodeName = static_cast<const wchar_t*>(node->baseName);
			else
				nodeName = static_cast<const wchar_t*>(node->nodeName);

			if (nodeName.CompareNoCase(name) != 0)
				continue;
			if (matchedNode)
				return false;
			matchedNode = node;
		}
		if (!matchedNode)
			return false;

		value = static_cast<const wchar_t*>(matchedNode->text);
		value.Trim();
		return !value.IsEmpty();
	}

	bool GetSimpleXmlTagText(
		const CString& xml,
		const wchar_t* name,
		CString& value)
	{
		CString openTag;
		openTag.Format(L"<%s>", name);
		CString closeTag;
		closeTag.Format(L"</%s>", name);

		const int openPosition = xml.Find(openTag);
		if (openPosition < 0)
			return false;

		const int valuePosition = openPosition + openTag.GetLength();
		const int closePosition = xml.Find(closeTag, valuePosition);
		if (closePosition < 0)
			return false;

		value = xml.Mid(valuePosition, closePosition - valuePosition);
		value.Trim();
		return !value.IsEmpty();
	}

	CString DecodeUtf8OrAnsi(const std::string& data)
	{
		if (data.empty())
			return CString();

		int length = ::MultiByteToWideChar(
			CP_UTF8,
			MB_ERR_INVALID_CHARS,
			data.data(),
			static_cast<int>(data.size()),
			nullptr,
			0);
		UINT codePage = CP_UTF8;
		DWORD flags = MB_ERR_INVALID_CHARS;
		if (length <= 0)
		{
			codePage = CP_ACP;
			flags = 0;
			length = ::MultiByteToWideChar(
				codePage,
				flags,
				data.data(),
				static_cast<int>(data.size()),
				nullptr,
				0);
		}
		if (length <= 0)
			return CString();

		std::vector<wchar_t> buffer(length + 1);
		if (::MultiByteToWideChar(
			codePage,
			flags,
			data.data(),
			static_cast<int>(data.size()),
			buffer.data(),
			length) <= 0)
		{
			return CString();
		}
		return CString(buffer.data(), length);
	}

	void AppendUpdateTrace(const CString& line)
	{
		wchar_t localAppData[MAX_PATH] = {};
		const DWORD envLength = ::GetEnvironmentVariableW(
			L"LOCALAPPDATA",
			localAppData,
			_countof(localAppData));
		if (envLength == 0 || envLength >= _countof(localAppData))
			return;

		CString directory(localAppData);
		directory += L"\\FBE Next";
		::CreateDirectoryW(directory, nullptr);

		CString path(directory);
		path += L"\\update-check-trace.log";

		HANDLE file = ::CreateFileW(
			path,
			FILE_APPEND_DATA,
			FILE_SHARE_READ,
			nullptr,
			OPEN_ALWAYS,
			FILE_ATTRIBUTE_NORMAL,
			nullptr);
		if (file == INVALID_HANDLE_VALUE)
			return;

		SYSTEMTIME time;
		::GetLocalTime(&time);
		CString text;
		text.Format(
			L"%04u-%02u-%02u %02u:%02u:%02u  %s\r\n",
			time.wYear,
			time.wMonth,
			time.wDay,
			time.wHour,
			time.wMinute,
			time.wSecond,
			line.GetString());

		const int utf8Length = ::WideCharToMultiByte(
			CP_UTF8,
			0,
			text,
			text.GetLength(),
			nullptr,
			0,
			nullptr,
			nullptr);
		if (utf8Length > 0)
		{
			std::vector<char> utf8(utf8Length);
			if (::WideCharToMultiByte(
				CP_UTF8,
				0,
				text,
				text.GetLength(),
				utf8.data(),
				utf8Length,
				nullptr,
				nullptr) > 0)
			{
				DWORD written = 0;
				::WriteFile(file, utf8.data(), static_cast<DWORD>(utf8.size()), &written, nullptr);
			}
		}

		::CloseHandle(file);
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

}

LRESULT CAboutDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&)
{
	m_bAllowResize = false;
	FbeApplyRuntimeDialogLocalization(m_hWnd, IDD_ABOUTBOX);
	SetWindowText(FbeLoadRuntimeString(IDS_ABOUT_WINDOW_CAPTION));
	SetDlgItemText(IDC_STATIC_BUILD, FbeLoadRuntimeString(IDS_ABOUT_BUILD_LABEL));

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
	m_UpdateButton.SetWindowText(FbeLoadRuntimeString(IDS_ABOUT_UPDATE_NOW));
	m_UpdateButton.ShowWindow(SW_HIDE);
	m_WhatsNewButton = GetDlgItem(IDC_WHATS_NEW);
	m_WhatsNewButton.SetWindowText(FbeLoadRuntimeStringByKey(L"fbe.about.whats_new", L"What's new..."));
	m_WhatsNewButton.ShowWindow(SW_HIDE);

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
	m_sCheckingUpdate = FbeLoadRuntimeString(IDS_UPDATE_CHECK);
	m_sConnecting = FbeLoadRuntimeString(IDS_UPDATE_CONNECTING);
	m_sCantConnect = FbeLoadRuntimeString(IDS_UPDATE_CANTCONNECT);
	m_sDownloadedFrom = FbeLoadRuntimeString(IDS_UPDATE_DOWNLOADEDFROM);
	m_sDownloaded = FbeLoadRuntimeString(IDS_UPDATE_DOWNLOADED);
	m_sDownloadCompleted = FbeLoadRuntimeString(IDS_UPDATE_DOWNLOADCOMPLETE);
	m_sDownloadReady = FbeLoadRuntimeString(IDS_UPDATE_DOWNLOADREADY);
	m_sDownloadError = FbeLoadRuntimeString(IDS_UPDATE_DOWNLOADERROR);
	m_sError404 = FbeLoadRuntimeString(IDS_UPDATE_404ERROR);
	m_sError403 = FbeLoadRuntimeString(IDS_UPDATE_403ERROR);
	m_sError407 = FbeLoadRuntimeString(IDS_UPDATE_407ERROR);
	m_sNotSupportRange = FbeLoadRuntimeString(IDS_UPDATE_NOTSUPPORTEDRANGE);
	m_sDownloadErrorStatus = FbeLoadRuntimeString(IDS_UPDATE_DOWNLOADERRORSTATUS);
	m_sIncorrectChecksum = FbeLoadRuntimeString(IDS_UPDATE_INCORRECTMD5);
	m_sNewVersionAvailable = FbeLoadRuntimeString(IDS_UPDATE_NEWVERSIONAVAILABLE);
	m_sHaveLatestVersion = FbeLoadRuntimeString(IDS_UPDATE_HAVELATESTVERSION);
	m_sLogoCaption = FbeLoadRuntimeString(IDS_ABOUT_LOGOCAPTION);

	// check FBE update
	CheckUpdate();

	return 0;
}

LRESULT CAboutDlg::OnCloseCmd(WORD, WORD wID, HWND, BOOL&)
{
	// A manifest request may be blocked in WinINet.  Closing the dialog must
	// not wait for its worker thread to observe cancellation.
	m_monitor.reset();
	AbandonAllDownload();
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
	m_UpdateManifestURL = GetUpdateManifestUrl(_Settings.GetUpdateChannel());
	CString startTrace; startTrace.Format(L"start manifest check: channel=%s manifest=%s",
		_Settings.GetUpdateChannel() == UpdateChannel::Prerelease ? L"prerelease" : L"stable", m_UpdateManifestURL.GetString());
	AppendUpdateTrace(startTrace);
    
	HTTP_SEND_HEADER ht = PrepareHeader(m_UpdateManifestURL);
	
	m_UpdateReady = false;
	m_UpdateURL.Empty();
	m_UpdateSHA256.Empty();
	m_UpdateReleaseTag.Empty();
	m_DownloadedSHA256.Empty();
	m_TotalDownloadSize = 0;

	// clear stringstream
	m_file.str("");

	int   nTaskID = AddDownload(ht);
    m_monitor.reset (new CDownloadMonitor(m_hWnd, nTaskID));
}

LRESULT CAboutDlg::OnWhatsNew(WORD, WORD, HWND, BOOL&)
{
	if (IsValidReleaseTag(m_UpdateReleaseTag)) {
		CString url(FBE_PROJECT_URL); url += L"/releases/tag/"; url += m_UpdateReleaseTag;
		if (reinterpret_cast<INT_PTR>(ShellExecute(m_hWnd, L"open", url, NULL, NULL, SW_SHOWNORMAL)) > 32) return 0;
	}
	SetDlgItemText(IDC_TEXT_STATUS, FbeLoadRuntimeStringByKey(L"fbe.about.release_page_failed", L"Unable to open release page"));
	return 0;
}

LRESULT CAboutDlg::OnUpdate(WORD, WORD wID, HWND, BOOL&)
{
	if (!m_UpdateURL.IsEmpty())
	{
		// A portable copy must never download and execute the installed setup.
		// The same release is offered as a ZIP in the browser, leaving its
		// Data directory and deployment mode under the user's control.
		if (DeploymentContext::CurrentMode() == DeploymentContext::Mode::Portable)
		{
			ShellExecute(m_hWnd, L"open", GetPortableUpdateUrl(m_UpdateURL), NULL, NULL, SW_SHOWNORMAL);
			return 0L;
		}
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
	CString effectiveUrl = resp.m_effective_url;
	if (effectiveUrl.IsEmpty())
		effectiveUrl = pTask->GetURL();
	CString trace;
	trace.Format(
		L"connected: status=%d content_length=%d url=%s effective=%s",
		resp.m_status_code,
		resp.m_content_length,
		pTask->GetURL().GetString(),
		effectiveUrl.GetString());
	AppendUpdateTrace(trace);
    if (resp.m_status_code == 0 ||
        !IsHttpsUrl(effectiveUrl) ||
        resp.m_content_length > maximumSize)
    {
		trace.Format(
			L"reject response: status=%d content_length=%d maximum=%d effective=%s",
			resp.m_status_code,
			resp.m_content_length,
			maximumSize,
			effectiveUrl.GetString());
		AppendUpdateTrace(trace);
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

	CString trace;
	trace.Format(
		L"finish status: status=%d content_length=%d downloaded=%d final_read=%d accepted=%d",
		resp.m_status_code,
		resp.m_content_length,
		nDownload,
		resp.m_final_read_result,
		bStatus ? 1 : 0);
	AppendUpdateTrace(trace);

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
	if (pTask->GetURL().CompareNoCase(m_UpdateManifestURL) == 0)
	{
		bool manifestHandled = false;
		try
		{
			const string manifestData = m_file.str();
			CString manifestText = DecodeUtf8OrAnsi(manifestData);
			CString trace;
			trace.Format(
				L"manifest bytes=%u chars=%d",
				static_cast<unsigned int>(manifestData.size()),
				manifestText.GetLength());
			AppendUpdateTrace(trace);

			MSXML2::IXMLDOMDocument2Ptr xmlDoc(U::CreateDocument(false));
			xmlDoc->put_async(VARIANT_FALSE);
			xmlDoc->put_validateOnParse(VARIANT_FALSE);
			xmlDoc->put_resolveExternals(VARIANT_FALSE);
			xmlDoc->setProperty(L"ProhibitDTD", _variant_t(VARIANT_TRUE));

			_bstr_t str(manifestText);
			if (xmlDoc->loadXML(str) == VARIANT_TRUE && !xmlDoc->doctype)
			{
				MSXML2::IXMLDOMElementPtr root = xmlDoc->GetdocumentElement();
				CString availableVersion;
				CString releaseTag;
				CString releaseType;
				CString beta;
				CString updateURL;
				CString updateSHA256;
				CString rootName;
				if (root)
				{
					if (root->baseName.length() > 0)
						rootName = static_cast<const wchar_t*>(root->baseName);
					else
						rootName = static_cast<const wchar_t*>(root->nodeName);
				}
				const bool rootOk = rootName.CompareNoCase(L"FBE") == 0;
				bool versionOk = rootOk && GetUniqueNodeText(root, L"Version", availableVersion);
				bool releaseTagOk = rootOk && GetUniqueNodeText(root, L"ReleaseTag", releaseTag);
				bool releaseTypeOk = rootOk && GetUniqueNodeText(root, L"ReleaseType", releaseType);
				bool betaOk = rootOk && GetUniqueNodeText(root, L"Beta", beta);
				bool urlOk = rootOk && GetProfileArtifact(root, updateURL, updateSHA256);
				bool shaOk = urlOk;
				const bool profileManifest = urlOk;
				if (!profileManifest)
				{
					urlOk = rootOk && GetUniqueNodeText(root, L"DownloadUrl", updateURL);
					shaOk = rootOk && GetUniqueNodeText(root, L"SHA256", updateSHA256);
				}
				// Legacy manifests predate ReleaseTag and describe stable releases only.
				if (!releaseTagOk && versionOk) { releaseTag.Format(L"v%s", availableVersion.GetString()); releaseTagOk = true; releaseType = L"stable"; releaseTypeOk = true; beta = L"false"; betaOk = true; }

				if ((!versionOk || !urlOk || !shaOk) && rootOk)
				{
					// update.xml — маленький контролируемый манифест без вложенных
					// одноимённых тегов. Fallback нужен, чтобы проверка обновлений не
					// зависела от особенностей MSXML DOM/XPath на конкретной системе.
					versionOk = GetSimpleXmlTagText(manifestText, L"Version", availableVersion);
					urlOk = GetSimpleXmlTagText(manifestText, L"DownloadUrl", updateURL);
					shaOk = GetSimpleXmlTagText(manifestText, L"SHA256", updateSHA256);
					AppendUpdateTrace(L"manifest DOM nodes fallback: simple tag extraction used");
				}

				trace.Format(
					L"manifest checks: root=%s rootOk=%d versionOk=%d urlOk=%d shaOk=%d isVersion=%d",
					rootName.GetString(),
					rootOk ? 1 : 0,
					versionOk ? 1 : 0,
					urlOk ? 1 : 0,
					shaOk ? 1 : 0,
					IsValidUpdateVersion(availableVersion) ? 1 : 0);
				AppendUpdateTrace(trace);

				if (rootOk &&
					versionOk &&
					urlOk &&
					shaOk &&
					releaseTagOk && releaseTypeOk && betaOk &&
					IsValidUpdateVersion(availableVersion) &&
					IsValidReleaseTag(releaseTag) &&
					releaseTag.Mid(1) == availableVersion &&
					(releaseType.CompareNoCase(L"stable") == 0 || releaseType.CompareNoCase(L"prerelease") == 0) &&
					((releaseType.CompareNoCase(L"stable") == 0 && beta.CompareNoCase(L"false") == 0) ||
					 (releaseType.CompareNoCase(L"prerelease") == 0 && beta.CompareNoCase(L"true") == 0)) &&
					((releaseType.CompareNoCase(L"stable") == 0 && !IsPrereleaseUpdateVersion(availableVersion)) ||
					 (releaseType.CompareNoCase(L"prerelease") == 0 && IsPrereleaseUpdateVersion(availableVersion))) &&
					(_Settings.GetUpdateChannel() != UpdateChannel::Stable || releaseType.CompareNoCase(L"stable") == 0))
				{
					trace.Format(
						L"manifest parsed: version=%s url=%s sha256=%s",
						availableVersion.GetString(),
						updateURL.GetString(),
						updateSHA256.GetString());
					AppendUpdateTrace(trace);
					trace.Format(L"channel=%s manifest=%s current=%S available=%s releaseTag=%s releaseType=%s",
						_Settings.GetUpdateChannel() == UpdateChannel::Prerelease ? L"prerelease" : L"stable",
						m_UpdateManifestURL.GetString(), build_release_version, availableVersion.GetString(),
						releaseTag.GetString(), releaseType.GetString());
					AppendUpdateTrace(trace);
					if (CompareUpdateVersions(availableVersion, CString(build_release_version)) > 0)
					{
						CString path = updateURL;
						const int queryPosition = path.FindOneOf(L"?#");
						if (queryPosition >= 0)
							path = path.Left(queryPosition);
						const CString extension(ATLPath::FindExtension(path));

						const bool portable = DeploymentContext::CurrentMode() == DeploymentContext::Mode::Portable;
						const CString baseVersion = GetUpdateBaseVersion(availableVersion);
						if (IsTrustedUpdateUrl(updateURL, releaseTag, baseVersion) &&
							extension.CompareNoCase(portable ? L".zip" : L".exe") == 0 &&
							IsSHA256(updateSHA256))
						{
							m_UpdateReady = true;
							m_UpdateURL = updateURL;
							m_UpdateSHA256 = updateSHA256;
							m_UpdateReleaseTag = releaseTag;
							CString newVersionStatus;
							newVersionStatus.Format(FbeLoadRuntimeStringByKey(
								L"fbe.about.new_version_available", L"A new version %s of FBE is available."),
								availableVersion.GetString());
							SetDlgItemText(IDC_TEXT_STATUS, newVersionStatus);
							m_UpdatePict.SetBitmap(m_StatusBitmaps[1]);
							m_UpdateButton.ShowWindow(SW_SHOW);
							m_WhatsNewButton.ShowWindow(SW_SHOW);
							AppendUpdateTrace(L"updateAvailable=1");
							manifestHandled = true;
						}
						else
						{
							AppendUpdateTrace(L"manifest rejected: untrusted update URL, extension or SHA256");
							SetDlgItemText(IDC_TEXT_STATUS, m_sDownloadError);
							m_UpdatePict.SetBitmap(m_StatusBitmaps[2]);
						}
					}
					else 
					{
						AppendUpdateTrace(L"manifest accepted: current version is latest");
						AppendUpdateTrace(L"updateAvailable=0");
						SetDlgItemText (IDC_TEXT_STATUS, m_sHaveLatestVersion);
						m_UpdatePict.SetBitmap(m_StatusBitmaps[0]);
						manifestHandled = true;
					}
				}
				else
				{
					AppendUpdateTrace(L"manifest rejected: missing root, required nodes or version format");
				}
			}
			else
			{
				MSXML2::IXMLDOMParseErrorPtr parseError = xmlDoc->parseError;
				CString reason;
				if (parseError)
					reason = static_cast<const wchar_t*>(parseError->reason);
				trace.Format(L"manifest XML load failed: %s", reason.GetString());
				AppendUpdateTrace(trace);
			}
		}
		catch (const _com_error& error)
		{
			CString trace;
			trace.Format(L"manifest COM exception: 0x%08X", static_cast<unsigned int>(error.Error()));
			AppendUpdateTrace(trace);
		}
		if (!manifestHandled)
		{
			AppendUpdateTrace(L"manifest handling failed");
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
	CString userAgent;
	userAgent.Format(L"FictionBookEditorNext/%S", build_release_version);
    ht.m_user_agent = userAgent;
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

		m_SaveUpdateBtnState = GetDlgItem(IDC_UPDATE).IsWindowVisible();
		m_SaveWhatsNewBtnState = GetDlgItem(IDC_WHATS_NEW).IsWindowVisible();
		m_SaveUpdateBtnEnabled = GetDlgItem(IDC_UPDATE).IsWindowEnabled();
		m_SaveWhatsNewBtnEnabled = GetDlgItem(IDC_WHATS_NEW).IsWindowEnabled();
		GetDlgItem(IDC_UPDATE).ShowWindow(SW_HIDE);
		GetDlgItem(IDC_UPDATE).EnableWindow(FALSE);
		GetDlgItem(IDC_WHATS_NEW).ShowWindow(SW_HIDE);
		GetDlgItem(IDC_WHATS_NEW).EnableWindow(FALSE);

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

		if (m_SaveUpdateBtnState)
		{
			GetDlgItem(IDC_UPDATE).ShowWindow(SW_SHOW);
			GetDlgItem(IDC_UPDATE).EnableWindow(m_SaveUpdateBtnEnabled);
		}
		if (m_SaveWhatsNewBtnState)
		{
			GetDlgItem(IDC_WHATS_NEW).ShowWindow(SW_SHOW);
			GetDlgItem(IDC_WHATS_NEW).EnableWindow(m_SaveWhatsNewBtnEnabled);
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

	filename.Format(L"%sFBENext-update-%s.exe", tempDirectory, updateIdText);
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
