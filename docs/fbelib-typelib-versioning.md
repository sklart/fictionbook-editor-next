# FBELib type library versioning

`FBELib` deliberately remains at type-library version **1.0**. The embedded
ExternalHelper obtains its `ITypeInfo` from the running FBE executable and
therefore does not depend on a registered per-user or machine-wide FBELib.

The 1.0 version must not be bumped merely because diagnostic methods are added:
installed plugins and portable copies can legitimately expose an older
registered library. Startup validation consequently reports compatibility in
two groups. Missing core methods are fatal for the requested operation; missing
diagnostic-only methods result in a warning and never prevent a document from
opening. New diagnostic methods must use new DISPIDs and remain optional.

A future 1.1 bump is reserved for a breaking public automation contract. It
requires explicit migration notes, regenerated export headers and validation of
installed as well as portable deployments.