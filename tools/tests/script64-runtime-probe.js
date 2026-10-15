// Runs inside the real FBE document. candidate is the unchanged bundled Run.
var messages = [];
function MsgBox(text) { messages.push(String(text)); }
function alert(text) { messages.push(String(text)); }
var results = [];
function exercise(name, text, pairs, selected, expected, refusal) {
    messages = [];
    var root = document.getElementById("fbw_body");
    root.innerHTML = '<DIV class="body"><DIV class="section"><P id="script64-probe"></P></DIV></DIV>';
    var p = document.getElementById("script64-probe");
    p.innerText = text;
    p.scrollIntoView(true);
    var range = document.body.createTextRange();
    range.moveToElementText(p);
    if (!selected) range.collapse(true);
    range.select();
    window.clipboardData.setData("Text", pairs);
    var error = "";
    try { candidate(); } catch (e) { error = String(e.message || e.description || e); }
    p = document.getElementById("script64-probe");
    var actual = p ? p.innerText : "<missing>";
    var ok = !error && actual == expected && (!refusal || messages.length > 0);
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
ordinaryRoot.innerHTML = '<DIV class="body"><DIV class="section"><P id="script64-probe">abc ab</P></DIV></DIV>';
var ordinaryRange = document.body.createTextRange();
ordinaryRange.moveToElementText(document.getElementById("script64-probe"));
ordinaryRange.select();
window.__script64Before = ordinaryRoot.innerHTML;
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
