(function() {
try {
 function assert(ok, message) { if(!ok) throw new Error(message); }
 function snapshot(form) {
  var tags=['INPUT','SELECT','TEXTAREA'], result=[];
  for(var t=0;t<tags.length;t++) {
   var controls=form.getElementsByTagName(tags[t]);
   for(var i=0;i<controls.length;i++) result.push(controls[i].id+'='+controls[i].value);
  }
  return result.join('\n');
 }
 function serialize() {
  var xml=new ActiveXObject('Msxml2.DOMDocument.6.0'); xml.async=false;
  xml.loadXML('<FictionBook xmlns="'+fbNS+'" xmlns:l="'+xlNS+'"/>');
  GetDesc(xml,null,null); return String(xml.xml);
 }
 function color(value) {
  value=String(value).replace(/\s/g,'').toLowerCase();
  var rgb=/^rgb\((\d+),(\d+),(\d+)\)$/.exec(value);
  if(rgb) { value='#'; for(var i=1;i<=3;i++) {var hex=Number(rgb[i]).toString(16);value+=(hex.length<2?'0':'')+hex;} }
  return value;
 }
 var fixtures=['minimal','full','multiple','binary','binary-geometry','missing-optional','unicode','invalid-ish','stress-100','stress-500'], timings=[];
 for(var i=0;i<fixtures.length;i++) {
  var xml=new ActiveXObject('Msxml2.DOMDocument.6.0');xml.async=false;
  assert(xml.load(window.external.GetStylePath()+'\\description-fixtures\\'+fixtures[i]+'.fb2'),'Load '+fixtures[i]);
  var started=(new Date()).getTime(); assert(LoadFromDOM(xml,'english'),'Transform '+fixtures[i]);
  var form=document.getElementById('fbw_desc'), title=document.getElementById('tiTitle'), author=document.getElementById('tiAuthor');
  assert(form && title && author && document.getElementById('tiGenre'),'Legacy fields exist');
  assert(document.getElementById('tiGenre').sourceIndex < author.sourceIndex && author.sourceIndex < title.sourceIndex,'Legacy order');
  assert(!form._descriptionSections && !document.getElementById('descriptionNavigation') && !document.getElementById('descriptionCoverPreview'),'No experimental controls');
  assert(!form.className || form.className.indexOf('description-')<0,'No experimental layout class');
  if(fixtures[i]=='unicode') assert(document.getElementById('tiLang').value=='zh-Hant' && document.getElementById('tiSrcLang').value=='de-DE','Extended language values');
  title.value='Edited metadata';
  var before=snapshot(form), saved=serialize(), originalTitle=title;
  var layouts=['modern','classic','unknown'];
  for(var j=0;j<layouts.length;j++) {
   assert(apiSetDescriptionLayout(layouts[j]),'Legacy layout bridge');
   assert(!form._descriptionSections && document.getElementById('tiTitle')===originalTitle,'No layout switch');
   assert(snapshot(form)==before && serialize()==saved,'Metadata preserved after old layout request');
  }
  var expected=form.getAttribute('data-description-theme-background');
  if(expected) {
   assert(color(form.currentStyle.backgroundColor)==color(expected),'Theme background');
   assert(color(title.currentStyle.color)==color(form.getAttribute('data-description-theme-foreground')),'Theme input text');
  }
  timings.push(fixtures[i]+': load_and_checks_ms='+((new Date()).getTime()-started));
 }
 document.documentElement.setAttribute('data-description-timings',timings.join('\n'));
 document.documentElement.setAttribute('data-description-regression','PASS');
} catch(error) {document.documentElement.setAttribute('data-description-regression','FAIL: '+error.message);}
})();
