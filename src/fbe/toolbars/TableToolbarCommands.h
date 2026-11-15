#pragma once
struct TableToolbarCommand { UINT bitmapResourceId; UINT commandId; LPCWSTR localizationKey; LPCWSTR fallbackText; };
constexpr size_t kTableToolbarCommandCount = 9;
extern const TableToolbarCommand kTableToolbarCommands[kTableToolbarCommandCount]; bool IsTableToolbarCommand(UINT commandId);
