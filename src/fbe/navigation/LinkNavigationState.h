#pragma once
#include <atlstr.h>

namespace FBELinkNavigation {
// Link origins are DOM-scoped.  Store a stable ordinal rather than an MSHTML
// interface pointer, which must never outlive a document replacement.
struct LinkNavigationState {
  CString targetId;
  long originUniqueNumber;
  LinkNavigationState() : originUniqueNumber(-1) {}
  void Reset() { targetId.Empty(); originUniqueNumber = -1; }
  bool HasOrigin() const { return !targetId.IsEmpty() && originUniqueNumber >= 0; }
};
} // namespace FBELinkNavigation
