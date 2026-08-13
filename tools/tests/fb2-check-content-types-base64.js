var fso = new ActiveXObject("Scripting.FileSystemObject");
// The shipped HTA is UTF-8, not UTF-16.  Use the system text mode here;
// parsing only needs the ASCII script delimiters and JavaScript syntax.
var file = fso.OpenTextFile(WScript.Arguments(0), 1, false, -2);
var text = file.ReadAll();
file.Close();
var match = /<script[^>]*>([\s\S]*?)<\/script>/i.exec(text);
if (!match) WScript.Quit(2);
eval(match[1]);

function valid(value) { validateBase64(value, 24); }
function invalid(value) {
    try { validateBase64(value, 24); throw new Error("accepted"); }
    catch (e) { if (e.message == "accepted") throw e; }
}

function makeBase64(bytes) {
    var groups = Math.ceil(bytes / 3), chunks = [], chunkGroups = 16384, index, count;
    for (index = 0; index < groups; index += chunkGroups) {
        count = Math.min(chunkGroups, groups - index);
        chunks.push(new Array(count + 1).join("QUJD"));
    }
    return chunks.join("");
}

function timedValidation(bytes) {
    var value = makeBase64(bytes), started = new Date().getTime();
    valid(value);
    return { value: value, elapsed: new Date().getTime() - started };
}

valid("iVBORw0KGgoAAAANSUhEUg== ");
valid("iVBORw0K\r\nGgoAAAANSUhEUg==");
var invalidValues = ["", "!", "iVBORw0K!GgoAAAANSUhEUg==", "iVBORw0KGgoAAAANSUhEUg==!", "iVBORw0KGgoAAAANSUhEUg=", "iVBORw0KGgoAAAANSUhEUg===", "iVBORw0KGgoAAAANSUhEU=Q==", "iVBORw0KGgoAAAANSUhEUg==QUJD"];
for (var i = 0; i < invalidValues.length; i++) invalid(invalidValues[i]);

var sizes = [1024 * 1024, 5 * 1024 * 1024];
if (WScript.Arguments.length > 1 && WScript.Arguments(1) == "--include-25mib") sizes.push(25 * 1024 * 1024);
var results = [], previous = null;
for (i = 0; i < sizes.length; ++i) {
    var sample = timedValidation(sizes[i]);
    var badCharacter = sample.value.substr(0, sample.value.length - 1) + "!";
    var badPadding = sample.value.substr(0, sample.value.length - 4) + "=QUJ";
    invalid(badCharacter); invalid(badPadding);
    if (previous && sample.elapsed > previous.elapsed * (sizes[i] / previous.bytes) * 3 + 250) throw new Error("Base64 validation is not approximately linear.");
    results.push({ bytes: sizes[i], elapsed: sample.elapsed }); previous = results[results.length - 1];
}
for (i = 0; i < results.length; ++i) WScript.Echo("Base64 " + results[i].bytes + " bytes: " + results[i].elapsed + " ms");
