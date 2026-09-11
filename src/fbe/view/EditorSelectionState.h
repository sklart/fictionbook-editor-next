#pragma once

#include <mshtml.h>
#include "../source/BodySourceSelectionState.h"

class EditorSelectionState
{
public:
	MSHTML::IHTMLTxtRangePtr& BodyRange() { return m_body; }
	MSHTML::IHTMLTxtRangePtr& DescriptionRange() { return m_description; }
	const MSHTML::IHTMLTxtRangePtr& BodyRange() const { return m_body; }
	const MSHTML::IHTMLTxtRangePtr& DescriptionRange() const { return m_description; }
	BodySourceSelectionState& BodySource() { return m_bodySource; }
	const BodySourceSelectionState& BodySource() const { return m_bodySource; }
	void Reset()
	{
		ClearHtmlRanges();
		m_bodySource.Reset();
	}
	void ClearHtmlRanges() { m_body = NULL; m_description = NULL; }

private:
	MSHTML::IHTMLTxtRangePtr m_body;
	MSHTML::IHTMLTxtRangePtr m_description;
	BodySourceSelectionState m_bodySource;
};
