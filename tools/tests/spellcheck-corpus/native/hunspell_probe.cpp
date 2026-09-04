#include <windows.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "hunspell.h"

namespace {

UINT DictionaryCodePage(const char* encoding) {
    if (!encoding || !*encoding) {
        return CP_UTF8;
    }
    std::string value(encoding);
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    value.erase(std::remove_if(value.begin(), value.end(), [](unsigned char ch) {
        return ch == '-' || ch == '_' || std::isspace(ch);
    }), value.end());

    if (value == "UTF8") return CP_UTF8;
    if (value == "KOI8R") return 20866;
    if (value == "WINDOWS1251" || value == "CP1251" || value == "MSCYRILLIC") return 1251;
    if (value == "ISO88591" || value == "LATIN1") return 28591;
    if (value == "ISO88595") return 28595;

    throw std::runtime_error("Unsupported dictionary encoding: " + std::string(encoding));
}

std::wstring Decode(const std::string& value, UINT codePage) {
    if (value.empty()) return {};
    int size = MultiByteToWideChar(codePage, MB_ERR_INVALID_CHARS, value.data(),
                                   static_cast<int>(value.size()), nullptr, 0);
    if (size <= 0 && codePage != CP_UTF8) {
        size = MultiByteToWideChar(codePage, 0, value.data(),
                                   static_cast<int>(value.size()), nullptr, 0);
    }
    if (size <= 0) throw std::runtime_error("Cannot decode text");
    std::wstring result(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(codePage, 0, value.data(), static_cast<int>(value.size()),
                        result.data(), size);
    return result;
}

std::string Encode(const std::wstring& value, UINT codePage) {
    if (value.empty()) return {};
    int size = WideCharToMultiByte(codePage, 0, value.data(), static_cast<int>(value.size()),
                                   nullptr, 0, nullptr, nullptr);
    if (size <= 0) throw std::runtime_error("Cannot encode text");
    std::string result(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(codePage, 0, value.data(), static_cast<int>(value.size()),
                        result.data(), size, nullptr, nullptr);
    return result;
}

std::string Convert(const std::string& value, UINT from, UINT to) {
    if (from == to) return value;
    return Encode(Decode(value, from), to);
}

std::string TrimLine(std::string line) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    return line;
}

std::string FindArgument(int argc, char** argv, const std::string& name) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (argv[i] == name) return argv[i + 1];
    }
    return {};
}

bool HasArgument(int argc, char** argv, const std::string& name) {
    for (int i = 1; i < argc; ++i) {
        if (argv[i] == name) return true;
    }
    return false;
}

class Dictionary {
public:
    explicit Dictionary(const std::string& base) {
        const std::string aff = base + ".aff";
        const std::string dic = base + ".dic";
        handle_ = Hunspell_create(aff.c_str(), dic.c_str());
        if (!handle_) throw std::runtime_error("Cannot load dictionary: " + base);
        codePage_ = DictionaryCodePage(Hunspell_get_dic_encoding(handle_));
    }

    ~Dictionary() {
        if (handle_) Hunspell_destroy(handle_);
    }

    bool SpellUtf8(const std::string& word) const {
        const std::string encoded = Convert(word, CP_UTF8, codePage_);
        return Hunspell_spell(handle_, encoded.c_str()) != 0;
    }

    std::vector<std::string> SuggestUtf8(const std::string& word) const {
        const std::string encoded = Convert(word, CP_UTF8, codePage_);
        char** list = nullptr;
        const int count = Hunspell_suggest(handle_, &list, encoded.c_str());
        std::vector<std::string> result;
        if (count > 0 && list) {
            result.reserve(static_cast<size_t>(count));
            for (int i = 0; i < count; ++i) {
                result.push_back(Convert(list[i], codePage_, CP_UTF8));
            }
            Hunspell_free_list(handle_, &list, count);
        }
        return result;
    }

private:
    Hunhandle* handle_ = nullptr;
    UINT codePage_ = CP_UTF8;
};

int ListMode(const Dictionary& dictionary, const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Cannot open input file: " + path);
    std::string word;
    while (std::getline(input, word)) {
        word = TrimLine(word);
        if (!word.empty() && !dictionary.SpellUtf8(word)) {
            std::cout << word << '\n';
        }
    }
    return 0;
}

int PipeMode(const Dictionary& dictionary) {
    std::cout << "@(#) FBE Next Hunspell probe\n";
    std::string word;
    while (std::getline(std::cin, word)) {
        word = TrimLine(word);
        if (word.empty()) {
            std::cout << "\n";
            continue;
        }
        if (dictionary.SpellUtf8(word)) {
            std::cout << "*\n";
            continue;
        }
        const auto suggestions = dictionary.SuggestUtf8(word);
        if (suggestions.empty()) {
            std::cout << "# " << word << " 0\n";
            continue;
        }
        std::cout << "& " << word << ' ' << suggestions.size() << " 0: ";
        for (size_t i = 0; i < suggestions.size(); ++i) {
            if (i) std::cout << ", ";
            std::cout << suggestions[i];
        }
        std::cout << '\n';
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        SetConsoleCP(CP_UTF8);
        SetConsoleOutputCP(CP_UTF8);
        const std::string base = FindArgument(argc, argv, "-d");
        if (base.empty()) {
            std::cerr << "Usage: hunspell-probe -i UTF-8 -d DICTIONARY_BASE (-l FILE | -a)\n";
            return 2;
        }
        Dictionary dictionary(base);
        if (HasArgument(argc, argv, "-l")) {
            return ListMode(dictionary, FindArgument(argc, argv, "-l"));
        }
        if (HasArgument(argc, argv, "-a")) {
            return PipeMode(dictionary);
        }
        std::cerr << "Specify -l FILE or -a\n";
        return 2;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 3;
    }
}
