// Parse embedded HTA JScript with the legacy Windows engine without invoking
// UI, registry or file operations from the scripts themselves.
(function () {
    var fso = new ActiveXObject("Scripting.FileSystemObject"), index, path, stream, text, match;
    if (WScript.Arguments.length === 0) { WScript.Echo("Не переданы HTA для проверки."); WScript.Quit(2); }
    for (index = 0; index < WScript.Arguments.length; ++index) {
        path = WScript.Arguments.Item(index);
        stream = fso.OpenTextFile(path, 1);
        text = stream.ReadAll();
        stream.Close();
        match = /<script\b[^>]*>([\s\S]*?)<\/script>/ig.exec(text);
        if (!match) { WScript.Echo("В HTA нет embedded script: " + path); WScript.Quit(3); }
        // Function construction is compile-only.  The local bindings permit
        // ordinary HTA top-level assignments (for example window.onload).
        new Function("window", "document", "alert", match[1]);
    }
    WScript.Echo("Legacy JScript parse passed.");
}());
