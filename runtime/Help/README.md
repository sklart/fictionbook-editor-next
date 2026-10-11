# Regular-expression help

The `regex-design.md` and `regex-source.md` files for all 12 supported UI locales are the editable sources. `tools/build/generate-regex-help-html.ps1 -Generate` creates the corresponding HTML pages; `-Check` verifies that the generated pages are current. Do not edit the HTML files by hand.

FBE opens the local HTML page in the user's browser. The lookup order is `Help/<current locale>/...`, then `Help/en-US/...`. If neither page exists or the browser cannot be opened, FBE displays a localized error. The help uses only local files and does not require a network connection.
