#pragma once
struct TableToolbarCommand { UINT bitmapResourceId; UINT commandId; LPCWSTR localizationKey; LPCWSTR fallbackText; };
extern const TableToolbarCommand kTableToolbarCommands[]; extern const size_t kTableToolbarCommandCount; bool IsTableToolbarCommand(UINT commandId);
