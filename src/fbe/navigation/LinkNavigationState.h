#pragma once
#include <atlstr.h>
namespace FBELinkNavigation {
struct LinkNavigationState {
  CString targetId;
  long originOrdinal;
  LinkNavigationState() : originOrdinal(-1) {}
  void Reset() {
    targetId.Empty();
    originOrdinal = -1;
  }
  bool HasOrigin() const { return !targetId.IsEmpty() && originOrdinal >= 0; }
};
} // namespace FBELinkNavigation
