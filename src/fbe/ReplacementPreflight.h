#pragma once

// Side-effect-free policy used by both Design-mode replacement commands.
// It deliberately has no dependency on dialogs, undo, or document ownership.
enum class ReplacementPreflightResult
{
	Allowed,
	CrossParagraph
};

ReplacementPreflightResult CheckReplacementRange(MSHTML::IHTMLTxtRangePtr range, bool regexp);
