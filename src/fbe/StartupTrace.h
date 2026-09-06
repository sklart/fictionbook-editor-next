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
	// Test-only progress marker. It is a no-op unless an isolated test explicitly
	// provides both FBE_NEXT_TEST_MODE=1 and a breadcrumb file path.
	void AppendTestStartupBreadcrumb(const char* phase);
	void WriteLateEnvironmentHeader();
	// Возвращает true, когда включён диагностический журнал текущего процесса.
	bool Enabled();
	// Определяет режим следующего запуска: настройка FBE имеет приоритет над переменной среды.
	bool IsEnabledForNextLaunch();
	// Возвращает true только для явно сохранённого пользовательского включения.
	// Одноразовый FBE_NEXT_TRACE не должен открывать интерактивные уведомления.
	bool IsEnabledByStoredNextLaunchPreference();
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
	// Returns metadata only: never exception/help text supplied by a script or COM server.
	CString FormatComExceptionMetadata(const EXCEPINFO* exceptionInfo, IErrorInfo* errorInfo);
	// Записывает обезличенное событие JavaScript. Содержимое книги не передаётся.
	void ScriptEvent(const wchar_t* code, const wchar_t* message);
	void Flush();
	void EmergencyFlush();
	CString CurrentLogPath();
	CString CurrentLogDirectory();
	// Creates a privacy-checked ZIP containing only the current diagnostic session
	// and generated technical reports. Dumps are deliberately never included.
	bool CreateDiagnosticPackage(CString& packagePath, CString& error);
	struct DiagnosticLogCleanupResult
	{
		unsigned int sessionsFound;
		unsigned int sessionsFullyDeleted;
		unsigned int sessionsPartiallyDeleted;
		unsigned int sessionsFailed;
		unsigned int filesDeleted;
		unsigned int filesFailed;
		DWORD lastError;
		DiagnosticLogCleanupResult() : sessionsFound(0), sessionsFullyDeleted(0), sessionsPartiallyDeleted(0), sessionsFailed(0), filesDeleted(0), filesFailed(0), lastError(ERROR_SUCCESS) { }
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
	CString NormalizeLogValue(const wchar_t* text, int maximumLength = 512);
	void Finish();
}
