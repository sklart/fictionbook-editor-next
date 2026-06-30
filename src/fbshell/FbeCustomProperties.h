#pragma once

#include <propkey.h>

namespace FBShellModern {

// Отдельный набор свойств FBE для modern shell-контура.
//
// Важно: этот GUID намеренно не совпадает с legacy FMTID_FB из старого
// ColumnProvider. Новый schema-контур для Проводника должен жить отдельно от
// исторической shell-интеграции.

extern const PROPERTYKEY PKEY_FBE_Sequence;
extern const PROPERTYKEY PKEY_FBE_Genre;
extern const PROPERTYKEY PKEY_FBE_DocumentVersion;
extern const PROPERTYKEY PKEY_FBE_DocumentDate;
extern const PROPERTYKEY PKEY_FBE_Keywords;
extern const PROPERTYKEY PKEY_FBE_DocumentId;

} // namespace FBShellModern
