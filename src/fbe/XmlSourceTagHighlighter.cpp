#include "stdafx.h"
#include "EditorEngine.h"
#include "xmlMatchedTagsHighlighter.h"

namespace {
bool IsStructuralDiagnostic(XmlTagMatchState state) {
	return state == XmlTagMatchState::MissingOpening || state == XmlTagMatchState::MissingClosing || state == XmlTagMatchState::Mismatched || state == XmlTagMatchState::Invalid;
}
int DiagnosticIndicator(XmlTagMatchState state) {
	switch (state) {
	case XmlTagMatchState::MissingOpening: return EDITOR_INDICATOR_XML_TAG_MISSING_OPENING;
	case XmlTagMatchState::MissingClosing: return EDITOR_INDICATOR_XML_TAG_MISSING_CLOSING;
	case XmlTagMatchState::Mismatched: return EDITOR_INDICATOR_XML_TAG_MISMATCHED;
	default: return EDITOR_INDICATOR_XML_TAG_INVALID;
	}
}
}

void XmlSourceTagHighlighter::ClearCurrentRanges()
{
	if (!_state) return;
	const struct RangeGroup { int indicator; vector<pair<int, int> >* ranges; } groups[] = {
		{ EDITOR_INDICATOR_TAG_MATCH, &_state->tagRanges }, { EDITOR_INDICATOR_TAG_ATTRIBUTE, &_state->attributeRanges }
	};
	for (size_t group = 0; group < _countof(groups); ++group) {
		_pEditView->execute(SCI_SETINDICATORCURRENT, groups[group].indicator);
		for (size_t i = 0; i < groups[group].ranges->size(); ++i) _pEditView->execute(SCI_INDICATORCLEARRANGE, (*groups[group].ranges)[i].first, (*groups[group].ranges)[i].second - (*groups[group].ranges)[i].first);
		groups[group].ranges->clear();
	}
}

void XmlSourceTagHighlighter::ClearDiagnosticRanges()
{
	if (!_state) return;
	for (const XmlMatchedTagsState::IndicatorRange& range : _state->diagnosticRanges) {
		_pEditView->execute(SCI_SETINDICATORCURRENT, range.indicator);
		_pEditView->execute(SCI_INDICATORCLEARRANGE, range.start, range.end - range.start);
	}
	_state->diagnosticRanges.clear();
}

void XmlSourceTagHighlighter::FillRange(int indicator, const XmlByteRange& range, vector<pair<int, int> >& ranges)
{
	if (range.empty()) return;
	_pEditView->execute(SCI_SETINDICATORCURRENT, indicator);
	_pEditView->execute(SCI_INDICATORFILLRANGE, static_cast<WPARAM>(range.start), static_cast<LPARAM>(range.end - range.start));
	ranges.emplace_back(static_cast<int>(range.start), static_cast<int>(range.end));
}

bool XmlSourceTagHighlighter::UpdateHighlight(const XmlTagHighlightOptions& options)
{
	if (!_state) return false;
	const int caret = static_cast<int>(_pEditView->execute(SCI_GETCURRENTPOS));
	const bool settingsChanged = _state->cachedHighlightEnabled != options.enabled || _state->cachedHighlightMode != static_cast<int>(options.mode) || _state->cachedHighlightAttributes != options.highlightAttributes;
	const bool diagnosticsChanged = _state->cachedShowErrors != options.showErrors;
	const bool matcherDirty = !_state->cachedMatcher || _state->matcherRevision != _state->documentRevision;
	if (!matcherDirty && !diagnosticsChanged && !settingsChanged && _state->cachedCaret == caret) return _state->cachedMatch;
	XmlTagMatcher& matcher = Matcher();
	if (matcherDirty || diagnosticsChanged) RefreshDiagnostics(options, matcher);
	ClearCurrentRanges();
	const XmlTagMatchResult result = matcher.ResultAt(static_cast<XmlBytePosition>(caret));
	if (!options.enabled || result.state != XmlTagMatchState::Matched) { _state->cachedCaret = caret; _state->cachedHighlightEnabled = options.enabled; _state->cachedHighlightMode = static_cast<int>(options.mode); _state->cachedHighlightAttributes = options.highlightAttributes; _state->cachedShowErrors = options.showErrors; _state->cachedMatch = false; return false; }
	FillRange(EDITOR_INDICATOR_TAG_MATCH, options.mode == XmlTagHighlightMode::FullTag ? result.currentTagRange : result.currentNameRange, _state->tagRanges);
	FillRange(EDITOR_INDICATOR_TAG_MATCH, options.mode == XmlTagHighlightMode::FullTag ? result.matchingTagRange : result.matchingNameRange, _state->tagRanges);
	if (options.highlightAttributes) for (const XmlByteRange& attribute : result.attributeRanges) FillRange(EDITOR_INDICATOR_TAG_ATTRIBUTE, attribute, _state->attributeRanges);
	_state->cachedCaret = caret; _state->cachedHighlightEnabled = options.enabled; _state->cachedHighlightMode = static_cast<int>(options.mode); _state->cachedHighlightAttributes = options.highlightAttributes; _state->cachedShowErrors = options.showErrors; _state->cachedMatch = true; return true;
}

XmlTagMatcher& XmlSourceTagHighlighter::Matcher()
{
	if (!_state->cachedMatcher || _state->matcherRevision != _state->documentRevision) {
		// SCI_GETTEXT is intentionally behind the modification revision check.
		// Ordinary caret, scroll and repaint updates use the cached matcher.
		const std::string documentText = _pEditView->getText();
		delete _state->cachedMatcher;
		_state->cachedMatcher = new XmlTagMatcher(documentText);
		_state->matcherRevision = _state->documentRevision;
	}
	return *_state->cachedMatcher;
}

void XmlSourceTagHighlighter::RefreshDiagnostics(const XmlTagHighlightOptions& options, XmlTagMatcher& matcher)
{
	ClearDiagnosticRanges();
	if (!options.showErrors) return;
	for (const XmlTagMatchResult& diagnostic : matcher.Diagnostics()) if (IsStructuralDiagnostic(diagnostic.state)) {
		const int indicator = DiagnosticIndicator(diagnostic.state);
		_pEditView->execute(SCI_SETINDICATORCURRENT, indicator);
		_pEditView->execute(SCI_INDICATORFILLRANGE, static_cast<WPARAM>(diagnostic.currentTagRange.start), static_cast<LPARAM>(diagnostic.currentTagRange.end - diagnostic.currentTagRange.start));
		_state->diagnosticRanges.push_back({ indicator, static_cast<int>(diagnostic.currentTagRange.start), static_cast<int>(diagnostic.currentTagRange.end) });
	}
}

bool XmlSourceTagHighlighter::GotoMatchingTag()
{
	const XmlTagMatchResult result = Matcher().ResultAt(static_cast<XmlBytePosition>(_pEditView->execute(SCI_GETCURRENTPOS)));
	if (result.state != XmlTagMatchState::Matched) return false;
	const XmlBytePosition caret = static_cast<XmlBytePosition>(_pEditView->execute(SCI_GETCURRENTPOS));
	const XmlBytePosition offset = caret > result.currentNameRange.start ? (std::min)(caret - result.currentNameRange.start, result.matchingNameRange.end - result.matchingNameRange.start) : 0;
	_pEditView->execute(SCI_GOTOPOS, static_cast<WPARAM>(result.matchingNameRange.start + offset));
	return true;
}

void XmlSourceTagHighlighter::GotoWrongTag()
{
	const vector<XmlTagMatchResult>& diagnostics = Matcher().Diagnostics(); if (diagnostics.empty()) return;
	const XmlBytePosition caret = static_cast<XmlBytePosition>(_pEditView->execute(SCI_GETCURRENTPOS)); XmlBytePosition target = diagnostics[0].currentTagRange.start;
	for (const XmlTagMatchResult& diagnostic : diagnostics) if (diagnostic.currentTagRange.start > caret) { target = diagnostic.currentTagRange.start; break; }
	_pEditView->execute(SCI_GOTOPOS, static_cast<WPARAM>(target));
}
