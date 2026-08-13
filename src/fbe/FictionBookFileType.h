#pragma once

// Keep the editable FictionBook extensions in one small, UI-independent place.
enum class FictionBookFileType
{
    Fb2,
    Fbd,
    Unknown
};

inline FictionBookFileType DetectFictionBookFileType(const CString& path)
{
    const int dot = path.ReverseFind(L'.');
    if (dot < 0) return FictionBookFileType::Unknown;
    const CString extension = path.Mid(dot);
    if (extension.CompareNoCase(L".fb2") == 0) return FictionBookFileType::Fb2;
    if (extension.CompareNoCase(L".fbd") == 0) return FictionBookFileType::Fbd;
    return FictionBookFileType::Unknown;
}

inline bool IsSupportedFictionBookFile(const CString& path)
{
    return DetectFictionBookFileType(path) != FictionBookFileType::Unknown;
}

inline bool IsFbdFile(const CString& path)
{
    return DetectFictionBookFileType(path) == FictionBookFileType::Fbd;
}
