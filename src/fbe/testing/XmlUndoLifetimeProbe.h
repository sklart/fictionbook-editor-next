#pragma once

class XmlScriptBackend;
struct IOleUndoUnit;
namespace FB { class Doc; }
namespace RuntimeTests {
// Native-only test hook: retains real XML payload snapshots in the production
// parent unit, without retaining MSHTML's private markup child units.
HRESULT CreateXmlUndoLifetimeProbe(XmlScriptBackend* backend, FB::Doc* document, IOleUndoUnit** result);
}
