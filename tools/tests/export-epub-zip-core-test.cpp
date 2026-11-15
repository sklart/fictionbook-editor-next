#include "../../src/export-epub/FbeEpubExport.h"
#include <iostream>

int wmain(int argc, wchar_t** argv) {
    if (argc != 2) return 2;
    fbe::epub::EpubBook book;
    book.title = L"Тест"; book.identifier = L"zip-test";
    fbe::epub::EpubChapter chapter;
    chapter.title = L"Глава"; chapter.bodyXhtml = L"<p>Текст <img src=\"../изображения/обложка.png\" alt=\"Обложка\"/></p>";
    book.chapters.push_back(chapter);
    fbe::epub::EpubResource image;
    image.id = L"image"; image.href = L"изображения/обложка.png"; image.mediaType = "image/png";
    image.data = { 0x89, 0x50, 0x4e, 0x47, 0x00, 0xff, 0x42 };
    book.resources.push_back(image);
    fbe::epub::EpubResource other;
    other.id = L"data"; other.href = L"данные/текст.txt"; other.mediaType = "text/plain";
    other.data.assign(150000, static_cast<std::uint8_t>('A')); book.resources.push_back(other);
    std::wstring error;
    if (!fbe::epub::EpubExporter().Export(book, argv[1], fbe::epub::EpubExportOptions(), &error)) {
        std::wcerr << error << L'\n'; return 1;
    }
    return 0;
}
