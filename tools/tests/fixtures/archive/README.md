# Archive runtime fixtures

The runtime suite never creates RAR archives. `archive-runtime-rar5.b64` is a
small, checked-in RAR5 container with one valid `book.fb2`; its decoded
SHA-256 is `e5f6e78cbde0799613aca7dc6997d42b31c826dda18702b091cb654b1f30e38e`.
It is decoded only into the test temporary directory and is used to check real
opening and the RAR-to-Save-As route without modifying the source archive.

RAR4 validation uses the locally supplied prepared archives outside the
repository. CI must not generate a RAR. The user explicitly excluded those
prepared archives from the online repository, so no copy is tracked here.
