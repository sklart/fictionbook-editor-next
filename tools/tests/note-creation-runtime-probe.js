// candidates contains the actual bundled Run functions, with only dialog/GoTo boundaries observed.
var fixtures=[], responses=[], messages=[], focusTarget=null, promptCount=0, current=null;
function MsgBox(text) { messages.push(String(text)); }
function prompt(text, value) { promptCount++; if (!responses.length) throw new Error("unexpected-prompt"); return responses.shift(); }
function GoTo(node) { focusTarget=node; window.GoTo(node); }
var root=document.getElementById("fbw_body"), before="", after="", foreign=null, foreignHTML="", foreignMarker=null, foreignMarkerHTML="", originalRange=null;
for (var c=0; c<candidates.length; c++) {
 var modes=["ordinary","existing","missing-body","bad-link","nested"];
 if (candidates[c].variant<=3) modes.push("middle");
 else modes.push("last-refusal");
 if (candidates[c].variant==2 || candidates[c].variant==5) modes.push("cancel","retry-cancel");
 if (candidates[c].variant==2) modes.push("fault","fault-before-body","empty-body","id-collision","fault-middle","selected","selected-fault");
 for (var m=0; m<modes.length; m++) fixtures.push({candidate:candidates[c], name:candidates[c].name+"-"+modes[m], mode:modes[m]});
}
function localId(link) { var href=link.getAttribute("href"); return href.substring(href.lastIndexOf("#")+1); }
function noMarkers() {
 var nodes=root.getElementsByTagName("A");
 for (var i=0; i<nodes.length; i++) if (nodes[i].getAttribute("data-fbe-note-create")) return false;
 return true;
}
function preserved() { return document.getElementById("foreign")===foreign && foreign.outerHTML===foreignHTML &&
 document.getElementById("fbe-note-create-1")===foreignMarker && foreignMarker.outerHTML===foreignMarkerHTML; }
function validPairs() {
 var prefix=current.candidate.kind=="notes" ? "n_" : "c_", links=root.getElementsByTagName("A"), count=0, targets=[];
 for (var i=0; i<links.length; i++) if (localId(links[i]).indexOf(prefix)==0) {
  var section=document.getElementById(localId(links[i]));
  if (!section || section.className!="section" || section.parentNode.getAttribute("fbname")!==current.candidate.kind) return false;
  var number=parseInt(localId(links[i]).substring(prefix.length),10);
  if (links[i].innerHTML!==(current.candidate.kind=="notes" ? "["+number+"]" : "<SUP>{"+number+"}</SUP>")) return false;
  for (var j=0; j<targets.length; j++) if (targets[j]===section) return false;
  targets.push(section);
  count++;
 }
 return count==(current.mode=="existing" || current.mode=="middle" ? 3 : 1);
}
window.__notesCount=function () { window.external.SetStatusBarText(String(fixtures.length)); };
window.__notesSetup=function (index) {
 current=fixtures[index]; messages=[]; focusTarget=null; promptCount=0;
 root.id="fbw_body";
 root.removeAttribute("data-undo-sentinel");
 root.innerHTML='<DIV class="body"><DIV class="section"><P id="foreign" title="keep"><A href="#foreign">Sclex_Note</A><EM>keep</EM></P><P id="fbe-note-create-1" title="keep-marker-name">other</P><P id="caret">before ab after</P></DIV></DIV>';
 var prefix=current.candidate.kind=="notes" ? "n_" : "c_";
 var klass=current.candidate.kind=="notes" ? ' class="note"' : "";
 var existing=current.mode=="existing" || current.mode=="middle" || current.mode=="last-refusal" || current.mode=="fault-middle";
 if (existing || current.mode=="bad-link" || current.mode=="nested") {
  var second=existing ? '<DIV class="section" id="'+prefix+'2"><DIV class="title"><P>2</P></DIV><P>second</P></DIV>' : "";
  root.insertAdjacentHTML("beforeEnd", '<DIV class="body" fbname="'+current.candidate.kind+'"><DIV class="title"><P>Existing</P></DIV><DIV class="section" id="'+prefix+'1"><DIV class="title"><P>1</P></DIV><P>first</P>'+(current.mode=="nested" ? '<DIV class="section" id="nested"><P>nested</P></DIV>' : '')+'</DIV>'+second+'</DIV>');
  if (existing) document.getElementById("caret").innerHTML='<A'+klass+' href="#'+prefix+'1">'+(current.candidate.kind=="notes" ? "[1]" : "<SUP>{1}</SUP>")+'</A> before <EM id="point">ab</EM> after <A'+klass+' href="#'+prefix+'2">'+(current.candidate.kind=="notes" ? "[2]" : "<SUP>{2}</SUP>")+'</A> tail';
  if (current.mode=="bad-link") document.getElementById("caret").innerHTML='<A'+klass+' href="#'+prefix+'missing">broken</A> ab';
 }
 if (current.mode=="empty-body") root.insertAdjacentHTML("beforeEnd",'<DIV class="body" fbname="'+current.candidate.kind+'"></DIV>');
 if (current.mode=="id-collision") root.insertAdjacentHTML("beforeEnd",'<P id="'+prefix+'1" title="foreign-id">keep</P>');
 var p=document.getElementById("caret"), range=document.body.createTextRange();
 range.moveToElementText(current.mode=="middle" || current.mode=="last-refusal" || current.mode=="fault-middle" ? document.getElementById("point") : p);
 if (current.mode=="selected" || current.mode=="selected-fault") range.findText("ab");
 else range.collapse(current.mode=="ordinary" || current.mode=="missing-body");
 if (current.mode=="ordinary") range.move("character",3);
 p.scrollIntoView(true); range.select(); originalRange=document.selection.createRange().duplicate();
 responses=current.mode=="cancel" ? [null] : current.mode=="retry-cancel" ? ["<b>bad",null] : ["text <b>bold</b><br>next"];
 foreign=document.getElementById("foreign"); foreignHTML=foreign.outerHTML;
 foreignMarker=document.getElementById("fbe-note-create-1"); foreignMarkerHTML=foreignMarker.outerHTML;
 if (current.mode=="missing-body") root.removeAttribute("id");
 before=root.outerHTML; window.__notesBefore=before;
 window.external.SetStatusBarText(current.mode=="cancel" || current.mode=="retry-cancel" || current.mode=="missing-body" || current.mode=="bad-link" || current.mode=="nested" || current.mode=="last-refusal" || current.mode=="id-collision" ? "refusal" : "change");
};
window.__notesRun=function () {
 var error=null, injected=false;
 var host=current.mode=="fault-middle" ? document.getElementById((current.candidate.kind=="notes" ? "n_" : "c_")+"1").parentNode : root;
 var method=current.mode=="fault-middle" ? "insertBefore" : "appendChild", append=host[method];
 var fault=current.mode=="fault" || current.mode=="fault-before-body" || current.mode=="fault-middle" || current.mode=="selected-fault";
 if (fault) host[method]=function (node, reference) {
  injected=true;
  if (current.mode=="fault") append.call(host,node);
  if (current.mode=="fault-middle") append.call(host,node,reference);
  throw new Error("notes-injected-failure");
 };
 try { current.candidate.run(); } catch (e) { error=e; }
 finally { if (fault) host[method]=append; }
 after=root.outerHTML; window.__notesAfter=after;
 var refusal=current.mode=="cancel" || current.mode=="retry-cancel" || current.mode=="missing-body" || current.mode=="bad-link" || current.mode=="nested" || current.mode=="last-refusal" || current.mode=="id-collision";
 var ok=noMarkers() && preserved();
 if (refusal) {
  var selection=document.selection.createRange();
  ok=ok && !error && before==after && originalRange.compareEndPoints("StartToStart",selection)==0 && originalRange.compareEndPoints("EndToEnd",selection)==0;
  if (current.mode=="cancel" || current.mode=="retry-cancel") ok=ok && promptCount==(current.mode=="cancel" ? 1 : 2);
  else ok=ok && promptCount==0 && messages.length>0;
 } else if (current.mode=="selected-fault") {
  ok=ok && injected && error && error.message=="notes-injected-failure" && before!=after &&
   !document.getElementById((current.candidate.kind=="notes" ? "n_" : "c_")+"1") &&
   document.getElementById("caret").innerText.indexOf("ab")<0;
 } else if (current.mode=="fault-before-body" || current.mode=="fault-middle") {
  ok=ok && injected && error && error.message=="notes-injected-failure" && before==after;
 } else {
  ok=ok && validPairs() && before!=after;
  if (current.mode=="fault") ok=ok && injected && error && error.message=="notes-injected-failure";
  else {
   ok=ok && !error && (current.candidate.variant==3 || current.candidate.variant==6 ? !!focusTarget : !focusTarget);
   var input=current.candidate.variant==2 || current.candidate.variant==5;
   ok=ok && promptCount==(input ? 1 : 0);
   var number=current.mode=="existing" ? 3 : current.mode=="middle" ? 2 : 1;
   var section=document.getElementById((current.candidate.kind=="notes" ? "n_" : "c_")+number);
   if (input) ok=ok && section.innerHTML.indexOf('<STRONG>bold</STRONG>')>=0 && section.innerText.indexOf('next')>=0;
   if (current.mode=="empty-body") ok=ok && section.parentNode.firstChild.className=="title";
   if (current.mode=="selected") ok=ok && document.getElementById("caret").innerText.indexOf("ab")<0 &&
    document.getElementById("caret").innerText.indexOf("before")==0 && document.getElementById("caret").innerText.indexOf("after")>0;
  }
 }
 window.__notesError=error ? String(error.message || error.description || error) : "";
 if (!ok && !error) window.__notesError="sameHTML:"+(before==after)+" preserved:"+preserved()+" markers:"+noMarkers()+" messages:"+messages.length+" prompts:"+promptCount;
 window.external.SetStatusBarText(ok ? "1" : "0");
};
window.__notesSeedHistory=function () {
 window.external.BeginUndoUnit(document,"fixture-sentinel");
 try { root.setAttribute("data-undo-sentinel","yes"); }
 finally { window.external.EndUndoUnit(document); }
 before=root.outerHTML; window.__notesBefore=before;
};
window.__notesSnapshot=function (which) { window.external.SetStatusBarText(root.outerHTML===(which=="before" ? before : after) ? "1" : "0"); };
window.__notesName=function () { window.external.SetStatusBarText(current.name); };
window.__notesDetails=function () { window.external.SetStatusBarText(encodeURIComponent(window.__notesError || "")); };
