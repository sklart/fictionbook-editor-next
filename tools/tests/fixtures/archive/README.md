# Archive runtime fixtures

The runtime suite never creates RAR archives. `archive-runtime-rar5.b64` is a
small, checked-in RAR5 container with one valid `book.fb2`; its decoded
SHA-256 is `e5f6e78cbde0799613aca7dc6997d42b31c826dda18702b091cb654b1f30e38e`.
`archive-runtime-rar5-multi.b64` is a separately authored RAR5 archive with
`book.fb2` and `book.fbd`; its decoded SHA-256 is
`bd8e72a8754c44075f00fbba7426cb8babbc6b0787e42660355f964e58ed5c43`.
They are decoded only into the test temporary directory and are used to check
real opening, selected FBD entry type, and the RAR-to-Save-As route without
modifying the source archive.

RAR4 validation is deliberately a separate local test
(`test-fbe-archive-rar4-local.ps1`). Current WinRAR 7 tooling on the build
host cannot create RAR4 archives, and the user's prepared RAR4 archives are
explicitly excluded from the online repository. CI therefore does not claim
RAR4 fixture coverage and never generates RAR archives.
