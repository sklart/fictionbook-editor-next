(function() {
try {
function assert(value, message) { if(!value) throw new Error(message); }
var pluralCounts=[1,2,3,4,5,11,12,14,21,22,25,101,102,111,112];
var pluralExpected=["файл","файла","файла","файла","файлов","файлов","файлов","файлов","файл","файла","файлов","файл","файла","файлов","файлов"];
for(var pluralIndex=0;pluralIndex<pluralCounts.length;pluralIndex++)
 assert(DescriptionFileCount(pluralCounts[pluralIndex],"файл|файла|файлов|slavic")==pluralCounts[pluralIndex]+" "+pluralExpected[pluralIndex],"Russian attachment plural "+pluralCounts[pluralIndex]);
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
  assert(color(title.currentStyle.backgroundColor)==color(form.getAttribute("data-description-theme-input-background")),"Input theme background: "+title.currentStyle.backgroundColor);
  assert(color(title.currentStyle.color)==color(expectedForeground),"Input theme foreground: "+title.currentStyle.color+" / "+expectedForeground);
  var legends=form.getElementsByTagName("LEGEND");
  for(var legendIndex=0;legendIndex<legends.length;legendIndex++)
   assert(color(legends[legendIndex].currentStyle.color)==color(expectedForeground),"Section title theme foreground");
 }
}
checkTheme();
var serializedBefore=serialize();
if(fixtures[fixtureIndex]=="full" || fixtures[fixtureIndex]=="missing-optional" || fixtures[fixtureIndex]=="minimal") {
 var toggleLayouts=["modern","classic"];
 for(var tl=0;tl<toggleLayouts.length;tl++) {
  apiSetDescriptionLayout(toggleLayouts[tl]);
  for(var ts=0;ts<form._descriptionSections.length;ts++) {
   var toggleSection=form._descriptionSections[ts], toggleButton=toggleSection._descriptionButton;
   assert(toggleButton && toggleSection._descriptionBody && toggleButton._descriptionSection==toggleSection,"Section has stable toggle target");
   if(toggleSection._descriptionBody.style.display!="none") toggleButton.click();
   for(var tt=0;tt<4;tt++) {
    toggleButton.click();
    assert(toggleSection._descriptionBody.style.display==(tt%2==0 ? "block" : "none"),"Section toggle display in "+toggleLayouts[tl]+": "+toggleSection._descriptionSectionId);
    assert(snapshot()==before && serialize()==serializedBefore,"Toggle preserves metadata and binary payloads");
   }
   toggleButton.click();
  }
 }
 apiSetDescriptionLayout("modern");
 for(var reset=0;reset<form._descriptionSections.length;reset++) {
  form._descriptionSections[reset]._descriptionClosed=reset!=0 && reset!=2;
  form._descriptionSections[reset]._descriptionClassicClosed=false;
  DescriptionShowSection(form._descriptionSections[reset]);
 }
}
if(fixtures[fixtureIndex]=="full") {
 var languages=["tiLang","tiSrcLang","stiLang","stiSrcLang"];
 for(var languageIndex=0;languageIndex<languages.length;languageIndex++) {
  var language=document.getElementById(languages[languageIndex]);
  assert(language && language.getElementsByTagName("OPTION").length>50,"Language selector retains catalog: "+languages[languageIndex]);
  assert(language.getElementsByTagName("OPTGROUP").length==0,"MSHTML language options do not retain grouped selection painting");
  var previousLanguage=language.value; language.value="en";
  assert(ReadLanguageValue(languages[languageIndex])=="en","Language selector preserves value: "+languages[languageIndex]);
  language.value=previousLanguage;
 }
 assert(document.getElementById("diID").readOnly && !document.getElementById("diID").disabled,"Document ID is readable information");
 assert(document.getElementById("tiGenre").getElementsByTagName("INPUT")[0].readOnly,"Genre is a read-only selector");
 assert(document.getElementById("tiCover")==form.all.tiCover,"Modern uses one cover control");
 var actionButtons=form.getElementsByTagName("BUTTON"), iconActions=0;
 for(var actionIndex=0;actionIndex<actionButtons.length;actionIndex++) {
  var actionButton=actionButtons[actionIndex];
  if(actionButton.className.indexOf("description-icon-action")>=0) {
   assert(actionButton.title && actionButton.getAttribute("aria-label")==actionButton.title,"Icon actions have localized tooltip and accessible name");
   assert(actionButton.innerText.length<=2,"Repeated action is a compact icon");
   iconActions++;
  }
 }
 assert(iconActions>5,"Full fixture includes compact repeated actions");
 assert(document.getElementById("descriptionCoverPreview").currentStyle.borderTopStyle!="none","Cover preview has a border");
 assert(document.getElementById("fullImg").currentStyle.borderTopStyle!="none","Full preview has a border");
 var originalParent=form.parentNode, originalNext=form.nextSibling, originalDisplay=form.style.display;
 var viewport=document.createElement("DIV");originalParent.appendChild(viewport);viewport.appendChild(form);form.style.display="block";
 var narrowLegend=form._descriptionSections[1].getElementsByTagName("LEGEND")[0], longHeading=document.createTextNode(" Source title information (fill in using the original language of the book)");narrowLegend.appendChild(longHeading);
 var closedSections=[];
 for(var sectionIndex=0;sectionIndex<form._descriptionSections.length;sectionIndex++) {
  closedSections.push(form._descriptionSections[sectionIndex]._descriptionClosed);
  form._descriptionSections[sectionIndex]._descriptionClosed=false;
 }
 var widths=[320,480,640,960], layouts=["classic","modern"];
 for(var fast=0;fast<2;fast++) {
 apiSetFastMode(fast!=0);
 for(var layoutIndex=0;layoutIndex<layouts.length;layoutIndex++) {
  apiSetDescriptionLayout(layouts[layoutIndex]);
  for(var widthIndex=0;widthIndex<widths.length;widthIndex++) {
   viewport.style.width=widths[widthIndex]+"px";DescriptionResize();
   assert(title.offsetWidth>0,"Narrow geometry must use a visible rendered form");
   if(widths[widthIndex]==960) {
    var firstField=form.getElementsByTagName("SPAN");
    for(var fieldIndex=0;fieldIndex<firstField.length;fieldIndex++)if(firstField[fieldIndex].className=="description-field")break;
    var fieldLabel=firstField[fieldIndex].getElementsByTagName("LABEL")[0], fieldInput=firstField[fieldIndex].getElementsByTagName("INPUT")[0];
    var labelRect=fieldLabel.getBoundingClientRect(), inputRect=fieldInput.getBoundingClientRect();
    assert(labelRect.bottom>inputRect.top && inputRect.bottom>labelRect.top,"Wide author label and input share a row");
    assert(Math.abs(document.getElementById("tiDate").getBoundingClientRect().top-document.getElementById("tiDateVal").getBoundingClientRect().top)<=4,"Book date values share a row");
    var person=document.getElementById("tiAuthor").getElementsByTagName("DIV")[0];
    var firstRect=person.all.first.getBoundingClientRect(), nickRect=person.all.nick.getBoundingClientRect();
    assert(Math.abs(firstRect.left-nickRect.left)<=4,"Author and contact rows align in "+layouts[layoutIndex]+" fast="+fast+": "+firstRect.left+"/"+nickRect.left);
    assert(Math.abs(person.all.middle.getBoundingClientRect().left-person.all.email.getBoundingClientRect().left)<=4,"Middle name and email columns align");
    assert(Math.abs(person.all.last.getBoundingClientRect().left-person.all.home.getBoundingClientRect().left)<=4,"Last name and website columns align");
    if(layouts[layoutIndex]=="classic") {
     var keywordLeft=document.getElementById("tiKwd").getBoundingClientRect().left;
     var dateLeft=document.getElementById("tiDate").getBoundingClientRect().left;
     assert(Math.abs(title.getBoundingClientRect().left-keywordLeft)<=5 && Math.abs(keywordLeft-dateLeft)<=5,"Classic title, keywords and date fields align");
    } else {
     var coverBox=document.getElementById("tiCover"), coverButtons=coverBox.getElementsByTagName("BUTTON"), coverAction=null;
     for(var cb=0;cb<coverButtons.length;cb++)if(coverButtons[cb].className=="description-add-action")coverAction=coverButtons[cb];
     assert(coverAction && coverAction.getBoundingClientRect().right<=coverBox.getBoundingClientRect().right+1,"Cover action stays inside its column");
     var binaryRows=document.getElementById("binobj").getElementsByTagName("DIV");
     if(binaryRows.length>=2) {
      var columns=["id","type","save","del"];
      for(var ci=0;ci<columns.length;ci++)
       assert(Math.abs(GetBinaryInput(binaryRows[0],columns[ci]).getBoundingClientRect().left-GetBinaryInput(binaryRows[1],columns[ci]).getBoundingClientRect().left)<=3,"Binary column alignment: "+columns[ci]);
     }
    }
   }
   var edge=viewport.getBoundingClientRect().right, overflow=[], descendants=form.getElementsByTagName("*");
   for(var narrowIndex=0;narrowIndex<descendants.length;narrowIndex++) {
    var element=descendants[narrowIndex], rect=element.getBoundingClientRect();
    if(rect.right>edge+1 && rect.bottom>rect.top && element.currentStyle.visibility!="hidden")overflow.push(element.tagName+"#"+element.id+":"+Math.round(rect.right-edge));
   }
   // IE7 does not include overflowing LEGENDs in scrollWidth: check both.
   assert(!overflow.length && viewport.scrollWidth<=viewport.clientWidth+1,"Description overflow "+layouts[layoutIndex]+" width="+widths[widthIndex]+" fast="+fast+": "+overflow.slice(0,12).join(", "));
   assert(snapshot()==before && serialize()==serializedBefore,"Resizing preserves fields and serialization");
  }
 }
 }
 apiSetFastMode(false);narrowLegend.removeChild(longHeading);
 for(var sectionIndex=0;sectionIndex<form._descriptionSections.length;sectionIndex++)form._descriptionSections[sectionIndex]._descriptionClosed=closedSections[sectionIndex];
 originalParent.insertBefore(form,originalNext);originalParent.removeChild(viewport);form.style.display=originalDisplay;apiSetDescriptionLayout("modern");
}
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
 assert(originalRow.getElementsByTagName("IMG")[0].currentStyle.cursor!="move","Thumbnail is not a draggable body image");
 var rawSize=GetBinaryInput(originalRow,"size"), rawDims=GetBinaryInput(originalRow,"dims");
 assert(rawSize && /^\d+$/.test(rawSize.value),"Legacy size.value remains raw bytes");
 assert(!rawDims || /^\d+x\d+$/.test(rawDims.value),"Legacy dims.value remains unchanged");
 assert(originalRow.all.size.value==rawSize.value && parseInt(originalRow.all.size.value,10)==Number(rawSize.value),"Bundled statistics/image-quality size access remains numeric");
 assert(!rawDims || originalRow.all.dims.value==rawDims.value,"Bundled statistics/image-quality dimensions access remains unchanged");
 var binarySpans=originalRow.getElementsByTagName("SPAN"), foundSize=false;
 for(var bs=0;bs<binarySpans.length;bs++)if(binarySpans[bs].className=="binary-size-display")foundSize=binarySpans[bs].innerText==DescriptionBinarySize(rawSize.value);
 assert(foundSize && rawSize.style.display=="none","Human-readable size is presentation only");
 assert(DescriptionBinarySize(73386).indexOf("71.7")>=0 || DescriptionBinarySize(73386).indexOf("71,7")>=0,"Binary size uses 1024-byte units");
 var binaryButtons=originalRow.getElementsByTagName("BUTTON"), binaryOrder=[];
 for(var bi=0;bi<binaryButtons.length;bi++) {
  binaryOrder.push(binaryButtons[bi].id);
  assert(binaryButtons[bi].getElementsByTagName("IMG").length==1 && binaryButtons[bi].title,"Binary action has image and tooltip");
 }
 assert(binaryOrder.join(",")=="show,save,del","Binary actions are view, save, remove");
 assert(document.getElementById("descriptionAttachmentSummary").parentNode.tagName=="LEGEND","Summary remains in section header when collapsed");
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
 ShowFullImage("fbw-internal:#"+originalId);
 assert(document.getElementById("fullImgPanel").style.visibility=="visible","Full attachment preview opens");
 HideFullImage();
 assert(document.getElementById("fullImgPanel").style.display=="none","Closing full preview removes it from document layout");
}
var id=document.getElementById("diID"), savedId=id.value;
id.value="";var invalidBefore=snapshot();id.fireEvent("onfocusout");
assert(snapshot()==invalidBefore && id.value=="","Validation must not repair data");
assert(id.className.indexOf("description-invalid")>=0,"Empty ID warning");
id.value=savedId;id.fireEvent("onfocusout");
assert(id.className.indexOf("description-invalid")<0,"Leaving a repaired field clears advisory validation in legacy MSHTML");
assert(snapshot()==before,"Validation preserves all other fields");
timings.push(fixtures[fixtureIndex]+": load_ms="+loadMs+", switch_validation_serialization_ms="+((new Date()).getTime()-switchStart));
}
document.documentElement.setAttribute("data-description-timings",timings.join("\n"));
document.documentElement.setAttribute("data-description-regression", "PASS");
} catch(error) { document.documentElement.setAttribute("data-description-regression", "FAIL: " + error.message); }
})();
