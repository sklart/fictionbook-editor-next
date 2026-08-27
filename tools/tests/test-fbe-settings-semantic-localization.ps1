[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$catalog = Get-Content -Raw -LiteralPath (Join-Path $root 'localization\app-ui\fbe-small-dialogs.json') | ConvertFrom-Json
$translations = @{
    'fbe.dialog.idd_setting_other.keep_manual' = @{ resource = 'IDD_SETTINGS_GENERAL'; targetId = 'IDC_KEEP'; values = @('Keep original encoding','Сохранять исходную кодировку','Зберігати початкове кодування','Ursprüngliche Kodierung beibehalten','Conserver le codage original','Conservar la codificación original','Mantieni la codifica originale','Zachowaj oryginalne kodowanie','Manter a codificação original','Oorspronkelijke codering behouden','Zachovat původní kódování','Запазвай оригиналната кодировка') }
    'fbe.dialog.idd_setting_other.restore_position' = @{ resource = 'IDD_SETTINGS_GENERAL'; targetId = 'IDC_RESTORE_POS'; values = @('Restore document position','Восстанавливать позицию в документе','Відновлювати позицію в документі','Dokumentposition wiederherstellen','Restaurer la position dans le document','Restaurar la posición en el documento','Ripristina la posizione nel documento','Przywracaj pozycję w dokumencie','Restaurar a posição no documento','Positie in document herstellen','Obnovit pozici v dokumentu','Възстановявай позицията в документа') }
    'fbe.dialog.idd_setting_other.clear_images' = @{ resource = 'IDD_SETTINGS_IMAGES'; targetId = 'IDC_OPTIONS_CLEARIMGS'; values = @('Insert empty images','Вставлять пустые изображения','Вставляти порожні зображення','Leere Bilder einfügen','Insérer des images vides','Insertar imágenes vacías','Inserisci immagini vuote','Wstawiaj puste obrazy','Inserir imagens vazias','Lege afbeeldingen invoegen','Vkládat prázdné obrázky','Вмъквай празни изображения') }
    'fbe.dialog.idd_setting_other.image_settings' = @{ resource = 'IDD_SETTINGS_IMAGES'; targetId = 'IDC_SETTINGS_OTHER_PASTE'; values = @('Paste images as','Вставлять изображения как','Вставляти зображення як','Bilder einfügen als','Coller les images en tant que','Pegar imágenes como','Incolla immagini come','Wklejaj obrazy jako','Colar imagens como','Afbeeldingen plakken als','Vkládat obrázky jako','Поставяй изображения като') }
    'fbe.dialog.idd_setting_other.nbsp_char' = @{ resource = 'IDD_SETTINGS_EDITOR'; targetId = 'IDC_SETTINGS_OTHER_NBSP_LABEL'; values = @('Display character:','Отображаемый символ:','Відображуваний символ:','Anzuzeigendes Zeichen:','Caractère à afficher :','Carácter mostrado:','Carattere visualizzato:','Wyświetlany znak:','Carácter a apresentar:','Weergegeven teken:','Zobrazovaný znak:','Показван символ:') }
}

$languages = @($catalog.targetLanguages)
foreach($key in $translations.Keys) {
    $expected = $translations[$key]
    $entry = $catalog.strings.$key
    if(-not $entry -or $entry.resource -ne $expected.resource -or $entry.targetId -ne $expected.targetId) { throw "Semantic localization metadata mismatch: $key" }
    for($index = 0; $index -lt $languages.Count; ++$index) {
        if($entry.translations.($languages[$index]) -ne $expected.values[$index]) { throw "Semantic localization mismatch: $key / $($languages[$index])" }
    }
}
Write-Host 'Settings semantic localization passed.'
