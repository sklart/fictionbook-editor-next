// Executes the production HTA registration functions against an in-memory
// WScript.Shell.  The PowerShell wrapper separately checks the real reg.exe
// deletion mechanism in an isolated HKCU key.
(function () {
    var nativeActiveXObject = ActiveXObject;
    var fileSystem = new nativeActiveXObject("Scripting.FileSystemObject");
    var values = {};

    function fail(message) { WScript.Echo(message); WScript.Quit(1); }
    function requireValue(path, message) { if (!values.hasOwnProperty(path)) fail(message + ": " + path); }
    function forbidValue(path, message) { if (values.hasOwnProperty(path)) fail(message + ": " + path); }
    function requirePrefix(prefix, present, message) {
        var found = false, path;
        for (path in values) if (values.hasOwnProperty(path) && path.indexOf(prefix) === 0) { found = true; break; }
        if (found !== present) fail(message + ": " + prefix);
    }
    function deleteTree(path) {
        var prefix = path.toLowerCase(), key, remove = [];
        for (key in values) if (values.hasOwnProperty(key) && key.toLowerCase().indexOf(prefix) === 0) remove.push(key);
        for (var index = 0; index < remove.length; ++index) delete values[remove[index]];
    }
    var shell = {
        RegWrite: function (path, value) { values[path] = value; },
        RegRead: function (path) { if (!values.hasOwnProperty(path)) throw new Error("not found"); return values[path]; },
        RegDelete: function (path) { if (!values.hasOwnProperty(path)) throw new Error("not found"); delete values[path]; },
        Run: function (command) {
            var match = /^reg\.exe delete "([^"]+)" \/f$/i.exec(command);
            if (match) deleteTree(match[1]);
            return 0;
        }
    };
    var fields = { f1: { value: "C:\\test\\handler.exe" }, f2: { value: "C:\\test\\archive.exe" }, f3: { value: "C:\\test\\fb2.exe" }, t1: { value: '"$1"' }, t2: { value: '"$1"' } };
    document = { getElementById: function (id) { return fields[id] || null; } };
    window = {};
    alert = function () {};
    ActiveXObject = function (name) {
        if (name === "WScript.Shell") return shell;
        if (name === "Scripting.FileSystemObject") return { FileExists: function () { return true; }, GetExtensionName: function () { return "exe"; } };
        throw new Error("unexpected ActiveX object: " + name);
    };

    function load(path) {
        var stream = fileSystem.OpenTextFile(path, 1), text = stream.ReadAll(), match;
        stream.Close();
        match = /<script[^>]*>([\s\S]*?)<\/script>/i.exec(text);
        if (!match) fail("Не найден JScript в " + path);
        return new Function("document", "ActiveXObject", "alert", "window",
            match[1] + "; return { applySettings: applySettings, resetSettings: resetSettings };")
            (document, ActiveXObject, alert, window);
    }
    if (WScript.Arguments.length !== 2) fail("Ожидались пути ConfigZipHandler.hta и ConfigRarHandler.hta.");
    var zipPath = WScript.Arguments.Item(0), rarPath = WScript.Arguments.Item(1);
    var base = "HKCU\\Software\\FictionBook Editor\\ArchHandler\\";
    var classes = "HKCU\\Software\\Classes\\";
    var capabilities = base + "Capabilities\\";
    var registered = "HKCU\\Software\\RegisteredApplications\\FictionBook Editor ArchHandler";
    var zipProgId = "FictionBookEditor.ArchHandler.zip", rarProgId = "FictionBookEditor.ArchHandler.rar";

    var zip = load(zipPath); zip.applySettings();
    var rar = load(rarPath); rar.applySettings();
    requirePrefix(base + "zip\\", true, "ZIP handler не зарегистрирован");
    requirePrefix(base + "rar\\", true, "RAR handler не зарегистрирован");
    requireValue(classes + ".zip\\OpenWithProgids\\" + zipProgId, "Не зарегистрирован ZIP OpenWithProgids");
    requireValue(classes + ".rar\\OpenWithProgids\\" + rarProgId, "Не зарегистрирован RAR OpenWithProgids");
    requireValue(capabilities + "FileAssociations\\.zip", "Не зарегистрирована ZIP capability");
    requireValue(capabilities + "FileAssociations\\.rar", "Не зарегистрирована RAR capability");
    requireValue(registered, "Не зарегистрировано приложение ArchHandler");

    zip = load(zipPath); zip.resetSettings();
    requirePrefix(base + "zip\\", false, "Reset ZIP не удалил ZIP handler");
    requirePrefix(classes + zipProgId, false, "Reset ZIP не удалил ZIP ProgID");
    forbidValue(classes + ".zip\\OpenWithProgids\\" + zipProgId, "Reset ZIP не удалил ZIP OpenWithProgids");
    forbidValue(capabilities + "FileAssociations\\.zip", "Reset ZIP не удалил ZIP capability");
    requirePrefix(base + "rar\\", true, "Reset ZIP повредил RAR handler");
    requireValue(classes + ".rar\\OpenWithProgids\\" + rarProgId, "Reset ZIP повредил RAR OpenWithProgids");
    requireValue(capabilities + "FileAssociations\\.rar", "Reset ZIP повредил RAR capability");
    requireValue(registered, "Reset ZIP удалил RegisteredApplications при работающем RAR");

    rar = load(rarPath); rar.resetSettings();
    requirePrefix(base + "rar\\", false, "Reset RAR не удалил RAR handler");
    requirePrefix(classes + rarProgId, false, "Reset RAR не удалил RAR ProgID");
    forbidValue(classes + ".rar\\OpenWithProgids\\" + rarProgId, "Reset RAR не удалил RAR OpenWithProgids");
    forbidValue(capabilities + "FileAssociations\\.rar", "Reset RAR не удалил RAR capability");
    requirePrefix(capabilities, false, "Reset RAR не удалил пустой Capabilities");
    forbidValue(registered, "Reset RAR не удалил RegisteredApplications");
    WScript.Echo("ArchHandler Reset behavioral script passed.");
}());
