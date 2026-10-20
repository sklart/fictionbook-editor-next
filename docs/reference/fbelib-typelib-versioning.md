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

## Compatibility invariants

The library identity remains `LIBID_FBELib`
(`37B16C7D-4400-4D7D-AA35-14C74E265EA4`) and the public
`IExternalHelper` IID remains `7269066E-2089-4408-B3F3-E8D75984D5A6`.
Existing DISPIDs, parameter order, invocation kind and Automation types are
immutable. Diagnostics occupies newly assigned DISPIDs 29 and 30 only; it does
not alter the core plugin contract.

The FBE executable embeds `FBE.tlb`; both installed and portable launches load
that embedded library with `REGKIND_NONE` before considering a registered copy.
If the registered 1.0 library is missing or incompatible, FBE performs a
per-user repair and verifies its direct path before the first `LoadRegTypeLib`.
This avoids changing machine-wide registration and leaves legacy plugins able to
use their compatible 1.0 contract.

The installer/package supplies the same `FBE.exe` resource as portable output;
there is no separately versioned FBELib artifact. The runtime test loads the
embedded resource from the produced executable and verifies all core and
diagnostic `IExternalHelper` FUNCDESC records, so a version change requires
updating that test and this compatibility analysis.
