#pragma once

#include <atlstr.h>
#include <mshtml.h>

namespace FBEReferenceNavigation {

enum class ResolutionStatus
{
  Unavailable,
  Candidate,
  Found,
  NoReferences
};

struct Resolution
{
  ResolutionStatus status = ResolutionStatus::Unavailable;
  MSHTML::IHTMLElementPtr element;
  MSHTML::IHTMLElementPtr scrollElement;

  bool CanNavigate() const { return status == ResolutionStatus::Candidate || status == ResolutionStatus::Found; }
  bool HasTarget() const { return status == ResolutionStatus::Found && element != nullptr; }
};

// Resolves the command's current document selection without changing it.  The
// check-only command path deliberately stops at Candidate, matching the legacy
// enablement behavior even when a broken fragment has no element in the DOM.
Resolution FindFootnoteTarget(MSHTML::IHTMLDocument2Ptr document, bool resolveTarget);

// Resolves the first document-order A element that refers to the current note
// section.  It performs no scrolling, selection changes, or UI reporting.
Resolution FindReferenceTarget(MSHTML::IHTMLDocument2Ptr document, bool resolveTarget);

} // namespace FBEReferenceNavigation
