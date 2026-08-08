/* FB2 recoder shared by cscript and fb2recode.hta.  Requires MSXML 6. */
(function (global) {
  var FB2_NS = "http://www.gribuser.ru/xml/fictionbook/2.0";
  var FSO = new ActiveXObject("Scripting.FileSystemObject");

  function normalizeEncoding(value) {
    value = String(value || "").replace(/^\s+|\s+$/g, "").toLowerCase();
    if (value === "utf8") return "utf-8";
    if (value === "cp1251" || value === "windows1251") return "windows-1251";
    if (value === "utf16" || value === "utf-16le") return "utf-16le";
    if (value === "utf-16be") return "utf-16be";
    return value;
  }
  function displayEncoding(value) { return normalizeEncoding(value) === "utf-8" ? "UTF-8" : normalizeEncoding(value); }
  function error(stage, message, code) { var e = new Error(message); e.stage = stage; e.code = code || 1; return e; }
  function bytes(path, count) {
    var s = new ActiveXObject("ADODB.Stream"); s.Type = 1; s.Open(); s.LoadFromFile(path);
    var b = s.Read(count || -1); s.Close(); return b;
  }
  function firstBytes(path) {
    var b = bytes(path, 4096), dom = xmlDocument(), node;
    node = dom.createElement("b"); node.dataType = "bin.hex"; node.nodeTypedValue = b;
    return node.text.toUpperCase();
  }
  function asciiHeader(hex) {
    var out = "", i, n;
    for (i = 0; i + 1 < hex.length; i += 2) { n = parseInt(hex.substr(i, 2), 16); out += n < 128 ? String.fromCharCode(n) : " "; }
    return out;
  }
  function declarationEncoding(header) {
    var m = header.match(/^\s*(?:\uFEFF)?<\?xml\s+[\s\S]*?\?>/i);
    if (!m) return "";
    m = m[0].match(/\bencoding\s*=\s*(['"])([^'"]+)\1/i);
    return m ? normalizeEncoding(m[2]) : "";
  }
  function adoCharset(enc) {
    enc = normalizeEncoding(enc);
    if (enc === "utf-16le") return "unicode";
    if (enc === "utf-16be") return "unicodeFFFE";
    return enc;
  }
  function detectEncoding(path) {
    var hex = firstBytes(path), bom = "", declared, h;
    if (hex.substr(0, 6) === "EFBBBF") bom = "utf-8";
    else if (hex.substr(0, 4) === "FFFE") bom = "utf-16le";
    else if (hex.substr(0, 4) === "FEFF") bom = "utf-16be";
    h = asciiHeader(hex);
    declared = declarationEncoding(h);
    if (!declared && bom && bom.indexOf("utf-16") === 0) declared = declarationEncoding(readText(path, adoCharset(bom)));
    /* A BOM-less UTF-16 declaration is still unambiguous from its byte order. */
    if (!declared && /^3C003F00/i.test(hex)) declared = declarationEncoding(readText(path, "unicode")) || "utf-16le";
    if (!declared && /^003C003F/i.test(hex)) declared = declarationEncoding(readText(path, "unicodeFFFE")) || "utf-16be";
    if (!declared && bom.indexOf("utf-16") === 0) declared = bom;
    if (!declared && !bom) throw error("определение кодировки", "Не найдены BOM или XML-декларация; кодировка не определена.");
    if (bom && declared && bom !== declared) throw error("определение кодировки", "BOM и XML-декларация указывают разные кодировки.");
    if (declared !== "utf-8" && declared !== "windows-1251" && declared !== "utf-16le" && declared !== "utf-16be")
      throw error("определение кодировки", "Неподдерживаемая кодировка: " + declared + ".");
    return { encoding: bom || declared, declared: declared || bom, bom: bom };
  }
  function readText(path, charset) {
    var s = new ActiveXObject("ADODB.Stream"); s.Type = 2; s.Charset = adoCharset(charset); s.Open(); s.LoadFromFile(path);
    var text = s.ReadText(); s.Close(); return text;
  }
  function writeText(path, text, charset) {
    var s = new ActiveXObject("ADODB.Stream"); s.Type = 2; s.Charset = adoCharset(charset); s.Open(); s.WriteText(text); s.SaveToFile(path, 2); s.Close();
  }
  function xmlDocument() {
    var d;
    try { d = new ActiveXObject("Msxml2.DOMDocument.6.0"); }
    catch (e) { throw error("создание XML-парсера", "Не удалось создать MSXML6.", 3); }
    d.async = false; d.preserveWhiteSpace = true; d.validateOnParse = false; d.resolveExternals = false;
    try { d.setProperty("ProhibitDTD", true); } catch (ignore) {}
    return d;
  }
  function validateXml(path) {
    var d = xmlDocument();
    if (!d.load(path)) { var p = d.parseError; throw error("проверка XML", p.reason + " (строка " + p.line + ", позиция " + p.linepos + ")"); }
    if (!d.documentElement || d.documentElement.baseName !== "FictionBook" || d.documentElement.namespaceURI !== FB2_NS)
      throw error("проверка XML", "Корневой элемент должен быть FictionBook в пространстве имён FB2.");
    return d;
  }
  function updateDeclaration(text, enc) {
    var m = text.match(/^\s*(?:\uFEFF)?<\?xml\s+([\s\S]*?)\?>/i), decl = "<?xml version=\"1.0\" encoding=\"" + displayEncoding(enc) + "\"?>";
    if (!m) return decl + "\r\n" + text;
    if (/\bencoding\s*=\s*(['"])[^'"]*\1/i.test(m[0]))
      decl = m[0].replace(/(\bencoding\s*=\s*['"])[^'"]*(['"])/i, "$1" + displayEncoding(enc) + "$2");
    else decl = m[0].replace(/\?>$/, " encoding=\"" + displayEncoding(enc) + "\"?>");
    return text.replace(m[0], decl);
  }
  function unrepresentable(text) {
    var bad = [], i, c;
    for (i = 0; i < text.length; i++) { c = text.charCodeAt(i); if (c > 0x7f && !(c >= 0x0400 && c <= 0x045f) && c !== 0x0401 && c !== 0x0451 && c !== 0x2116 && c !== 0x00a0) {
      if (bad.length < 8) bad.push("'" + text.charAt(i) + "' @ " + (i + 1));
    }}
    return bad;
  }
  function tempPath(path) { return FSO.BuildPath(FSO.GetParentFolderName(path), ".fb2recode-" + (new Date().getTime()) + "-" + Math.floor(Math.random() * 100000) + ".tmp"); }
  function replaceSafely(original, temp, options) {
    var backup = original + ".bak", parked = tempPath(original) + ".original";
    try {
      if (options.backup) {
        if (FSO.FileExists(backup)) { if (!options.overwrite) throw error("резервная копия", "Файл .bak уже существует."); FSO.DeleteFile(backup, true); }
        FSO.CopyFile(original, backup, false);
      }
      /* Both moves are in the original directory.  The original remains parked
         until the verified temporary file has taken its place, so a failed
         replacement can be rolled back without truncating the source. */
      FSO.MoveFile(original, parked);
      try { FSO.MoveFile(temp, original); }
      catch (moveError) { FSO.MoveFile(parked, original); throw moveError; }
      FSO.DeleteFile(parked, true);
    } catch (e) { if (FSO.FileExists(parked) && !FSO.FileExists(original)) try { FSO.MoveFile(parked, original); } catch (ignored) {} throw error("замена оригинала", e.message, 4); }
  }
  function processFile(path, options) {
    var info, text, target = normalizeEncoding(options.encoding), temp, bad;
    try {
      info = detectEncoding(path); text = readText(path, info.encoding); validateXml(path);
      if (info.encoding === target && normalizeEncoding(info.declared) === target) return { status: "same", path: path };
      text = updateDeclaration(text, target);
      if (target === "windows-1251" && !options.force) { bad = unrepresentable(text); if (bad.length) throw error("проверка Windows-1251", "Непредставимые символы: " + bad.join(", ") + ". Рекомендуется UTF-8."); }
      if (options.dryRun) return { status: "converted", path: path, dryRun: true };
      temp = tempPath(path); writeText(temp, text, target); validateXml(temp);
      if (detectEncoding(temp).encoding !== target || normalizeEncoding(detectEncoding(temp).declared) !== target) throw error("проверка записи", "XML-декларация не соответствует фактической кодировке.");
      replaceSafely(path, temp, options); return { status: "converted", path: path };
    } catch (e) { if (temp && FSO.FileExists(temp)) try { FSO.DeleteFile(temp, true); } catch (ignore) {} return { status: "error", path: path, stage: e.stage || "обработка", message: e.message, code: e.code || 1 }; }
  }
  function collect(folder, recursive, out) {
    var f, fs, subs;
    try { f = FSO.GetFolder(folder); fs = new Enumerator(f.Files); for (; !fs.atEnd(); fs.moveNext()) if (FSO.GetExtensionName(fs.item().Path).toLowerCase() === "fb2") out.push(fs.item().Path);
      if (recursive) { subs = new Enumerator(f.SubFolders); for (; !subs.atEnd(); subs.moveNext()) collect(subs.item().Path, true, out); } }
    catch (e) { out.errors.push({ status:"error", path:folder, stage:"обход папки", message:e.message, code:1 }); }
  }
  function run(paths, options, notify) {
    var files = [], i, result, stats = { found:0, same:0, converted:0, skipped:0, errors:0 }, results = [];
    files.errors = []; for (i = 0; i < paths.length; i++) { if (FSO.FolderExists(paths[i])) collect(paths[i], options.recursive, files); else files.push(paths[i]); }
    stats.found = files.length; for (i = 0; i < files.errors.length; i++) results.push(files.errors[i]);
    for (i = 0; i < files.length; i++) { if (notify) notify("[" + (i + 1) + "/" + files.length + "] " + files[i]); result = processFile(files[i], options); results.push(result); stats[result.status === "same" ? "same" : result.status === "converted" ? "converted" : "errors"]++; }
    return { stats:stats, results:results };
  }
  function usage() { return "fb2recode.js /encoding:utf-8 <file.fb2>\nfb2recode.js /encoding:windows-1251 /dir <folder> [/recursive] [/backup|/no-backup] [/dry-run] [/overwrite] [/report:<file>] [/quiet] [/help]"; }
  function cli() {
    var a = WScript.Arguments, opt = { backup:true, recursive:false, dryRun:false, overwrite:false, force:false }, paths = [], report = "", i, x, r;
    for (i=0;i<a.Count();i++) { x = String(a.Item(i)); if (/^\/encoding:/i.test(x)) opt.encoding=normalizeEncoding(x.substr(x.indexOf(":")+1)); else if (/^\/report:/i.test(x)) opt.report=x.substr(x.indexOf(":")+1); else if (/^\/dir$/i.test(x)) { i++; if(i<a.Count()) paths.push(String(a.Item(i))); else { WScript.Echo(usage()); WScript.Quit(2); } } else if (/^\/recursive$/i.test(x)) opt.recursive=true; else if (/^\/backup$/i.test(x)) opt.backup=true; else if (/^\/no-backup$/i.test(x)) opt.backup=false; else if (/^\/dry-run$/i.test(x)) opt.dryRun=true; else if (/^\/overwrite$/i.test(x)) opt.overwrite=true; else if (/^\/force$/i.test(x)) opt.force=true; else if (/^\/quiet$/i.test(x)) opt.quiet=true; else if (/^\/(help|\?)$/i.test(x)) { WScript.Echo(usage()); WScript.Quit(0); } else if (x.charAt(0)==="/") { WScript.Echo(usage()); WScript.Quit(2); } else paths.push(x); }
    if ((opt.encoding!=="utf-8" && opt.encoding!=="windows-1251") || !paths.length) { WScript.Echo(usage()); WScript.Quit(2); }
    r = run(paths,opt,function(s){if(!opt.quiet) WScript.Echo(s);}); for(i=0;i<r.results.length;i++) { x=r.results[i]; report += x.path+"\t"+x.status+(x.message?"\t"+x.stage+": "+x.message:"")+"\r\n"; if(!opt.quiet) WScript.Echo(x.status+": "+x.path+(x.message?" — "+x.stage+": "+x.message:"")); }
    report += "Найдено файлов: "+r.stats.found+"\r\nУже соответствуют: "+r.stats.same+"\r\nПреобразовано: "+r.stats.converted+"\r\nПропущено: "+r.stats.skipped+"\r\nОшибок: "+r.stats.errors+"\r\n"; if(opt.report) { var t=FSO.CreateTextFile(opt.report,true,true); t.Write(report); t.Close(); } if(!opt.quiet) WScript.Echo(report); WScript.Quit(r.stats.errors ? 1 : 0);
  }
  global.FB2Recode = { run:run, processFile:processFile, normalizeEncoding:normalizeEncoding, usage:usage };
  if (typeof WScript !== "undefined") cli();
}(this));
