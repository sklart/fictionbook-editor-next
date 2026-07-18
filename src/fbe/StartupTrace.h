#pragma once

namespace StartupTrace
{
	void Start();
	void Mark(const wchar_t* stage);
	// Возвращает true, когда включён диагностический журнал.
	bool Enabled();
	// Записывает событие в диагностический журнал FBE_NEXT_TRACE.
	// Категория помогает отделить запуск, документ, COM и перенос выделения.
	void Event(const wchar_t* category, const wchar_t* stage);
	void Finish();
}
