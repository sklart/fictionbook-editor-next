#include "stdafx.h"
#include "ImportOptionsDialog.h"

#include <commctrl.h>
#include <vector>

namespace
{
    enum
    {
        IDC_OPT_COVER = 2001,
        IDC_OPT_IMAGES,
        IDC_OPT_NOTES,
        IDC_OPT_NAV_TITLES,
        IDC_OPT_REPAIR_ENCODING,
        IDC_OPT_SKIP_SERVICE,
        IDC_OPT_TABLES,
        IDC_OPT_LISTS,
        IDC_OPT_POEMS,
        IDC_OPT_SUBTITLES,
        IDC_OPT_SPLIT_HEADINGS,
        IDC_OPT_LINKS,
        IDC_OPT_CLEAN_TYPOGRAPHY,
        IDC_OPT_PAGE_BREAKS,
        IDC_OPT_SKIP_HIDDEN,
        IDC_OPT_VALIDATE,
        IDC_OPT_DIAGNOSTIC,
        IDC_OPT_LOG,
        IDC_OPT_SAVE_FB2,
        IDC_OPT_CSS_SEMANTICS,
        IDC_OPT_REMOVE_BACKLINKS,
        IDC_OPT_REMOVE_SERVICE_SECTIONS,
        IDC_OPT_LOG_ON_WARNINGS,
        IDC_OPT_SAVE_INTERMEDIATE_ON_ERROR,
        IDC_OPT_KEEP_TEMP,
        IDC_SVG_MODE = 2101,
        IDC_DEFAULTS = 2201,
        IDC_IMPORT = IDOK,
        IDC_CANCEL = IDCANCEL
    };

    struct CheckDef
    {
        int id;
        LPCWSTR text;
        LPCWSTR tooltip;
        bool EpubImportOptions::*field;
    };

    const CheckDef kChecks[] =
    {
        { IDC_OPT_COVER, L"Импортировать обложку", L"Ищет обложку в OPF по properties=cover-image, meta name=cover и cover.xhtml. В FB2 добавляется <coverpage> и соответствующий <binary>.", &EpubImportOptions::importCover },
        { IDC_OPT_IMAGES, L"Импортировать изображения", L"Преобразует XHTML <img> в FB2 <image l:href=\"#...\"/> и добавляет использованные файлы изображений как <binary> в конце FB2.", &EpubImportOptions::importImages },
        { IDC_OPT_NOTES, L"Импортировать сноски", L"Переносит EPUB-сноски/endnotes/footnotes в отдельный FB2 <body name=\"notes\"> и делает ссылки <a type=\"note\"> рабочими в Fiction Book Editor.", &EpubImportOptions::importNotes },
        { IDC_OPT_NAV_TITLES, L"Использовать nav.xhtml / toc.ncx для заголовков", L"Берёт названия глав из EPUB-оглавления, если в XHTML-файле нет нормального h1/h2 или имя файла выглядит техническим.", &EpubImportOptions::useNavigationTitles },
        { IDC_OPT_REPAIR_ENCODING, L"Исправлять типичные ошибки кодировки", L"Пытается восстановить текст вида РђРЅРЅРѕС‚Р°С†РёСЏ в нормальную кириллицу. Применяется к заголовкам, метаданным и тексту.", &EpubImportOptions::repairEncoding },
        { IDC_OPT_SKIP_SERVICE, L"Пропускать служебные страницы", L"Не добавляет в основной текст cover.xhtml, nav.xhtml, toc.xhtml, страницы оглавления и отдельные документы со сносками.", &EpubImportOptions::skipServicePages },
        { IDC_OPT_TABLES, L"Импортировать таблицы", L"Преобразует XHTML table/tr/td/th в FB2 table/tr/td/th с сохранением colspan, rowspan, align и valign там, где они есть.", &EpubImportOptions::importTables },
        { IDC_OPT_LISTS, L"Импортировать списки", L"Преобразует XHTML ul/ol/li в отдельные FB2-абзацы: маркированные списки получают символ •, нумерованные — номера 1., 2., 3.", &EpubImportOptions::importLists },
        { IDC_OPT_POEMS, L"Распознавать стихи, эпиграфы и цитаты", L"По тегам epub:type и CSS-классам poem, stanza, verse, epigraph, cite пытается создавать FB2 <poem>, <stanza>, <v>, <epigraph> и <cite>.", &EpubImportOptions::importPoemsEpigraphs },
        { IDC_OPT_SUBTITLES, L"Распознавать подзаголовки", L"Элементы с epub:type=subtitle или CSS-классом subtitle/subhead переносятся как FB2 <subtitle>, а не как обычные абзацы.", &EpubImportOptions::importSubtitles },
        { IDC_OPT_SPLIT_HEADINGS, L"Создавать секции по заголовкам", L"Если внутри одного XHTML-файла есть несколько h1/h2/h3, плагин создаёт несколько FB2 <section>, а не складывает все заголовки как подзаголовки одной главы.", &EpubImportOptions::splitSectionsByHeadings },
        { IDC_OPT_LINKS, L"Сохранять ссылки и якоря", L"Сохраняет внутренние и внешние ссылки EPUB как FB2 <a l:href=\"...\">. Для элементов с id/name старается сохранить идентификаторы.", &EpubImportOptions::preserveLinks },
        { IDC_OPT_CLEAN_TYPOGRAPHY, L"Очищать типографику текста", L"Удаляет мягкие переносы, заменяет неразрывные пробелы обычными, нормализует частые служебные пробельные символы. Это уменьшает мусор после импорта из EPUB.", &EpubImportOptions::cleanTypography },
        { IDC_OPT_PAGE_BREAKS, L"Импортировать разделители и номера страниц", L"Переносит XHTML hr и pagebreak/doc-pagebreak как <empty-line/>. Если в EPUB указан номер страницы, добавляет его мелким текстом после разрыва.", &EpubImportOptions::importPageBreaks },
        { IDC_OPT_SKIP_HIDDEN, L"Пропускать скрытые элементы", L"Не импортирует элементы с hidden, aria-hidden=true, display:none, visibility:hidden и типичными CSS-классами hidden/sr-only. Убирает технический и экранно-дикторский мусор из текста.", &EpubImportOptions::skipHiddenElements },
        { IDC_OPT_VALIDATE, L"Проверять итоговый FB2 перед открытием", L"Перед передачей в FBE выполняет базовую XML-проверку и добавляет предупреждения по отсутствующим body, binary или notes-ссылкам.", &EpubImportOptions::validateResult },
        { IDC_OPT_DIAGNOSTIC, L"Добавлять диагностический раздел в книгу", L"Вставляет в начало FB2 отдельный раздел с предупреждениями импорта. Полезно для отладки, но обычно не нужно для готовой книги.", &EpubImportOptions::addDiagnosticSection },
        { IDC_OPT_LOG, L"Писать лог импорта рядом с EPUB", L"Создаёт текстовый файл .ImportEPUB.log рядом с исходным EPUB: найденный OPF, количество глав, картинок, сносок, пропуски и ошибки.", &EpubImportOptions::writeImportLog },
        { IDC_OPT_SAVE_FB2, L"Сохранять FB2-копию рядом с EPUB", L"После успешного импорта дополнительно сохраняет полученный FB2 как файл .imported.fb2 рядом с исходным EPUB. Удобно для проверки, сравнения и анализа без повторного запуска FBE.", &EpubImportOptions::saveFb2Copy },
        { IDC_OPT_CSS_SEMANTICS, L"Использовать CSS-классы для структуры", L"Учитывает смысловые CSS-классы: subtitle, epigraph, poem, stanza, verse, text-author, note, footnote, sidebar, dedication. Полноценные визуальные CSS-стили в FB2 не переносятся.", &EpubImportOptions::useCssSemanticClasses },
        { IDC_OPT_REMOVE_BACKLINKS, L"Удалять обратные ссылки из сносок", L"Удаляет из текста сносок служебные ссылки возврата вида ↩, back, backlink и элементы role=doc-backlink. Основные ссылки из текста к сноскам сохраняются.", &EpubImportOptions::removeFootnoteBacklinks },
        { IDC_OPT_REMOVE_SERVICE_SECTIONS, L"Удалять служебные и пустые разделы", L"Дополнительно пропускает пустые, навигационные и технические разделы: cover, titlepage, toc, nav, landmarks, page-list, copyright-page, calibre-служебные блоки.", &EpubImportOptions::removeServiceSections },
        { IDC_OPT_LOG_ON_WARNINGS, L"Создавать лог при ошибках и предупреждениях", L"Если импорт завершился ошибкой или были предупреждения проверки FB2, лог .ImportEPUB.log создаётся автоматически, даже когда постоянное логирование выключено.", &EpubImportOptions::writeLogOnWarnings },
        { IDC_OPT_SAVE_INTERMEDIATE_ON_ERROR, L"Сохранять промежуточный FB2 при ошибке", L"Если FB2 был сформирован, но проверка выявила проблемы, рядом с EPUB сохраняется .failed-import.fb2 для анализа результата без повторного импорта.", &EpubImportOptions::saveIntermediateFb2OnError },
        { IDC_OPT_KEEP_TEMP, L"Сохранять временную распаковку при ошибке", L"Если импорт завершится ошибкой, временная папка с распакованным EPUB не удаляется. Это удобно для анализа проблемных файлов.", &EpubImportOptions::keepTempOnError }
    };



    const int kGroupContent[] =
    {
        IDC_OPT_COVER,
        IDC_OPT_NAV_TITLES,
        IDC_OPT_SPLIT_HEADINGS,
        IDC_OPT_CSS_SEMANTICS,
        IDC_OPT_TABLES,
        IDC_OPT_LISTS,
        IDC_OPT_POEMS,
        IDC_OPT_SUBTITLES,
        IDC_OPT_PAGE_BREAKS
    };

    const int kGroupImages[] =
    {
        IDC_OPT_IMAGES
    };

    const int kGroupLinksNotes[] =
    {
        IDC_OPT_NOTES,
        IDC_OPT_LINKS,
        IDC_OPT_REMOVE_BACKLINKS
    };

    const int kGroupCleanup[] =
    {
        IDC_OPT_REPAIR_ENCODING,
        IDC_OPT_CLEAN_TYPOGRAPHY,
        IDC_OPT_SKIP_SERVICE,
        IDC_OPT_REMOVE_SERVICE_SECTIONS,
        IDC_OPT_SKIP_HIDDEN
    };

    const int kGroupDiagnostics[] =
    {
        IDC_OPT_VALIDATE,
        IDC_OPT_LOG,
        IDC_OPT_LOG_ON_WARNINGS,
        IDC_OPT_DIAGNOSTIC,
        IDC_OPT_SAVE_FB2,
        IDC_OPT_SAVE_INTERMEDIATE_ON_ERROR,
        IDC_OPT_KEEP_TEMP
    };

    struct GroupDef
    {
        LPCWSTR title;
        LPCWSTR tooltip;
        const int* items;
        int count;
        bool hasSvgMode;
    };

    const GroupDef kGroups[] =
    {
        { L"Содержимое и структура", L"Параметры, влияющие на основное содержимое FB2, дерево секций, оглавление, таблицы, списки, стихи и разрывы страниц.", kGroupContent, static_cast<int>(_countof(kGroupContent)), false },
        { L"Изображения и SVG", L"Параметры импорта обычных изображений и SVG. SVG может быть преобразован в PNG/JPEG через optional-модуль ImportEPUBLunaSVG.dll или заменён видимой заглушкой.", kGroupImages, static_cast<int>(_countof(kGroupImages)), true },
        { L"Ссылки и сноски", L"Параметры сохранения внутренних/внешних ссылок, EPUB-сносок и удаления обратных ссылок из notes.", kGroupLinksNotes, static_cast<int>(_countof(kGroupLinksNotes)), false },
        { L"Очистка и фильтрация", L"Параметры исправления текста, удаления скрытых элементов и служебных EPUB-страниц.", kGroupCleanup, static_cast<int>(_countof(kGroupCleanup)), false },
        { L"Проверка, логирование и отладка", L"Параметры проверки результата, создания логов и сохранения промежуточных FB2-файлов для анализа.", kGroupDiagnostics, static_cast<int>(_countof(kGroupDiagnostics)), false }
    };

    const CheckDef* FindCheckDef(int id)
    {
        for (int i = 0; i < static_cast<int>(_countof(kChecks)); ++i)
        {
            if (kChecks[i].id == id)
                return &kChecks[i];
        }
        return nullptr;
    }

    const wchar_t* kSettingsKey = L"Software\\FBETeam\\FictionBook Editor\\ImportEPUB";

    DWORD BoolToDword(bool value)
    {
        return value ? 1u : 0u;
    }

    bool DwordToBool(DWORD value)
    {
        return value != 0;
    }

    void LoadBoolValue(CRegKey& key, LPCWSTR name, bool& value)
    {
        DWORD data = 0;
        if (key.QueryDWORDValue(name, data) == ERROR_SUCCESS)
            value = DwordToBool(data);
    }

    void SaveBoolValue(CRegKey& key, LPCWSTR name, bool value)
    {
        key.SetDWORDValue(name, BoolToDword(value));
    }

    void ResetControlsToOptions(HWND hwnd, const EpubImportOptions& options)
    {
        for (int i = 0; i < static_cast<int>(_countof(kChecks)); ++i)
        {
            HWND chk = GetDlgItem(hwnd, kChecks[i].id);
            if (chk)
                SendMessageW(chk, BM_SETCHECK, options.*(kChecks[i].field) ? BST_CHECKED : BST_UNCHECKED, 0);
        }

        HWND combo = GetDlgItem(hwnd, IDC_SVG_MODE);
        if (combo)
        {
            int mode = options.svgConversionMode;
            if (mode < SVG_IMPORT_KEEP || mode > SVG_IMPORT_SKIP)
                mode = SVG_IMPORT_CONVERT_PNG;
            SendMessageW(combo, CB_SETCURSEL, static_cast<WPARAM>(mode), 0);
        }
    }

    class OptionsWindow
    {
    public:
        OptionsWindow(HWND owner, EpubImportOptions& options)
            : m_owner(owner), m_options(options), m_hwnd(nullptr), m_tooltip(nullptr), m_done(false), m_result(false)
        {
        }

        bool DoModal()
        {
            INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES };
            InitCommonControlsEx(&icc);

            WNDCLASSW wc = {};
            wc.lpfnWndProc = StaticWndProc;
            wc.hInstance = _AtlBaseModule.GetModuleInstance();
            wc.lpszClassName = L"FBEImportEPUBOptionsWindow";
            wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
            wc.hIcon = LoadIcon(_AtlBaseModule.GetModuleInstance(), MAKEINTRESOURCE(101));
            wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
            RegisterClassW(&wc);

            const int width = 700;
            const int height = 920;
            RECT rc = { 0, 0, width, height };
            AdjustWindowRectEx(&rc, WS_CAPTION | WS_SYSMENU | WS_POPUP, FALSE, WS_EX_DLGMODALFRAME);

            int x = CW_USEDEFAULT;
            int y = CW_USEDEFAULT;
            if (m_owner)
            {
                RECT pr = {};
                GetWindowRect(m_owner, &pr);
                x = pr.left + ((pr.right - pr.left) - (rc.right - rc.left)) / 2;
                y = pr.top + ((pr.bottom - pr.top) - (rc.bottom - rc.top)) / 2;
            }

            m_hwnd = CreateWindowExW(
                WS_EX_DLGMODALFRAME,
                wc.lpszClassName,
                L"Настройки импорта EPUB",
                WS_CAPTION | WS_SYSMENU | WS_POPUP,
                x, y, rc.right - rc.left, rc.bottom - rc.top,
                m_owner, nullptr, _AtlBaseModule.GetModuleInstance(), this);

            if (!m_hwnd)
                return true; // fail open: use defaults and continue import

            if (m_owner)
                EnableWindow(m_owner, FALSE);

            ShowWindow(m_hwnd, SW_SHOW);
            UpdateWindow(m_hwnd);

            MSG msg = {};
            while (!m_done && GetMessageW(&msg, nullptr, 0, 0) > 0)
            {
                if (!IsDialogMessageW(m_hwnd, &msg))
                {
                    TranslateMessage(&msg);
                    DispatchMessageW(&msg);
                }
            }

            if (m_owner)
            {
                EnableWindow(m_owner, TRUE);
                SetActiveWindow(m_owner);
            }

            return m_result;
        }

    private:
        static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
        {
            OptionsWindow* self = nullptr;
            if (msg == WM_NCCREATE)
            {
                CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
                self = reinterpret_cast<OptionsWindow*>(cs->lpCreateParams);
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            }
            else
            {
                self = reinterpret_cast<OptionsWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            }

            if (self)
                return self->WndProc(hwnd, msg, wp, lp);
            return DefWindowProcW(hwnd, msg, wp, lp);
        }

        LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
        {
            switch (msg)
            {
            case WM_CREATE:
                CreateControls(hwnd);
                return 0;

            case WM_COMMAND:
                switch (LOWORD(wp))
                {
                case IDC_IMPORT:
                    ReadOptions();
                    Finish(true);
                    return 0;
                case IDC_DEFAULTS:
                    m_options = EpubImportOptions();
                    ResetControlsToOptions(m_hwnd, m_options);
                    return 0;
                case IDC_CANCEL:
                    Finish(false);
                    return 0;
                }
                break;

            case WM_CLOSE:
                Finish(false);
                return 0;

            case WM_DESTROY:
                return 0;
            }
            return DefWindowProcW(hwnd, msg, wp, lp);
        }

        void CreateControls(HWND hwnd)
        {
            HFONT font = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

            HWND title = CreateWindowW(L"STATIC", L"Выберите параметры преобразования EPUB в FB2:",
                WS_CHILD | WS_VISIBLE, 14, 14, 650, 22, hwnd, nullptr, _AtlBaseModule.GetModuleInstance(), nullptr);
            SendMessageW(title, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

            m_tooltip = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr,
                WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
                CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                hwnd, nullptr, _AtlBaseModule.GetModuleInstance(), nullptr);
            SendMessageW(m_tooltip, TTM_SETMAXTIPWIDTH, 0, 680);

            const int groupX = 14;
            const int groupW = 660;
            const int checkX = groupX + 14;
            const int checkW = groupW - 28;
            int y = 44;

            for (int g = 0; g < static_cast<int>(_countof(kGroups)); ++g)
            {
                const GroupDef& group = kGroups[g];
                const int groupHeight = 24 + group.count * 22 + (group.hasSvgMode ? 30 : 0) + 8;

                HWND box = CreateWindowW(L"BUTTON", group.title,
                    WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                    groupX, y, groupW, groupHeight,
                    hwnd, nullptr, _AtlBaseModule.GetModuleInstance(), nullptr);
                SendMessageW(box, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
                AddTooltip(box, group.tooltip);

                int itemY = y + 20;
                for (int i = 0; i < group.count; ++i)
                {
                    const CheckDef* def = FindCheckDef(group.items[i]);
                    if (!def)
                        continue;

                    HWND chk = CreateWindowW(L"BUTTON", def->text,
                        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
                        checkX, itemY, checkW, 20, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(def->id)),
                        _AtlBaseModule.GetModuleInstance(), nullptr);
                    SendMessageW(chk, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
                    SendMessageW(chk, BM_SETCHECK, m_options.*(def->field) ? BST_CHECKED : BST_UNCHECKED, 0);
                    AddTooltip(chk, def->tooltip);
                    itemY += 22;
                }

                if (group.hasSvgMode)
                {
                    HWND svgLabel = CreateWindowW(L"STATIC", L"SVG-изображения:",
                        WS_CHILD | WS_VISIBLE, checkX, itemY + 2, 160, 20, hwnd, nullptr, _AtlBaseModule.GetModuleInstance(), nullptr);
                    SendMessageW(svgLabel, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
                    AddTooltip(svgLabel, L"FB2/FBE обычно не отображает SVG напрямую. Для режимов PNG/JPEG используется необязательная ImportEPUBLunaSVG.dll рядом с плагином/утилитой. Если её нет или рендеринг не удался, будет вставлена видимая заглушка.");

                    HWND svgCombo = CreateWindowW(L"COMBOBOX", nullptr,
                        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_TABSTOP,
                        checkX + 165, itemY, checkW - 165, 180, hwnd, reinterpret_cast<HMENU>(IDC_SVG_MODE),
                        _AtlBaseModule.GetModuleInstance(), nullptr);
                    SendMessageW(svgCombo, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
                    SendMessageW(svgCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Оставлять SVG как есть"));
                    SendMessageW(svgCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Преобразовывать SVG в PNG"));
                    SendMessageW(svgCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Преобразовывать SVG в JPEG"));
                    SendMessageW(svgCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Пропускать SVG"));
                    int svgMode = m_options.svgConversionMode;
                    if (svgMode < SVG_IMPORT_KEEP || svgMode > SVG_IMPORT_SKIP)
                        svgMode = SVG_IMPORT_CONVERT_PNG;
                    SendMessageW(svgCombo, CB_SETCURSEL, static_cast<WPARAM>(svgMode), 0);
                    AddTooltip(svgCombo, L"Рекомендуется PNG. ImportEPUB сам не содержит LunaSVG: он ищет ImportEPUBLunaSVG.dll рядом с ImportEPUB.dll или ImportEPUBBatch.exe. При отсутствии DLL создаётся PNG/JPEG-заглушка с именем SVG-файла.");
                }

                y += groupHeight + 6;
            }

            HWND hint = CreateWindowW(L"STATIC", L"Подсказка: наведите мышь на любой пункт или заголовок раздела, чтобы увидеть пояснение.",
                WS_CHILD | WS_VISIBLE, 18, y + 4, 650, 22, hwnd, nullptr, _AtlBaseModule.GetModuleInstance(), nullptr);
            SendMessageW(hint, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

            const int buttonY = y + 36;
            HWND defaults = CreateWindowW(L"BUTTON", L"По умолчанию", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
                18, buttonY, 130, 28, hwnd, reinterpret_cast<HMENU>(IDC_DEFAULTS), _AtlBaseModule.GetModuleInstance(), nullptr);
            HWND ok = CreateWindowW(L"BUTTON", L"ОК", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP,
                475, buttonY, 90, 28, hwnd, reinterpret_cast<HMENU>(IDC_IMPORT), _AtlBaseModule.GetModuleInstance(), nullptr);
            HWND cancel = CreateWindowW(L"BUTTON", L"Отмена", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
                575, buttonY, 90, 28, hwnd, reinterpret_cast<HMENU>(IDC_CANCEL), _AtlBaseModule.GetModuleInstance(), nullptr);
            SendMessageW(defaults, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            SendMessageW(ok, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            SendMessageW(cancel, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            AddTooltip(defaults, L"Возвращает все параметры к рекомендуемым значениям по умолчанию. Окно при этом не закрывается.");
        }

        void AddTooltip(HWND control, LPCWSTR text)
        {
            if (!m_tooltip || !control || !text)
                return;

            TOOLINFOW ti = {};
            ti.cbSize = sizeof(ti);
            ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
            ti.hwnd = m_hwnd;
            ti.uId = reinterpret_cast<UINT_PTR>(control);
            ti.lpszText = const_cast<LPWSTR>(text);
            SendMessageW(m_tooltip, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&ti));
        }

        void ReadOptions()
        {
            for (int i = 0; i < static_cast<int>(_countof(kChecks)); ++i)
            {
                HWND chk = GetDlgItem(m_hwnd, kChecks[i].id);
                m_options.*(kChecks[i].field) = (SendMessageW(chk, BM_GETCHECK, 0, 0) == BST_CHECKED);
            }

            HWND svgCombo = GetDlgItem(m_hwnd, IDC_SVG_MODE);
            if (svgCombo)
            {
                LRESULT sel = SendMessageW(svgCombo, CB_GETCURSEL, 0, 0);
                if (sel >= SVG_IMPORT_KEEP && sel <= SVG_IMPORT_SKIP)
                    m_options.svgConversionMode = static_cast<int>(sel);
            }
        }

        void Finish(bool result)
        {
            m_result = result;
            m_done = true;
            if (m_hwnd)
            {
                DestroyWindow(m_hwnd);
                m_hwnd = nullptr;
            }
        }

        HWND m_owner;
        EpubImportOptions& m_options;
        HWND m_hwnd;
        HWND m_tooltip;
        bool m_done;
        bool m_result;
    };
}

void LoadImportOptions(EpubImportOptions& options)
{
    // Start with code defaults so new options added in future versions get sane values.
    options = EpubImportOptions();

    CRegKey key;
    if (key.Open(HKEY_CURRENT_USER, kSettingsKey, KEY_READ) != ERROR_SUCCESS)
        return;

    LoadBoolValue(key, L"ImportCover", options.importCover);
    LoadBoolValue(key, L"ImportImages", options.importImages);
    LoadBoolValue(key, L"ImportNotes", options.importNotes);
    LoadBoolValue(key, L"UseNavigationTitles", options.useNavigationTitles);
    LoadBoolValue(key, L"RepairEncoding", options.repairEncoding);
    LoadBoolValue(key, L"SkipServicePages", options.skipServicePages);
    LoadBoolValue(key, L"ImportTables", options.importTables);
    LoadBoolValue(key, L"ImportLists", options.importLists);
    LoadBoolValue(key, L"ImportPoemsEpigraphs", options.importPoemsEpigraphs);
    LoadBoolValue(key, L"ImportSubtitles", options.importSubtitles);
    LoadBoolValue(key, L"SplitSectionsByHeadings", options.splitSectionsByHeadings);
    LoadBoolValue(key, L"PreserveLinks", options.preserveLinks);
    LoadBoolValue(key, L"CleanTypography", options.cleanTypography);
    LoadBoolValue(key, L"ImportPageBreaks", options.importPageBreaks);
    LoadBoolValue(key, L"SkipHiddenElements", options.skipHiddenElements);
    LoadBoolValue(key, L"ValidateResult", options.validateResult);
    LoadBoolValue(key, L"AddDiagnosticSection", options.addDiagnosticSection);
    LoadBoolValue(key, L"WriteImportLog", options.writeImportLog);
    LoadBoolValue(key, L"SaveFb2Copy", options.saveFb2Copy);
    LoadBoolValue(key, L"UseCssSemanticClasses", options.useCssSemanticClasses);
    LoadBoolValue(key, L"RemoveFootnoteBacklinks", options.removeFootnoteBacklinks);
    LoadBoolValue(key, L"RemoveServiceSections", options.removeServiceSections);
    LoadBoolValue(key, L"WriteLogOnWarnings", options.writeLogOnWarnings);
    LoadBoolValue(key, L"SaveIntermediateFb2OnError", options.saveIntermediateFb2OnError);
    LoadBoolValue(key, L"KeepTempOnError", options.keepTempOnError);
    DWORD svgMode = static_cast<DWORD>(options.svgConversionMode);
    if (key.QueryDWORDValue(L"SvgConversionMode", svgMode) == ERROR_SUCCESS &&
        svgMode <= static_cast<DWORD>(SVG_IMPORT_SKIP))
    {
        options.svgConversionMode = static_cast<int>(svgMode);
    }
}

void SaveImportOptions(const EpubImportOptions& options)
{
    CRegKey key;
    if (key.Create(HKEY_CURRENT_USER, kSettingsKey) != ERROR_SUCCESS)
        return;

    SaveBoolValue(key, L"ImportCover", options.importCover);
    SaveBoolValue(key, L"ImportImages", options.importImages);
    SaveBoolValue(key, L"ImportNotes", options.importNotes);
    SaveBoolValue(key, L"UseNavigationTitles", options.useNavigationTitles);
    SaveBoolValue(key, L"RepairEncoding", options.repairEncoding);
    SaveBoolValue(key, L"SkipServicePages", options.skipServicePages);
    SaveBoolValue(key, L"ImportTables", options.importTables);
    SaveBoolValue(key, L"ImportLists", options.importLists);
    SaveBoolValue(key, L"ImportPoemsEpigraphs", options.importPoemsEpigraphs);
    SaveBoolValue(key, L"ImportSubtitles", options.importSubtitles);
    SaveBoolValue(key, L"SplitSectionsByHeadings", options.splitSectionsByHeadings);
    SaveBoolValue(key, L"PreserveLinks", options.preserveLinks);
    SaveBoolValue(key, L"CleanTypography", options.cleanTypography);
    SaveBoolValue(key, L"ImportPageBreaks", options.importPageBreaks);
    SaveBoolValue(key, L"SkipHiddenElements", options.skipHiddenElements);
    SaveBoolValue(key, L"ValidateResult", options.validateResult);
    SaveBoolValue(key, L"AddDiagnosticSection", options.addDiagnosticSection);
    SaveBoolValue(key, L"WriteImportLog", options.writeImportLog);
    SaveBoolValue(key, L"SaveFb2Copy", options.saveFb2Copy);
    SaveBoolValue(key, L"UseCssSemanticClasses", options.useCssSemanticClasses);
    SaveBoolValue(key, L"RemoveFootnoteBacklinks", options.removeFootnoteBacklinks);
    SaveBoolValue(key, L"RemoveServiceSections", options.removeServiceSections);
    SaveBoolValue(key, L"WriteLogOnWarnings", options.writeLogOnWarnings);
    SaveBoolValue(key, L"SaveIntermediateFb2OnError", options.saveIntermediateFb2OnError);
    SaveBoolValue(key, L"KeepTempOnError", options.keepTempOnError);
    key.SetDWORDValue(L"SvgConversionMode", static_cast<DWORD>(options.svgConversionMode));
}

bool ShowImportOptionsDialog(HWND owner, EpubImportOptions& options)
{
    OptionsWindow wnd(owner, options);
    return wnd.DoModal();
}
