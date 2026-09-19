/* Executes the shipped section splitter against MSXML; it intentionally calls
   saveOneSectionAsSeparateDocument instead of reproducing its transformation. */
(function () {
  function text(path) { var s=new ActiveXObject("ADODB.Stream"); s.Type=2; s.Charset="utf-8"; s.Open(); s.LoadFromFile(path); var v=s.ReadText(); s.Close(); return v; }
  function status(path, value) { var s=new ActiveXObject("ADODB.Stream"); s.Type=2; s.Charset="utf-8"; s.Open(); s.WriteText(value); s.SaveToFile(path,2); s.Close(); }
  try {
    if (WScript.Arguments.Count()!=4) WScript.Quit(2);
    var htaPath=WScript.Arguments.Item(0), sourcePath=WScript.Arguments.Item(1), outputFolder=WScript.Arguments.Item(2), statusPath=WScript.Arguments.Item(3);
    var controls={ radio1:{checked:false},radio2:{checked:false},radio3:{checked:true},radio4:{checked:false},checkbox1:{checked:false},filenameTemplate:{value:"{number}_{author}_{title}"},unicodeNames:{checked:true},outputFolder:{value:outputFolder},filenamePreview:{value:""} };
    var document={getElementById:function(id){return controls[id]||null;}};
    var location={pathname:htaPath}; var alert=function(){};
    var match=/<SCRIPT type="text\/javascript">([\s\S]*?)<\/SCRIPT>/i.exec(text(htaPath)); if(!match) WScript.Quit(3); eval(match[1]);
    xmlDoc=createXmlDocument(); configureXPath(xmlDoc);
    if(!xmlDoc.load(sourcePath)||xmlDoc.parseError.errorCode) throw new Error("fixture parse failed");
    savedFictionBook=xmlDoc.selectSingleNode("/fb:FictionBook").cloneNode(true); openedFileName=sourcePath;
    selectedElements={}; myLinks={4:0,5:1};
    updateFilenamePreview();
    if(controls.filenamePreview.value.indexOf("Container")<0) throw new Error("filename preview does not use the first available section without selection");
    selectedElements={5:5}; myLinks={5:1};
    controls.radio3.checked=false; controls.radio1.checked=true;
    updateFilenamePreview();
    var previewName=controls.filenamePreview.value;
    if(!previewName) throw new Error("filename preview is empty for selected section");
    /* Select the nested section.  The utility deliberately unwraps a selected
       parent when it contains subsections, so selecting the child also keeps
       the resulting FB2 structurally valid. */
    saveOneSectionAsSeparateDocument(1,1);
    var fso=new ActiveXObject("Scripting.FileSystemObject"), files=new Enumerator(fso.GetFolder(outputFolder).Files), output="";
    for(;!files.atEnd();files.moveNext()) if(files.item().Path!=sourcePath && /\.fb2$/i.test(files.item().Name)) output=files.item().Path;
    if(!output) throw new Error("no split output");
    if(previewName!=output) throw new Error("filename preview does not match saved filename");
    var result=createXmlDocument(); configureXPath(result); if(!result.load(output)||result.parseError.errorCode) throw new Error("saved result parse failed");
    if(!result.selectSingleNode("//fb:binary[@id='pic']")||!result.selectSingleNode("//fb:body[@name='notes']//fb:section[@id='n1']")||!result.selectSingleNode("//fb:body[@name='comments']//fb:section[@id='c1']")) throw new Error("linked resources were not retained");
    if(result.selectSingleNode("//fb:body[@name='notes']//fb:section[@id='unused']")||result.selectSingleNode("//fb:body[@name='comments']//fb:section[@id='unusedc']")) throw new Error("unlinked note/comment was retained");

    /* Exercise the public batch path as well: it resolves display rows through
       myLinks and must continue with the next section after one save fails. */
    xmlDoc=createXmlDocument(); configureXPath(xmlDoc);
    if(!xmlDoc.load(sourcePath)||xmlDoc.parseError.errorCode) throw new Error("fixture reload failed");
    savedFictionBook=xmlDoc.selectSingleNode("/fb:FictionBook").cloneNode(true);
    controls.radio1.checked=false; controls.radio3.checked=true;
    selectedElements={7:7,8:8};
    myLinks={7:0,8:1};
    var realSaveOne=saveOneSectionAsSeparateDocument, calls=[], messages=[];
    fso.DeleteFile(output,true);
    alert=function(message) { messages.push(message); };
    saveOneSectionAsSeparateDocument=function(titleNum, fileCnt) {
      calls.push(titleNum);
      if(titleNum==0) throw new Error("injected first-section failure");
      return realSaveOne(titleNum,fileCnt);
    };
    saveSectionsAsSeparateDocuments();
    saveOneSectionAsSeparateDocument=realSaveOne;
    files=new Enumerator(fso.GetFolder(outputFolder).Files); output="";
    for(;!files.atEnd();files.moveNext()) if(files.item().Path!=sourcePath && /\.fb2$/i.test(files.item().Name)) output=files.item().Path;
    if(calls.length!=2||calls[0]!=0||calls[1]!=1||!output) throw new Error("myLinks batch selection did not continue after an error");
    if(messages.length!=2) throw new Error("batch error report is missing");
    status(statusPath,"ok");
  } catch(e) { status(statusPath,"error: "+(e.message||e.description||e)); WScript.Quit(1); }
}());
