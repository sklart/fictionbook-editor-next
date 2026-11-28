#pragma once

#include <atlbase.h>
#include <atlstr.h>
#include <msxml6.h>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace FbeFb3 {

inline constexpr wchar_t DescriptionNamespace[] = L"http://www.fictionbook.org/FictionBook3/description";
inline constexpr wchar_t BodyNamespace[] = L"http://www.fictionbook.org/FictionBook3/body";
inline constexpr wchar_t ImageRelationship[] = L"http://www.fictionbook.org/FictionBook3/relationships/image";

struct PartNameLess {
    bool operator()(const std::wstring& left, const std::wstring& right) const;
};

struct Part {
    std::wstring name;
    std::filesystem::path file;
    std::wstring contentType;
};

struct Relationship {
    std::wstring type;
    std::wstring target;
    bool external = false;
};

using Relationships = std::map<std::wstring, Relationship, std::less<>>;

struct Error {
    std::wstring message;
    const wchar_t* code = L"invalid-document";
    std::wstring element{};
    std::wstring location{};
};

// A package owns its private extraction directory. Part files are valid for
// the lifetime of the package. XML parsing requires the caller's COM apartment.
class Package final {
public:
    explicit Package(const std::filesystem::path& source);
    ~Package();
    Package(const Package&) = delete;
    Package& operator=(const Package&) = delete;

    const Part& Description() const { return *m_description; }
    const Part& Body() const { return *m_body; }
    const Part* Cover() const { return m_cover; }
    const std::vector<std::wstring>& Warnings() const { return m_warnings; }
    const Part* FindPart(const std::wstring& name) const;
    const Relationships& Links(const std::wstring& sourcePart) const;

private:
    void Open(const std::filesystem::path& source);
    void ReadContentTypes(const Part& manifest);
    Relationships ReadRelationships(const Part& part, const std::wstring& source);
    std::filesystem::path m_scratch;
    std::map<std::wstring, Part, PartNameLess> m_parts;
    std::map<std::wstring, Relationships, PartNameLess> m_links;
    std::vector<std::wstring> m_warnings;
    const Part* m_description = nullptr;
    const Part* m_body = nullptr;
    const Part* m_cover = nullptr;
};

CComPtr<IXMLDOMDocument2> LoadXmlPart(const Part& part);
CComPtr<IXMLDOMDocument2> LoadXmlBytes(const std::wstring& name, const std::vector<BYTE>& bytes);
CComPtr<IXMLDOMDocument2> LoadFb2Snapshot(const std::vector<BYTE>& bytes);

}
