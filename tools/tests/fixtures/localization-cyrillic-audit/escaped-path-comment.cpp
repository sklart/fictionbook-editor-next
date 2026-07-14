// Этот fixture проверяет, что анализатор не принимает комментарий за часть
// строкового литерала после обратных слешей в C++-пути.

const wchar_t* const kProfilePath = L"C:\\Users\\FBE Next\\";

// Русский комментарий не является пользовательской строкой интерфейса.
const wchar_t* const kFallbackMessage = L"English fallback";
