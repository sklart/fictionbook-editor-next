#pragma once

// Shared by SettingsNext and CMainFrame; must target the main window.
#define WM_FBE_APPLY_XML_SOURCE_THEME (WM_APP + 0x145)

#include <algorithm>
#include <map>
#include <vector>

#include "resource.h"
#include "res1.h"
#include "utils.h"
#include "XMLSerializer\Serializable.h"

const DWORD FBE_INTERFACE_LANGUAGE_AUTO = 0;
const DWORD FBE_INTERFACE_LANGUAGE_ENGLISH = 0xFBE001;
const DWORD FBE_INTERFACE_LANGUAGE_RUSSIAN = 0xFBE002;
const DWORD FBE_INTERFACE_LANGUAGE_UKRAINIAN = 0xFBE003;
const DWORD FBE_INTERFACE_LANGUAGE_GERMAN = 0xFBE004;
const DWORD FBE_INTERFACE_LANGUAGE_FRENCH = 0xFBE005;
const DWORD FBE_INTERFACE_LANGUAGE_SPANISH = 0xFBE006;
const DWORD FBE_INTERFACE_LANGUAGE_ITALIAN = 0xFBE007;
const DWORD FBE_INTERFACE_LANGUAGE_POLISH = 0xFBE008;
const DWORD FBE_INTERFACE_LANGUAGE_PORTUGUESE = 0xFBE009;
const DWORD FBE_INTERFACE_LANGUAGE_DUTCH = 0xFBE00A;
const DWORD FBE_INTERFACE_LANGUAGE_CZECH = 0xFBE00B;
const DWORD FBE_INTERFACE_LANGUAGE_BULGARIAN = 0xFBE00C;

const DWORD XML_SRC_SPECIAL_CHARS_WORD_LIKE = 0;
const DWORD XML_SRC_SPECIAL_CHARS_TEXT_LABELS = 1;

/*const int ILANG_ENGLISH = 0;
const int ILANG_RUSSIAN = 1;*/

class WordsItem : public ISerializable, public IObjectFactory
{
public:
	CString	m_word;

	int		m_count;
	CString	m_sCount;

	int		m_percent;
	int		m_prc_idx;

	WordsItem() : m_count(0), m_percent(0), m_prc_idx(0) { }
	WordsItem(CString word, int count) : m_word(word), m_count(count), m_percent(0), m_prc_idx(0) { }

	int GetProperties(std::vector<CString>& properties)
	{
		properties.push_back(L"Value");
		properties.push_back(L"Counted");
		return properties.size();
	}

	bool GetPropertyValue(const CString& sProperty, CProperty& property)
	{
		if(sProperty == L"Value")
		{
			property = m_word;
			return true;
		}
		else if(sProperty == L"Counted")
		{
			CString counted;
			counted.Format(L"%d", m_count);
			property = counted;
			return true;
		}
		return false;
	}

	bool SetPropertyValue(const CString& sProperty, CProperty& property)
	{
		if(sProperty == L"Value")
		{
			m_word = property;
			return true;
		}
		else if(sProperty == L"Counted")
		{
			m_count = StrToInt(property.GetStringValue());
			return true;
		}
		return false;
	}

	bool HasMultipleInstances()
	{
		return true;
	}

	CString GetClassName()
	{
		return L"Word";
	}


	CString GetID()
	{
		return L"";
	}

	ISerializable* Create()
	{
		return new WordsItem;
	}

	void Destroy(ISerializable* obj)
	{
		delete obj;
	}

	bool operator==(const WordsItem& wi)
	{
		return m_word.CompareNoCase(wi.m_word) == 0;
	}
};

class CHotkey : public ISerializable, public IObjectFactory
{
public:
	CString m_name;
	CString m_reg_name;
	ACCEL m_accel;
	ACCEL m_def_accel;
	CString m_desc;
	wchar_t m_char_val; // value for symbol hotkey

	CHotkey() : m_accel(), m_def_accel(), m_char_val(0) {}

	CHotkey(CString reg_name, int IDS_CMD_NAME, WORD fVirt, WORD cmd, WORD key, CString descr = L"") : m_reg_name(reg_name), m_accel(), m_def_accel(), m_desc(descr), m_char_val(0)
	{
		m_name = FbeLoadCString(IDS_CMD_NAME);

		m_def_accel.fVirt = FVIRTKEY | fVirt;
		m_def_accel.cmd = cmd;
		m_def_accel.key = key;

		m_accel = m_def_accel;
	}

	CHotkey(CString reg_name, int IDS_CMD_NAME, CString uchar,  WORD fVirt, WORD cmd, WORD key, CString descr = L"") : m_reg_name(reg_name), m_accel(), m_def_accel(), m_desc(descr), m_char_val(0)
	{
		m_name = FbeLoadCString(IDS_CMD_NAME);
		m_name += uchar;

		m_def_accel.fVirt = FVIRTKEY | fVirt;
		m_def_accel.cmd = cmd;
		m_def_accel.key = key;

		m_accel = m_def_accel;
	}

	CHotkey(CString reg_name, CString name, wchar_t symbol,  WORD fVirt, WORD cmd, WORD key, CString descr = L"") : m_name(name), m_reg_name(reg_name), m_accel(), m_def_accel(), m_desc(descr), m_char_val(symbol)
	{


		m_def_accel.fVirt = FVIRTKEY | fVirt;
		m_def_accel.cmd = cmd;
		m_def_accel.key = key;

		m_accel = m_def_accel;
	}

	CHotkey(CString reg_name, CString cmd_name, WORD fVirt, WORD cmd, WORD key, CString descr = L"") : m_name(cmd_name), m_reg_name(reg_name), m_accel(), m_def_accel(), m_desc(descr), m_char_val(0)
	{

		m_def_accel.fVirt = FVIRTKEY | fVirt;
		m_def_accel.cmd = cmd;
		m_def_accel.key = key;

		m_accel = m_def_accel;
	}

	bool operator < (const CHotkey& other) const
	{
		return (m_name.CompareNoCase(other.m_name) < 0);
	}

	// ISerializable interface
	int GetProperties(std::vector<CString>& properties)
	{
		properties.push_back(L"Name");
		properties.push_back(L"Accel");

		return properties.size();
	}

	bool GetPropertyValue(const CString& sProperty, CProperty& property)
	{
		if(sProperty == L"Name")
		{
			property = m_reg_name;
			return true;
		}
		if(sProperty == L"Accel")
		{
			CString temp;
			temp.Format(L"%u;%u",
				m_accel.fVirt, m_accel.key);
			property = temp;
			return true;
		}

		return false;
	}

	bool SetPropertyValue(const CString& sProperty, CProperty& sValue)
	{
		if(sProperty == L"Name")
		{
			m_reg_name = sValue.GetStringValue();
			return true;
		}
		else if(sProperty == L"Accel")
		{
			CString str = sValue.GetStringValue();
			int n = 0, curPos = 0;

			while(!str.Tokenize(L";", curPos).IsEmpty())
				n++;

			CString* tokens = new CString[n];
			curPos = n =0;

			CString temp;
			while(!(temp = str.Tokenize(L";", curPos)).IsEmpty())
			{
				tokens[n] = temp;
				n++;
			}

			if(n == 2)
			{
				m_accel.fVirt = StrToInt(tokens[0]);
				m_accel.key = StrToInt(tokens[1]);
			}

			delete[] tokens;

			return true;
		}

		return false;
	}

	bool HasMultipleInstances()
	{
		return false;
	}

	CString GetClassName()
	{
		return L"Hotkey";
	}

	CString GetID()
	{
		return L"";
	}

	ISerializable* Create()
	{
		return new CHotkey;
	}

	void Destroy(ISerializable* obj)
	{
		delete obj;
	}
};

class CHotkeysGroup : public ISerializable, public IObjectFactory
{
public:
	CString m_name;
	CString m_reg_name;
	std::vector<CHotkey> m_hotkeys;

	CHotkey m_hotkey_factory;
	std::vector<void*> m_ptr_hotkeys;

	CHotkeysGroup() = default;

	CHotkeysGroup(CString reg_name, int IDS_GROUP_NAME) : m_reg_name(reg_name)
	{
		m_name = FbeLoadCString(IDS_GROUP_NAME);
	}

	bool operator < (const CHotkeysGroup& other) const
	{
		return (m_name.CompareNoCase(other.m_name) < 0);
	}

	int GetProperties(std::vector<CString>& properties)
	{
		properties.push_back(L"GroupName");
		properties.push_back(L"Hotkey");

		return properties.size();
	}

	bool GetPropertyValue(const CString& sProperty, CProperty& property)
	{
		if(sProperty == L"GroupName")
		{
			property = m_reg_name;
			return true;
		}
		if(sProperty == L"Hotkey")
		{
			for(unsigned long i = 0 ; i < m_hotkeys.size(); ++i)
				m_ptr_hotkeys.push_back(&m_hotkeys[i]);
			
			property = m_ptr_hotkeys;
			property.SetFactory(&m_hotkey_factory);

			return true;
		}

		return false;
	}

	bool SetPropertyValue(const CString& sProperty, CProperty& sValue)
	{
		if(sProperty == L"GroupName")
		{
			m_reg_name = sValue.GetStringValue();
			return true;
		}
		if(sProperty == L"Hotkey")
		{
			std::vector<void*>::iterator iter = m_ptr_hotkeys.begin();

			while(iter != m_ptr_hotkeys.end())
			{
				CHotkey* pHotkey = static_cast<CHotkey*>(*iter);
				delete pHotkey;
				++iter;
			}

			CProperty::CopyPtrList(m_ptr_hotkeys, sValue.GetObjectList());

			m_hotkeys.clear();
			for(unsigned long i = 0 ; i < m_ptr_hotkeys.size(); ++i)
			{
				CHotkey* temp = (CHotkey*)m_ptr_hotkeys[i];
				m_hotkeys.push_back(*temp);
			}

			return true;
		}

		return false;
	}

	bool HasMultipleInstances()
	{
		return true;
	}

	CString GetClassName()
	{
		return L"HkGroup";
	}

	CString GetID()
	{
		return L"";
	}

	ISerializable* Create()
	{
		return new CHotkeysGroup;
	}

	void Destroy(ISerializable* obj)
	{
		delete obj;
	}
};

class DESCSHOWINFO : public ISerializable, public IObjectFactory
{
public:
	std::map<CString, bool> elements;

	DESCSHOWINFO()
	{
		SetDefaults();
	}

	// Default fields showing in description
	void SetDefaults()
	{
		elements[L"ci_all"] = true;
		elements[L"sti_all"] = false;
		elements[L"di_id"] = true;
		elements[L"id"] = true;
		elements[L"ti_kw"] = true;
		elements[L"ti_nic_mail_web"] = true;
		elements[L"ti_genre_match"] = true;
	}

	// ISerializable interface
	int GetProperties(std::vector<CString>& properties);
	bool GetPropertyValue(const CString& sProperty, CProperty& sValue);
	bool SetPropertyValue(const CString& sProperty, CProperty& sValue);
	bool HasMultipleInstances();
	CString GetClassName();
	CString GetID();

	// IObjectFactory
	ISerializable* Create();
	void Destroy(ISerializable*);
};

class TREEITEMSHOWINFO : public ISerializable, public IObjectFactory
{
public:
	std::map<CString, bool> items;

	TREEITEMSHOWINFO();
	void SetDefaults();

	// ISerializable interface
	int GetProperties(std::vector<CString>& properties);
	bool GetPropertyValue(const CString& sProperty, CProperty& sValue);
	bool SetPropertyValue(const CString& sProperty, CProperty& sValue);
	bool HasMultipleInstances();
	CString GetClassName();
	CString GetID();

	// IObjectFactory
	ISerializable* Create();
	void Destroy(ISerializable*);
};

enum XmlSrcColorPalette
{
	// Первые три значения уже записывались в настройки ранних сборок 3.0.7.
	// Их номера нельзя менять: это сохранит выбранную пользователем палитру.
	XML_SRC_COLOR_PALETTE_HISTORICAL = 0,
	// Зарезервировано для совместимости с настройками ранних сборок 3.0.7.
	// Отдельная контрастная схема больше не предлагается и приводится к FBE Light.
	XML_SRC_COLOR_PALETTE_LEGACY_CONTRAST = 1,
	XML_SRC_COLOR_PALETTE_FBE_DARK = 2,
	XML_SRC_COLOR_PALETTE_FBE_LIGHT = 3,
	// Зарезервировано для совместимости; приводится к FBE Dark.
	XML_SRC_COLOR_PALETTE_LEGACY_HIGH_CONTRAST_DARK = 4,
	// Новое значение по умолчанию. В зависимости от темы Windows выбирается
	// FBE Light или FBE Dark; при невозможности определения — FBE Light.
	XML_SRC_COLOR_PALETTE_SYSTEM,
	XML_SRC_COLOR_PALETTE_FBE_HIGH_CONTRAST_LIGHT,
	XML_SRC_COLOR_PALETTE_FBE_HIGH_CONTRAST_DARK,

	// Имена прежнего API оставлены для исходной и профильной совместимости.
	XML_SRC_COLOR_PALETTE_CLASSIC = XML_SRC_COLOR_PALETTE_HISTORICAL,
	XML_SRC_COLOR_PALETTE_DARK = XML_SRC_COLOR_PALETTE_FBE_DARK,
};

enum XmlSrcColorGroup
{
	XML_SRC_COLOR_TEXT = 0,
	XML_SRC_COLOR_TAG,
	XML_SRC_COLOR_ATTRIBUTE,
	XML_SRC_COLOR_STRING,
	XML_SRC_COLOR_COMMENT,
	// Добавлено в конец, чтобы не менять номера сохранённых цветов текста.
	XML_SRC_COLOR_BACKGROUND,
	XML_SRC_COLOR_GROUP_COUNT,
};

const DWORD XML_SRC_COLOR_DEFAULT = 0xffffffffUL;

// Стабильные логические роли оформления редактора исходного XML. Лексер
// Scintilla распознаёт синтаксис, а эта таблица определяет только внешний вид.
// Новые темы и семантические правила FB2 должны опираться на эти роли, а не на
// номера SCE_H_* и не на имена конкретных тегов.
enum XmlSrcStyleToken
{
	XML_SRC_STYLE_EDITOR_BACKGROUND = 0,
	XML_SRC_STYLE_EDITOR_FOREGROUND,
	XML_SRC_STYLE_SELECTION_BACKGROUND,
	XML_SRC_STYLE_SELECTION_FOREGROUND,
	XML_SRC_STYLE_CURRENT_LINE_BACKGROUND,
	XML_SRC_STYLE_CARET,
	XML_SRC_STYLE_LINE_NUMBER,
	XML_SRC_STYLE_LINE_NUMBER_ACTIVE,
	XML_SRC_STYLE_MATCHING_TAG_BACKGROUND,
	XML_SRC_STYLE_MATCHING_TAG_BORDER,
	XML_SRC_STYLE_XML_TEXT,
	XML_SRC_STYLE_XML_TAG_NAME,
	XML_SRC_STYLE_XML_TAG_DELIMITER,
	XML_SRC_STYLE_XML_ATTRIBUTE_NAME,
	XML_SRC_STYLE_XML_ATTRIBUTE_VALUE,
	XML_SRC_STYLE_XML_NAMESPACE,
	XML_SRC_STYLE_XML_COMMENT,
	XML_SRC_STYLE_XML_ENTITY,
	XML_SRC_STYLE_XML_CDATA,
	XML_SRC_STYLE_XML_PROCESSING_INSTRUCTION,
	XML_SRC_STYLE_XML_DOCTYPE,
	XML_SRC_STYLE_XML_ERROR,
	XML_SRC_STYLE_XML_WARNING,
	XML_SRC_STYLE_TOKEN_COUNT,
};

// Librusec is an alternative genre taxonomy, not a forced replacement for
// the normal locale-specific catalog.
enum class GenreCatalog
{
	Standard,
	Librusec,
};

class CSettings : public ISerializable, public IObjectFactory
{
	CRegKey		m_key;
	CString		m_key_path;

	bool		m_keep_encoding; // save with opened encoding
	CString		m_default_encoding;

	DWORD		m_search_options;

	DWORD		m_collorBG;
	DWORD		m_collorFG;
	DWORD		m_font_size;
	CString		m_font;
	CString		m_srcfont;

	bool		m_xml_src_wrap;
	bool		m_xml_src_syntaxHL;
	DWORD		m_xml_src_color_palette;
	CString		 m_xml_src_theme_id;
	DWORD		m_xml_src_colors[XML_SRC_COLOR_GROUP_COUNT];
	bool		m_xml_src_tagHL;
	bool		m_xml_src_showEOL;
	bool		m_xml_src_showSpace;
	bool		m_xml_src_showSpecialChars;
	DWORD		m_xml_src_specialCharsStyle;

	bool		m_fast_mode;
	bool		m_view_status_bar;
	bool		m_view_doc_tree;

	// added by SeNS
	bool		m_usespell_check;
	bool		m_highlght_check;
	CString		m_custom_dict;
	DWORD		m_custom_dict_codepage;
	CString		m_nbsp_char;
	CString		m_old_nbsp;
	bool		m_change_kbd_layout_check;
	DWORD		m_keyb_layout;
	bool		m_show_line_numbers;
	DWORD		m_image_type;
	DWORD		m_jpeg_quality;
	DWORD		m_image_import_format; // 0 Auto, 1 JPEG, 2 PNG
	DWORD		m_image_import_jpeg_quality;
	bool		m_image_import_keep_supported;
	///

	DWORD		m_splitter_pos;
	CString		m_toolbars_settings;
	CString		m_script_command_ids;

	bool		m_restore_file_position;

	DWORD		m_interface_lang_id;
	GenreCatalog	m_genre_catalog;

	bool		m_need_restart;

	CString		m_scripts_folder;

	bool		m_insimage_ask;
	bool		m_ins_clear_image;
	bool		m_create_backup_file;
	bool		m_show_full_path_in_window_title;

	bool		m_show_words_excls;

	WINDOWPLACEMENT m_words_dlg_placement;
	WINDOWPLACEMENT m_wnd_placement;

	DESCSHOWINFO m_desc;
	TREEITEMSHOWINFO m_tree_items;

	// added by SeNS: view dimensions for external helper
	int m_viewWidth, m_viewHeight;
	HWND m_hMainWindow;

public:
	std::vector<CHotkeysGroup> m_hotkey_groups;
	int keycodes; // total number of accelerators
	std::vector<WordsItem> m_words;

public:
	CSettings();
	~CSettings();

	void Init();
	void InitHotkeyGroups();
	void Close();

	// ISerializable interface
	int GetProperties(std::vector<CString>& properties);
	bool GetPropertyValue(const CString& sProperty, CProperty& sValue);
	bool SetPropertyValue(const CString& sProperty, CProperty& sValue);
	bool HasMultipleInstances();
	CString GetClassName();
	CString GetID();

	// IObjectFactory
	ISerializable* Create();
	void Destroy(ISerializable*);
	void SetDefaults();

	void Load();
	void Save();

	void LoadHotkeyGroups();
	void SaveHotkeyGroups();

	CHotkeysGroup* GetGroupByName(const CString& name);
	CHotkey* GetHotkeyByName(const CString& name, CHotkeysGroup& group);

	void LoadWords();
	void SaveWords();

	bool NeedRestart()const;

	bool KeepEncoding()const;
	bool XmlSrcWrap()const;
	bool XmlSrcSyntaxHL()const;
	DWORD GetXmlSrcColorPalette()const;
	CString GetXmlSrcThemeId()const;
	// Raw value is used only when an active user theme is deleted.
	CString GetStoredXmlSrcThemeId()const;
	DWORD GetXmlSrcColor(XmlSrcColorGroup group)const;
	bool HasXmlSrcCustomColor(XmlSrcColorGroup group)const;
	static XmlSrcColorGroup GetXmlSrcColorGroup(XmlSrcStyleToken token);
	static DWORD GetXmlSrcDefaultColor(DWORD palette, XmlSrcColorGroup group);
	static DWORD GetXmlSrcThemeColor(DWORD palette, XmlSrcStyleToken token);
	DWORD GetXmlSrcStyleColor(XmlSrcStyleToken token)const;
	bool XmlSrcTagHL()const;
	bool XmlSrcShowEOL()const;
	bool XmlSrcShowSpace()const;
	bool XmlSrcShowSpecialChars()const;
	DWORD XmlSrcSpecialCharsStyle()const;
	bool FastMode()const;
	bool ViewStatusBar()const;
	bool ViewDocumentTree()const;
	bool RestoreFilePosition()const;

	CString GetKeyPath()const;

	CString GetDefaultEncoding()const;
	DWORD	GetSearchOptions()const;
	DWORD	GetColorBG()const;
	DWORD	GetColorFG()const;
	DWORD	GetFontSize()const;
	CString	GetFont()const;
	CString	GetSrcFont()const;
	DWORD	GetSplitterPos()const;	
	CString GetToolbarsSettings()const;
	CString GetScriptCommandIds()const;
	const CRegKey& GetKey()const;

	// added by SeNS
	bool    GetUseSpellChecker()const;
	bool	GetHighlightMisspells()const;
	CString GetCustomDict()const;
	DWORD	GetCustomDictCodepage()const;
	CString GetNBSPChar()const;
	CString GetOldNBSPChar()const;
	bool	GetChangeKeybLayout()const;
	DWORD	GetKeybLayout()const;
	bool	XMLSrcShowLineNumbers()const;
	DWORD	GetImageType()const;
	DWORD	GetJpegQuality()const;
	DWORD	GetImageImportFormat()const;
	DWORD	GetImageImportJpegQuality()const;
	bool	GetImageImportKeepSupported()const;

	bool	GetExtElementStyle(const CString& elem)const;
	bool	GetWindowPosition(WINDOWPLACEMENT& wpl)const;
	DWORD	GetInterfaceLanguageID()const;
	DWORD GetEffectiveInterfaceLanguageID()const;
	CString GetInterfaceLanguageDllName()const;
	CString GetInterfaceLocaleName()const;
	CString GetLocalizedGenresFileName()const;
	GenreCatalog GetGenreCatalog()const;
	CString GetGenreCatalogFileName()const;
	CString GetGenreCatalogLegacyFileName()const;
	CString ResolveGenreCatalogFileName()const;
	CString GetInterfaceLanguageName()const;
	CString GetScriptsFolder() const;
	CString GetDefaultScriptsFolder();
	bool	IsDefaultScriptsFolder();
	bool	GetInsImageAsking()const;
	bool	GetIsInsClearImage()const;
	bool	GetCreateBackupFile()const;
	bool	GetShowFullPathInWindowTitle()const;
	bool	GetShowWordsExcls()const;
	bool	GetWordsDlgPosition(WINDOWPLACEMENT &wpl)const;

	bool	GetDocTreeItemState(const CString& item, bool default_state);

	void	SetKeepEncoding(bool keep, bool apply = false);
	void	SetDefaultEncoding(const CString& encoding, bool apply = false);	
	void	SetSearchOptions(DWORD opt, bool apply = false);
	void	SetColorBG(DWORD color, bool apply = false);
	void	SetColorFG(DWORD color, bool apply = false);
	void	SetFontSize(DWORD size, bool apply = false);
	void	SetXmlSrcWrap(bool wrap, bool apply = false);
	void	SetXmlSrcSyntaxHL(bool hl, bool apply = false);
	void	SetXmlSrcColorPalette(DWORD palette, bool apply = false);
	void	SetXmlSrcThemeId(const CString& id, bool apply = false);
	void	SetXmlSrcColor(XmlSrcColorGroup group, DWORD color, bool apply = false);
	void	SetXmlSrcTagHL(bool hl, bool apply = false);
	void	SetXmlSrcShowEOL(bool eol, bool apply = false);
	void	SetXmlSrcShowSpace(bool eol, bool apply = false);
	void	SetXmlSrcShowSpecialChars(bool show, bool apply = false);
	void	SetXmlSrcSpecialCharsStyle(DWORD style, bool apply = false);
	void	SetFastMode(bool mode,  bool apply = false);
	void	SetFont(const CString& font, bool apply = false);
	void	SetSrcFont(const CString& font, bool apply = false);
	void	SetViewStatusBar(bool view,  bool apply = false);
	void	SetViewDocumentTree(bool view,  bool apply = false);
	void	SetSplitterPos(DWORD pos,  bool apply = false);
	void	SetToolbarsSettings(CString& settings,  bool apply = false);
	void	SetScriptCommandIds(const CString& ids, bool apply = false);
	void	SetExtElementStyle(const CString& elem, bool ext, bool apply = false);
	void	SetWindowPosition(const WINDOWPLACEMENT& wpl,  bool apply = false);
	void	SetRestoreFilePosition(bool restore, bool apply = false);	
	void	SetInterfaceLanguage(DWORD Language, bool apply = false);
	void	SetGenreCatalog(GenreCatalog catalog, bool apply = false);
	void	SetScriptsFolder(const CString& fullpath, bool apply = false);
	void	SetInsImageAsking(const bool value, bool apply = false);
	void	SetIsInsClearImage(const bool value, bool apply = false);
	void	SetCreateBackupFile(const bool value, bool apply = false);
	void	SetShowFullPathInWindowTitle(const bool value, bool apply = false);
	void	SetDocTreeItemState(const CString& item, bool state);
	void	SetShowWordsExcls(const bool value, bool apply = false);
	void	SetWordsDlgPosition(const WINDOWPLACEMENT& wpl,  bool apply = false);

	void	SetNeedRestart();

	CString	m_initial_scripts_folder;

	// added by SeNS
	void	SetUseSpellChecker(const bool value, bool apply = false);
	void	SetHighlightMisspells(const bool value, bool apply = false);
	void	SetCustomDict(const ATL::CString &value, bool apply = false);
	void	SetCustomDictCodepage(const DWORD value, bool apply = false);
	void	SetNBSPChar(const ATL::CString &value, bool apply = false);
	void	SetChangeKeybLayout(const bool value, bool apply = false);
	void	SetKeybLayout(const DWORD value, bool apply = false);
	void	SetXMLSrcShowLineNumbers(const bool value, bool apply = false);
	void	SetImageType(const DWORD value, bool apply = false);
	void	SetJpegQuality(const DWORD value, bool apply = false);
	void	SetImageImportFormat(const DWORD value, bool apply = false);
	void	SetImageImportJpegQuality(const DWORD value, bool apply = false);
	void	SetImageImportKeepSupported(const bool value, bool apply = false);

	void	SetViewWidth(int width) { m_viewWidth = width; }
	void	SetViewHeight(int height) { m_viewHeight = height; }
	int		GetViewWidth() { return m_viewWidth; }
	int		GetViewHeight() { return m_viewHeight; }
	void	SetMainWindow(HWND hwnd) { m_hMainWindow = hwnd; }
	HWND	GetMainWindow() { return m_hMainWindow; }

};
