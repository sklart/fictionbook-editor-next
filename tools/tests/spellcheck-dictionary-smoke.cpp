#include <windows.h>
#include <atlstr.h>
#include <atlcoll.h>
#include <hunspell.h>
#include "SpellText.h"
#include "CustomDictionaryIO.h"
#include "Splitter.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <iterator>
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

static bool ProductionTokenizerSmoke() {
    CSplitter splitter(FbeSpellAlphaExceptions());
    for (const wchar_t* expected : { L"don't", L"don’t", L"під'їзд", L"під’їзд", L"підʼїзд" }) {
        CString source(expected);
        CWords words;
        splitter.Split(&source, &words);
        if (words.GetSize() != 1 || words.GetValueAt(0) != expected) {
            std::cerr << "production tokenizer split an apostrophe word\n";
            return false;
        }
    }
    return true;
}

static bool ProductionApostropheRestoreSmoke() {
    return FbeRestoreSourceApostropheStyle(L"don’t", L"don't") == L"don’t" &&
        FbeRestoreSourceApostropheStyle(L"підʼїзд", L"під'їзд") == L"підʼїзд" &&
        FbeRestoreSourceApostropheStyle(L"don't", L"don't") == L"don't";
}

static bool ProductionCustomDictionarySmoke() {
    wchar_t tempDirectory[MAX_PATH];
    wchar_t tempFile[MAX_PATH];
    if (!GetTempPathW(_countof(tempDirectory), tempDirectory) ||
        !GetTempFileNameW(tempDirectory, L"fbe", 0, tempFile)) return false;
    DeleteFileW(tempFile);
    CSimpleArray<CString> original;
    const CString word(L"фбеспеллеруникальный");
    original.Add(word);
    FbeSaveCustomDictionary(tempFile, CP_UTF8, original);
    original.RemoveAll();
    std::ifstream input(tempFile, std::ios::binary);
    const std::vector<char> bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    const CStringA expected = FbeEncodeDictionaryWord(word, CP_UTF8);
    CSimpleArray<CString> restored;
    FbeLoadCustomDictionary(tempFile, CP_UTF8, restored);
    const CStringA utf8 = FbeEncodeDictionaryWord(word, CP_UTF8);
    const CStringA koi8r = FbeEncodeDictionaryWord(word, 20866);
    const std::vector<char> wordBytes(expected.GetString(), expected.GetString() + expected.GetLength());
    const bool validEol = (bytes.size() == wordBytes.size() + 1 && bytes.back() == '\n') ||
        (bytes.size() == wordBytes.size() + 2 && bytes[bytes.size() - 2] == '\r' && bytes.back() == '\n');
    const bool ok = bytes.size() >= wordBytes.size() &&
        std::equal(wordBytes.begin(), wordBytes.end(), bytes.begin()) && validEol &&
        utf8 != koi8r && restored.GetSize() == 1 && restored[0] == word &&
        FbeCustomDictionaryContains(restored, word) &&
        !FbeCustomDictionaryContains(restored, L"фбеспеллеруникальная") &&
        !ATLPath::FileExists(CString(tempFile) + L".tmp");
    DeleteFileW(tempFile);
    return ok;
}

static bool ProductionApostropheSpellFlow(Hunhandle* dict, UINT cp, const std::initializer_list<const wchar_t*> words) {
    for (const wchar_t* source : words) {
        const CString normalized = FbePrepareDictionaryWord(CString(source));
        const CStringA encoded = FbeEncodeDictionaryWord(normalized, cp);
        if (encoded.IsEmpty() || !Hunspell_spell(dict, encoded)) {
            std::cerr << "production apostrophe spell flow failed\n";
            return false;
        }
    }
    return true;
}

static bool ProductionApostropheSuggestionFlow(Hunhandle* dict, UINT cp, const wchar_t* source, const wchar_t* expected, const wchar_t* restoredExpected) {
    const CString prepared = FbePrepareDictionaryWord(CString(source));
    const CStringA encoded = FbeEncodeDictionaryWord(prepared, cp);
    char** list = nullptr;
    const int count = Hunspell_suggest(dict, &list, encoded);
    bool found = false;
    for (int i = 0; i < count; ++i) {
        const CString suggestion = FbeDecodeDictionaryWord(list[i], cp);
        if (suggestion == expected) {
            found = FbeRestoreSourceApostropheStyle(source, suggestion) == restoredExpected;
            break;
        }
    }
    Hunspell_free_list(dict, &list, count);
    if (!found) std::cerr << "production apostrophe suggestion flow failed\n";
    return found;
}

static bool ProductionKoi8rRoundTrip(Hunhandle* dict) {
    bool ok = true;
    for (const wchar_t* expected : { L"собака", L"ёлка", L"ёжик", L"всё" }) {
        CString unicode(expected);
        const CStringA encoded = FbeEncodeDictionaryWord(unicode, 20866);
        const CString roundTrip = FbeDecodeDictionaryWord(encoded, 20866);
        if (roundTrip != unicode || !Hunspell_spell(dict, encoded)) {
            std::cerr << "production KOI8-R round trip failed\n";
            ok = false;
        }
    }
    const CStringA typo = FbeEncodeDictionaryWord(CString(L"собка"), 20866);
    char** list = nullptr;
    const int count = Hunspell_suggest(dict, &list, typo);
    bool found = false;
    for (int i = 0; i < count; ++i) {
        const CString suggestion = FbeDecodeDictionaryWord(list[i], 20866);
        if (suggestion == L"собака") found = true;
        if (suggestion.Find(L"�") >= 0) ok = false;
    }
    Hunspell_free_list(dict, &list, count);
    return ok && found;
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
        ok &= Spell(dict, L"ChatGPT", cp, true) && Spell(dict, L"LLM", cp, true) && Spell(dict, L"codebase", cp, true) && Spell(dict, L"tokenize", cp, true) && Spell(dict, L"tokenization", cp, true) && Spell(dict, L"influencer", cp, true) && Spell(dict, L"doomscrolling", cp, true) && Spell(dict, L"staycation", cp, true) && Spell(dict, L"Kyiv", cp, true);
        ok &= Spell(dict, L"recieve", cp, false) && Spell(dict, L"definately", cp, false) && Spell(dict, L"teh", cp, false);
        ok &= ProductionApostropheSpellFlow(dict, cp, { L"don't", L"don’t", L"donʼt" });
        ok &= ProductionApostropheSuggestionFlow(dict, cp, L"don'tt", L"don't", L"don't");
        ok &= ProductionApostropheSuggestionFlow(dict, cp, L"don’tt", L"don't", L"don’t");
        ok &= ProductionApostropheSuggestionFlow(dict, cp, L"donʼtt", L"don't", L"donʼt");
        const std::string customWord = Encode(L"FbeCustomUnicode", cp);
        ok &= Hunspell_add(dict, customWord.c_str()) == 0 && Spell(dict, L"FbeCustomUnicode", cp, true);
    } else if (std::string(name) == "ru_RU") {
        for (const wchar_t* word : { L"собака", L"корова", L"молоко", L"жираф", L"ёлка", L"ёжик", L"всё", L"литература", L"редактор", L"Собака" }) ok &= Spell(dict, word, cp, true);
        // Compatibility probes retain the factual behavior of this exact release.
        for (const wchar_t* word : { L"сабака", L"карова", L"малако" }) ReportSpell(dict, word, cp, "ru_RU compatibility");
        // Confirmed against Goudron 1.0.8 with bundled Hunspell 1.7.3.
        for (const wchar_t* word : { L"компьютерр", L"редакторр", L"литератуура", L"молокоо", L"жирафф" }) ok &= Spell(dict, word, cp, false);
        ok &= HasSuggestion(dict, L"собка", L"собака", cp);
        ok &= ProductionKoi8rRoundTrip(dict);
    } else if (std::string(name) == "uk_UA") {
        for (const wchar_t* word : { L"Україна", L"український", L"Київ", L"ґрунт", L"література", L"редактор" }) ok &= Spell(dict, word, cp, true);
        ok &= ProductionApostropheSpellFlow(dict, cp, { L"під'їзд", L"під’їзд", L"підʼїзд" });
        ok &= ProductionApostropheSuggestionFlow(dict, cp, L"підʼїз", L"під'їзд", L"підʼїзд");
        for (const wchar_t* word : { L"украйінський", L"Києв", L"ґрунтт" }) ok &= Spell(dict, word, cp, false);
        // Record the current upstream dict_uk ICONV behavior for Latin-only words.
        // Do not treat it as an FBE-specific contract; upstream issue #306 remains open.
        for (const wchar_t* word : { L"hello", L"foobar", L"Microsoft", L"London", L"ChatGPT", L"qwerty" }) std::cout << "uk_UA ICONV " << Encode(word, CP_UTF8) << "=" << Hunspell_spell(dict, Encode(word, CP_UTF8).c_str()) << "\n";
    } else if (std::string(name) == "de_DE") {
        for (const wchar_t* word : { L"Deutschland", L"Wörterbuch", L"Straße", L"Fußball", L"größer", L"Überraschung", L"Abbiegerspur", L"Abgeltungssteuer", L"Abgasgräting", L"Aasvogel" }) ok &= Spell(dict, word, cp, true);
        for (const wchar_t* word : { L"Deutchland", L"Wörterbuh", L"Fußbaal" }) ok &= Spell(dict, word, cp, false);
        ok &= HasSuggestion(dict, L"Deutchland", L"Deutschland", cp);
    } else {
        ok = false;
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
    bool ok = ProductionTokenizerSmoke() && ProductionApostropheRestoreSmoke() && ProductionCustomDictionarySmoke();
    ok &= TestDictionary(argv[1], "en_US", "UTF-8", CP_UTF8);
    ok &= TestDictionary(argv[1], "ru_RU", "KOI8-R", 20866);
    ok &= TestDictionary(argv[1], "uk_UA", "UTF-8", CP_UTF8);
    ok &= TestDictionary(argv[1], "de_DE", "ISO8859-1", 28591);
    return ok ? 0 : 1;
}
