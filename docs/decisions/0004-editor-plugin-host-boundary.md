# 0004. Host плагинов остаётся editor-only подсистемой

## Контекст

`PluginManager` загружает bundled DLL из manifest, а `PluginApiV2` создаёт
COM-объекты host и snapshot для вызовов из главного окна. Оба файла раньше
находились в корне `src/fbe`, хотя не являются ни public IDL-контрактом, ни
общей FB2-реализацией.

## Решение

`PluginManager.{h,cpp}` и `PluginApiV2.{h,cpp}` расположены в
`src/fbe/plugins`. Их единственный потребитель — FBE; project и filters явно
отражают это размещение. Допустимы зависимости на FBE/MFC, COM и общие
локализационные helper-ы, но не на private UI-координаторы (`mainfrm`, view или
SettingsDlg).

## Последствия

Перенос не меняет IDL, CLSID, IID, загрузку DLL, v2 negotiation или правила
владения COM-объектами. `test-fbe-plugin-host-boundary.ps1` фиксирует путь,
project/filter и запрет UI-зависимостей; существующие policy и ABI tests
проверяют поведение.
