# Editable regular-expression help

FBE loads `regex-design.md` or `regex-source.md` each time the Full Help dialog opens.

The lookup order is `Help/<current locale>/...`, then `Help/en-US/...`, then a short built-in message. Complete reviewed translations are supplied for all 12 supported UI locales. The `en-US` fallback remains only for an unknown or unavailable locale; the application never writes these files.
