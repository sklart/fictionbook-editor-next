#include <windows.h>
#include <hunspell.h>
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

static std::string Encode(const std::wstring& value, UINT cp) {
    const DWORD flags = cp == CP_UTF8 ? WC_ERR_INVALID_CHARS : 0;
    int n = WideCharToMultiByte(cp, flags, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(n > 0 ? n : 0, '\0');
    if (n > 1) WideCharToMultiByte(cp, flags, value.c_str(), -1, result.data(), n, nullptr, nullptr);
    if (!result.empty()) result.pop_back();
    return result;
}

static bool Spell(Hunhandle* dict, const wchar_t* word, UINT cp, bool expected) {
    const std::string encoded = Encode(word, cp);
    if (encoded.empty() && *word) {
        std::cerr << "cannot encode spelling input\n";
        return false;
    }
    const int result = Hunspell_spell(dict, encoded.c_str());
    const bool actual = result != 0;
    if (actual != expected) std::cerr << "unexpected spelling result: " << Encode(word, CP_UTF8) << "=" << result << "\n";
    return actual == expected;
}

static bool HasSuggestion(Hunhandle* dict, const wchar_t* word, const wchar_t* expected, UINT cp) {
    const std::string encoded = Encode(word, cp);
    char** list = nullptr;
    const int count = Hunspell_suggest(dict, &list, encoded.c_str());
    bool found = false;
    const std::string needle = Encode(expected, cp);
    for (int i = 0; i < count; ++i) if (needle == list[i]) found = true;
    Hunspell_free_list(dict, &list, count);
    return found;
}

static void ReportSpell(Hunhandle* dict, const wchar_t* word, UINT cp, const char* group) {
    const std::string encoded = Encode(word, cp);
    std::cout << group << " " << Encode(word, CP_UTF8) << "=" << Hunspell_spell(dict, encoded.c_str()) << "\n";
}

static bool TestDictionary(const std::string& directory, const char* name, const char* encoding, UINT cp) {
    const std::string base = directory + "\\" + name;
    const auto started = std::chrono::steady_clock::now();
    Hunhandle* dict = Hunspell_create((base + ".aff").c_str(), (base + ".dic").c_str());
    if (!dict) return false;
    const auto loaded = std::chrono::steady_clock::now();
    bool ok = std::string(Hunspell_get_dic_encoding(dict)) == encoding;
    if (std::string(name) == "en_US") {
        ok &= Spell(dict, L"computer", cp, true) && Spell(dict, L"literature", cp, true) && Spell(dict, L"dictionary", cp, true) && Spell(dict, L"editor", cp, true);
        ok &= Spell(dict, L"ChatGPT", cp, true) && Spell(dict, L"codebase", cp, true) && Spell(dict, L"Kyiv", cp, true);
        ok &= Spell(dict, L"recieve", cp, false) && Spell(dict, L"definately", cp, false) && Spell(dict, L"teh", cp, false);
        ok &= Spell(dict, L"don't", cp, true) && Spell(dict, L"don’t", cp, true);
        const std::string customWord = Encode(L"FbeCustomUnicode", cp);
        ok &= Hunspell_add(dict, customWord.c_str()) == 0 && Spell(dict, L"FbeCustomUnicode", cp, true);
    } else if (std::string(name) == "ru_RU") {
        for (const wchar_t* word : { L"собака", L"корова", L"молоко", L"жираф", L"ёлка", L"ёжик", L"всё", L"литература", L"редактор", L"Собака" }) ok &= Spell(dict, word, cp, true);
        // Compatibility probes retain the factual behavior of this exact release.
        for (const wchar_t* word : { L"сабака", L"карова", L"малако" }) ReportSpell(dict, word, cp, "ru_RU compatibility");
        // Confirmed against Goudron 1.0.8 with bundled Hunspell 1.7.3.
        for (const wchar_t* word : { L"компьютерр", L"редакторр", L"литератуура", L"молокоо", L"жирафф" }) ok &= Spell(dict, word, cp, false);
        ok &= HasSuggestion(dict, L"собка", L"собака", cp);
    } else {
        for (const wchar_t* word : { L"Україна", L"український", L"Київ", L"ґрунт", L"література", L"редактор", L"під'їзд", L"підʼїзд" }) ok &= Spell(dict, word, cp, true);
        for (const wchar_t* word : { L"украйінський", L"Києв", L"ґрунтт" }) ok &= Spell(dict, word, cp, false);
        // dict_uk ICONV deliberately strips Latin characters; record this upstream behavior.
        for (const wchar_t* word : { L"hello", L"foobar", L"Microsoft", L"London", L"ChatGPT", L"qwerty" }) std::cout << "uk_UA ICONV " << Encode(word, CP_UTF8) << "=" << Hunspell_spell(dict, Encode(word, CP_UTF8).c_str()) << "\n";
    }
    const auto completed = std::chrono::steady_clock::now();
    std::cout << name << " loadMs="
              << std::chrono::duration_cast<std::chrono::milliseconds>(loaded - started).count()
              << " checksMs="
              << std::chrono::duration_cast<std::chrono::milliseconds>(completed - loaded).count() << "\n";
    Hunspell_destroy(dict);
    return ok;
}

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    bool ok = TestDictionary(argv[1], "en_US", "UTF-8", CP_UTF8);
    ok &= TestDictionary(argv[1], "ru_RU", "KOI8-R", 20866);
    ok &= TestDictionary(argv[1], "uk_UA", "UTF-8", CP_UTF8);
    return ok ? 0 : 1;
}
