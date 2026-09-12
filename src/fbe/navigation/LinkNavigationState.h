#pragma once
#include <atlstr.h>

namespace FBELinkNavigation {
// Link origins are DOM-scoped.  Store a stable ordinal rather than an MSHTML
// interface pointer, which must never outlive a document replacement.
struct LinkNavigationState {
  CString targetId;
  long originOrdinal;
  LinkNavigationState() : originOrdinal(-1) {}
  void Reset() { targetId.Empty(); originOrdinal = -1; }
  bool HasOrigin() const { return !targetId.IsEmpty() && originOrdinal >= 0; }
};
} // namespace FBELinkNavigation
