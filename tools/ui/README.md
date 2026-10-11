# UI Layout Audit

Run against an already open FBE or FBV process:

```powershell
./tools/ui/invoke-ui-layout-audit.ps1 -ProcessId $processId -Language ru-RU -OutputPath ./out/tests/ui-layout-ru.json
```

The UTF-8 JSON report records each visible window and control, including HWND,
control ID, text, locale supplied by the caller, DPI, screen rectangle and
monitor work area. It flags windows outside the work area and possible text
clipping. Text measurements use each native Static or Button control's current
`WM_GETFONT` font. Possible overlaps are reported for visible sibling controls.
`GetDpiForWindow` is resolved dynamically so the probe also runs on Windows 7.

For deliberate overlaps or controls whose custom paint invalidates a heuristic,
pass `-ExceptionsPath` with a JSON file such as:

```json
{
  "ignoredControlIds": [1001],
  "ignoredOverlapPairs": ["1002:1003"]
}
```

Findings remain in the report with `ignored: true` for review. Hidden controls,
group boxes, icon statics, combo internals and custom rendered text are outside
the corresponding heuristics. An audit samples only windows open at capture
time; it does not replace the deferred GUI/DPI/localization matrix.
