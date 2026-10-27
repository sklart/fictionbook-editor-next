(function() {
try {
function assert(value, message) { if(!value) throw new Error(message); }
assert(IsDescriptionNewTemplate(window.external.GetStylePath()+"\\blank.fb2"),"Actual New template is recognized");
assert(!IsDescriptionNewTemplate(window.external.GetStylePath()+"\\books\\blank.fb2"),"Ordinary book named blank.fb2 is preserved");
assert(!IsDescriptionNewTemplate(window.external.GetStylePath()+"\\books\\my-blank.fb2"),"Template detection does not use a filename substring");
var fixtures=["minimal","full","multiple","binary","missing-optional","unicode","invalid-ish","stress-100","stress-500"], timings=[];
for(var fixtureIndex=0; fixtureIndex<fixtures.length; fixtureIndex++) {
var input=new ActiveXObject("Msxml2.DOMDocument.6.0");input.async=false;
assert(input.load(window.external.GetStylePath()+"\\description-fixtures\\"+fixtures[fixtureIndex]+".fb2"),"Load fixture");
var loadStart=(new Date()).getTime();
assert(LoadFromDOM(input,"english"),"Production fixture load");
var loadMs=(new Date()).getTime()-loadStart;
var form=document.getElementById("fbw_desc");
if(fixtures[fixtureIndex]=="unicode") {
 assert(document.getElementById("tiLang").value=="zh-Hant","Extended book language survives catalog population");
 assert(document.getElementById("tiSrcLang").value=="de-DE","Extended source language survives catalog population");
}
document.getElementById("tiTitle").value="Edited metadata";
document.getElementById("diID").value="unchanged-document-id";
document.getElementById("stylesheetId").value="p { color: green; }";
function snapshot() {
 var tags=["INPUT","SELECT","TEXTAREA"], result=[];
 for(var t=0;t<tags.length;t++) {
 var controls=form.getElementsByTagName(tags[t]);
 for(var i=0;i<controls.length;i++)result.push(controls[i].id+"="+controls[i].value);
 }
 result.sort(); return result.join("\n")+"\n"+document.getElementById("stylesheetId").value;
}
function serialize() {
 var output=new ActiveXObject("Msxml2.DOMDocument.6.0");output.async=false;
 output.loadXML('<FictionBook xmlns="'+fbNS+'" xmlns:l="'+xlNS+'"/>');
 GetDesc(output,null,null);GetBinaries(output);return String(output.xml);
}
var before=snapshot(), title=document.getElementById("tiTitle"), author=document.getElementById("tiAuthor");
function checkTheme() {
 function color(value) {
  value=String(value).replace(/\s/g, "").toLowerCase();
  var rgb=/^rgb\((\d+),(\d+),(\d+)\)$/.exec(value);
  if(rgb) { value="#";for(var component=1;component<=3;component++){var hex=Number(rgb[component]).toString(16);value+=(hex.length<2?"0":"")+hex;} }
  return value;
 }
 var expectedBackground=form.getAttribute("data-description-theme-background"), expectedForeground=form.getAttribute("data-description-theme-foreground");
 assert(expectedBackground && expectedForeground,"Native theme fixture must be installed");
 if(expectedBackground) {
  assert(color(form.currentStyle.backgroundColor)==color(expectedBackground),"Description theme background: "+form.currentStyle.backgroundColor+" / "+expectedBackground);
  assert(color(title.currentStyle.backgroundColor)==color(expectedBackground),"Input theme background: "+title.currentStyle.backgroundColor+" / "+expectedBackground);
  assert(color(title.currentStyle.color)==color(expectedForeground),"Input theme foreground: "+title.currentStyle.color+" / "+expectedForeground);
 }
}
checkTheme();
var serializedBefore=serialize();
assert(form._descriptionSections.length==7,"Seven Modern sections");
assert(form._descriptionSections[0]._descriptionClosed===false,"Book expanded");
assert(form._descriptionSections[2]._descriptionClosed===false,"Document expanded");
assert(snapshot()==before,"Initializing Modern preserves metadata");
var switchStart=(new Date()).getTime();
for(var repeat=0;repeat<3;repeat++) {
 apiSetDescriptionLayout("classic"); assert(snapshot()==before,"Classic preserves values");
 checkTheme();
 apiSetDescriptionLayout("modern"); assert(snapshot()==before,"Modern preserves values");
 checkTheme();
 assert(serialize()==serializedBefore,"Shared serialization and binary payloads survive switching");
 assert(document.getElementById("tiTitle")==title && document.getElementById("tiAuthor")==author,"Controls retain identity");
}
apiSetDescriptionLayout("invalid");assert(descriptionLayout=="modern","Safe layout fallback");
var buttons=form.getElementsByTagName("BUTTON"), toggles=0;
for(var i=0;i<buttons.length;i++)if(buttons[i].className=="description-section-toggle")toggles++;
assert(toggles==7,"Switching must not duplicate section controls");
if(fixtures[fixtureIndex]=="full") {
 var binaries=document.getElementById("binobj"), rows=binaries.getElementsByTagName("DIV");
 var originalRow=rows[0], originalId=GetBinaryInput(originalRow,"id").value;
 var idInput=GetBinaryInput(originalRow,"id");
 idInput.value="description-renamed-cover.png";idInput.fireEvent("onchange");
 assert(document.getElementById("tiCover").getElementsByTagName("SELECT")[0].value=="#description-renamed-cover.png","Rename updates cover reference");
 idInput.value=originalId;idInput.fireEvent("onchange");
 var count=rows.length;
 var addedId=apiAddBinary("","description-unreferenced.png","image/png",originalRow.base64data);
 assert(rows.length==count+1,"Add binary through production DOM/store bridge");
 binaries._descriptionClosed=false;DescriptionShowSection(binaries);
 var addedRow=rows[rows.length-1];
 assert(addedRow.getElementsByTagName("IMG")[0].getAttribute("src"),"Attachment thumbnail populated");
 var bodyReference=document.createElement("IMG");bodyReference.src="fbw-internal:#"+addedId;
 bodyReference.setAttribute("data-description-preview","true");document.getElementById("fbw_body").appendChild(bodyReference);
 assert(BinaryIsReferenced(addedId),"Body references must remain protected even with a preview-like attribute");
 bodyReference.removeNode(true);
 assert(!BinaryIsReferenced(addedId),"Own thumbnail must not count as a metadata/body reference");
 Remove(addedRow);assert(rows.length==count,"Remove unreferenced binary with a visible thumbnail");
 assert(serialize()==serializedBefore,"Binary add/remove/rename restores original serialized data");
}
var id=document.getElementById("diID"), savedId=id.value;
id.value="";var invalidBefore=snapshot();DescriptionValidate();
assert(snapshot()==invalidBefore && id.value=="","Validation must not repair data");
assert(id.className.indexOf("description-invalid")>=0,"Empty ID warning");
id.value=savedId;DescriptionValidate();
assert(snapshot()==before,"Validation preserves all other fields");
timings.push(fixtures[fixtureIndex]+": load_ms="+loadMs+", switch_validation_serialization_ms="+((new Date()).getTime()-switchStart));
}
document.documentElement.setAttribute("data-description-timings",timings.join("\n"));
document.documentElement.setAttribute("data-description-regression", "PASS");
} catch(error) { document.documentElement.setAttribute("data-description-regression", "FAIL: " + error.message); }
})();
