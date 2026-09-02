/* Executes the shipped HTA saveCheckedXML function under WSH.  This is a
   regression harness, not a second implementation of safe replacement. */
(function () {
  function writeStatus(path, code, message) {
    var stream = new ActiveXObject("ADODB.Stream");
    stream.Type = 2; stream.Charset = "utf-8"; stream.Open();
    stream.WriteText(String(code) + "\r\n" + String(message) + "\r\n");
    stream.SaveToFile(path, 2); stream.Close();
  }
  function readUtf8(path) {
    var stream = new ActiveXObject("ADODB.Stream");
    stream.Type = 2; stream.Charset = "utf-8"; stream.Open();
    stream.LoadFromFile(path); var text = stream.ReadText(); stream.Close(); return text;
  }
  var statusPath = "";
  try {
    if (WScript.Arguments.Count() !== 4) WScript.Quit(2);
    var htaPath = WScript.Arguments.Item(0), sourcePath = WScript.Arguments.Item(1), destinationPath = WScript.Arguments.Item(2);
    statusPath = WScript.Arguments.Item(3);
    var hta = readUtf8(htaPath), script = /<SCRIPT type="text\/javascript">([\s\S]*?)<\/SCRIPT>/i.exec(hta);
    if (!script) WScript.Quit(3);
    var document = {}, location = { pathname: htaPath };
    eval(script[1]);
    var source = createXmlDocument();
    configureXPath(source);
    if (!source.load(sourcePath) || source.parseError.errorCode) WScript.Quit(4);
    saveCheckedXML(source, destinationPath);
    writeStatus(statusPath, 0, "saved");
    WScript.Quit(0);
  } catch (e) {
    var errorText = e.message || e.description || String(e);
    if (statusPath) writeStatus(statusPath, 1, "error: " + errorText);
    WScript.Quit(1);
  }
}());
