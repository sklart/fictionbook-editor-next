#pragma once

#include <vector>
#include <string>

// Owns a separate menu tree while leaving the caller's menu and its bitmap
// handles untouched. This is a building block for measuring palette menus
// without changing already-measured native items in place.
class PaletteMenuSnapshot
{
	struct Mapping { HMENU snapshot; HMENU source; };
	HMENU m_root = NULL;
	std::vector<Mapping> m_menus;
	using Transform = bool (*)(MENUITEMINFOW&, const wchar_t*, void*);
	Transform m_transform = NULL;
	void* m_transformContext = NULL;
	HMENU Copy(HMENU source)
	{
		if(!::IsMenu(source)) return NULL;
		HMENU copy = ::CreatePopupMenu();
		if(!copy) return NULL;
		m_menus.push_back({copy, source});
		for(int i = 0; i < ::GetMenuItemCount(source); ++i) {
			MENUITEMINFOW info = {}; info.cbSize = sizeof(info);
			info.fMask = MIIM_FTYPE | MIIM_STATE | MIIM_ID | MIIM_SUBMENU | MIIM_BITMAP | MIIM_CHECKMARKS | MIIM_DATA | MIIM_STRING;
			if(!::GetMenuItemInfoW(source, i, TRUE, &info) || (info.fType & MFT_OWNERDRAW)) { ::DestroyMenu(copy); return NULL; }
			std::vector<wchar_t> text(static_cast<size_t>(info.cch) + 1, 0);
			info.dwTypeData = text.data(); info.cch = static_cast<UINT>(text.size());
			if(!::GetMenuItemInfoW(source, i, TRUE, &info)) { ::DestroyMenu(copy); return NULL; }
			const HMENU child = info.hSubMenu;
			if(child) {
				info.hSubMenu = Copy(child);
				if(!info.hSubMenu) { ::DestroyMenu(copy); return NULL; }
			}
			if(m_transform && !m_transform(info, text.data(), m_transformContext)) {
				if(child) ::DestroyMenu(info.hSubMenu);
				::DestroyMenu(copy); return NULL;
			}
			if(!::InsertMenuItemW(copy, i, TRUE, &info)) {
				if(child) ::DestroyMenu(info.hSubMenu);
				::DestroyMenu(copy); return NULL;
			}
		}
		return copy;
	}
public:
	PaletteMenuSnapshot() = default;
	PaletteMenuSnapshot(const PaletteMenuSnapshot&) = delete;
	PaletteMenuSnapshot& operator=(const PaletteMenuSnapshot&) = delete;
	~PaletteMenuSnapshot() { if(m_root) ::DestroyMenu(m_root); }
	bool Create(HMENU source, Transform transform = NULL, void* context = NULL) {
		if(m_root) return false;
		m_menus.clear(); m_transform = transform; m_transformContext = context; m_root = Copy(source);
		if(!m_root) m_menus.clear();
		return m_root != NULL;
	}
	HMENU Root() const { return m_root; }
	HMENU Source(HMENU snapshot) const {
		for(const auto& item : m_menus) if(item.snapshot == snapshot) return item.source;
		return NULL;
	}
};
