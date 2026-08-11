function readUtf8(path) { var s=new ActiveXObject("ADODB.Stream"); s.Type=2; s.Charset="utf-8"; s.Open(); s.LoadFromFile(path); var t=s.ReadText(); s.Close(); return t; }
if (WScript.Arguments.Count() !== 4) WScript.Quit(2);
this.__FB2RECODE_LIBRARY__=true;
eval(readUtf8(WScript.Arguments(0)));
var files=[WScript.Arguments(1),WScript.Arguments(2),WScript.Arguments(3)], calls=0;
var result=FB2Recode.run(files,{encoding:"windows-1251",backup:false,overwrite:false,force:false,recursive:false,dryRun:false,shouldCancel:function(){return calls++>=1;}});
if (!result.cancelled || result.stats.found!==3 || result.stats.converted!==1 || result.stats.skipped!==2 || result.stats.errors!==0) WScript.Quit(3);
WScript.Echo("cancelled");
