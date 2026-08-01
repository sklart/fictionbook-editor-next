#pragma once

namespace StartupTrace
{
	void Start();
	void Mark(const wchar_t* stage);
	// Возвращает true, когда включён диагностический журнал текущего процесса.
	bool Enabled();
	// Определяет режим следующего запуска: настройка FBE имеет приоритет над переменной среды.
	bool IsEnabledForNextLaunch();
	// Сохраняет режим следующего запуска в пользовательских настройках FBE.
	bool SetEnabledForNextLaunch(bool enabled);
	void Event(const wchar_t* category, const wchar_t* code, const wchar_t* message);
	void Warning(const wchar_t* category, const wchar_t* code, const wchar_t* message);
	// Записывает информационное событие диагностического журнала.
	void Event(const wchar_t* category, const wchar_t* stage);
	// Записывает ошибку с устойчивым кодом этапа и немедленно сбрасывает журнал.
	void Error(const wchar_t* category, const wchar_t* code, const wchar_t* message);
	// Записывает HRESULT без показа дополнительного диалога.
	void HResult(const wchar_t* category, const wchar_t* code, HRESULT result,
		const wchar_t* message = L"");
	void ComException(const wchar_t* category, const wchar_t* code, HRESULT result,
		const EXCEPINFO* exceptionInfo, IErrorInfo* errorInfo, const wchar_t* message = L"");
	// Записывает обезличенное событие JavaScript. Содержимое книги не передаётся.
	void ScriptEvent(const wchar_t* code, const wchar_t* message);
	void Flush();
	void EmergencyFlush();
	CString CurrentLogPath();
	CString CurrentLogDirectory();
	CString LastStageCode();
	CString LastStageMessage();
	DWORD LastWriteError();
	CString SanitizeLogText(const wchar_t* text, int maximumLength = 512);
	CString RedactPath(const wchar_t* text);
	CString SanitizeExceptionText(const wchar_t* text);
	CString NormalizeLogValue(const wchar_t* text, int maximumLength = 512);
	void Finish();
}
