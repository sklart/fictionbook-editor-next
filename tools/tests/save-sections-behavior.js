/* Executes the shipped HTA saveCheckedXML function under WSH.  This is a
   regression harness, not a second implementation of safe replacement. */
(function () {
  function readUtf8(path) {
    var stream = new ActiveXObject("ADODB.Stream");
    stream.Type = 2; stream.Charset = "utf-8"; stream.Open();
    stream.LoadFromFile(path); var text = stream.ReadText(); stream.Close(); return text;
  }
  try {
    if (WScript.Arguments.Count() !== 3) WScript.Quit(2);
    var htaPath = WScript.Arguments.Item(0), sourcePath = WScript.Arguments.Item(1), destinationPath = WScript.Arguments.Item(2);
    var hta = readUtf8(htaPath), script = /<SCRIPT type="text\/javascript">([\s\S]*?)<\/SCRIPT>/i.exec(hta);
    if (!script) WScript.Quit(3);
    var document = {}, location = { pathname: htaPath };
    eval(script[1]);
    var source = createXmlDocument();
    configureXPath(source);
    if (!source.load(sourcePath) || source.parseError.errorCode) WScript.Quit(4);
    saveCheckedXML(source, destinationPath);
    WScript.Echo("saved");
  } catch (e) {
    WScript.Echo("error: " + (e.message || e));
    WScript.Quit(1);
  }
}());
