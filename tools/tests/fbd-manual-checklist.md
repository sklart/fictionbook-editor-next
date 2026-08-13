# FBD integration checklist

Run this checklist against a release build before publishing an installer.

- Open each fixture in `tools/tests/fixtures/fbd`; a body-less FBD must remain editable and its metadata, cover, and Unicode text must be visible.
- Save an untouched body-less FBD and confirm that no synthetic `<body>` is written. Edit the visual body, save, and confirm that the edited body is preserved.
- Use Save As to convert FBD to FB2; the saved FB2 must contain a valid minimal body. Use Save As to convert FB2 to FBD; choose the FBD filter and confirm that the extension is `.fbd` exactly once.
- With an FBD document active, press F8 in both source and body views. It must report that FB2 schema validation is not applicable, without reporting a fabricated FB2 schema failure.
- In Source, edit `book-title`, author and annotation in `description_only.fbd`, then switch to Description or Body. The edits must remain and the switch must not report an FB2 schema failure. Remove a closing XML tag, press F8, and confirm that the parser error, line and column point into the current Source text.
- Insert an inline image into the synthetic body of a body-less FBD, save, close and reopen it. The image must remain; its element makes the synthetic placeholder a real body.
- Open `empty_body.fbd`, Save As `*.fb2`, then press F8. The whitespace/empty body must be replaced by a minimal FB2 body that passes the unmodified FB2 schema.
- Install the optional FBD association over an existing `.fbd` handler, then uninstall. The prior association must be restored. If the handler is changed after installation, uninstall must leave that later association unchanged.
