var fso = new ActiveXObject("Scripting.FileSystemObject");
var file = fso.OpenTextFile(WScript.Arguments(0), 1, false, -1);
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

valid("iVBORw0KGgoAAAANSUhEUg== ");
valid("iVBORw0K\r\nGgoAAAANSUhEUg==");
var invalidValues = ["", "!", "iVBORw0K!GgoAAAANSUhEUg==", "iVBORw0KGgoAAAANSUhEUg==!", "iVBORw0KGgoAAAANSUhEUg=", "iVBORw0KGgoAAAANSUhEUg===", "iVBORw0KGgoAAAANSUhEU=Q==", "iVBORw0KGgoAAAANSUhEUg==QUJD"];
for (var i = 0; i < invalidValues.length; i++) invalid(invalidValues[i]);
