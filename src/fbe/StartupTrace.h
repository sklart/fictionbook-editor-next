#pragma once
#include <vector>
#include <memory>
#include <string>
#include <atomic>

namespace StartupTrace
{
	struct CrashTraceSnapshot
	{
		bool snapshotAvailable;
		bool diagnosticEnabled;
		bool usingTempFallback;
		wchar_t sessionId[40];
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
    enum class Operation { None, Load, Save, Validate, Script, ViewSwitch, Count };
    enum class EditorMode { Unknown, Body, Description, Source };
    // Fixed-size counters, available even without trace; no document payloads.
    ULONGLONG BeginOperation(Operation operation) noexcept;
    void EndOperation(ULONGLONG token, bool succeeded) noexcept;
    class ScopedOperation
    {
        ULONGLONG token;
        bool succeeded = false;
    public:
        explicit ScopedOperation(Operation operation) noexcept : token(BeginOperation(operation)) {}
        ~ScopedOperation() noexcept { EndOperation(token, succeeded); }
        void Succeeded() noexcept { succeeded = true; }
        ScopedOperation(const ScopedOperation&) = delete;
        ScopedOperation& operator=(const ScopedOperation&) = delete;
    };
    void SetActiveEditorMode(EditorMode mode) noexcept;
    CString ActiveEditorModeName();
    CString OperationStatisticsReport(bool atShutdown = false);
    int RunDiagnosticPackageTest();
	// Test-only progress marker. It is a no-op unless an isolated test explicitly
	// provides both FBE_NEXT_TEST_MODE=1 and a breadcrumb file path.
	void AppendTestStartupBreadcrumb(const char* phase);
	void WriteLateEnvironmentHeader();
	// Возвращает true, когда включён диагностический журнал текущего процесса.
	bool Enabled();
	// Aggregate UI dispatch diagnostics.  These functions are no-ops unless
	// the existing diagnostic trace is active.
	void CountUiComCall();
	ULONGLONG UiComCallCount();
	void CountUiCheckCommand();
	ULONGLONG UiCheckCommandCount();
	void CountUiSelectionContainerQuery();
	ULONGLONG UiSelectionContainerQueryCount();
	void CountUiSelectionContextContainerQuery();
	ULONGLONG UiSelectionContextContainerQueryCount();
	void CountUiSelectionStructConQuery();
	ULONGLONG UiSelectionStructConQueryCount();
	void CountUiSelectionStructTableConQuery();
	ULONGLONG UiSelectionStructTableConQueryCount();
	// Fixed-cost document operations. These remain aggregate diagnostics and
	// are enabled only with the existing trace switch.
	void CountXsltTemplateBuild();
	ULONGLONG XsltTemplateBuildCount();
	void CountXsdSchemaLoad();
	ULONGLONG XsdSchemaLoadCount();
	void CountSourceSerialization();
	ULONGLONG SourceSerializationCount();
	// Определяет режим следующего запуска: настройка FBE или переменная среды включают трассировку.
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
    struct DiagnosticSession
    {
        CString id, logPath, metadataPath, crashPath, started, ended, version, outcome;
        CString crashFingerprint; // Identity of the exact report offered for review.
        DWORD processId = 0;
        bool legacy = false;
        bool crashAmbiguous = false;
    };
    struct DiagnosticComponent
    {
        CString name, status;
        std::vector<BYTE> bytes;
        bool optional = true;
    };
    // Only the builder can populate a snapshot. Save reads these exact bytes.
    class DiagnosticPackageSnapshot
    {
    public:
        const std::vector<DiagnosticComponent>& Components() const { return components; }
        bool Valid() const { return valid; }
    private:
        std::vector<DiagnosticComponent> components;
        bool valid = false;
        friend bool PrepareDiagnosticPackage(const DiagnosticSession&, const std::vector<CString>&,
            std::shared_ptr<const DiagnosticPackageSnapshot>&, CString&);
    };
    // Per-thread cancellation for diagnostic I/O only. Existing entry points stay compatible.
    class DiagnosticTaskScope
    {
        const std::atomic_bool* previous;
    public:
        explicit DiagnosticTaskScope(const std::atomic_bool& cancelled);
        ~DiagnosticTaskScope();
        DiagnosticTaskScope(const DiagnosticTaskScope&) = delete;
        DiagnosticTaskScope& operator=(const DiagnosticTaskScope&) = delete;
    };
    bool IsDiagnosticCrashReviewed(const DiagnosticSession& session);
    bool MarkDiagnosticCrashReviewed(const DiagnosticSession& session);
    bool HasUnreviewedDiagnosticCrashes(const std::vector<DiagnosticSession>& sessions);
    bool SaveDiagnosticPackageUnique(const DiagnosticPackageSnapshot& snapshot, const CString& requested,
        CString& actual, CString& error);
    CString SessionId();
    CString SessionStarted();
    CString BasicEnvironmentReport();
    CString TraceSource();
    std::vector<DiagnosticSession> EnumerateDiagnosticSessions();
    bool PrepareDiagnosticPackage(const DiagnosticSession& session, const std::vector<CString>& excluded,
        std::shared_ptr<const DiagnosticPackageSnapshot>& snapshot, CString& error);
    bool SaveDiagnosticPackage(const DiagnosticPackageSnapshot& snapshot, const CString& path, CString& error);
	struct DiagnosticLogCleanupResult
	{
		unsigned int sessionsFound;
		unsigned int sessionsFullyDeleted;
		unsigned int sessionsPartiallyDeleted;
		unsigned int sessionsFailed;
		unsigned int filesDeleted;
		unsigned int filesFailed;
		ULONGLONG bytesDeleted;
		DWORD lastError;
		DiagnosticLogCleanupResult() : sessionsFound(0), sessionsFullyDeleted(0), sessionsPartiallyDeleted(0), sessionsFailed(0), filesDeleted(0), filesFailed(0), bytesDeleted(0), lastError(ERROR_SUCCESS) { }
	};
	// Removes completed diagnostic sessions while preserving the current process trace.
	DiagnosticLogCleanupResult ClearOldLogSessions();
    DiagnosticLogCleanupResult CleanupDiagnosticSessions(size_t keep, bool preview = false);
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
