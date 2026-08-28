#include "../../src/fbe/KeyboardLayoutSelection.h"
#include <cassert>

int main()
{
	const KeyboardLayoutEntry russian = { 0x00000419, L"00000419" };
	const KeyboardLayoutEntry english = { 0x00000409, L"00000409" };
	const KeyboardLayoutEntry englishAlt = { 0x00010409, L"00010409" };
	assert(ResolveLegacyKeyboardLayoutId(0x04190419) == L"00000419");
	assert(ResolveLegacyKeyboardLayoutId(0x00000409) == L"00000409");
	assert(ResolveLegacyKeyboardLayoutId(0).empty());
	assert(ResolveKeyboardLayoutSelection(L"00010409", 0, 0, { english, englishAlt }).kind == KeyboardLayoutSelectionKind::ExactKlid);
	assert(ResolveKeyboardLayoutSelection(L"00010409", 0, 0, { english }).kind == KeyboardLayoutSelectionKind::UnavailableKlid);
	assert(ResolveKeyboardLayoutSelection(L"", 0x04190419, 0, { russian }).index == 0);
	assert(ResolveKeyboardLayoutSelection(L"", 0x0409, 0, { english }).kind == KeyboardLayoutSelectionKind::MigratedLegacy);
	assert(ResolveKeyboardLayoutSelection(L"", 0x0409, 0x00010409, { english, englishAlt }).index == 1);
	assert(ResolveKeyboardLayoutSelection(L"", 0x0409, 0x00000419, { english, englishAlt, russian }).kind == KeyboardLayoutSelectionKind::UnresolvedLegacy);
	assert(ResolveKeyboardLayoutSelection(L"", 0, 0x00000419, { english, russian }).kind == KeyboardLayoutSelectionKind::CurrentDefault);
	assert(ResolveKeyboardLayoutSelection(L"", 0, 0, {}).kind == KeyboardLayoutSelectionKind::None);
	return 0;
}
