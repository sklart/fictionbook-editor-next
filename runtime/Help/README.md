# Editable regular-expression help

FBE loads `regex-design.md` or `regex-source.md` each time the Full Help dialog opens.

The lookup order is `Help/<current locale>/...`, then `Help/en-US/...`, then a short built-in message. The reviewed, complete translations in this package are `en-US` and `ru-RU`. Other supported UI locales deliberately use the tested `en-US` fallback until a reviewed Markdown translation is added; the application never writes these files.
