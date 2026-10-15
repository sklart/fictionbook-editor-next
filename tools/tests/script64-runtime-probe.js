// Runs inside the real FBE document. candidate is the unchanged bundled Run.
var messages = [];
function MsgBox(text) { messages.push(String(text)); }
function alert(text) { messages.push(String(text)); }
var results = [];
function exercise(name, text, pairs, selected, expected, refusal, mode) {
    messages = [];
    var root = document.getElementById("fbw_body");
    root.innerHTML = '<DIV class="body"><DIV class="section"><P id="script64-probe"></P></DIV></DIV>';
    var p = document.getElementById("script64-probe");
    p.innerText = text;
    if (mode == "nodes") p.innerHTML = 'a<EM>b<STRONG>c</STRONG></EM> ab';
    if (mode == "textarea") {
        root.insertAdjacentHTML("beforeEnd", '<TEXTAREA id="script64-field"></TEXTAREA>');
        p = document.getElementById("script64-field");
        p.value = text;
    }
    p.scrollIntoView(true);
    var range = mode == "textarea" ? p.createTextRange() : document.body.createTextRange();
    if (mode != "textarea") range.moveToElementText(p);
    if (mode == "textarea") { range.collapse(true); range.moveStart("character", 1); range.moveEnd("character", 2); }
    if (!selected) range.collapse(true);
    range.select();
    if (pairs === null) window.clipboardData.clearData();
    else window.clipboardData.setData("Text", pairs);
    var beforeHTML = root.innerHTML;
    var error = "";
    try { candidate(); } catch (e) { error = String(e.message || e.description || e); }
    p = document.getElementById(mode == "textarea" ? "script64-field" : "script64-probe");
    var actual = p ? (mode == "textarea" ? p.value : p.innerText) : "<missing>";
    var markers = document.getElementById("BlockStart") || document.getElementById("BlockEnd") || document.getElementById("CursorPosition");
    var preserved = !refusal || (messages.length == 1 && root.innerHTML == beforeHTML);
    var structure = mode != "nodes" || p.innerHTML == 'A<EM>B<STRONG>c</STRONG></EM> AB';
    var ok = !error && actual == expected && preserved && structure && !markers;
    results.push(name + "=" + (ok ? "1" : "0"));
    results.push(name + "-actual=" + encodeURIComponent(actual));
    results.push(name + "-messages=" + messages.length);
    if (error) results.push(name + "-error=" + encodeURIComponent(error));
}
exercise("empty-selection", "abc ab", "aA\r\nbB", false, "abc ab", true);
exercise("empty-list", "abc ab", "", true, "abc ab", true);
exercise("invalid-list", "abc ab", "abc", true, "abc ab", true);
exercise("special", ".+*?()[]^$|/", ".A\r\n+B\r\n*C\r\n?D\r\n(E\r\n)F\r\n[G\r\n]H\r\n^I\r\n$J\r\n|K\r\n/L", true, "ABCDEFGHIJKL", false);
exercise("backslash", "a\\b", "\\Z", true, "aZb", false);
exercise("quote", 'a"b', '"Z', true, "aZb", false);
exercise("replacement-special", "abc", 'a$\r\nb\\\r\nc"', true, '$\\"', false);
exercise("mixed-special", "a.b+c", 'a$\r\n.\\\r\nb"\r\n+|\r\nc/', true, '$\\"|/', false);
exercise("newlines", "abcd", " aA \n\nbB\rcC\r\ndD\r", true, "ABCD", false);
exercise("cyrillic", "абя", "аА\nбБ\nяЯ", true, "АБЯ", false);
exercise("no-text", "abc ab", null, true, "abc ab", true);
exercise("utf16-reject", "abc", "\uD83D\uDE00A", true, "abc", true);
exercise("invalid-late", "ab", "aA\nbBB", true, "ab", true);
exercise("nodes", "abc ab", "aA\nbB", true, "ABc AB", false, "nodes");
exercise("textarea", "xaby", "aA\nbB", true, "xABy", false, "textarea");
exercise("numeric-order", "2", "21\n1X", true, "1", false);
// Actual legacy JScript object enumeration: duplicate keys overwrite in place.
var legacyObject = {"2":"B", "1":"A", "a":"x", "a":"y", "b":"z"};
var legacyOrder = [];
for (var legacyKey in legacyObject) legacyOrder.push(legacyKey + legacyObject[legacyKey]);
results.push("legacy-order=" + legacyOrder.join(","));
exercise("duplicates", "ab", "aA\r\nbB\r\naC", true, "CB", false);
exercise("cascade", "ab", "ab\r\nbc", true, "cc", false);
// Prepare the last fixture, then let native code discard ALL previous undo units
// before invoking the original candidate. Setup cannot impersonate script Undo.
var ordinaryRoot = document.getElementById("fbw_body");
window.__script64PrepareOrdinary = function () {
    ordinaryRoot.innerHTML = '<DIV class="body"><DIV class="section"><P id="script64-probe">abc ab</P></DIV></DIV>';
    var range = document.body.createTextRange();
    range.moveToElementText(document.getElementById("script64-probe"));
    range.select();
    window.__script64Before = ordinaryRoot.innerHTML;
};
var refusalBefore = "";
window.__script64RefusalSetup = function (index) {
    window.__script64PrepareOrdinary();
    if (index == 0) { var range = document.selection.createRange(); range.collapse(true); range.select(); }
    if (index == 3) window.clipboardData.clearData();
    else window.clipboardData.setData("Text", index == 1 ? "" : (index == 2 ? "aA\nbBB" : "aA"));
    refusalBefore = ordinaryRoot.innerHTML;
};
window.__script64RefusalRun = function () {
    messages = [];
    var failed = false;
    try { candidate(); } catch (e) { failed = true; }
    var ok = !failed && messages.length == 1 && ordinaryRoot.innerHTML == refusalBefore &&
        !document.getElementById("BlockStart") && !document.getElementById("BlockEnd");
    window.external.SetStatusBarText(ok ? "1" : "0");
};
window.__script64FaultSetup = function () {
    window.__script64PrepareOrdinary();
    document.getElementById("script64-probe").innerHTML = 'a<EM>b<STRONG>c</STRONG></EM> ab';
    var range = document.body.createTextRange();
    range.moveToElementText(document.getElementById("script64-probe"));
    range.select();
    window.clipboardData.setData("Text", "aA");
};
window.__script64FaultRun = function () {
    var split = String.prototype.split, injected = false;
    String.prototype.split = function (separator) {
        if (String(this) == "b" && separator == "a") { injected = true; throw new Error("script64-injected-failure"); }
        return split.apply(this, arguments);
    };
    var failed = false;
    try { candidate(); } catch (e) { failed = true; }
    finally { String.prototype.split = split; }
    var p = document.getElementById("script64-probe");
    var ok = injected && failed && p.innerText == "Abc ab" &&
        !document.getElementById("BlockStart") && !document.getElementById("BlockEnd");
    window.external.SetStatusBarText(ok ? "1" : "0");
};
window.__script64Ordinary = function () {
    window.clipboardData.setData("Text", "aA\r\nbB");
    messages = [];
    var error = "";
    try { candidate(); } catch (e) { error = String(e.message || e.description || e); }
    var actual = document.getElementById("script64-probe").innerText;
    window.__script64After = ordinaryRoot.innerHTML;
    results.push("ordinary=" + (!error && actual == "ABc AB" ? "1" : "0"));
    if (error) results.push("ordinary-error=" + encodeURIComponent(error));
    window.external.SetStatusBarText(results.join("\n"));
};
window.external.SetStatusBarText(results.join("\n"));
