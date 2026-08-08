#pragma once

namespace StartupTrace
{
	struct CrashTraceSnapshot
	{
		bool snapshotAvailable;
		bool diagnosticEnabled;
		bool usingTempFallback;
		DWORD processId;
		DWORD threadId;
		wchar_t currentLogPath[MAX_PATH];
		wchar_t currentLogDirectory[MAX_PATH];
		wchar_t lastEventCode[64];
		wchar_t lastEventMessage[512];
		wchar_t lastDocumentStage[64];
		wchar_t lastScriptOperationStage[64];
		wchar_t lastScriptFailureStage[64];
		wchar_t lastHResultFailure[512];
		wchar_t lastDispatchFailure[512];
	};
	void Start();
	void WriteLateEnvironmentHeader();
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
	void DispatchResult(const wchar_t* category, const wchar_t* code, HRESULT result, const wchar_t* message = L"");
	void ComException(const wchar_t* category, const wchar_t* code, HRESULT result,
		const EXCEPINFO* exceptionInfo, IErrorInfo* errorInfo, const wchar_t* message = L"");
	// Записывает обезличенное событие JavaScript. Содержимое книги не передаётся.
	void ScriptEvent(const wchar_t* code, const wchar_t* message);
	void Flush();
	void EmergencyFlush();
	CString CurrentLogPath();
	CString CurrentLogDirectory();
	struct DiagnosticLogCleanupResult
	{
		unsigned int sessionsFound;
		unsigned int sessionsDeleted;
		unsigned int filesDeleted;
		unsigned int filesFailed;
		DWORD lastError;
		DiagnosticLogCleanupResult() : sessionsFound(0), sessionsDeleted(0), filesDeleted(0), filesFailed(0), lastError(ERROR_SUCCESS) { }
	};
	// Removes completed diagnostic sessions while preserving the current process trace.
	DiagnosticLogCleanupResult ClearOldLogSessions();
	bool TryGetCrashTraceSnapshot(CrashTraceSnapshot& snapshot);
	CString LastStageCode();
	CString LastStageMessage();
	CString LastDocumentStage();
	CString LastScriptOperationStage();
	CString LastComFailure();
	DWORD LastWriteError();
	CString SanitizeLogText(const wchar_t* text, int maximumLength = 512);
	CString RedactPath(const wchar_t* text);
	CString SanitizeExceptionText(const wchar_t* text);
	CString NormalizeLogValue(const wchar_t* text, int maximumLength = 512);
	void Finish();
}
