#pragma once

#include <memory>

class CMainFrame;

namespace FB { class Doc; }

class PendingDocument
{
public:
	PendingDocument(CMainFrame& frame, FB::Doc* previous);
	~PendingDocument();

	FB::Doc& Document() const;
	void Rollback();
	FB::Doc* Commit();

private:
	FB::Doc* m_previous;
	std::unique_ptr<FB::Doc> m_document;
};
