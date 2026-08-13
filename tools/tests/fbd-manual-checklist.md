# FBD integration checklist

Run this checklist against a release build before publishing an installer.

- Open each fixture in `tools/tests/fixtures/fbd`; a body-less FBD must remain editable and its metadata, cover, and Unicode text must be visible.
- Save an untouched body-less FBD and confirm that no synthetic `<body>` is written. Edit the visual body, save, and confirm that the edited body is preserved.
- Use Save As to convert FBD to FB2; the saved FB2 must contain a valid minimal body. Use Save As to convert FB2 to FBD; choose the FBD filter and confirm that the extension is `.fbd` exactly once.
- With an FBD document active, press F8 in both source and body views. It must report that FB2 schema validation is not applicable, without reporting a fabricated FB2 schema failure.
- Install the optional FBD association over an existing `.fbd` handler, then uninstall. The prior association must be restored. If the handler is changed after installation, uninstall must leave that later association unchanged.
