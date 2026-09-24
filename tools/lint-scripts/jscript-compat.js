// Compile-only compatibility check for the Microsoft JScript engine used by FBE.
// It never executes runtime scripts, dialogs, ActiveX calls or DOM operations.
(function () {
    var fso = new ActiveXObject("Scripting.FileSystemObject"), root, failures = 0, checked = 0, listOnly = false;

    function ignored(path) {
        var lower = path.toLowerCase();
        return lower.indexOf("\\generated\\") >= 0 || lower.indexOf("\\vendor\\") >= 0 ||
            /\\jquery[^\\]*\.js$/.test(lower) || /\\jszip[^\\]*\.js$/.test(lower);
    }

    function read(path) {
        // OpenTextFile defaults to the host ANSI code page, which changes the
        // token stream for UTF-8 files on CI machines. ADODB.Stream decodes
        // the repository's UTF-8 runtime sources explicitly.
        var stream = new ActiveXObject("ADODB.Stream"), text;
        stream.Type = 2;
        stream.Charset = "utf-8";
        stream.Open();
        stream.LoadFromFile(path);
        text = stream.ReadText();
        stream.Close();
        return text;
    }

    function sourceHash(source) {
        var hash = 2166136261, index;
        for (index = 0; index < source.length; index++) {
            hash ^= source.charCodeAt(index);
            hash += (hash << 1) + (hash << 4) + (hash << 7) + (hash << 8) + (hash << 24);
        }
        return (hash >>> 0).toString(16);
    }

    function failureKey(path, block, error, source) {
        var file = "runtime/" + path.substr(root.length + 1).replace(/\\/g, "/");
        return encodeURIComponent(file) + "|" + block + "|" + error.number + "|" + sourceHash(source);
    }

    function compile(path, source, label, block) {
        try {
            new Function("window", "document", "alert", source);
            checked++;
        } catch (error) {
            failures++;
            if (listOnly) WScript.Echo("FAIL " + failureKey(path, block, error, source));
            else WScript.Echo(path + label + ": " + (error.description || error.message));
        }
    }

    function checkHtml(path) {
        var text = read(path), pattern = /<script\b([^>]*)>([\s\S]*?)<\/script\s*>/ig, match, count = 0;
        while ((match = pattern.exec(text)) !== null) {
            if (/\bsrc\s*=/i.test(match[1])) continue;
            count++;
            compile(path, match[2], " (inline script " + count + ")", count);
        }
    }

    function checkFolder(folder) {
        var files = new Enumerator(folder.Files), folders = new Enumerator(folder.SubFolders), file, child, extension;
        for (; !files.atEnd(); files.moveNext()) {
            file = files.item();
            if (ignored(file.Path)) continue;
            extension = fso.GetExtensionName(file.Name).toLowerCase();
            if (extension === "js") compile(file.Path, read(file.Path), "", 0);
            if (extension === "html" || extension === "htm") checkHtml(file.Path);
        }
        for (; !folders.atEnd(); folders.moveNext()) {
            child = folders.item();
            if (!ignored(child.Path)) checkFolder(child);
        }
    }

    if (WScript.Arguments.length < 1 || WScript.Arguments.length > 2) {
        WScript.Echo("Передайте каталог runtime для проверки JScript.");
        WScript.Quit(2);
    }
    root = fso.GetAbsolutePathName(WScript.Arguments.Item(0));
    listOnly = WScript.Arguments.length === 2 && WScript.Arguments.Item(1) === "--list";
    if (!fso.FolderExists(root)) {
        WScript.Echo("Не найден каталог runtime: " + root);
        WScript.Quit(2);
    }
    checkFolder(fso.GetFolder(root));
    if (failures && !listOnly) WScript.Quit(1);
    WScript.Echo("Microsoft JScript compatibility passed; checked " + checked + " script block(s).");
}());
