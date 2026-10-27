// Версия файла - 2.3

// Object reference maker
Object.prototype.$=function $(val){if(val)this.valueOf=this.toSource=this.toString=function(){return val};return val;};

var IDOK     = 1;
var IDCANCEL = 2;
var IDABORT  = 3;
var IDRETRY  = 4;
var IDIGNORE = 5;
var IDYES    = 6;
var IDNO     = 7;

window.onerror = errorHandler; // document.lvl=0;
var ImagesInfo = new Array();

//// == DESCRIPTION UI ======================================
var descriptionLayout = "modern";
function DescriptionResize()
{
 var root = document.getElementById("fbw_desc");
 if(!root) return;
 // MSHTML document mode 7 has no CSS media queries. Use the containing
 // viewport, not scrollWidth: a long LEGEND can extend beyond it in this mode.
 var width = root.parentNode.clientWidth;
 root.className = "description-" + descriptionLayout + (width <= 760 ? " description-narrow" : "") + (width <= 480 ? " description-compact" : "");
}
function apiSetDescriptionLayout(value)
{
 descriptionLayout = value == "classic" ? "classic" : "modern";
 var desc = document.getElementById("fbw_desc");
 if(desc) { DescriptionApplyPresentation(desc); DescriptionResize(); }
 return true;
}

// Presentation state belongs to the current form. No metadata is cloned or
// reparsed when switching layouts, so in-progress values and binary handles stay.
function DescriptionTopFieldset(control, root)
{
 var result = null;
 while(control && control != root)
 {
  if(control.tagName == "FIELDSET") result = control;
  control = control.parentNode;
 }
 return result;
}

function DescriptionToggleSection(button)
{
 var section = button.parentNode.parentNode;
 section._descriptionClosed = !section._descriptionClosed;
 DescriptionShowSection(section);
}

function DescriptionShowSection(section)
{
 var closed = descriptionLayout == "modern" && section._descriptionClosed;
 section.className = "description-section" + (closed ? " description-collapsed" : "");
 var button = section.getElementsByTagName("LEGEND")[0].getElementsByTagName("BUTTON")[0];
 if(button) { button.innerText = closed ? "+" : "\u2212"; button.setAttribute("aria-expanded", closed ? "false" : "true"); }
 if(section.id == "binobj" && typeof DescriptionRefreshAttachments == "function") DescriptionRefreshAttachments();
}

function DescriptionMoveForModern(root, node, before)
{
 if(!node || !before) return;
 // Keep a hidden marker for restoring the Classic node position.
 var marker = document.createElement("SPAN");
 marker.className = "description-position";
 marker.style.display = "none";
 node.parentNode.insertBefore(marker, node);
 root._descriptionMoved.push({node:node, marker:marker});
 before.parentNode.insertBefore(node, before);
}

function DescriptionUpdateCover()
{
 var preview = document.getElementById("descriptionCoverPreview");
 var cover = document.getElementById("tiCover");
 if(!preview || !cover) return;
 var selects = cover.getElementsByTagName("SELECT");
 var href = selects.length ? String(selects[0].value) : "";
 if(href) { preview.src = "fbw-internal:" + href; preview.style.display = "block"; }
 else { preview.removeAttribute("src"); preview.style.display = "none"; }
}

function DescriptionApplyPresentation(root)
{
 if(!root._descriptionSections || root._descriptionApplied == descriptionLayout) return;
 root._descriptionApplied = descriptionLayout;
 root.className = "description-" + descriptionLayout;
 if(descriptionLayout == "classic")
 {
  for(var i=root._descriptionMoved.length-1; i>=0; i--)
  {
   var moved = root._descriptionMoved[i];
   moved.marker.parentNode.insertBefore(moved.node, moved.marker);
   moved.marker.parentNode.removeChild(moved.marker);
  }
  root._descriptionMoved = [];
  var optional = root._descriptionOptional;
  for(var i=0; i<optional.length; i++) optional[i].node.style.display = optional[i].display;
 }
 else
 {
  var title = document.getElementById("tiTitle");
  var label = title ? title.previousSibling : null;
  while(label && label.nodeType != 1) label = label.previousSibling;
  var genres = document.getElementById("tiGenre");
  DescriptionMoveForModern(root, label, genres);
  DescriptionMoveForModern(root, title, genres);
  DescriptionMoveForModern(root, document.getElementById("tiAuthor"), genres);
  var line = title ? title.nextSibling : null;
  if(line && line.tagName == "BR") DescriptionMoveForModern(root, line, genres);
  // Move the existing cover controls; there is still only one cover model.
  DescriptionMoveForModern(root, document.getElementById("tiCover"), label);
  var optional = root._descriptionOptional;
  for(var i=0; i<optional.length; i++) { optional[i].display = optional[i].node.style.display; optional[i].node.style.display = "block"; }
 }
 for(var i=0; i<root._descriptionSections.length; i++) DescriptionShowSection(root._descriptionSections[i]);
 DescriptionUpdateCover();
 DescriptionResize();
}

function InitDescriptionUI(root)
{
 TraceScript("J910", "operation=Description UI initialize");
 root._descriptionSections = [];
 root._descriptionMoved = [];
 root._descriptionOptional = [];
 root._descriptionApplied = "";
 var controls = ["tiTitle","stiTitle","diID","piName","ci","binobj","stylesheetId"];
 for(var i=0; i<controls.length; i++)
 {
  var section = DescriptionTopFieldset(document.getElementById(controls[i]), root);
  if(!section) continue;
  section._descriptionClosed = i != 0 && i != 2;
  var legend = section.getElementsByTagName("LEGEND")[0];
  var button = document.createElement("BUTTON");
  button.className = "description-section-toggle";
  button.title = legend.innerText;
  button.onclick = function() { DescriptionToggleSection(this); };
  legend.insertBefore(button, legend.firstChild);
  root._descriptionSections.push(section);
 }
 TraceScript("J911", "operation=Description sections initialized");
 var spans = root.getElementsByTagName("SPAN");
 for(var i=0; i<spans.length; i++)
 {
  // Modern exposes supported optional fields without changing saved view prefs.
  if(spans[i].id && spans[i].id != "id" && spans[i].className != "id")
   root._descriptionOptional.push({node:spans[i], display:spans[i].style.display});
 }
 var cover = document.getElementById("tiCover");
 if(cover)
 {
  var preview = document.createElement("IMG");
  preview.id = "descriptionCoverPreview";
  preview.className = "description-cover-preview";
  preview.alt = cover.getElementsByTagName("LEGEND")[0].innerText;
  cover.insertBefore(preview, cover.firstChild.nextSibling);
 }
 TraceScript("J912", "operation=Description presentation");
 DescriptionApplyPresentation(root);
 DescriptionStyleButtons(root);
 InitDescriptionActions(root);
 DescriptionValidate();
 if(!window._descriptionResizeHook) {
  window.attachEvent("onresize", DescriptionResize);
  window._descriptionResizeHook = true;
 }
 if(!root._descriptionChangeHook) {
  root.attachEvent("onchange", DescriptionValidationChanged);
  // change does not bubble in the legacy MSHTML document mode; focusout does.
  root.attachEvent("onfocusout", DescriptionValidationChanged);
  root._descriptionChangeHook = true;
 }
 TraceScript("J919", "operation=Description UI ready");
}


//// == DESCRIPTION BINARY UI ===============================
function DescriptionAddAttachment()
{
 window.external._DescriptionAddAttachment();
}

function DescriptionLoadThumbnail(row)
{
 var images = row.getElementsByTagName("IMG");
 var id = GetBinaryInput(row, "id");
 if(images.length && id && images[0].getAttribute("src") != "fbw-internal:#" + id.value) images[0].src = "fbw-internal:#" + id.value;
}

function DescriptionStyleBinary(row)
{
 row.className = "description-binary";
 DescriptionStyleButtons(row);
 var type = GetBinaryInput(row, "type");
 if(type && IsImageBinaryType(type.value))
 {
  var preview = document.createElement("IMG");
  preview.className = "description-binary-thumbnail";
  preview.setAttribute("data-description-preview", "true");
  preview.alt = "";
  row.insertBefore(preview, row.firstChild);
  row.onmouseover = function() { DescriptionLoadThumbnail(this); };
 }
}

function DescriptionRefreshAttachments()
{
 var root = document.getElementById("fbw_desc");
 if(!root || !root._descriptionSections) return;
 var section = document.getElementById("binobj");
 var rows = section.getElementsByTagName("DIV"), total = 0;
 for(var i=0; i<rows.length; i++)
 {
  var images = rows[i].getElementsByTagName("IMG");
  if(images.length && images[0].getAttribute("src")) DescriptionLoadThumbnail(rows[i]);
  var size = GetBinaryInput(rows[i], "size");
  if(size) total += Number(size.value) || 0;
  // Bound eager decoding. Other thumbnails load when their row is hovered.
  if(i<30 && descriptionLayout == "modern" && !section._descriptionClosed) DescriptionLoadThumbnail(rows[i]);
 }
 var summary = document.getElementById("descriptionAttachmentSummary");
 if(summary) summary.innerText = rows.length + " / " + total + " B";
 var add = document.getElementById("descriptionAddAttachment");
 if(add && add.parentNode == section && add.nextSibling) section.appendChild(add);
}

function DescriptionAddRepeated(containerId)
{
 var container = document.getElementById(containerId);
 if(!container) return;
 var rows = container.getElementsByTagName("DIV");
 if(!rows.length) return;
 var row = dClone(rows[rows.length-1]);
 var inputs = row.getElementsByTagName("INPUT");
 for(var i=0; i<inputs.length; i++) inputs[i].value = "";
 var spans = row.getElementsByTagName("SPAN");
 for(var i=0; i<spans.length; i++) if(spans[i].id == "id" || spans[i].className == "id") spans[i].setAttribute("value", "");
 var action = container.firstChild;
 while(action && (action.nodeType != 1 || action.className != "description-add-action")) action = action.nextSibling;
 container.insertBefore(row, action);
 PutSpacers(container);
 DescriptionStyleButtons(row);
 if(typeof DescriptionValidate == "function") DescriptionValidate();
 var first = row.getElementsByTagName("INPUT")[0];
 if(first && !first.disabled) first.focus();
}

function DescriptionAddAction(parent, text, handler)
{
 var button = document.createElement("BUTTON");
 button.className = "description-add-action";
 button.innerText = text; button.onclick = handler;
 parent.appendChild(button);
 return button;
}


//// == DESCRIPTION VALIDATION ==============================
// Advisory only: never write a form value, create a UUID, or repair metadata.
function DescriptionValidationChanged()
{
 DescriptionUpdateCover();
 DescriptionValidate();
}

function DescriptionValidate()
{
 var root = document.getElementById("fbw_desc");
 if(!root || !root._descriptionSections) return;
 var previous = root._descriptionInvalid || [];
 for(var i=0; i<previous.length; i++)
 {
  previous[i].className = previous[i].className.replace(/\s*description-invalid/g, "");
  previous[i].title = previous[i]._descriptionValidationTitle || "";
 }
 var marked = [], messages = [];
 function warn(control, key, fallback)
 {
  var text = DescriptionText(key, fallback);
  if(control)
  {
   if(control.className.indexOf("description-invalid") < 0)
   {
    control._descriptionValidationTitle = control.title;
    control.className += " description-invalid";
    marked.push(control);
   }
   control.title = text;
  }
  for(var m=0; m<messages.length; m++) if(messages[m] == text) return;
  messages.push(text);
 }
 function empty(value) { return !String(value || "").replace(/\s/g, ""); }
 var required = ["tiTitle", "diID", "diDate"];
 for(var i=0; i<required.length; i++)
 {
  var field = document.getElementById(required[i]);
  if(field && empty(field.value)) warn(field, "required", "A required value is empty.");
 }
 var authors = ["tiAuthor", "diAuthor"];
 for(var i=0; i<authors.length; i++)
 {
  var authorRoot = document.getElementById(authors[i]);
  if(!authorRoot) continue;
  var rows = authorRoot.getElementsByTagName("DIV");
  for(var j=0; j<rows.length; j++)
  {
   var filled = false, names = ["first", "middle", "last", "nick"];
   for(var k=0; k<names.length; k++) if(rows[j].all[names[k]] && !empty(rows[j].all[names[k]].value)) filled = true;
   if(!filled) warn(rows[j], "author_empty", "A required author is empty.");
  }
 }
 var inputs = root.getElementsByTagName("INPUT");
 for(var i=0; i<inputs.length; i++)
 {
  var field = inputs[i], value = String(field.value);
  var numericValue = value.replace(/^\s+|\s+$/g, "");
  if(field.id == "match" && value && (!/^[+-]?\d+$/.test(numericValue) || Number(numericValue) < 0 || Number(numericValue) > 100))
   warn(field, "match_invalid", "Genre match must be an integer from 0 to 100.");
  if(field.id == "number" && value && !/^[+-]?\d+$/.test(numericValue))
   warn(field, "number_invalid", "Sequence number must be an integer.");
 }
 var binaries = document.getElementById("binobj"), ids = {};
 if(binaries)
 {
  var rows = binaries.getElementsByTagName("DIV");
  for(var i=0; i<rows.length; i++)
  {
   var input = GetBinaryInput(rows[i], "id");
   if(!input) continue;
   var key = "$" + input.value;
   if(ids[key]) { warn(ids[key], "binary_duplicate", "Duplicate attachment ID."); warn(input, "binary_duplicate", "Duplicate attachment ID."); }
   else ids[key] = input;
  }
 }
 var covers = ["tiCover", "stiCover"];
 for(var i=0; i<covers.length; i++)
 {
  var cover = document.getElementById(covers[i]);
  if(!cover) continue;
  var selects = cover.getElementsByTagName("SELECT");
  for(var j=0; j<selects.length; j++)
   if(selects[j].value && !ids["$" + String(selects[j].value).replace(/^#/, "")])
    warn(selects[j], "cover_missing", "The cover refers to a missing attachment.");
 }
 root._descriptionInvalid = marked;
 var summary = document.getElementById("descriptionValidation");
 if(!summary)
 {
  summary = document.createElement("P"); summary.id = "descriptionValidation";
  summary.className = "description-validation"; summary.setAttribute("role", "status");
  root.insertBefore(summary, root.firstChild);
 }
 summary.innerText = messages.join("\n");
 summary.style.display = messages.length ? "block" : "none";
}


function DescriptionText(key, fallback)
{
 return GetLocalizedStringOrDefault("fbe.description." + key, fallback);
}

function DescriptionStyleButtons(root)
{
 var buttons = root.getElementsByTagName("BUTTON");
 for(var i=0; i<buttons.length; i++)
 {
  var button = buttons[i];
  if(button.tagName != "BUTTON") continue;
  // The shared XSL still owns the handlers; only their captions change.
  var text = button.innerText;
  var key = "", fallback = "", icon = "";
  if(text == "r") { key = "remove"; fallback = "Remove"; icon = "\u00d7"; }
  else if(text == "2") { key = "duplicate"; fallback = "Duplicate"; icon = "\u29c9"; }
  else if(text == "4") { key = "choose"; fallback = "Choose"; icon = "\u2026"; }
  else if(text == "N") { key = "view"; fallback = "View"; icon = "\u2315"; }
  else if(text == "\u00cd") { key = "save"; fallback = "Save"; icon = "\u2193"; }
  if(key) {
   var label = DescriptionText(key, fallback);
   button.innerText = icon; button.title = label; button.setAttribute("aria-label", label);
   button.className += (button.className ? " " : "") + "description-icon-action";
  }
 }
}

function InitDescriptionActions(root)
{
 if(document.getElementById("sti_all")) {
  var sourceSection=DescriptionTopFieldset(document.getElementById("stiTitle"), root);
  DescriptionAddAction(sourceSection.getElementsByTagName("LEGEND")[0], DescriptionText("add_source", "Add source work"), function() {
   sourceSection._descriptionClosed=false; DescriptionShowSection(sourceSection); document.getElementById("stiTitle").focus();
  });
 }
 var cover = document.getElementById("tiCover");
 DescriptionAddAction(cover, DescriptionText("change_cover", "Change cover"), function() {
  var select = cover.getElementsByTagName("SELECT")[0];
  if(select) { select.size = select.size > 1 ? 1 : Math.min(8, Math.max(2, select.options.length)); select.focus(); }
 });
 var author = document.getElementById("tiAuthor");
 DescriptionAddAction(author, DescriptionText("add_author", "Add author"), function() { DescriptionAddRepeated("tiAuthor"); });
 var genre = document.getElementById("tiGenre");
 DescriptionAddAction(genre, DescriptionText("add_genre", "Add genre"), function() { DescriptionAddRepeated("tiGenre"); });
 var attachments = document.getElementById("binobj");
 var summary = document.createElement("P"); summary.id = "descriptionAttachmentSummary";
 attachments.insertBefore(summary, attachments.firstChild.nextSibling);
 DescriptionAddAction(attachments, DescriptionText("add_attachment", "Add attachment"), DescriptionAddAttachment).id = "descriptionAddAttachment";
 DescriptionRefreshAttachments();
}

var prevImageShowTimer = null;
// The note preview lives outside fbw_body, so showing it never changes the
// editable book DOM (and consequently does not create an Undo entry).
var notePreviewState = { panel: null, link: null, showTimer: null, hideTimer: null };
var diagnosticTraceEnabled = false;
var diagnosticVerboseTraceEnabled = false;
var diagnosticOperationStage = "J000";
var diagnosticLastTraceEvent = "J000";
var diagnosticFailureStage = "";
// 0 = unknown, 1 = available, -1 = unavailable for this document.
var diagnosticTraceBridgeState = 0;
function SetDiagnosticOperationStage(code) { diagnosticOperationStage = code; }
function apiGetDiagnosticOperationStage() { return diagnosticOperationStage; }
function apiGetDiagnosticFailureStage() { return diagnosticFailureStage; }
function apiGetDiagnosticLastTraceEvent() { return diagnosticLastTraceEvent; }
function apiGetDiagnosticLastStage() { return diagnosticOperationStage; }
function apiGetDiagnosticTraceBridgeState() { return diagnosticTraceBridgeState; }
function apiSetDiagnosticTraceEnabled(enabled)
{
 var wasEnabled = diagnosticTraceEnabled;
 diagnosticTraceEnabled = enabled ? true : false;
 if(!wasEnabled && diagnosticTraceEnabled) diagnosticTraceBridgeState = 0;
 TraceDiagnosticEvent("J001", "operation=apiSetDiagnosticTraceEnabled");
 return true;
}
function apiSetDiagnosticTraceOptions(enabled, verbose)
{
 diagnosticVerboseTraceEnabled = verbose ? true : false;
 return apiSetDiagnosticTraceEnabled(enabled);
}
function TraceOperation(code, message)
{
 SetDiagnosticOperationStage(code);
 TraceDiagnosticEvent(code, message);
}
function TraceVerboseOperation(code, message)
{
 SetDiagnosticOperationStage(code);
 if(diagnosticVerboseTraceEnabled) TraceDiagnosticEvent(code, message);
}
function TraceDiagnosticSummary(code, message)
{
 TraceOperation(code, message);
}
function TraceDiagnosticEvent(code, message)
{
 diagnosticLastTraceEvent = code;
 if(!diagnosticTraceEnabled || diagnosticTraceBridgeState == -1) return;
 try
 {
  window.external.TraceScript(code, message);
  diagnosticTraceBridgeState = 1;
 }
 catch(ignore)
 {
  diagnosticTraceBridgeState = -1;
 }
}
function TraceScript(code, message)
{
 SetDiagnosticOperationStage(code);
 TraceDiagnosticEvent(code, message);
}
function IsDiagnosticFaultInjectionEnabled(point)
{
 try {
  var injected = window.fbeNextFaultInjection;
  if(injected == point) return true;
  var points = injected ? String(injected).split("+") : [];
  for(var index = 0; index < points.length; ++index) if(points[index] == point) return true;
  return false;
 }
 catch(ignore) { return false; }
}
function IsDiagnosticFaultInjectionActive()
{
 try { return !!window.fbeNextFaultInjection; }
 catch(ignore) { return false; }
}
function DiagError(code, operation, error)
{
 var failedStage = diagnosticFailureStage || diagnosticOperationStage || code;
 var details = "level=error; failed-stage=" + failedStage + "; operation=" + operation;
 var errorName = "other";
 try
 {
  if(error)
  {
   var rawName = error.name;
   if(rawName == "Error" || rawName == "TypeError" || rawName == "RangeError" || rawName == "SyntaxError" || rawName == "Microsoft JScript runtime error") errorName = rawName;
   details += "; name=" + errorName + "; name-present=" + (rawName ? 1 : 0) + "; description-present=" + (error.description ? 1 : 0) + "; message-present=" + (error.message ? 1 : 0);
   if(typeof error.number == "number" && isFinite(error.number)) details += "; number=" + error.number;
   if(typeof error.lineNumber == "number" && isFinite(error.lineNumber)) details += "; line=" + error.lineNumber;
  }
 }
 catch(ignore) {}
 TraceDiagnosticEvent(code, details + "; details=omitted");
}

//======================================
// Public API
//======================================

//--------------------------------------
// Gets a binary object for the memory protocol to display book images in the browser

function FillCoverList()
{
	return FillLists();
}

function apiGetBinary(id)
{
 var bin_objects=document.all.binobj.getElementsByTagName("DIV");

 for(var i=0; i<bin_objects.length; i++)
 {
  if(bin_objects[i].all.id.value==id) return bin_objects[i].base64data;
 }
}

function IsImageBinaryType(type)
{
 var normalized=String(type || "").toLowerCase().replace(/^\s+|\s+$/g,"");
 if(normalized=="image/jpg" || normalized=="image/pjpeg") normalized="image/jpeg";
 return normalized.indexOf("image/")==0;
}

function LocalizedBinaryMessage(key)
{
 return window.external.GetLocalizedString(key);
}

function GetLocalizedStringOrDefault(key, fallback)
{
 var localized="";
 try { localized=window.external.GetLocalizedString(key); } catch(e) {}
 if(localized==null || String(localized).replace(/^\s+|\s+$/g,"")==="") return fallback;
 return String(localized);
}

function GetBinaryInput(binary, name)
{
 if(!binary || !binary.all) return null;
 return binary.all[name] || null;
}

function BinaryIdExists(id, exceptBinary)
{
 try {
  if(!exceptBinary && IsBinaryXmlId(id))
  {
   return window.external._BinaryStoreUniqueId(id, document.all.binobj) != id;
  }
 } catch(e) {}
 var binObjects=document.all.binobj.getElementsByTagName("DIV");
 for(var i=0; i<binObjects.length; ++i)
 {
  if(binObjects[i] != exceptBinary && GetBinaryInput(binObjects[i], "id").value == id) return true;
 }
 return false;
}

// Keep XML NCName BMP and supplementary ranges exactly aligned with FbeBinary::NormalizeXmlId.
// JScript ES3 cannot query Unicode character categories, so both layers use this
// explicit common subset and never disagree about an editable binary ID.
function IsBinaryXmlLetterCodeUnit(code)
{
 return (code>=65 && code<=90) || (code>=97 && code<=122) ||
  (code>=0x00c0 && code<=0x00d6) || (code>=0x00d8 && code<=0x00f6) ||
  (code>=0x00f8 && code<=0x02ff) || (code>=0x0370 && code<=0x037d) ||
  (code>=0x037f && code<=0x1fff) || (code>=0x200c && code<=0x200d) ||
  (code>=0x2070 && code<=0x218f) || (code>=0x2c00 && code<=0x2fef) ||
  (code>=0x3001 && code<=0xd7ff) || (code>=0xf900 && code<=0xfdcf) ||
  (code>=0xfdf0 && code<=0xfffd);
}

function IsBinaryXmlSupplementaryPair(id, index)
{
 var high=id.charCodeAt(index), low=id.charCodeAt(index+1);
 return high>=0xd800 && high<=0xdb7f && low>=0xdc00 && low<=0xdfff;
}

function IsBinaryXmlId(id)
{
 id=String(id || "");
 if(id=="") return false;
 var index=0, first=id.charCodeAt(0);
 if(IsBinaryXmlSupplementaryPair(id, 0)) index=2;
 else if(first!=95 && !IsBinaryXmlLetterCodeUnit(first)) return false;
 for(var i=index; i<id.length; ++i)
 {
  if(IsBinaryXmlSupplementaryPair(id, i)) { ++i; continue; }
  var value=id.charCodeAt(i);
  if(value!=45 && value!=46 && value!=95 && !(value>=48 && value<=57) && value!=0x00b7 &&
   !(value>=0x0300 && value<=0x036f) && !(value>=0x203f && value<=0x2040) && !IsBinaryXmlLetterCodeUnit(value)) return false;
 }
 return true;
}
function IsBinaryReferenceValue(value, id)
{
 return value == "#"+id || value == "fbw-internal:#"+id;
}

function SetBinaryReference(element, name, value)
{
 // MSHTML accepts direct href/src assignment. Avoid setAttribute here: in
 // some document modes it reports an unsupported host operation even when
 // the subsequent property assignment would be valid.
 try { element[name]=value; } catch(e) {}
}
function UpdateBinaryReferences(oldId, newId)
{
 // document.all is the compatible MSHTML collection in the document mode used
 // by the editor. getElementsByTagName("*") reports "Operation is not
 // supported" there, which previously stopped a binary-ID rename at line 212.
 var elements=document.all;
 var selectedReferences=[];
 for(var i=0; elements && i<elements.length; ++i)
 {
  var element=elements[i];
  if(!element) continue;
  var href=element.getAttribute ? element.getAttribute("href") : null;
  if(href == null && element.href) href=element.href;
  if(IsBinaryReferenceValue(href, oldId))
  {
   SetBinaryReference(element, "href", "#"+newId);
  }

  var src=element.getAttribute ? element.getAttribute("src") : null;
  if(src == null && element.src) src=element.src;
  if(IsBinaryReferenceValue(src, oldId))
  {
   SetBinaryReference(element, "src", "fbw-internal:#"+newId);
  }

  if(String(element.tagName).toUpperCase()=="SELECT" && element.value == "#"+oldId)
   selectedReferences.push({list:element, value:"#"+newId});
 }
 return selectedReferences;
}
function RestoreBinaryReferenceSelects(selectedReferences)
{
 if(!selectedReferences) return;
 for(var i=0; i<selectedReferences.length; ++i)
 {
  var reference=selectedReferences[i];
  var options=reference.list.getElementsByTagName("OPTION");
  for(var j=0; j<options.length; ++j)
   if(options[j].value==reference.value) { reference.list.value=reference.value; break; }
 }
}

function BinaryIsReferenced(id)
{
 // Use the same MSHTML-compatible collection as UpdateBinaryReferences.
 var elements=document.all;
 if(!elements) throw new Error("Binary reference collection unavailable");
 for(var i=0; i<elements.length; ++i)
 {
  var element=elements[i];
  if(!element) continue;
  // A thumbnail is presentation owned by this same row, not an FB2 reference.
  if(element.getAttribute && String(element.getAttribute("data-description-preview")) == "true" &&
     element.parentNode && element.parentNode.parentNode == document.all.binobj) continue;
  var href=element.getAttribute ? element.getAttribute("href") : null;
  var src=element.getAttribute ? element.getAttribute("src") : null;
  if(href == null && element.href) href=element.href;
  if(src == null && element.src) src=element.src;
  if(IsBinaryReferenceValue(href, id) || IsBinaryReferenceValue(src, id)) return true;
  if(String(element.tagName).toUpperCase()=="SELECT" && element.value == "#"+id) return true;
 }
 return false;
}

function OnBinaryChange(inputOverride)
{
 var input=inputOverride || (window.event ? window.event.srcElement : null);
 var binary=input ? input.parentNode : null;
 if(!input || !binary) { RebuildImagesInfo(); FillLists(); return; }

 var idInput=GetBinaryInput(binary, "id");
 var typeInput=GetBinaryInput(binary, "type");
 if(input == idInput)
 {
  var oldId=idInput.getAttribute("oldId");
  if(oldId == null) oldId=idInput.value;
  var newId=String(idInput.value).replace(/^\s+|\s+$/g, "");
  if(newId == "" || !IsBinaryXmlId(newId) || BinaryIdExists(newId, binary))
  {
   idInput.value=oldId;
   MsgBox(newId == "" ? LocalizedBinaryMessage("fbe.binary.id.empty") : (!IsBinaryXmlId(newId) ? LocalizedBinaryMessage("fbe.binary.id.invalid") : LocalizedBinaryMessage("fbe.binary.id.duplicate")));
   return;
  }
  idInput.value=newId;
  if(oldId != newId) var selectedReferences=UpdateBinaryReferences(oldId, newId);
  BinaryStoreChanged("rename", binary);
  idInput.setAttribute("oldId", newId);
 }
 else if(input == typeInput)
 {
  var cached=CachedBinaryInfo(idInput.value);
  BuildBinaryControls(binary, "", idInput.value, typeInput.value, binary.base64data, cached);
  BinaryStoreChanged("register", binary);
 }
 RebuildImagesInfo();
 FillLists();
 RestoreBinaryReferenceSelects(selectedReferences);
}

function RebuildImagesInfo()
{
 var binObjects=document.all.binobj.getElementsByTagName("DIV");
 ImagesInfo.length=0;
 for(var i=0; i<binObjects.length; ++i)
 {
  var binary=binObjects[i];
  if(!IsImageBinaryType(binary.all.type.value)) continue;
  var info=CachedBinaryInfo(binary.all.id.value);
  var dims=info ? (info.width == null ? "" : info.width+"x"+info.height) : window.external.GetImageDimsByData(binary.base64data);
  if(dims == "") continue;
  var separator=dims.search("x");
  if(separator == -1) continue;
  var imageInfo=new Object();
  imageInfo.src="fbw-internal:#"+binary.all.id.value;
  imageInfo.id=binary.all.id.value;
  imageInfo.width=dims.substring(0, separator);
  imageInfo.height=dims.substring(separator+1, dims.length);
  ImagesInfo.push(imageInfo);
 }
}

//--------------------------------------
// Adds a binary object.
// Don't forget to call FillCoverList() when you have finished adding objects!

// Private native bridge, outside the immutable public scripting contract.
// Legacy hosts/test doubles keep their original metadata path.
function BinaryStoreChanged(action, element)
{
 try { window.external._BinaryStoreChanged(action, element); } catch(e) {}
 if(typeof DescriptionRefreshAttachments == "function") DescriptionRefreshAttachments();
}

function CachedBinaryInfo(id)
{
 try { return eval("("+window.external.GetBinaryInfo(id)+")"); } catch(e) { return null; }
}

function BuildBinaryControls(div, fullpath, id, type, data)
{
 var cached=arguments.length>5 ? arguments[5] : null;
 var html='<button id="del" ' +
                 'onmouseover="HighlightBorder(this.parentNode, true, \'solid\', \'2px\', \'#FF0000\');" ' +
                 'onmouseout="HighlightBorder(this.parentNode, false);" ' +
                 'onclick="Remove(this.parentNode);" unselectable="on">&#x72;</button>';
 html += '<button id="save" onclick="SaveBinary(this.parentNode);" unselectable="on">&#xcd;</button>';
 var dims="";
 if(IsImageBinaryType(type))
 {
  html += '<button id="show"' +
  'onmouseover="ShowImageOfBin(this.parentNode,\'prev\');"' +
  'onmouseout="HidePrevImage();"' +
  'onclick="ShowImageOfBin(this.parentNode,\'full\');"' +
  'unselectable="on">&#x4e;</button>';

 }

 html += '<label unselectable="on">ID:</label><input type="text" maxlength="256" id="id" style="width:20em;"><label unselectable="on">Type:</label><input type="text" style="width:8em;" maxlength="256" id="type" value="">';
 html += '<label unselectable="on">Size:</label><input type="text" disabled id="size" style="width:5em;" value="' + (cached ? cached.size : window.external.GetBinarySize(data)) + '">';

 if(IsImageBinaryType(type))
 {
  dims = cached && cached.width != null ? cached.width+"x"+cached.height : (fullpath != "" ? window.external.GetImageDimsByPath(fullpath) : "");
  if(dims == "") dims = window.external.GetImageDimsByData(data);
  if(dims != "") html += '<label unselectable="on">w*h:</label><input type="text" disabled id="dims" style="width:6em;" value="' + dims + '">';
 }

 div.innerHTML=html;
 div.all.id.value=id;
 div.all.type.value=type;
 div.all.id.setAttribute("oldId",id);
 div.all.id.onchange=OnBinaryChange;
 div.all.type.onchange=OnBinaryChange;
 DescriptionStyleBinary(div);
 return dims;
}

function SaveBinary(binary)
{
 var id=GetBinaryInput(binary, "id");
 if(id) SaveImage("fbw-internal:#"+id.value);
}

function apiAddBinary(fullpath, id, type, data)
{
	// check if this id already exists
	var idx = 0;
	var curid = id;
	var indexed=false;
	try { if(IsBinaryXmlId(id)) { curid=window.external._BinaryStoreUniqueId(id, document.all.binobj); indexed=true; } } catch(e) {}
	var bo = indexed ? null : document.all.binobj.getElementsByTagName("DIV");

	for(; !indexed;)
	{
		var found = false;
		for(var i = 0; i < bo.length; i++)
			if(bo[i].all.id.value == curid)
			{
				found = true;
				break;
			}

		if(!found) break;

		curid = id + "_" + idx;
		idx++;
	}

	var div = document.createElement("DIV");

	var dims=BuildBinaryControls(div, fullpath, curid, type, data);
	if(IsImageBinaryType(type))
	{
		var imghref = "fbw-internal:#" + curid;

		if(dims != "")
			{
				var imgWidth = dims.substring(0, dims.search("x"));
				var imgHeight = dims.substring(dims.search("x") + 1, dims.length);

				var ImageInfo = new Object();
				ImageInfo.src = imghref;
				ImageInfo.id = curid;
				ImageInfo.width = imgWidth;
				ImageInfo.height = imgHeight;

				ImagesInfo.push(ImageInfo);
			}
	}

	// Install the native legacy property before its first assignment. The DOM
	// behavior retains only a weak document handle, never the payload itself.
	window.external._BinaryStoreChanged("attach", div);
	div.base64data = data;

	document.all.binobj.appendChild(div);
	BinaryStoreChanged("register", div);

	return curid;
}

function GetImageData(id)
{
	var bo = document.all.binobj.getElementsByTagName("DIV");
	for(var i = 0; i < bo.length; i++)
	{
		if(bo[i].all.id.value == id)
			return bo[i].base64data;
	}

	return;
}

function HighlightBorder(element, override, style, width, color)
{
	if(!element ||!element.style)
		return;

	if(override)
	{
		element.style.border = style + " " + width + " " + color;
	}
	else
		element.style.border = "";
}

function ShowPrevImage(source)
{
	var prevImgPanel = document.getElementById("prevImgPanel");
	var prevImg = document.getElementById("prevImg");

	if(!prevImgPanel || !prevImg) return;

	var idx = -1;
	for(var i = 0; i < ImagesInfo.length; ++i)
	{
		if(ImagesInfo[i].src == source)
		{
			idx = i;
			break;
		}
	}

	if(idx == -1)
		return;

	var imgWidth = ImagesInfo[idx].width;
	var imgHeight = ImagesInfo[idx].height;

	var btnHeight = event.srcElement.offsetHeight;

	var coordX = event.clientX;
	var coordY = event.clientY;

	var scrollX = 0;
	var scrollY = 0;
	if(document.documentElement && (document.documentElement.scrollTop || document.documentElement.scrollLeft))
	{
		scrollX += document.documentElement.scrollLeft;
		scrollY += document.documentElement.scrollTop;
	}

	var winWidth = 0;;
	var winHeight = 0;
	if (document.documentElement && document.documentElement.clientWidth && document.documentElement.clientHeight)
	{
		winWidth = document.documentElement.clientWidth;
		winHeight = document.documentElement.clientHeight;
	}

	var place = "top";
	var baseWidth = coordX;
	if(baseWidth > (winWidth - coordX))
		baseWidth = winWidth - coordX;

	baseWidth = baseWidth*2;

	var baseHeight = coordY;
	if(baseHeight < (winHeight - coordY))
	{
		baseHeight =  winHeight - coordY;
		place = "bottom";
	}

	baseHeight -= btnHeight;

  var ratio;
  if (baseWidth < 200)
  {
    ratio = 200/baseWidth;
    baseWidth = 200;
    baseHeight *= ratio;
  }

  var scaleTo = "width";
  ratio = baseWidth/imgWidth;
  if(imgWidth < imgHeight)
  {
     scaleTo = "height";
     ratio = baseHeight/imgHeight;
  }

  if(imgWidth > baseWidth || imgHeight > baseHeight)
  {
    switch (scaleTo)
    {
      case "width":
        imgWidth = baseWidth;
        imgHeight = ratio * imgHeight;
        if(imgHeight > baseHeight)
        {
          ratio = baseHeight/imgHeight;
          imgHeight = baseHeight;
          imgWidth = ratio * imgWidth;
        }
      break;
      case "height":
        imgHeight = baseHeight;
        imgWidth = ratio * imgWidth;
        if(imgWidth > baseWidth)
        {
          ratio = baseWidth/imgWidth;
          imgWidth = baseWidth;
          imgHeight = ratio * imgHeight;
        }
      break;
    }
  }

	prevImg.src = source;
	prevImg.width = imgWidth ;
	prevImg.height = imgHeight;
	prevImg.style.cursor = "default";

	prevImgPanel.style.left = (coordX + scrollX - Math.round(imgWidth/2)) + "px";
	switch(place)
	{
	case "top":
		prevImgPanel.style.top = (coordY + scrollY - Math.round(imgHeight) - btnHeight) + "px";
		break;
	case "bottom":
		prevImgPanel.style.top = (coordY + scrollY + btnHeight) + "px";
		break;
	}

	if(prevImageShowTimer != null) window.clearTimeout(prevImageShowTimer);
	prevImageShowTimer=window.setTimeout(function() {
		prevImgPanel.style.visibility = "visible";
		prevImageShowTimer=null;
	}, 500);
}

function HidePrevImage()
{
	var prevImgPanel = document.getElementById("prevImgPanel");
	var prevImg = document.getElementById("prevImg");

	if(prevImageShowTimer != null)
	{
		window.clearTimeout(prevImageShowTimer);
		prevImageShowTimer=null;
	}

	if(!prevImgPanel || !prevImg) return;

	prevImg.src = "";
	prevImg.width = 0;
	prevImg.height = 0;
	prevImgPanel.style.visibility = "hidden";
}

// ---------------------------------------------------------------------------
// Note link preview.  Event delegation is deliberate here: Body is rebuilt by
// TransformXML and a book may contain a very large number of note links.

function NotePreviewEvent(e)
{
	return e || window.event;
}

function IsNotePreviewLink(element)
{
	if(!element || element.nodeType != 1 || String(element.tagName).toLowerCase() != "a") return false;
	var className = " " + String(element.className || "") + " ";
	if(className.indexOf(" note ") == -1) return false;
	return GetNotePreviewTargetId(element) != "";
}

function GetNotePreviewTargetId(link)
{
	if(!link) return "";
	var href = "";
	try { href = String(link.getAttribute("href", 2) || ""); } catch(ignore) {}
	var id = GetLocalNotePreviewFragment(href);
	if(id != "") return id;
	// MSHTML's getAttribute(..., 2) is the primary path.  Properties are
	// fallbacks only, and absolute URLs must name this exact document.
	try { href = String(link.href || ""); } catch(ignore) { href = ""; }
	var expandedHref = href;
	id = GetLocalNotePreviewFragment(expandedHref);
	if(id != "") return id;
	try { href = String(link.hash || ""); } catch(ignore) { href = ""; }
	if(!expandedHref && href.charAt(0) == "#") return DecodeNotePreviewFragment(href.substring(1));
	return "";
}

function DecodeNotePreviewFragment(fragment)
{
	if(!fragment) return "";
	try { return decodeURIComponent(fragment); } catch(ignore) { return fragment; }
}

function GetNotePreviewDocumentUrl()
{
	var urls = GetNotePreviewDocumentUrls();
	return urls.length ? urls[0] : "";
}

function GetNotePreviewDocumentUrls()
{
	var urls = [];
	function add(value) {
		value = String(value || "");
		var hash = value.indexOf("#");
		if(hash != -1) value = value.substring(0, hash);
		// about:blank is the temporary host page used before FBE navigates to
		// main.html.  It must not make an actual file: link look external.
		if(!value || value.toLowerCase() == "about:blank") return;
		for(var i = 0; i < urls.length; ++i) if(NormalizeNotePreviewUrl(urls[i]) == NormalizeNotePreviewUrl(value)) return;
		urls.push(value);
	}
	// Legacy MSHTML can expose a different usable spelling through URL and
	// location.href.  Retain both and compare normalized document parts below.
	try { add(document.URL); } catch(ignore) {}
	try { add(document.location.href); } catch(ignore) {}
	try { add(window.location.href); } catch(ignore) {}
	return urls;
}

function NormalizeNotePreviewUrl(url)
{
	url = String(url || "").replace(/\\/g, "/");
	try { url = decodeURIComponent(url); } catch(ignore) {}
	return url.toLowerCase();
}

function GetLocalNotePreviewFragment(href)
{
	href = String(href || "");
	if(!href) return "";
	if(href.indexOf("fbw-internal:#") == 0) return DecodeNotePreviewFragment(href.substring(14));
	if(href.charAt(0) == "#") return DecodeNotePreviewFragment(href.substring(1));
	var hash = href.lastIndexOf("#");
	if(hash == -1 || hash == href.length - 1) return "";
	var linkDocumentUrl = href.substring(0, hash), documentUrls = GetNotePreviewDocumentUrls();
	for(var i = 0; i < documentUrls.length; ++i)
		if(NormalizeNotePreviewUrl(linkDocumentUrl) == NormalizeNotePreviewUrl(documentUrls[i]))
			return DecodeNotePreviewFragment(href.substring(hash + 1));
	return "";
}

function GetNotePreviewLink(element, body)
{
	if(element && element.nodeType != 1) element = element.parentNode;
	while(element && element != body)
	{
		if(IsNotePreviewLink(element)) return element;
		element = element.parentNode;
	}
	return IsNotePreviewLink(body) ? body : null;
}

function GetNotePreviewRect(element)
{
	if(!element) return null;
	if(element.getBoundingClientRect) return element.getBoundingClientRect();
	var left = 0, top = 0, current = element;
	while(current) { left += current.offsetLeft || 0; top += current.offsetTop || 0; current = current.offsetParent; }
	return { left: left, top: top, right: left + (element.offsetWidth || 0), bottom: top + (element.offsetHeight || 0) };
}

function GetNotePreviewVerticalAlign(element, body)
{
	var current = element;
	while(current && current != body)
	{
		var tag = String(current.tagName || "").toLowerCase();
		if(tag == "sup") return "super";
		if(tag == "sub") return "sub";
		var align = current.currentStyle ? current.currentStyle.verticalAlign : (window.getComputedStyle ? window.getComputedStyle(current, null).verticalAlign : current.style.verticalAlign);
		if(align == "super" || align == "sub") return align;
		current = current.parentNode;
	}
	return "";
}

function PointInNotePreviewRect(x, y, rect, align)
{
	// The narrow horizontal allowance makes a small superscript practical to
	// target, while the one-sided vertical allowance avoids opening notes while
	// the pointer simply crosses ordinary text.
	var horizontalPadding = 4;
	var towardLinePadding = 6;
	var top = rect.top - 2, bottom = rect.bottom + 2;
	if(align == "super") bottom += towardLinePadding;
	if(align == "sub") top -= towardLinePadding;
	return x >= rect.left - horizontalPadding && x <= rect.right + horizontalPadding && y >= top && y <= bottom;
}

function NotePreviewDistance(x, y, rect)
{
	var dx = x < rect.left ? rect.left - x : (x > rect.right ? x - rect.right : 0);
	var dy = y < rect.top ? rect.top - y : (y > rect.bottom ? y - rect.bottom : 0);
	return dx * dx + dy * dy;
}

function FindNotePreviewLinkAt(body, x, y, eventTarget)
{
	var candidates = [], direct = GetNotePreviewLink(eventTarget, body);
	if(direct) candidates.push(direct);

	// MSHTML returns the element actually painted at a point.  Probe a small
	// neighbourhood instead of walking every link in the book: superscripts
	// are reached above the pointer and subscripts below it.
	if(document.elementFromPoint)
	{
		var offsets = [ [0, 0], [-4, 0], [4, 0], [0, -4], [0, 4], [0, -8], [0, 8], [-4, -6], [4, -6], [-4, 6], [4, 6] ];
		for(var i = 0; i < offsets.length; ++i)
		{
			var element = document.elementFromPoint(x + offsets[i][0], y + offsets[i][1]);
			var link = GetNotePreviewLink(element, body), known = false;
			for(var j = 0; link && j < candidates.length; ++j) if(candidates[j] == link) { known = true; break; }
			if(link && !known) candidates.push(link);
		}
	}

	var closest = null, closestDistance = -1;
	for(var candidateIndex = 0; candidateIndex < candidates.length; ++candidateIndex)
	{
		var link = candidates[candidateIndex];
		var rect = GetNotePreviewRect(link);
		if(!rect || !PointInNotePreviewRect(x, y, rect, GetNotePreviewVerticalAlign(link, body))) continue;
		var distance = NotePreviewDistance(x, y, rect);
		if(closestDistance == -1 || distance < closestDistance)
		{
			closest = link;
			closestDistance = distance;
		}
	}
	return closest;
}

function GetNotePreviewScrollLeft()
{
	return (document.documentElement && document.documentElement.scrollLeft) || document.body.scrollLeft || 0;
}

function GetNotePreviewScrollTop()
{
	return (document.documentElement && document.documentElement.scrollTop) || document.body.scrollTop || 0;
}

function EnsureNotePreviewPanel()
{
	var panel = notePreviewState.panel || document.getElementById("fbNotePreview");
	if(panel) { notePreviewState.panel = panel; return panel; }
	panel = document.createElement("DIV");
	panel.id = "fbNotePreview";
	panel.className = "fb-note-preview";
	panel.contentEditable = false;
	panel.style.visibility = "hidden";
	panel.onmouseenter = function() { if(notePreviewState.hideTimer != null) { window.clearTimeout(notePreviewState.hideTimer); notePreviewState.hideTimer = null; } };
	panel.onmouseleave = function() { ScheduleNotePreviewHide(); };
	panel.onmousedown = function(e) { e = NotePreviewEvent(e); e.returnValue = false; if(e.preventDefault) e.preventDefault(); return false; };
	panel.onclick = panel.onmousedown;
	document.body.appendChild(panel);
	notePreviewState.panel = panel;
	return panel;
}

function GetNotePreviewBodyStyle()
{
	var body = document.getElementById("fbw_body"), style = null, fallback = null;
	try { style = body && (body.currentStyle || (window.getComputedStyle ? window.getComputedStyle(body, null) : null)); } catch(ignore) {}
	try { fallback = document.body && (document.body.currentStyle || (window.getComputedStyle ? window.getComputedStyle(document.body, null) : null)); } catch(ignore) {}
	function value(name, defaultValue) {
		var result = "";
		try { result = style ? String(style[name] || "") : ""; } catch(ignore) {}
		if(!result || result == "transparent") try { result = fallback ? String(fallback[name] || "") : ""; } catch(ignore) {}
		return result && result != "transparent" ? result : defaultValue;
	}
	return {
		background: value("backgroundColor", "#ffffff"),
		color: value("color", "#000000"),
		fontFamily: value("fontFamily", ""),
		fontSize: value("fontSize", "")
	};
}

function ApplyNotePreviewBodyStyle(panel)
{
	var style = GetNotePreviewBodyStyle();
	panel.style.backgroundColor = style.background;
	panel.style.color = style.color;
	if(style.fontFamily) panel.style.fontFamily = style.fontFamily;
	if(style.fontSize) panel.style.fontSize = style.fontSize;
	return style;
}

function GetNotePreviewColors()
{
	var style = GetNotePreviewBodyStyle();
	return { background: style.background, color: style.color };
}

function NotePreviewHasClass(node, className)
{
	return (" " + String(node.className || "") + " ").indexOf(" " + className + " ") != -1;
}

function AppendNotePreviewContent(panel, target)
{
	for(var child = target.firstChild; child; child = child.nextSibling)
	{
		// A section title is navigation context rather than the note itself.
		// Only this immediate title DIV is skipped: the first paragraph stays.
		if(child.nodeType == 1 && String(child.tagName).toLowerCase() == "div" && NotePreviewHasClass(child, "title")) continue;
		var clone = child.cloneNode(true);
		SanitizeNotePreviewNode(clone);
		panel.appendChild(clone);
	}
}

function GetNotePreviewBaseFontSize(panel)
{
	var fontSize = "", match, value;
	try { fontSize = String(document.getElementById("fbw_body").currentStyle.fontSize || ""); } catch(ignore) {}
	if(!fontSize) try { fontSize = String(panel.currentStyle.fontSize || ""); } catch(ignore) {}
	match = /([0-9.]+)\s*(px|pt)?/i.exec(fontSize);
	if(!match) return 16;
	value = parseFloat(match[1]);
	if(!value || value < 1) return 16;
	return String(match[2] || "").toLowerCase() == "pt" ? value * 96 / 72 : value;
}

function GetNotePreviewFontSizes(panel)
{
	var candidates = [100, 95, 90, 85, 80], result = [], baseFontSize = GetNotePreviewBaseFontSize(panel);
	for(var i = 0; i < candidates.length; ++i)
	{
		// A relative percentage alone can make text illegibly small in a body
		// with a small user font.  Always write the resolved value in px so that
		// 95% is precisely relative to fbw_body rather than an inherited popup font.
		var size = Math.round(baseFontSize * candidates[i]) / 100;
		if(candidates[i] == 100 || size >= 11) result.push(size);
	}
	return result;
}

function GetNotePreviewNaturalHeight(panel)
{
	var height = panel.scrollHeight || panel.offsetHeight || 0;
	// MSHTML can under-report scrollHeight while a hidden absolute DIV is being
	// laid out.  Its child P/EM line boxes are already accurate, so include the
	// bottom-most direct child before deciding that a note fits without scroll.
	for(var child = panel.firstChild; child; child = child.nextSibling)
	{
		if(child.nodeType != 1) continue;
		var bottom = (child.offsetTop || 0) + (child.offsetHeight || 0);
		if(bottom > height) height = bottom;
	}
	return height;
}

function FitNotePreview(panel, link)
{
	var rect = GetNotePreviewRect(link), docElement = document.documentElement;
	var viewportWidth = (docElement && docElement.clientWidth) || document.body.clientWidth;
	var viewportHeight = (docElement && docElement.clientHeight) || document.body.clientHeight;
	var margin = 8, gap = 6;
	var above = rect ? rect.top - margin - gap : viewportHeight / 2;
	var below = rect ? viewportHeight - rect.bottom - margin - gap : viewportHeight / 2;
	var maxHeight = Math.max(80, Math.min(Math.round(viewportHeight * 0.70), Math.max(above, below)));
	var maxWidth = Math.max(160, Math.min(viewportWidth - margin * 2, Math.round(viewportWidth * 0.75)));
	var widths = [280, 340, 420, 500, 560, 600];
	var lastWidth = Math.min(widths[0], maxWidth);
	panel.style.width = "auto";
	panel.style.height = "auto";
	panel.style.maxHeight = "none";
	var fonts = GetNotePreviewFontSizes(panel), lastFont = fonts[fonts.length - 1];
	panel.style.overflow = "hidden";
	for(var fontIndex = 0; fontIndex < fonts.length; ++fontIndex)
	{
		panel.style.fontSize = fonts[fontIndex] + "px";
		for(var widthIndex = 0; widthIndex < widths.length; ++widthIndex)
		{
			var width = Math.min(widths[widthIndex], maxWidth);
			if(widthIndex && width == lastWidth) continue;
			panel.style.width = width + "px";
			var naturalHeight = GetNotePreviewNaturalHeight(panel);
			lastWidth = width; lastFont = fonts[fontIndex];
			// Leave a couple of pixels for MSHTML's late line-box rounding so the
			// final italic word (for example "Прим. ред.") cannot be clipped.
			if(naturalHeight + 2 <= maxHeight)
			{
				panel.style.maxHeight = maxHeight + "px";
				panel.style.overflow = "hidden";
				return { margin: margin, gap: gap, viewportWidth: viewportWidth, viewportHeight: viewportHeight };
			}
		}
	}
	panel.style.width = lastWidth + "px";
	panel.style.fontSize = lastFont + "px";
	panel.style.maxHeight = maxHeight + "px";
	panel.style.overflow = "auto";
	return { margin: margin, gap: gap, viewportWidth: viewportWidth, viewportHeight: viewportHeight };
}

function PositionNotePreview(panel, link, fit)
{
	var rect = GetNotePreviewRect(link);
	if(!rect) return;
	var viewportWidth = fit.viewportWidth, viewportHeight = fit.viewportHeight;
	var scrollLeft = GetNotePreviewScrollLeft(), scrollTop = GetNotePreviewScrollTop();
	var margin = fit.margin, gap = fit.gap;
	var panelWidth = panel.offsetWidth, panelHeight = panel.offsetHeight;
	var left = rect.left + (rect.right - rect.left - panelWidth) / 2;
	left = Math.max(margin, Math.min(left, viewportWidth - panelWidth - margin));
	var above = rect.top - margin - gap, below = viewportHeight - rect.bottom - margin - gap;
	var top = above >= below ? rect.top - panelHeight - gap : rect.bottom + gap;
	top = Math.max(margin, Math.min(top, viewportHeight - panelHeight - margin));
	panel.style.left = Math.round(left + scrollLeft) + "px";
	panel.style.top = Math.round(top + scrollTop) + "px";
}

function ShowNotePreview(link)
{
	if(!link || notePreviewState.link != link) return;
	var panel = EnsureNotePreviewPanel();
	var id = GetNotePreviewTargetId(link), target = id ? document.getElementById(id) : null;
	TraceNotePreviewEvent("J611", "operation=notePreview; stage=target-resolved; found=" + (target ? 1 : 0));
	panel.innerHTML = "";
	if(target)
	{
		AppendNotePreviewContent(panel, target);
		TraceNotePreviewEvent("J612", "operation=notePreview; stage=clone-sanitized");
	}
	else
	{
		panel.appendChild(document.createTextNode("Примечание не найдено"));
	}
	panel.style.visibility = "hidden";
	// Fit starts from 100%; copy Body typography first so 100% is the actual
	// editor font size rather than the document/default popup font.
	ApplyNotePreviewBodyStyle(panel);
	PositionNotePreview(panel, link, FitNotePreview(panel, link));
	panel.style.visibility = "visible";
	TraceNotePreviewEvent("J613", "operation=notePreview; stage=popup-visible");
}

function TraceNotePreviewEvent(code, message)
{
	// Mouse movement is deliberately verbose-only and must not continuously
	// replace diagnosticLastTraceEvent.  Do not route this through
	// TraceDiagnosticEvent, which intentionally updates that global marker.
	if(typeof diagnosticVerboseTraceEnabled == "undefined")
	{
		// Stand-alone regression harnesses provide only this minimal sink.
		if(typeof TraceDiagnosticEvent == "function") TraceDiagnosticEvent(code, message);
		return;
	}
	SetDiagnosticOperationStage(code);
	if(!diagnosticVerboseTraceEnabled || !diagnosticTraceEnabled || diagnosticTraceBridgeState == -1) return;
	try
	{
		window.external.TraceScript(code, message);
		diagnosticTraceBridgeState = 1;
	}
	catch(ignore) { diagnosticTraceBridgeState = -1; }
}

function SanitizeNotePreviewNode(node)
{
	if(!node || node.nodeType != 1) return;
	var attributes = node.attributes;
	for(var i = attributes.length - 1; i >= 0; --i)
	{
		var attribute = attributes[i];
		if(!attribute || (typeof attribute.specified != "undefined" && !attribute.specified)) continue;
		var name = String(attribute.nodeName || "").toLowerCase();
		if(name == "id" || name == "contenteditable" || name == "unselectable" || name == "tabindex" ||
			name == "hidefocus" || name == "spellcheck" || name.indexOf("on") == 0)
		{
			try { node.removeAttribute(attribute.nodeName); } catch(ignore) {}
		}
	}
	// The panel itself is contentEditable=false.  Do not set this property on
	// every cloned descendant: legacy MSHTML can stop painting text in nested
	// inline elements (notably EM) after that mutation.
	for(var child = node.firstChild; child; child = child.nextSibling) SanitizeNotePreviewNode(child);
}

function ScheduleNotePreview(link)
{
	if(notePreviewState.hideTimer != null) { window.clearTimeout(notePreviewState.hideTimer); notePreviewState.hideTimer = null; }
	if(notePreviewState.link == link) return;
	if(notePreviewState.showTimer != null) window.clearTimeout(notePreviewState.showTimer);
	notePreviewState.link = link;
	TraceNotePreviewEvent("J610", "operation=notePreview; stage=hover-detected");
	notePreviewState.showTimer = window.setTimeout(function() {
		notePreviewState.showTimer = null;
		ShowNotePreview(link);
	}, 500);
}

function HideNotePreview()
{
	if(notePreviewState.showTimer != null) { window.clearTimeout(notePreviewState.showTimer); notePreviewState.showTimer = null; }
	if(notePreviewState.hideTimer != null) { window.clearTimeout(notePreviewState.hideTimer); notePreviewState.hideTimer = null; }
	notePreviewState.link = null;
	if(notePreviewState.panel) notePreviewState.panel.style.visibility = "hidden";
}

function ScheduleNotePreviewHide()
{
	if(notePreviewState.hideTimer != null) return;
	// Leave enough time to cross the small gap between the link and its popup.
	notePreviewState.hideTimer = window.setTimeout(function() { notePreviewState.hideTimer = null; HideNotePreview(); }, 250);
}

function IsInsideNotePreviewBody(body, element)
{
	for(var current = element; current; current = current.parentNode)
		if(current == body) return true;
	return false;
}

function HandleNotePreviewMouseMove(body, e)
{
	e = NotePreviewEvent(e);
	var target = e && (e.target || e.srcElement);
	if(!e || !IsInsideNotePreviewBody(body, target)) return;
	var link = FindNotePreviewLinkAt(body, e.clientX, e.clientY, target);
	if(link) ScheduleNotePreview(link); else ScheduleNotePreviewHide();
}

function InitNotePreview(body)
{
	if(!body || body.notePreviewHandlersAttached) return;
	body.notePreviewHandlersAttached = true;
	var onMouseMove = function(e) { HandleNotePreviewMouseMove(body, e); };
	body.onmousemove = onMouseMove;
	// A document-level Events2 sink can prevent an element property handler
	// from seeing mousemove on older MSHTML hosts. attachEvent coexists with
	// that sink and preserves delegated preview handling without per-link hooks.
	try { if(document.attachEvent) document.attachEvent("onmousemove", onMouseMove); } catch(ignore) {}
	body.onmouseleave = function() { ScheduleNotePreviewHide(); };
	body.onkeydown = function(e) { e = NotePreviewEvent(e); if((e.keyCode || e.which) == 27) HideNotePreview(); };
}

function ShowFullImage(source)
{
	HidePrevImage();

	var fullImgPanel = document.getElementById("fullImgPanel");
	var fullImg = document.getElementById("fullImg");

	if(!fullImgPanel || !fullImg) return;

	var idx = -1;
	for(var i = 0; i < ImagesInfo.length; ++i)
	{
		if(ImagesInfo[i].src == source)
		{
			idx = i;
			break;
		}
	}

	if(idx == -1)
		return;

	var imgWidth = ImagesInfo[idx].width;
	var imgHeight = ImagesInfo[idx].height;

	var scrollX = 0;
	var scrollY = 0;
	if(document.documentElement && (document.documentElement.scrollTop || document.documentElement.scrollLeft))
	{
		scrollX += document.documentElement.scrollLeft;
		scrollY += document.documentElement.scrollTop;
	}

	var winWidth = 0;;
	var winHeight = 0;
	if (document.documentElement && document.documentElement.clientWidth && document.documentElement.clientHeight)
	{
		winWidth = document.documentElement.clientWidth;
		winHeight = document.documentElement.clientHeight;
	}

	fullImg.src = source;
	fullImg.width = imgWidth;
	fullImg.height = imgHeight;
	if (imgHeight < winHeight ) fullImg.style.top = ((winHeight-imgHeight) / 2)+"px";
	else fullImg.style.top = "0px";
	fullImg.style.cursor = "default";

	fullImgPanel.style.left = "0px";
	fullImgPanel.style.width = (winWidth) + "px";

	fullImgPanel.style.top = (scrollY) + "px";
	if (winHeight < imgHeight) winHeight = imgHeight;
	fullImgPanel.style.height = (winHeight) + "px";

	fullImgPanel.style.display = "block";
	fullImgPanel.style.visibility = "visible";
}

function HideFullImage()
{
	var fullImgPanel = document.getElementById("fullImgPanel");
	var fullImg = document.getElementById("fullImg");

	if(!fullImgPanel || !fullImg) return;

	fullImg.src = "";
	fullImg.width = 0;
	fullImg.height = 0;
	fullImgPanel.style.visibility = "hidden";
	// A hidden, viewport-sized panel can still enlarge the document scroll area.
	fullImgPanel.style.display = "none";
}


function SaveImage(source)
{
  var bin_objects=document.all.binobj.getElementsByTagName("DIV");
  for(var i=0, cnt=0; i<bin_objects.length; i++)
  {

     if ("fbw-internal:#"+bin_objects[i].all.id.value == source)
     {
       window.external.SaveBinary(bin_objects[i].all.id.value, bin_objects[i].base64data, 1);
       break;
     }
  }
}

var xsltTemplateCache = {};
function LoadXSL(path, lang)
{
	TraceScript("J400", "operation=LoadXSL");
	var cacheKey = path + "\n" + lang;
	if (xsltTemplateCache[cacheKey]) {
		TraceScript("J405", "operation=LoadXSL cache-hit");
		return xsltTemplateCache[cacheKey];
	}
	TraceScript("J410", "operation=create XSLTemplate");
	var xslt = new ActiveXObject("Msxml2.XSLTemplate.6.0");
	TraceScript("J420", "operation=create FreeThreadedDOMDocument");
	var xsl = new ActiveXObject("Msxml2.FreeThreadedDOMDocument.6.0");
	xsl.async = false;
	TraceScript("J430", "operation=ResolveExternals");
	xsl.setProperty("ResolveExternals", true);

	TraceScript("J440", "operation=xsl.load");
	xsl.load(path);
	if(xsl.parseError.errorCode)
	{
		TraceScript("J450", "operation=XSL parse error");
		errCantLoad(xsl, path);
		return false;
	}
	var doc = xsl.documentElement;
	if(!doc)
	{
		TraceScript("J460", "operation=XSL documentElement missing");
		return false;
	}
	var imp = null;
	var child = doc.firstChild;
	while(child)
	{
		if(child.nodeType == 1 && (child.nodeName == "xsl:import" || child.nodeName == "xsl:include" || child.baseName == "import" || child.baseName == "include"))
		{
			imp = child;
			break;
		}
		child = child.nextSibling;
	}
	if(!imp || !imp.attributes)
	{
		TraceScript("J461", "operation=XSL import/include missing");
		return false;
	}
	var href = imp.attributes.getNamedItem("href");
	if(!href)
	{
		TraceScript("J462", "operation=XSL href missing");
		return false;
	}
	TraceScript("J470", "operation=language XSL");
	if(lang == "russian") href.nodeValue = "rus.xsl";
	if(lang == "english") href.nodeValue = "eng.xsl";
	if(lang == "ukrainian") href.nodeValue = "ukr.xsl";
	TraceScript("J480", "operation=stylesheet assignment");
	xslt.stylesheet = xsl;
	xsltTemplateCache[cacheKey] = xslt;
	TraceScript("J499", "operation=LoadXSL success");
	return xslt;
}
function ClickOnDesc()
{
  var srcName = event.srcElement.nodeName;
  if (srcName=="FIELDSET" || srcName=="LABEL" || srcName=="DIV" ||srcName=="LEGEND") {
    document.body.focus();
  }
}

function ShowCoverImage(prntEl,fullImg)
{
 if (!prntEl) return;
 var list=prntEl.getElementsByTagName("SELECT");
 if (!list[0] || !list[0].value) return;
 if (fullImg)
  ShowFullImage("fbw-internal:"+list[0].value);
 else
  ShowPrevImage("fbw-internal:"+list[0].value);
}

function TransformXML(xslt, dom)
{
	TraceScript("J500", "operation=TransformXML");
	TraceScript("J510", "operation=fbw_body");
	var body = document.getElementById("fbw_body");
	if(!body) { TraceScript("J512", "operation=fbw_body missing"); return false; }
	TraceScript("J511", "operation=fbw_desc");
	var desc = document.getElementById("fbw_desc");
	if(!desc) { TraceScript("J513", "operation=fbw_desc missing"); return false; }

	TraceScript("J520", "operation=createProcessor");
	var proc=xslt.createProcessor();
	TraceScript("J521", "operation=input assignment");
	proc.input=dom;
	proc.addParameter("descriptionLayout", descriptionLayout);
	TraceScript("J530", "operation=description mode");
	proc.setStartMode("description");
	TraceScript("J531", "operation=description transform");
	proc.transform();
	TraceScript("J532", "operation=description output");
	TraceScript("J540", "operation=desc.innerHTML");
	desc._descriptionSections = null;
	desc.innerHTML=proc.output;
	TraceScript("J550", "operation=PutBinaries");
	PutBinaries(dom);
	TraceScript("J560", "operation=SetupDescription");
	SetupDescription(desc);
	TraceScript("J570", "operation=onclick");
	desc.onclick=ClickOnDesc;
	TraceScript("J580", "operation=body mode");
	proc.setStartMode("body");
	TraceScript("J581", "operation=body transform");
	proc.transform();
	TraceScript("J582", "operation=body output");
	TraceScript("J590", "operation=body.innerHTML");
	HideNotePreview();
	body.innerHTML=proc.output;
	TraceScript("J595", "operation=InflateParagraphs");
	window.external.InflateParagraphs(body);
	// One delegated handler set is enough for all current and future note links.
	InitNotePreview(body);
	TraceScript("J597", "operation=document properties");
	document.urlprefix="fbw-internal:";
	TraceScript("J599", "operation=TransformXML success");
	return true;
}
function ShowDescElements()
{
  TraceScript("J800", "operation=ShowDescElements");
  var desc = document.getElementById("fbw_desc");
  if(!desc) { TraceScript("J890", "operation=ShowDescElements description missing"); return false; }
  var shown=0; var spans = desc.getElementsByTagName("SPAN");
  TraceScript("J810", "operation=SPAN count; count=" + spans.length);
  for(var i=0; i < spans.length; i++)
  {
    var elem_id = spans[i].getAttribute("id");
    if(elem_id)
    {
      TraceVerboseOperation("J820", "operation=GetExtendedStyle; index=" + i);
      var extendedStyle=window.external.GetExtendedStyle(elem_id);
      TraceVerboseOperation("J821", "operation=GetExtendedStyle result; index=" + i);
      TraceVerboseOperation("J830", "operation=ShowElement; index=" + i);
      ShowElement(elem_id, extendedStyle);
      TraceVerboseOperation("J831", "operation=ShowElement result; index=" + i);
      shown++;
    }
  }
  TraceDiagnosticSummary("J899", "operation=ShowDescElements success; element-count=" + spans.length + "; element-shown=" + shown);
  return true;
}
function LoadFromDOM(dom, lang)
{
	TraceScript("J300", "operation=LoadFromDOM");
	TraceScript("J310", "operation=namespaces");
	dom.setProperty("SelectionNamespaces", "xmlns:fb='"+fbNS+"' xmlns:xlink='"+xlNS+"'");
	TraceScript("J320", "operation=GetStylePath");
	var xpath=window.external.GetStylePath()+"\\fb2.xsl";
	TraceScript("J321", "operation=GetStylePath result");
	TraceScript("J330", "operation=LoadXSL");
	var xsl=LoadXSL(xpath, lang);
	TraceScript("J331", "operation=LoadXSL result");
	if(!xsl) return false;
	TraceScript("J340", "operation=TransformXML");
	var ret=TransformXML(xsl, dom);
	TraceScript("J341", "operation=TransformXML result");
	if(!ret) return false;
	TraceScript("J350", "operation=ShowDescElements");
	if(!ShowDescElements()) return false;
	InitDescriptionUI(document.getElementById("fbw_desc"));
	TraceScript("J351", "operation=ShowDescElements result");
	TraceScript("J399", "operation=LoadFromDOM success");
	return true;
}
function XmlFromText(text)
{
	var xml = new ActiveXObject("Msxml2.DOMDocument.6.0"); // 4.0->6.0 TaF issues 201
	xml.async=false;
	xml.preserveWhiteSpace = true;
	xml.loadXML(text);
	if(xml.parseError.errorCode)
	{
		//errCantLoad(xml, path);
		return xml.parseError;
	}
	return xml;
}

function recursiveChangeNbsp(elem, repChar) {
 var el=elem;
 while (el) {
  if (el.nodeType==3) el.nodeValue=el.nodeValue.replace(/\u00A0/g, repChar);
  if (el.nodeType==1 && el.firstChild) recursiveChangeNbsp(el.firstChild, repChar);
  el=el.nextSibling;
 }
}

function apiLoadFB2(path, lang, logicalPath)
{
	var displayPath=logicalPath || path;
	diagnosticFailureStage = "";
	diagnosticOperationStage = "J100";
	diagnosticLastTraceEvent = "J100";
	TraceDiagnosticEvent("J100", "operation=apiLoadFB2");
	var css=null;
	var css_filename="";
	var result=false;
	var operationCompleted=false;
	var cssRestoreFailed=false;
	var originalException=null;
	var controlledFailure={ apiLoadFB2: true };
	var controlledFailureOccurred=false;
	var primaryFailureStage="";
	function failControlled()
	{
		primaryFailureStage=diagnosticOperationStage;
		throw controlledFailure;
	}
	try
	{
	if(IsDiagnosticFaultInjectionEnabled("api-load-exception"))
	{
		TraceScript("J105", "operation=apiLoadFB2 injected exception");
		throw new Error("diagnostic fault injection");
	}
	if(IsDiagnosticFaultInjectionEnabled("api-load-private-exception"))
	{
		TraceScript("J105", "operation=apiLoadFB2 injected private exception");
		throw new Error("BOOK_PRIVATE_PLAIN_TEXT_MARKER; BOOK_PRIVATE_XML_<tag>; BASE64_PRIVATE_MARKER; C:\\Users\\PrivateUser\\secret.fb2; \\\\server\\private\\book.fb2; file:///C:/private/book.fb2; field=value; level=info");
	}
	if(IsDiagnosticFaultInjectionEnabled("api-load-return-false"))
	{
		TraceScript("J106", "operation=apiLoadFB2 injected false result");
		failControlled();
	}
	TraceScript("J101", "operation=css lookup");
	css=document.getElementById("css");
	if(!css) { TraceScript("J104", "operation=CSS element missing"); failControlled(); }
	TraceScript("J102", "operation=save css href");
	css_filename = css.href;
	TraceScript("J103", "operation=disable css");
	css.href="";
	TraceScript("J110", "operation=create DOMDocument");
	var xml = new ActiveXObject("Msxml2.DOMDocument.6.0");
	TraceScript("J111", "operation=configure DOM");
	xml.async=false;
	xml.preserveWhiteSpace = true;
	TraceScript("J112", "operation=xml.load");
	xml.load(path);
	TraceScript("J113", "operation=xml.load result");
	if(xml.parseError.errorCode)
	{
		TraceScript("J114", "operation=XML parse error");
		errCantLoad(xml, displayPath);
		failControlled();
	}

	TraceScript("J120", "operation=read declaration");
	var pi = xml.firstChild;
	var encoding;
	if (pi)
	{
		var attr = pi.attributes;
		if (attr)
		{
			var enc = attr.getNamedItem("encoding");
			if(enc)
			{
				TraceScript("J121", "operation=encoding present");
				encoding = enc.text;
			}
		}
	}

	TraceScript("J130", "operation=SelectionNamespaces");
	xml.setProperty("SelectionNamespaces", "xmlns:fb='"+fbNS+"' xmlns:xlink='"+xlNS+"'");
	TraceScript("J140", "operation=GetNBSP");
	var nbspChar=window.external.GetNBSP();
	TraceScript("J141", "operation=GetNBSP result; present=" + (nbspChar ? 1 : 0));
	if(nbspChar && nbspChar!="\u00A0")
		{
			TraceScript("J150", "operation=annotation NBSP conversion");
			var sel=xml.selectSingleNode("/fb:FictionBook/fb:description/fb:title-info/fb:annotation");
			if(sel) recursiveChangeNbsp(sel,nbspChar);
			TraceScript("J151", "operation=history NBSP conversion");
			sel=xml.selectSingleNode("/fb:FictionBook/fb:description/fb:document-info/fb:history");
			if(sel) recursiveChangeNbsp(sel,nbspChar);
			TraceScript("J152", "operation=body NBSP conversion");
			sel=xml.selectSingleNode("/fb:FictionBook/fb:body");
			while(sel) {
				if(sel.nodeName=="body") recursiveChangeNbsp(sel,nbspChar);
				sel=sel.nextSibling;
			}
	}
	TraceScript("J160", "operation=LoadFromDOM");
	if (!LoadFromDOM(xml, lang))
	{
		TraceScript("J161", "operation=LoadFromDOM result");
		MsgBox("Error: can't prepare document for Body mode.");
		failControlled();
	}
	TraceScript("J161", "operation=LoadFromDOM result");
	if(IsDiagnosticFaultInjectionEnabled("controlled-load-failure"))
	{
		TraceDiagnosticEvent("J162", "operation=LoadFromDOM injected controlled failure");
		failControlled();
	}
	TraceScript("J170", "operation=selection.empty");
	document.selection.empty();
	TraceScript("J171", "operation=selection.empty result");
	TraceScript("J180", "operation=fbw_desc lookup");
	var desc = document.getElementById("fbw_desc");
	if(!desc) { TraceScript("J182", "operation=description element missing"); failControlled(); }
	TraceScript("J181", "operation=diID lookup");
	var id=desc.all.diID;
	if(!id) { TraceScript("J183", "operation=diID element missing"); failControlled(); }
	if(id)
	if(IsDescriptionNewTemplate(displayPath))
	{
		InitializeNewDescription(desc);
		TraceScript("J190", "operation=GetUUID");
		id.value=window.external.GetUUID();
		DescriptionValidate();
		TraceScript("J191", "operation=GetUUID result");
	}
	else id.value=id.value;
	TraceScript("J200", "operation=apiShowDesc");
	if(!apiShowDesc(false)) failControlled();
	TraceScript("J201", "operation=apiShowDesc result");
	result=encoding;
	operationCompleted=true;
	}
	catch(e)
	{
		if(e===controlledFailure)
		{
			controlledFailureOccurred=true;
			diagnosticFailureStage=primaryFailureStage || diagnosticOperationStage;
		}
		if(e!==controlledFailure)
		{
			originalException=e;
			diagnosticFailureStage = diagnosticFailureStage || diagnosticOperationStage || "J900";
			DiagError("J900", "apiLoadFB2", e);
		}
	}
	finally
	{
		if(css)
		{
			TraceDiagnosticEvent("J210", "operation=CSS restore begin");
			try { if(IsDiagnosticFaultInjectionEnabled("css-restore-failure")) throw new Error("diagnostic fault injection"); css.href = css_filename; TraceDiagnosticEvent("J211", "operation=CSS restore success"); }
			catch(e)
			{
				TraceDiagnosticEvent("J212", "level=error; operation=CSS restore failure; load-result=" + (operationCompleted ? "success" : "failure"));
				cssRestoreFailed=true;
				if(!originalException && !controlledFailureOccurred)
				{
					diagnosticFailureStage="J212";
					DiagError("J212", "CSS restore", e);
				}
			}
		}
	}
	if(originalException)
		throw originalException;
	if(cssRestoreFailed)
		return false;
	if(operationCompleted)
	{
		TraceScript("J299", "operation=apiLoadFB2 success");
		diagnosticFailureStage = "";
		return result;
	}
	return false;
}
function apiShowDesc(state)
{
	TraceScript("J850", "operation=apiShowDesc; requested-state=" + (state ? 1 : 0));
	TraceScript("J851", "operation=body present");
	var body=document.getElementById("fbw_body");
	if(!body) { TraceScript("J854", "operation=apiShowDesc body missing"); return false; }
	TraceScript("J852", "operation=desc present");
	var desc=document.getElementById("fbw_desc");
	if(!desc) { TraceScript("J855", "operation=apiShowDesc description missing"); return false; }
	if(state)
	{
		document.ScrollSave=document.body.scrollTop;
		desc.style.display="block";
		document.body.scrollTop=0;
		body.style.display="none";
		DescriptionResize();
	}
	else
	{
		desc.style.display="none";
		body.style.display="block";
		document.body.scrollTop=document.ScrollSave;
	}
	TraceScript("J853", "operation=display changed");
	TraceScript("J859", "operation=apiShowDesc completed");
	return true;
}
function apiRunCmd(path, htmlFolder, charset)
{
	var executionToken=window.external._EnterScriptContext(String(path));
	try {

	var messageTemplate=GetLocalizedStringOrDefault("fbe.script.run_failed", "The script\n\n{path}\n\ncould not be started because it could not be loaded successfully.\n\nThe file may no longer be at its expected path.");
	var scriptPath=String(path==null ? "" : path);
	window.msgText=messageTemplate.indexOf("{path}")>=0 ? messageTemplate.replace("{path}", scriptPath) : messageTemplate+"\n\n"+scriptPath;
	window.Run=function() {
	  if (window.msgText) alert(window.msgText);
	};
	if(!LoadUserCommandScript(path, charset))
		return;
        Run();
	window.Run=function() {
	  if (window.msgText) alert(window.msgText);
	};

	} finally { window.external._LeaveScriptContext(executionToken); }
}

// MSHTML may reuse a script whose src has already been assigned. Replace the
// element so every command invocation is a fresh load; the id remains stable
// for legacy combo scripts which address userCmd directly.
function LoadUserCommandScript(path, charset)
{
	var executionToken=window.external._EnterScriptContext(String(path));
	try {

	var previous=document.getElementById("userCmd");
	if(!previous || !previous.parentNode)
		return null;
	var script=document.createElement("script");
	script.id="userCmd";
	script.language="JavaScript";
	script.type="text/javascript";
	if(charset) script.charset=charset;
	previous.parentNode.replaceChild(script,previous);
	script.src="file:///"+path;
	return script;

	} finally { window.external._LeaveScriptContext(executionToken); }
}

function apiGetClassName(path)
{
	var executionToken=window.external._EnterScriptContext(String(path));
	try {

	if(!LoadUserCommandScript(path))
		return;
	return GetClassName();

	} finally { window.external._LeaveScriptContext(executionToken); }
}

function apiGetTitle(path)
{
	var executionToken=window.external._EnterScriptContext(String(path));
	try {

	if(!LoadUserCommandScript(path))
		return;
	return GetTitle();

	} finally { window.external._LeaveScriptContext(executionToken); }
}

function apiProcessCmd(path)
{
	var executionToken=window.external._EnterScriptContext(String(path));
	try {

	if(!LoadUserCommandScript(path))
		return;
	ProcessCmd();

	} finally { window.external._LeaveScriptContext(executionToken); }
}

function SetInFrame(range, tagName, className)
{
	range.pasteHTML("<" + tagName + " classname=\"" + className + "\">" + range.htmlText + "</" + tagName + ">");
}

function RemoveOuterTags(node)
{
	node.removeNode(false);
}

function apiCleanUp(className)
{
	var divs = document.all.tags("DIV");
	for(var i=divs.length-1; i >= 0; i--)
	{
		if (divs[i].className == className)
			RemoveOuterTags(divs[i]);
	}
}

function apiCheckRunnableScript()
{
  try
  {
    if(Run)
    {
      return true;
    }
  }
  catch(e)
  {
    return false;
  }
}

function apiSetFastMode(fast)
{
  var css=document.getElementById("css");
  if(!css)
    return;

  if(fast)
    css.href="main_fast.css";
  else
    css.href="main.css";
}

//======================================
// Internal private functions
//======================================

function MsgBox(str)
{
	window.external.MsgBox(str);
}
function AskYesNo(str)
{
	return window.external.AskYesNo(str);
}
function InputBox(msg, value, result)
{
	result.$ = window.external.InputBox(msg, "FBE script message", value);
	return window.external.GetModalResult();
}
//--------------------------------------
// Our own, less scary error handler

function SanitizeDiagnosticFileName(url)
{
	var value = "";
	try { value = String(url || "").replace(/^.*[\\\/]/, ""); } catch(ignore) {}
	if(value.length > 128) value = value.substr(0, 128);
	var safe = "";
	for(var index = 0; index < value.length; index++)
	{
		var character = value.charAt(index);
		if((character >= "a" && character <= "z") || (character >= "A" && character <= "Z") || (character >= "0" && character <= "9") || character == "." || character == "-" || character == "_") safe += character;
		else safe += "_";
	}
	return safe;
}

function errorHandler(msg,url,lno)
{
	var fileName = SanitizeDiagnosticFileName(url);
	var line = "";
	if(typeof lno == "number" && isFinite(lno)) line = lno;
	diagnosticFailureStage = diagnosticFailureStage || diagnosticOperationStage || "J900";
	DiagError("J900", "window.onerror", { number: 0, name: "Error", description: msg, message: msg, lineNumber: lno });
	var documentMode = "";
	var compatMode = "";
	try { documentMode = document.documentMode; compatMode = document.compatMode; } catch(ignore) {}
	TraceDiagnosticEvent("J901", "operation=window.onerror; file-present=" + (fileName ? 1 : 0) + "; file-name=" + fileName + "; line=" + line + "; document-mode=" + documentMode + "; compat-mode=" + compatMode + "; trace-bridge-state=" + diagnosticTraceBridgeState);
	if(!IsDiagnosticFaultInjectionActive()) try { MsgBox("Error at line "+lno+":\n"+msg+" "); } catch(ignore) {}
	return true;
}

function errCantLoad(xd,file)
{
	MsgBox("\""+file+"\" loading error:\n\n"+xd.parseError.reason+"  \nLine: "+xd.parseError.line+", char: "+xd.parseError.linepos+"\n");
}

function FillImageList(list, bin_objects)
{
	if(list.id=='href')
	{
		var cover=list.value;

		list.innerHTML='';

		var newopt=document.createElement('option');
		newopt.value='';
		newopt.innerHTML='';
		list.appendChild(newopt);

		for(var j=0; j<bin_objects.length; j++)
		{
			var contentType=String(bin_objects[j].all.type.value || "").toLowerCase();
			if(contentType=="image/jpg" || contentType=="image/pjpeg") contentType="image/jpeg";
			var pic_id=bin_objects[j].all.id.value;
			
			//if(pic_id.toLowerCase().indexOf(".jpg")==-1 && pic_id.toLowerCase().indexOf(".png")==-1  && pic_id.toLowerCase().indexOf(".jpeg")==-1)
			if((typeof contentType!="string") || pic_id=="" || (contentType.toLowerCase()!="image/jpeg" && contentType.toLowerCase()!="image/png"))
			{
				continue;
			}

			var newopt=document.createElement('option');
			newopt.value='#'+pic_id;
			newopt.innerHTML=pic_id;

			if(newopt.value==cover)
			{
				newopt.selected=true;
			}

			list.appendChild(newopt);
		}
		if(cover && list.value != cover) { var missing=document.createElement("OPTION"); missing.value=cover; missing.innerText=cover; list.appendChild(missing); list.value=cover; }
	}
}

function FillLists()
{
	var bin_objects = document.all.binobj.getElementsByTagName("DIV");

	var lists=document.all.tiCover.getElementsByTagName("select"); // drop-down lists
	for(var i=0; i<lists.length; i++)
	{
		FillImageList(lists[i], bin_objects);
	}

	var stilists=document.all.stiCover.getElementsByTagName("select"); // drop-down lists
	for(var i=0; i<stilists.length; i++)
	{
		FillImageList(stilists[i], bin_objects);
	}
}

function SelectDescriptionLanguage(list)
{
 if(!list) return;
 var selected=list.value, options=list.getElementsByTagName("OPTION");
 var original=null, known=null;
 for(var i=0;i<options.length;i++)
 {
  if(options[i].innerHTML=="-org-") original=options[i];
  else if(options[i].value==selected) known=options[i];
 }
 if(known)
 {
  if(original) original.removeNode(true);
  known.selected=true;
 }
 else if(original)
 {
  // The catalog is a convenience, not a whitelist of valid FB2 language tags.
  original.innerText=selected;
  original.selected=true;
 }
}

function SelectLanguages()
{
 var ids=["tiLang","tiSrcLang","stiLang","stiSrcLang"];
 for(var i=0;i<ids.length;i++) SelectDescriptionLanguage(document.getElementById(ids[i]));
}
//-----------------------------------------------
// Generates new ID and puts it into the field

function NewDocumentID(msg)
{
	var desc=document.getElementById("fbw_desc");
	if(!desc)
		return;

	if(AskYesNo(msg))
	{
		desc.all.diID.value=window.external.GetUUID();
	}
}
//-----------------------------------------------
// Puts lines to space cloned elements in description
// Also prevents deletion of last element

function PutSpacers(obj)
{
 if(obj.id=="binobj") return;

 var divs=obj.getElementsByTagName("DIV"); if(divs.length==0) return;

 var s=0;

 if(obj.nodeName!="DIV")
 {
  divs[0].style.marginTop="0";
  divs[0].style.paddingTop="0";
  divs[0].style.borderTop="none";
  divs[0].all.del.disabled=(divs.length<=1);
  s=1;
 }

 for(var i=s; i<divs.length; i++)
 {
  divs[i].all.del.disabled=false;
  divs[i].style.marginTop="0.2em";
  divs[i].style.paddingTop="0.2em";
  divs[i].style.borderTop="solid #D0D0BF 1px";
 }
}
//-----------------------------------------------
// Puts spacers between all clones in description

function InitFieldsets()
{
 var fss=document.body.getElementsByTagName("fieldset");
 for(var i=0; i<fss.length; i++) PutSpacers(fss[i]);
}
//-----------------------------------------------
// Initializes default fields if needed.
// Initializes fieldsets.

function SetDocumentVersion(desc)
{
	var version=desc.all.diVersion;
	if(version && version.value=="")
		version.value="1.0";
}

function SetProgramUsed(desc)
{
  var prgs = desc.all.diProgs;
  var ver;

  if(prgs)
  {
    ver = window.external.GetProgramVersion();

    if(prgs.value == "")
      prgs.value = ver;
    else
    {
      if(prgs.value.search("FictionBook Editor") == -1)
      {
        prgs.value += ", "+ver;
      }
    }
  }
}

function SetCurrentDate(desc)
{
	var date=new Date();
	var ms=Array("January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December");

	var year=date.getFullYear();
	var day=date.getDate();
	if(day<10)
		day="0"+day;

	var  mon=date.getMonth() + 1;

	if(mon<10)
		mon="0"+mon;

	var dt=desc.all.diDate;
	if(dt && dt.value.length==0)
		dt.value = day+" "+ms[mon - 1]+" "+year;

	var dv=desc.all.diDateVal;
	if(dv && dv.value.length==0)
		dv.value = year +"-"+mon  +"-"+day;
}



function IsDescriptionNewTemplate(path)
{
 var normalized=String(path || "").replace(/[\\\/]+/g,"\\").toLowerCase();
 if(!/(^|\\)blank\.fb2$/.test(normalized)) return false;
 var template;
 try { template=String(window.external.GetStylePath()).replace(/[\\\/]+$/,"")+"\\blank.fb2"; }
 catch(ignore) { return false; }
 return normalized==template.replace(/[\\\/]+/g,"\\").toLowerCase();
}

function InitializeNewDescription(desc)
{
	TraceScript("J710", "operation=SetDocumentVersion");
	SetDocumentVersion(desc);
	TraceScript("J720", "operation=GetProgramVersion");
	SetProgramUsed(desc);
	TraceScript("J730", "operation=SetCurrentDate");
	SetCurrentDate(desc);
}

function SetupDescription(desc)
{
	// Existing metadata is user data, including empty optional fields. Defaults
	// belong only to the new-document path, never to display or validation.
	TraceScript("J700", "operation=SetupDescription");
	TraceScript("J750", "operation=InitFieldsets");
	InitFieldsets();
	TraceScript("J760", "operation=SelectLanguages");
	SelectLanguages();
	DescriptionStyleButtons(desc);
	TraceScript("J799", "operation=SetupDescription success");
}//-----------------------------------------------

//function InflateParagraphs(e)
//{
// var ps = e.getElementsByTagName("P");
// for(var i=0; i<ps.length; i++) window.external.inflateBlock(ps[i])=true;

// for(var i=0; i<ps.length; i++)
//  if(ps[i].innerHTML=="") ps[i].innerHTML="&nbsp;";
//}
//-----------------------------------------------

function InflateIt(elem) // Seems the same as above...
{
 if(!elem || elem.nodeType!=1) return;

 if(elem.tagName=="P"){  window.external.inflateBlock(elem)=true; return; }

 elem=elem.firstChild;

 while(elem){ InflateIt(elem); elem=elem.nextSibling; }
}
//--------------------------------------
// Shows Genres menu and gets the genre

function GetGenre(d)
{
 var v=window.external.GenrePopup(d, window.event.screenX, window.event.screenY);
 if(v.length>0){ d.parentNode.all.genre.value=v; }
}
//--------------------------------------

function dClone(obj)
{
 var qn=obj.cloneNode(true); var cn=qn.firstChild;

 while(cn)
 {
  var nn=cn.nextSibling; if(cn.nodeName=="DIV") cn.removeNode(true);

  cn=nn;
 }
 return qn;
}
//-------------------

function Remove(obj)
{
	var pic_id = "";

	if(obj.base64data != null) // this is a binary object
	{
		pic_id = GetBinaryInput(obj, "id").value;
		var referenced;
		try { referenced=BinaryIsReferenced(pic_id); }
		catch(e)
		{
			MsgBox(LocalizedBinaryMessage("fbe.binary.delete.check_failed"));
			return;
		}
		if(referenced)
		{
			MsgBox(LocalizedBinaryMessage("fbe.binary.delete.referenced"));
			return;
		}

		if(IsImageBinaryType(obj.all.type.value))
		{
			var idx = -1;
			for(var i = 0; i < ImagesInfo.length; ++i)
			{
				if(ImagesInfo[i].id == obj.all.id.value)
				{
					idx = i;
					break;
				}
			}

			if(idx != -1)
				ImagesInfo.splice(idx, 1);
		}

	}

 // delete

 var pn=obj.parentNode; obj.removeNode(true); PutSpacers(pn);
 BinaryStoreChanged("remove", obj);

 // update images

 if(pic_id!="")
 {
  pic_id="fbw-internal:#"+pic_id;

  var imgs=document.getElementsByTagName("IMG");

  for(var i=0; i<imgs.length; i++)
   if(imgs[i].src==pic_id)
   {
    imgs[i].src=""; imgs[i].src=pic_id; // force image update
   }

  // update Cover lists

  FillLists();
 }
}
//-------------------

function Clone(obj)
{
 obj.insertAdjacentElement("afterEnd",dClone(obj)); PutSpacers(obj.parentNode);

 if(obj.id=='href') FillLists(); // this is the Cover field
}
//-------------------

function ChildClone(obj)
{
 obj.appendChild(dClone(obj)); PutSpacers(obj);
}
//--------------------------------------

//// == DESC == ///////////////////////////////////////////////////////////////////


var fbNS="http://www.gribuser.ru/xml/fictionbook/2.0";
var xlNS="http://www.w3.org/1999/xlink";

function Indent(node,len)
{
 var s="\r\n"; while (len--) s=s+" ";

 node.appendChild(node.ownerDocument.createTextNode(s));
}

function SetAttr(node,name,val)
{
 var an=node.ownerDocument.createNode(2,name,fbNS);
 an.appendChild(node.ownerDocument.createTextNode(val));
 node.setAttributeNode(an);
}

function MakeText(node,name,val,force,indent)
{
 var ret=true;
 if(val.length==0)
 {
  if(!force) return false;
  ret=false;
 }
 var tn=node.ownerDocument.createNode(1,name,fbNS);
 tn.appendChild(node.ownerDocument.createTextNode(val));
 Indent(node,indent);
 node.appendChild(tn);
 return ret;
}

function MakeAuthor(node,name,dv,force,indent)
{
 var au=node.ownerDocument.createNode(1,name,fbNS);
 var added=false;
 if(dv.all.first.value.length>0 || dv.all.middle.value.length>0 ||
      dv.all.last.value.length>0 || dv.all.nick.value.length==0)
 {
  added=MakeText(au,"first-name",dv.all.first.value,true,indent+1) || added;
  added=MakeText(au,"middle-name",dv.all.middle.value,false,indent+1) || added;
  added=MakeText(au,"last-name",dv.all.last.value,true,indent+1) || added;
 }
 added=MakeText(au,"nickname",dv.all.nick.value,false,indent+1) || added;
 added=MakeText(au,"home-page",dv.all.home.value,false,indent+1) || added;
 added=MakeText(au,"email",dv.all.email.value,false,indent+1) || added;
 // id follows the contact fields in FB2, also for nickname-only authors.
 var authorSpans=dv.getElementsByTagName("SPAN");
 for(var authorIndex=0;authorIndex<authorSpans.length;authorIndex++)
  if(authorSpans[authorIndex].id=="id" || authorSpans[authorIndex].className=="id")
  { added=MakeText(au,"id",String(authorSpans[authorIndex].getAttribute("value") || ""),false,indent+1) || added; break; }

 if(added || force)
 {
  Indent(node,indent); node.appendChild(au); Indent(au,indent);
 }

 return added;
}

function MakeDate(node,d,v,indent,force)
{
 if(!d.length && !v.length && !force) return false;
 var dt=node.ownerDocument.createNode(1,"date",fbNS);
 var added=false;
 if(v.length>0){ added=true; SetAttr(dt,"value",v); }
 if(d.length>0) added=true;

 dt.appendChild(node.ownerDocument.createTextNode(d));
 Indent(node,indent);
 node.appendChild(dt);
 return added;
}

function DescriptionDateWasPresent(id)
{
 var field=DescriptionControls().all[id];
 return field && field.getAttribute("data-fbe-date-present")=="1";
}

function MakeSeq2(xn,hn,indent)
{
 var added=false;
 var newxn=xn.ownerDocument.createNode(1,"sequence",fbNS);
 var name=hn.all("name",0).value;
 var num=hn.all("number",0).value;
 SetAttr(newxn,"name",name);
 if(num.length > 0) SetAttr(newxn,"number",num);

 for(var cn=hn.firstChild;cn;cn=cn.nextSibling)
   if(cn.nodeName=="DIV")  added=MakeSeq2(newxn,cn,indent+1) || added;

 if(added || name.length>0 || num.length>0)
 {
   Indent(xn,indent);
   xn.appendChild(newxn);
   if(newxn.hasChildNodes()) Indent(newxn,indent);
   added=true;
 }
 return added;
}

function MakeSeq(xn,hn,indent)
{
  var added=false;
  for(var cn=hn.firstChild;cn;cn=cn.nextSibling)
    if(cn.nodeName=="DIV") added=MakeSeq2(xn,cn,indent) || added;

  return added;
}

function IsEmpty(ii)
{
  if(!ii || !ii.hasChildNodes()) return true;

  for(var v=ii.firstChild;v;v=v.nextSibling)
    if(v.nodeType==1 && v.baseName!="empty-line") return false;

  return true;
}

var fbeAnalysisSnapshot = false;
var fbeAnalysisMarkup = null;
var fbeAnalysisDescription = null;

function DescriptionControls()
{
 // Retrieving a live SELECT can materialize selected attributes. Keep every
 // description control lookup detached throughout a read-only snapshot.
 return fbeAnalysisSnapshot ? fbeAnalysisDescription : document;
}

function ReadDescriptionValue(control)
{
 return control.value;
}

function ReadLanguageValue(id)
{
 if(!fbeAnalysisSnapshot) return document.all[id].value;
 // Reading a live SELECT value makes MSHTML add an implicit selected attribute.
 // Parse only the detached description markup for a read-only analysis snapshot.
 var pattern=new RegExp("<SELECT\\b[^>]*\\bid=(?:\\\""+id+"\\\"|'"+id+"'|"+id+")(?:\\s|>)[\\s\\S]*?</SELECT>","i");
 var match=pattern.exec(fbeAnalysisMarkup);
 if(!match) return "";
 var detached=document.createElement("DIV");
 detached.innerHTML=match[0];
 var select=detached.getElementsByTagName("SELECT")[0];
 return select ? select.value : "";
}

function MakeTitleInfo(doc,desc,ann,indent)
{
 var ti=doc.createNode(1,"title-info",fbNS);
 Indent(desc,indent);
 desc.appendChild(ti);

 // genres
 var list=DescriptionControls().all.tiGenre.getElementsByTagName("DIV");
 for(var i=0; i<list.length; i++)
 {
  var ge=doc.createNode(1,"genre",fbNS);
  if (list.item(i).all.match)
  {
    var match=list.item(i).all.match.value;
    if(match.length>0 && match!="100") SetAttr(ge,"match",match);
  }

  ge.appendChild(doc.createTextNode(list.item(i).all.genre.value));
  Indent(ti,indent+1);
  ti.appendChild(ge);
 }

 // authors
 var added=false;
 list=DescriptionControls().all.tiAuthor.getElementsByTagName("DIV");
 for(var i=0; i<list.length; i++) added=MakeAuthor(ti,"author",list.item(i),false,indent+1) || added;

 if(!added && list.length>0) MakeAuthor(ti,"author",list.item(0),true,indent+1);

 MakeText(ti,"book-title",DescriptionControls().all.tiTitle.value,true,indent+1);

 // annotation, will be filled by body.xsl
 if(!IsEmpty(ann)){ Indent(ti,indent+1); ti.appendChild(ann); }

 MakeText(ti,"keywords",DescriptionControls().all.tiKwd.value,false,indent+1);
 MakeDate(ti,DescriptionControls().all.tiDate.value,DescriptionControls().all.tiDateVal.value,indent+1,DescriptionDateWasPresent("tiDate"));

 // coverpage images
 list=DescriptionControls().all.tiCover.getElementsByTagName("DIV");
 var cp=doc.createNode(1,"coverpage",fbNS);
 for(var i=0; i<list.length; i++)
   if(ReadDescriptionValue(list.item(i).all.href).length>0)
   {
     var xn=doc.createNode(1,"image",fbNS);
     var an=doc.createNode(2,"l:href",xlNS);
     an.appendChild(doc.createTextNode(ReadDescriptionValue(list.item(i).all.href)));
     xn.setAttributeNode(an);
     Indent(cp,indent+2);
     cp.appendChild(xn);
   }
 if(cp.hasChildNodes())
 {
   Indent(ti,indent+1); ti.appendChild(cp);
 }

 MakeText(ti,"lang",ReadLanguageValue("tiLang"),false,indent+1);
 MakeText(ti,"src-lang",ReadLanguageValue("tiSrcLang"),false,indent+1);

 // translator
 list=DescriptionControls().all.tiTrans.getElementsByTagName("DIV");

 for(var i=0; i<list.length; i++) MakeAuthor(ti,"translator",list.item(i),false,indent+1);

 // sequence
 MakeSeq(ti,DescriptionControls().all.tiSeq,indent+1);
 Indent(ti,indent);
}

function IsAuthorExist(node,name,dv) {
  var au=node.ownerDocument.createNode(1,name,fbNS);
  var exist=false;
  if (dv.all.first.value.length>0 || dv.all.middle.value.length>0 ||
      dv.all.last.value.length>0 || dv.all.nick.value.length==0) {
    exist=IsTextExist(dv.all.first.value) || exist;
    exist=IsTextExist(dv.all.middle.value) || exist;
    exist=IsTextExist(dv.all.last.value) || exist;
  }
  exist=IsTextExist(dv.all.nick.value) || exist;
  exist=IsTextExist(dv.all.home.value) || exist;
  exist=IsTextExist(dv.all.email.value) || exist;

  return exist;
}

function IsTextExist(val) {
  if (val.length==0)
      return false;
  return true;
}

function IsSeqExist2(xn,hn) {
  var exist=false;
  var newxn=xn.ownerDocument.createNode(1,"sequence",fbNS);
  var name=hn.all("name",0).value;
  var num=hn.all("number",0).value;
  SetAttr(newxn,"name",name);
  if (num.length > 0)
    SetAttr(newxn,"number",num);
  for (var cn=hn.firstChild;cn;cn=cn.nextSibling) {
    if (cn.nodeName=="DIV")
      exist=IsSeqExist2(newxn,cn) || exist;
  }
  if (exist || name.length>0 || num.length>0)
    exist=true;

  return exist;
}

function IsSeqExist(xn,hn) {
  var exist=false;
  for (var cn=hn.firstChild;cn;cn=cn.nextSibling)
    if (cn.nodeName=="DIV")
      exist=IsSeqExist2(xn,cn) || exist;
  return exist;
}

function IsSTBFieldTextExist(sti,desc,doc) {
  var exist=false;

  // authors
  var list=DescriptionControls().all.stiAuthor.getElementsByTagName("DIV");
  for (var i=0;i<list.length;++i)
  {
    exist=IsAuthorExist(sti,"author",list.item(i)) || exist;
	}
  //if (!exist && list.length>0)
    //exist=false;

  // book title
  exist=IsTextExist(DescriptionControls().all.stiTitle.value) || exist;

  // genres
  list=DescriptionControls().all.stiGenre.getElementsByTagName("DIV");
  for (var i=0;i<list.length;++i) {
    var ge=doc.createNode(1,"genre",fbNS);
    var match=list.item(i).all.match.value;
    var g=list.item(i).all.genre.value;
    if (g.length>0)
      exist=true || exist;
    if (match.length>0 && match!="100"){
      exist=true || exist;
    }
  }

  // annotation, will be filled by body.xsl
  /*if (!IsEmpty(ann)) {
    exist=true;
  }*/

  // keywords
  exist=IsTextExist(DescriptionControls().all.stiKwd.value) || exist;

  // date
  var dt=sti.ownerDocument.createNode(1,"date",fbNS);
  if (DescriptionControls().all.stiDateVal.value.length>0)
    exist=true;
  if (DescriptionControls().all.stiDate.value.length>0)
    exist=true;

  // coverpage images
  list=DescriptionControls().all.stiCover.getElementsByTagName("DIV");
  var cp=doc.createNode(1,"coverpage",fbNS);
  for (var i=0;i<list.length;++i) {
    if (ReadDescriptionValue(list.item(i).all.href).length>0)
      exist=true;
  }
  if (cp.hasChildNodes())
    exist=true;

  // lang
  exist=IsTextExist(ReadLanguageValue("stiLang")) || exist;
  exist=IsTextExist(ReadLanguageValue("stiSrcLang")) || exist;

  // translator
  list=DescriptionControls().all.stiTrans.getElementsByTagName("DIV");
  for (var i=0;i<list.length;++i)
    exist=IsAuthorExist(sti,"translator",list.item(i)) || exist;

  // sequence
  exist=IsSeqExist(sti,DescriptionControls().all.stiSeq) || exist;

  return exist;
}

function MakeSourceTitleInfo(doc,desc,ann,indent)
{
	var sti=doc.createNode(1,"src-title-info",fbNS);
	if(IsSTBFieldTextExist(sti,desc,doc))
	{
		Indent(desc,indent);
		desc.appendChild(sti);

		// genres
		var list=DescriptionControls().all.stiGenre.getElementsByTagName("DIV");
		for (var i=0;i<list.length;++i)
		{
			var ge=doc.createNode(1,"genre",fbNS);
			var match=list.item(i).all.match.value;
			if (match.length>0 && match!="100")
				SetAttr(ge,"match",match);
			ge.appendChild(doc.createTextNode(list.item(i).all.genre.value));
			Indent(sti,indent+1);
			sti.appendChild(ge);
		}

		// authors
		var added=false;
		list=DescriptionControls().all.stiAuthor.getElementsByTagName("DIV");
		for (var i=0;i<list.length;++i)
			added=MakeAuthor(sti,"author",list.item(i),false,indent+1) || added;
		if (!added && list.length>0)
			MakeAuthor(sti,"author",list.item(0),true,indent+1);

		MakeText(sti,"book-title",DescriptionControls().all.stiTitle.value,true,indent+1);
		MakeText(sti,"keywords",DescriptionControls().all.stiKwd.value,false,indent+1);
		MakeDate(sti,DescriptionControls().all.stiDate.value,DescriptionControls().all.stiDateVal.value,indent+1,DescriptionDateWasPresent("stiDate"));

		// coverpage images
		list=DescriptionControls().all.stiCover.getElementsByTagName("DIV");
		var cp=doc.createNode(1,"coverpage",fbNS);
		for (var i=0;i<list.length;++i)
			if (ReadDescriptionValue(list.item(i).all.href).length>0)
			{
				var xn=doc.createNode(1,"image",fbNS);
				var an=doc.createNode(2,"l:href",xlNS);
				an.appendChild(doc.createTextNode(ReadDescriptionValue(list.item(i).all.href)));
				xn.setAttributeNode(an);
				Indent(cp,indent+2);
				cp.appendChild(xn);
			}
		if (cp.hasChildNodes())
		{
			Indent(sti,indent+1);
			sti.appendChild(cp);
		}

		MakeText(sti,"lang",ReadLanguageValue("stiLang"),false,indent+1);
		MakeText(sti,"src-lang",ReadLanguageValue("stiSrcLang"),false,indent+1);

		// translator
		list=DescriptionControls().all.stiTrans.getElementsByTagName("DIV");
		for (var i=0;i<list.length;++i)
		  MakeAuthor(sti,"translator",list.item(i),false,indent+1);

		// sequence
		MakeSeq(sti,DescriptionControls().all.stiSeq,indent+1);
		Indent(sti,indent);
	}
}

function MakeDocInfo(doc,desc,hist,indent)
{
 var added=false; var di=doc.createNode(1,"document-info",fbNS);

 // authors
 var i; var list=DescriptionControls().all.diAuthor.getElementsByTagName("DIV");

 for(i=0; i<list.length; i++) added=MakeAuthor(di,"author",list.item(i),false,indent+1) || added;

 if(!added && list.length>0) MakeAuthor(di,"author",list.item(0),true,indent+1);

 added=MakeText(di,"program-used",DescriptionControls().all.diProgs.value,false,indent+1) || added;
 added=MakeDate(di,DescriptionControls().all.diDate.value,DescriptionControls().all.diDateVal.value,indent+1,true) || added;

 // src-url
 list=DescriptionControls().all.diURL.getElementsByTagName("INPUT");
 for(i=0; i<list.length; i++) added=MakeText(di,"src-url",list.item(i).value,false,indent+1) || added;

 added=MakeText(di,"src-ocr",DescriptionControls().all.diOCR.value,false,indent+1) || added;
 added=MakeText(di,"id",DescriptionControls().all.diID.value,true,indent+1) || added;
 added=MakeText(di,"version",DescriptionControls().all.diVersion.value,true,indent+1) || added;

 // history
 if(!IsEmpty(hist))
 {
  Indent(di,indent+1); di.appendChild(hist); added=true;
 }

  // only append document info it is non-empty
 if(added)
 {
  Indent(desc,indent); desc.appendChild(di); Indent(di,indent);
 }
}

function MakePubInfo(doc,desc,indent)
{
 var added=false; var pi=doc.createNode(1,"publish-info",fbNS);

 added=MakeText(pi,"book-name",DescriptionControls().all.piName.value,false,indent+1) || added;
 added=MakeText(pi,"publisher",DescriptionControls().all.piPub.value,false,indent+1) || added;
 added=MakeText(pi,"city",DescriptionControls().all.piCity.value,false,indent+1) || added;
 added=MakeText(pi,"year",DescriptionControls().all.piYear.value,false,indent+1) || added;
 added=MakeText(pi,"isbn",DescriptionControls().all.piISBN.value,false,indent+1) || added;

 // sequence
 added=MakeSeq(pi,DescriptionControls().all.piSeq,indent+1) || added;

 // only append publisher info it is non-empty
 if(added)
 {
  Indent(desc,indent);
  desc.appendChild(pi);
  Indent(pi,indent);
 }
}

function MakeCustInfo(doc,desc,indent)
{
 var list=DescriptionControls().all.ci.getElementsByTagName("DIV");

 for(var i=0; i<list.length; i++)
 {
  var t=list.item(i).all.type.value;
  var v=list.item(i).all.val.value;
  if(t.length>0 || v.length>0)
  {
   var ci=doc.createNode(1,"custom-info",fbNS);
   SetAttr(ci,"info-type",t);
   ci.appendChild(doc.createTextNode(v));
   Indent(desc,indent);
   desc.appendChild(ci);
  }
 }
}

function MakeStylesheets(doc,indent)
{
 if (!document.getElementById("stylesheetId")) return;
 var s=document.getElementById("stylesheetId").value;
 if (s=="") return;
 var styles=doc.createNode(1,"stylesheet",fbNS);
 SetAttr(styles, "type", "text/css");
 Indent(doc.documentElement,1);
 doc.documentElement.appendChild(styles);
 styles.appendChild(doc.createTextNode(s));
}

function GetDesc(doc,ann,hist)
{
 MakeStylesheets(doc,1);
 var desc=doc.createNode(1,"description",fbNS);
 Indent(doc.documentElement,1);
 doc.documentElement.appendChild(desc);
 MakeTitleInfo(doc,desc,ann,2);
 MakeSourceTitleInfo(doc,desc,ann,2);
 MakeDocInfo(doc,desc,hist,2);
 MakePubInfo(doc,desc,2);
 MakeCustInfo(doc,desc,2);
 Indent(desc,1);
}

function GetDescForAnalysis(doc,ann,hist)
{
 fbeAnalysisSnapshot=true;
 try {
  fbeAnalysisMarkup=document.getElementById("fbw_desc").outerHTML;
  fbeAnalysisDescription=document.createElement("DIV");
  fbeAnalysisDescription.innerHTML=fbeAnalysisMarkup;
  GetDesc(doc,ann,hist);
 }
 finally { fbeAnalysisDescription=null; fbeAnalysisMarkup=null; fbeAnalysisSnapshot=false; }
}

function GetBinaries(doc)
{
 var bo=document.all.binobj.getElementsByTagName("DIV");
 var progressStep=25;
 var progressText=LocalizedBinaryMessage("fbe.binary.processing_images");
 for(var i=0; i<bo.length; i++)
 {
  var newb=doc.createNode(1,"binary",fbNS);
  newb.dataType="bin.base64";
  newb.nodeTypedValue=bo[i].base64data;

  SetAttr(newb,"id",bo[i].all.id.value);
  SetAttr(newb,"content-type",bo[i].all.type.value);

  newb.dataType=undefined;
  Indent(doc.documentElement,1);
  doc.documentElement.appendChild(newb);
  if(bo.length>=50 && ((i+1)%progressStep==0 || i+1==bo.length))
   window.external.SetStatusBarText(progressText.replace("{0}",i+1).replace("{1}",bo.length));
 }
}

// load a list of binary objects from document
function PutBinaries(doc)
{
 BinaryStoreChanged("reset", document.all.binobj);
 TraceScript("J600", "operation=PutBinaries");
 ImagesInfo.length=0;
 var failureCount=0; var success=0; var bl=doc.selectNodes("/fb:FictionBook/fb:binary");
 TraceScript("J610", "operation=binary count; count=" + bl.length);
 for(var i=0; i<bl.length; i++)
 {
  if(bl[i].tagName!="binary") continue;
  TraceVerboseOperation("J611", "operation=binary; index=" + i);
  var id=bl[i].getAttribute("id"); var dt; var decoder=null;
  try
  {
   // Typing an existing text node makes MSXML intern its full base64 content.
   // Set the type before assigning text on a detached, temporary decoder instead.
   decoder=doc.createNode(1,"binary",fbNS);
   decoder.dataType="bin.base64";
   for(var part=bl[i].firstChild;part;part=part.nextSibling) decoder.appendChild(part.cloneNode(true));
   TraceVerboseOperation("J620", "operation=nodeTypedValue; index=" + i);
   dt=decoder.nodeTypedValue;
   TraceVerboseOperation("J621", "operation=nodeTypedValue result; index=" + i);
  }
  catch(e)
  {
   TraceScript("J622", "operation=invalid Base64; index=" + i);
   DiagError("J622", "nodeTypedValue", e);
   if(failureCount++<3) MsgBox("Invalid base64 data for "+id); continue;
  }
  finally { decoder=null; }
  TraceVerboseOperation("J630", "operation=apiAddBinary; index=" + i);
  apiAddBinary("", id, bl[i].getAttribute("content-type"),dt);
  TraceVerboseOperation("J631", "operation=apiAddBinary result; index=" + i);
  success++;
 }
 var additionalFailureCount = failureCount > 3 ? failureCount - 3 : 0;
 if(additionalFailureCount) MsgBox(additionalFailureCount+" more invalid images ignored");
 TraceScript("J640", "operation=FillLists");
 FillLists();
 TraceScript("J641", "operation=FillLists result");
 TraceDiagnosticSummary("J699", "operation=PutBinaries success; binary-count=" + bl.length + "; binary-success=" + success + "; binary-failures=" + failureCount);
}
//// == BODY == ///////////////////////////////////////////////////////////////////

function KillDivs(e)
{
 var divs = e.getElementsByTagName("DIV");

 while(divs.length > 0) divs[0].removeNode(false);
}
//-----------------------------------------------

function GoTo(elem)
{
 if(!elem) return;
 var b=elem.getBoundingClientRect();
 if (b.bottom-b.top<=window.external.getViewHeight())
  window.scrollBy(0,(b.top+b.bottom-window.external.getViewHeight())/2);
 else
  window.scrollBy(0,b.top);
 var r=document.selection.createRange();
 if (!r || !("compareEndPoints" in r)) return;
 r.moveToElementText(elem);
 r.collapse(true);
 if(r.parentElement()!==elem && r.move("character",1)==1) r.move("character",-1);
 r.select();
}
//-----------------------------------------------
function GoToEndOfElement(elem)
{
 if(!elem) return;
 var b=elem.getBoundingClientRect();
 if (b.bottom-b.top<=window.external.getViewHeight())
  window.scrollBy(0,(b.top+b.bottom-window.external.getViewHeight())/2);
 else
  window.scrollBy(0,b.top);
 var r=document.selection.createRange();
 if (!r || !("compareEndPoints" in r)) return;
 r.moveToElementText(elem);
 r.collapse(false);
 r.select();
}
//-----------------------------------------------
function SkipOver(np,n1,n2,n3)
{
 while (np)
 {
  if(!(np.tagName=="P" && !np.firstChild && !window.external.inflateBlock(np)) &&  // not an empty P
	(!n1 || (np.tagName!=n1 && np.className!=n1)) && // and not n1
	(!n2 || (np.tagName!=n2 && np.className!=n2)) && // and not n2
	(!n3 || (np.tagName!=n3 && np.className!=n3)))   // and not n3
      break;
   np=np.nextSibling;
 }
 return np;
}
//-----------------------------------------------

function StyleCheck(cp,st)
{
 if(!cp || cp.tagName != "P") return false;

 var pp=cp.parentElement;
 if(!pp || pp.tagName != "DIV") return false;

 switch (st)
 {
  case "":
      if(pp.className!="section" && pp.className!="title" && pp.className!="epigraph" &&
	  pp.className!="history" && pp.className!="cite" && pp.className!="annotation")
	return false;
  break;

  case "subtitle":
    if(pp.className!="section" && pp.className!="stanza" && pp.className!="cite" && pp.className != "annotation")
      return false;
  break;

  case "text-author":
    if(pp.className!="cite" && pp.className!="epigraph" && 
        (cp.nodeName!="P" || cp.parentNode.className!="stanza" ||
          (cp.previousSibling && cp.previousSibling.className=="title") ||
          (!cp.previousSibling && !cp.parentNode.previousSibling) ||
          (!cp.previousSibling && cp.parentNode.previousSibling && cp.parentNode.previousSibling.className!="stanza") ||
          (cp.parentNode.nextSibling && cp.parentNode.nextSibling.className=="stanza"))) return false;
    if((cp.nextSibling && cp.nextSibling.className!="text-author")) return false;
  break;

  case "code":
    if((cp.className == "text-author" || cp.className == "subtitle" && pp.className == "section") ||
         (pp.className == "stanza" && cp.tagName == "P") ||
         (cp.className == "" && pp.className == "section" && cp.tagName == "P") ||
         ((cp.className == "td" || cp.className == "th") && pp.className == "tr"))
      return true;
    else
      return false;

  break;
 }

 return true;
}
//-----------------------------------------------

function SetStyle(cp,check,name)
{
 if(!StyleCheck(cp,name)) return;

 if(check) return true;

 if(name.length==0) name="normal";

 window.external.BeginUndoUnit(document,name+" style");
 window.external.SetStyleEx(document, cp, name);
 //cp.className=name;
 if (name=="text-author" && cp && cp.parentNode && cp.parentNode.className=="stanza") {
   var stanza1=cp.parentNode;
   var node1=cp.removeNode(true);
   stanza1.parentNode.insertBefore(node1,stanza1.nextSibling);
   if (!stanza1.firstChild && stanza1.previousSibling) {
     var rng1=document.body.createTextRange();
     rng1.moveToElementText(stanza1.previousSibling); 
     rng1.collapse(false);
     rng1.select();
   }
   if (!stanza1.firstChild) stanza1.removeNode(true);
 }
 /*if (name=="normal") {
   var stanza1=cp.previousSibling;
   if (stanza1.className=="stanza") {
     var node1=cp.removeNode(true);
     stanza1.appendChild(node1);
   }
 }*/
 window.external.EndUndoUnit(document);
}
//-----------------------------------------------

function StyleNormal(cp,check)
{
  return SetStyle(cp,check,"");
}
//-----------------------------------------------

function StyleTextAuthor(cp,check)
{
  return SetStyle(cp,check,"text-author");
}
//-----------------------------------------------

function StyleSubtitle(cp,check)
{
  return SetStyle(cp,check,"subtitle");
}
//-----------------------------------------------

function StyleCode(check, cp, range)
{
  if(check && cp && range)
  {
    // Selecting a table by its MSHTML move handle yields a ControlRange,
    // which has no duplicate() and cannot be formatted as text.
    if (!('duplicate' in range)) return false;
    // That is due to MSHTML bug
    if(cp.tagName == "P")
    {
      var html = new String(range.htmlText);
      if(html.indexOf("<DIV") != -1)
      {
        return false;
      }
    }

    if(cp.tagName == "DIV" && cp.className == "section")
    {
      var end = range.duplicate();
      range.collapse(true);
      end.collapse(false);

      var per = range.parentElement();
      var ped = end.parentElement();

      while(per && per.tagName != "P")
        per = per.parentElement;
      while(ped && ped.tagName != "P")
        ped = ped.parentElement;

      if(per && ped && per.parentElement == ped.parentElement)
      {
        return true;
      }
    }
  }


  return SetStyle(cp, check, "code");
}

function IsCode(cp)
{
  while(cp && cp.tagName != "DIV")
  {
    if(cp.tagName == "SPAN" && cp.className == "code")
      return true;
    cp = cp.parentElement;
  }
  return false;
}

function TextIntoHTML(s) {
 if (!s) return "";
 var re0=new RegExp("&","g");
 var re0_="&amp;";
 var re1=new RegExp("<","g");
 var re1_="&lt;";
 var re2=new RegExp(">","g");
 var re2_="&gt;";
 return s.replace(re0,re0_).replace(re1,re1_).replace(re2,re2_);
}

function AddTitle(cp, check)
{
  if(!cp)
    return;

  if(cp.tagName == "P")
    cp = cp.parentElement;

  if(cp.tagName != "DIV")
    return;

  if(cp.className != "body" && cp.className != "section" && cp.className != "stanza" && cp.className != "poem")
    return;

  var np = cp.firstChild;
  if(np)
  {
    if(cp.className == "body" && np.tagName == "DIV" && np.className == "image")
      np = np.nextSibling;
    if(np.tagName == "DIV" && np.className == "title")
      return;
  }

  var sel = document.selection.createRange();
  if (sel)
    if (sel.text)
      if(cp.innerText.length < sel.text.length)
        return;

  if(check)
    return true;

  window.external.BeginUndoUnit(document, "add title");

  var div = document.createElement("DIV");
  div.className = "title";

  var del = false;

  if(sel.text == "" || cp.className=="body")
  {
    var pp = document.createElement("P");
    pp.innerText="";
    window.external.inflateBlock(pp) = true;
    div.appendChild(pp);
  } else {
    if(sel.htmlText.indexOf("<DIV") == -1)
    {
      div.innerHTML = "<P>"+TextIntoHTML(sel.text).replace(/\r+\n/gi,"</P><P>")+"</P>";
      del = true;
    }
  }

  if(del)
  {
    sel.text = "";
  }

  InsBefore(cp, np, div);

  GoTo(div);

  window.external.EndUndoUnit(document);
}

//-----------------------------------------------

function AddBody(check)
{
 if(check) return true;

 window.external.BeginUndoUnit(document,"add body");
 var newbody=document.createElement("DIV");
 newbody.className="body";
 newbody.innerHTML='<DIV class="title"><P></P></DIV><DIV class="section"><P></P></DIV>';

 var body = document.getElementById("fbw_body"); if(!body) return false;

 body.appendChild(newbody);
 InflateIt(newbody);
 window.external.EndUndoUnit(document);
 GoTo(newbody);
}
//-----------------------------------------------

function GetCP(cp)
{
 if(!cp) return;

 if(cp.tagName=="P") cp=cp.parentElement;

 if(cp.tagName=="DIV" && cp.className=="title") cp=cp.parentElement;

 if(cp.tagName!="DIV") return;

 return cp;
}
//-----------------------------------------------

function InsBefore(parent,ref,item)
{
 if(ref)  ref.insertAdjacentElement("beforeBegin",item);
 else  parent.insertAdjacentElement("beforeEnd",item);
}
//-----------------------------------------------

function InsBeforeHTML(parent,ref,ht)
{
 if(ref)  ref.insertAdjacentHTML("beforeBegin",ht);
 else  parent.insertAdjacentHTML("beforeEnd", ht);
}
//-----------------------------------------------

function CloneContainer(cp,check)
{
 cp=GetCP(cp); if(!cp) return;

 switch(cp.className)
 {
  case "section": case "poem": case "stanza": case "cite": case "epigraph": break;
  default: return;
 }

 if(check) return true;

 // A shallow clone also copies the XML ID and MSHTML-only attributes.
 var ncp=document.createElement("DIV");
 ncp.className=cp.className;
 if(cp.className=="section" || cp.className=="poem" || cp.className=="stanza" || cp.className=="cite")
 {
  var language=cp.getAttribute("xml:lang",2);
  if(language) ncp.setAttribute("xml:lang",language,0);
 }

 switch (cp.className)
 {
  case "section": ncp.innerHTML='<DIV class="title"><P></P></DIV><P></P>'; break;
  case "poem":    ncp.innerHTML='<DIV class="title"><P></P></DIV><DIV class="stanza"><P></P></DIV>'; break;

  case "stanza": case "cite": case "epigraph": ncp.innerHTML='<P></P>'; break;
 }
 InflateIt(ncp);
 var parent=cp.parentNode, before=StructuralNodeOrder(parent), failed=false;
 window.external.BeginUndoUnit(document,"clone "+cp.className);
 try { cp.insertAdjacentElement("afterEnd",ncp); }
 catch(error)
 {
  failed=true;
  try { if(ncp.parentNode) ncp.parentNode.removeChild(ncp); } catch(rollbackError) {}
  try { StructuralRestoreNodeOrder(parent,before); } catch(rollbackError) {}
 }
 finally { window.external.EndUndoUnit(document); }
 if(failed) return StructuralOperationFailure(StructuralNodeOrderMatches(parent,before) && !ncp.parentNode);
 GoTo(ncp);
 return true;
}
//-----------------------------------------------

var imgcode="<DIV onresizestart='return false' contentEditable='false' class='image' href='#undefined'><IMG src='fbw-internal:#undefined'></DIV>";

function EscapeBlockImageAttribute(value)
{
 return String(value).replace(/&/g,"&amp;").replace(/</g,"&lt;").replace(/>/g,"&gt;").replace(/"/g,"&quot;").replace(/'/g,"&#39;");
}

// UI refresh asks these related predicates together. Keep their legacy
// implementations authoritative, but cross the MSHTML/JScript boundary once.
function GetBodyCommandState(cp, insideCode, insideSpan)
{
  var state = 0;
  function put(bit, value) { if(value) state |= bit; }
  put(1, AddTitle(cp,true)); put(2, CloneContainer(cp,true));
  put(4, StyleNormal(cp,true)); put(8, StyleSubtitle(cp,true));
  put(16, StyleTextAuthor(cp,true)); put(32, InsImage(true) && !insideCode && !insideSpan);
  put(64, InsInlineImage(true)); put(128, AddImage(cp,true) && !insideCode && !insideSpan);
  put(256, AddEpigraph(cp,true)); put(512, AddAnnotation(cp,true));
  put(1024, AddTA(cp,true)); put(2048, MergeContainers(cp,true));
  put(4096, RemoveOuterContainer(cp,true)); put(8192, StyleCode(true,cp,document.selection.createRange()));
  put(16384, insideCode);
  return state;
}

function BlockImageHTML(id)
{
 var imageId=id=="" ? "undefined" : id;
 var escapedId=EscapeBlockImageAttribute(imageId);
 return "<DIV onresizestart='return false' contentEditable='false' class='image' href='#"+escapedId+"'><IMG src='fbw-internal:#"+escapedId+"'></DIV>";
}

function MoveCaretToParagraphStart(paragraph)
{
 if(!paragraph || !document.body) return;
 var caret=document.body.createTextRange();
 caret.moveToElementText(paragraph);
 caret.collapse(true);
 caret.select();
}

function InsImage(check, id)
{
 var rng=document.selection.createRange();

 if(!rng || !("compareEndPoints" in rng)) return;

 if(rng.compareEndPoints("StartToEnd",rng)!=0)
 {
  rng.collapse(true); if(rng.move("character",1)==1) rng.move("character",-1);
 }

 var cp=rng.parentElement();
 var paragraph=cp;
 while(paragraph && paragraph.tagName!="P") paragraph=paragraph.parentElement;
 var section=cp;
 while(section && (section.tagName!="DIV" || section.className!="section")) section=section.parentElement;
 if(!paragraph || !section) return;
 var owner=paragraph.parentElement;
 while(owner && owner.tagName!="DIV") owner=owner.parentElement;
 if(!owner || owner.sourceIndex!=section.sourceIndex) return;

 if(check) return true;

 var imageHtml=BlockImageHTML(id);
 var whole=document.body.createTextRange();
 whole.moveToElementText(paragraph);
 var left=whole.duplicate();
 left.setEndPoint("EndToStart",rng);
 var right=whole.duplicate();
 right.setEndPoint("StartToEnd",rng);
 var leftHTML=left.htmlText;
 var rightHTML=right.htmlText;
 var undoStarted=false;
 var inserted=null;
 var caretParagraph=null;
 var operationError=null;
 var cleanupError=null;
 try
 {
  window.external.BeginUndoUnit(document,"insert image");
  undoStarted=true;
  var leftText=left.text;
  var rightText=right.text;
  if(leftText==null || leftText=="")
  {
   paragraph.insertAdjacentHTML("beforeBegin",imageHtml);
   inserted=paragraph.previousSibling;
   caretParagraph=paragraph;
  }
  else if(rightText==null || rightText=="")
  {
   paragraph.insertAdjacentHTML("afterEnd",imageHtml);
   inserted=paragraph.nextSibling;
  }
 else
 {
   var rightPart=paragraph.cloneNode(false);
   if(rightPart.id) rightPart.removeAttribute("id");
   paragraph.innerHTML=leftHTML;
   rightPart.innerHTML=rightHTML;
   InflateIt(paragraph);
   InflateIt(rightPart);
   paragraph.insertAdjacentHTML("afterEnd",imageHtml);
   inserted=paragraph.nextSibling;
   inserted.insertAdjacentElement("afterEnd",rightPart);
   MoveCaretToParagraphStart(rightPart);
  }
 }
 catch(error)
 {
  operationError=error;
  throw error;
 }
 finally
 {
  try
  {
   if(undoStarted) window.external.EndUndoUnit(document);
  }
  catch(error)
  {
   cleanupError=error;
  }
  if(!operationError && cleanupError) throw cleanupError;
 }
 if(inserted && caretParagraph) MoveCaretToParagraphStart(caretParagraph);
 return inserted;
}

//-----------------------------------------------

var inlineimgcode="<SPAN onresizestart='return false' contentEditable='false' class='image' href='#undefined'><IMG src='fbw-internal:#undefined'></SPAN>";

function InsInlineImage(check, id)
{
 var rng=document.selection.createRange();

 if(!rng || !("compareEndPoints" in rng)) return;

 if(rng.compareEndPoints("StartToEnd",rng)!=0)
 {
  rng.collapse(true); if(rng.move("character",1)==1) rng.move("character",-1);
 }

 var pe=rng.parentElement(); while(pe && pe.tagName!="DIV") pe=pe.parentElement;

 if(!pe || (pe.className.search(/section|annotation|history|title|epigraph|cite|stanza/)<0)) return;

 if(check) return true;

 window.external.BeginUndoUnit(document,"insert inline image");
 if(id=="")
  rng.pasteHTML(inlineimgcode);
 else
     rng.pasteHTML("<SPAN onresizestart='return false' contentEditable='false' class='image' href='#" + id + "'><IMG src='fbw-internal:#" + id + "'></SPAN>");

 window.external.EndUndoUnit(document);

 return rng.parentElement();
}
//-----------------------------------------------

function AddImage(cp, check)
{
 cp=GetCP(cp); if(!cp) return;

 if(cp.className!="body" && cp.className!="section") return;

 var np=cp.firstChild;
 if (cp.className=="body") np=SkipOver(np,null,null,null);
 else                      np=SkipOver(np,"title","epigraph",null);

 if(np && np.tagName=="DIV" && np.className=="image") return;

 if(check) return true;

 window.external.BeginUndoUnit(document,"add image");
 InsBeforeHTML(cp, np, imgcode);
 window.external.EndUndoUnit(document);
}
//-----------------------------------------------

function AddEpigraph(cp,check)
{
  cp = GetCP(cp);
  if(!cp) return;

  if(cp.className != "body" && cp.className != "section" && cp.className != "poem") return;

  var pp=cp.firstChild;
  if(cp.className == "body") // different order
    pp = SkipOver(pp, "title", "image", "epigraph");
  else
    pp = SkipOver(pp, "title", "epigraph", null);

  if(check) return true;

  if (document.selection.type && document.selection.type=="Control") {
   MsgBox("Вы используете не тот тип выделения, с которым работает вставка эпиграфа. Выделяйте текст для будущего эпиграфа не кликом по картинке, а движением мыши слева направо или справа налево. Либо задайте выделение, используя клавиатуру.");
   return;
  }

  var rng = document.selection.createRange();
  var txt = "";
  var pps;

  if(rng && rng.text != "")
  {
    var dpps = document.createElement("DIV");
    dpps.innerHTML = rng.htmlText;
    pps = dpps.getElementsByTagName("P");
    if(pps.length == 0) {
     dpps.innerHTML = "<P>"+rng.htmlText+"</P>";
     pps = dpps.getElementsByTagName("P");
     if(pps.length == 0) {
      txt = rng.text;
     }
    }
  }

  window.external.BeginUndoUnit(document,"add epigraph");
  var ep=document.createElement("DIV");
  ep.className="epigraph";
  if(txt != "")
  {
    var pwt = document.createElement("P");
    pwt.innerText = txt;
    ep.appendChild(pwt);
  }
  else if(pps && pps.length > 0)
  {
    var upTag = "";
    for(var i = 0; i < pps.length; ++i)
    {
      var pwt = document.createElement("P");
      if(i == pps.length - 2
         && pps[i].children
         && pps[i].children.length
         && pps[i].children.length == 1)
      {
        upTag = pps[i].children[0].tagName;
      }
      if(i == pps.length - 1)
      {
        var pptext = pps[i].innerText;
        var pptags = new Array();
        var ppall = pps[i].all;
        var j = 0;

        for(var k = 0; k < ppall.length; ++k)
        {
          if(ppall[k].innerText && ppall[k].innerText == pptext)
            pptags[j++] = ppall[k].tagName;
        }

	if (pptags.length > 0 && pps.length > 1)
	{
          for(var p=0; p<pptags.length; ++p)
          {
            if(pptags[p] != upTag)
            {
              pwt.className = "text-author";
              break;
            }
          }
	}
      }
      pwt.innerHTML = pps[i].innerHTML;
      ep.appendChild(pwt);
    }
  }
  else
    ep.appendChild(document.createElement("P"));

  InsBefore(cp, pp, ep);
  InflateIt(ep);
  rng.pasteHTML("");
  if(pp && (!pp.innerText || pp.innerText == "" || pp.innerText == " ") && pp.className!="image")
    pp.removeNode(true);
  window.external.EndUndoUnit(document);

  GoToEndOfElement(ep);
}
//-----------------------------------------------

function AddAnnotation(cp, check)
{
 cp=GetCP(cp); if(!cp) return;

 if(cp.className!="section") return;

 var pp=SkipOver(cp.firstChild,"title","epigraph","image");

 if(pp && pp.tagName=="DIV" && pp.className=="annotation") return;

 if(check) return true;

 window.external.BeginUndoUnit(document,"add annotation");

 var ep=document.createElement("DIV");
 ep.className="annotation";
 ep.appendChild(document.createElement("P"));
 InsBefore(cp,pp,ep);
 InflateIt(ep);

 window.external.EndUndoUnit(document);

 GoTo(ep);
}
//-----------------------------------------------

function AddTA(cp,check)
{
 cp=GetCP(cp);

 while(cp)
 {
  if(cp.tagName=="DIV" && (cp.className=="poem" || cp.className=="epigraph" || cp.className=="cite")) break;

  cp=cp.parentElement;
 }

 if(!cp) return;

 var lc=cp.lastChild; if(lc && lc.tagName=="P" && lc.className=="text-author") return;

 if(check) return true;

 window.external.BeginUndoUnit(document,"add text author");

 var np=document.createElement("P");
 np.className="text-author";
 window.external.inflateBlock(np)=true;
 cp.appendChild(np);
 window.external.EndUndoUnit(document);
 GoTo(np);
}
//-----------------------------------------------

function StructuralFailure(key, fallback, check)
{
 if(!check) MsgBox(GetLocalizedStringOrDefault(key,fallback));
 return false;
}

function StructuralNextElement(node)
{
 for(var next=node.nextSibling; next; next=next.nextSibling)
 {
  if(next.nodeType==1) return next;
  if(next.nodeType!=3 || /\S/.test(next.nodeValue)) return null;
 }
 return null;
}

function StructuralChildren(node)
{
 var children=[];
 for(var child=node.firstChild; child; child=child.nextSibling)
 {
  if(child.nodeType==1) children.push(child);
  else if(child.nodeType!=3 || /\S/.test(child.nodeValue)) return null;
 }
 return children;
}

function StructuralNodeOrder(parent)
{
 var nodes=[];
 for(var child=parent.firstChild; child; child=child.nextSibling) nodes.push(child);
 return nodes;
}

function StructuralRestoreNodeOrder(parent, nodes)
{
 var next=null;
 for(var i=nodes.length-1; i>=0; --i)
 {
  if(nodes[i].parentNode!=parent || nodes[i].nextSibling!=next)
  {
   if(next) parent.insertBefore(nodes[i],next);
   else parent.appendChild(nodes[i]);
  }
  next=nodes[i];
 }
}

function StructuralNodeOrderMatches(parent, nodes)
{
 var current=parent.firstChild;
 for(var i=0; i<nodes.length; ++i)
 {
  if(current!=nodes[i]) return false;
  current=current.nextSibling;
 }
 return !current;
}

function StructuralOperationFailure(restored)
{
 if(restored)
  return StructuralFailure("fbe.structure.operation.failed","The structural change failed; the document was left unchanged.",false);
 return StructuralFailure("fbe.structure.operation.rollback_failed","The structural change failed and could not be fully restored. Save a copy and inspect the document.",false);
}

function StructuralHasExtraAttributes(node)
{
 if(node.id) return true;
 for(var i=0; i<node.attributes.length; ++i)
 {
  var attribute=node.attributes[i];
  var name=String(attribute.nodeName).toLowerCase();
  if(attribute.specified && name!="class" && name!="classname") return true;
 }
 return false;
}

// Only accept layouts whose concatenation is known to satisfy sectionType.
function StructuralSectionKind(section)
{
 var children=StructuralChildren(section), phase=0, kind="";
 if(!children) return null;
 for(var i=0; i<children.length; ++i)
 {
  var child=children[i], name=String(child.className || "").toLowerCase();
  if(child.tagName=="DIV" && name=="title" && phase==0) { phase=1; continue; }
  if(child.tagName=="DIV" && name=="epigraph" && phase<=2) { phase=2; continue; }
  if(child.tagName=="DIV" && name=="image" && phase<=2) { phase=3; continue; }
  if(child.tagName=="DIV" && name=="annotation" && phase<=3) { phase=4; continue; }
  if(child.tagName=="DIV" && name=="section")
  {
   if(kind=="text") return null;
   kind="nested"; phase=5; continue;
  }
  if((child.tagName=="P" && (name=="" || name=="subtitle" || name=="empty-line")) ||
     (child.tagName=="DIV" && (name=="poem" || name=="cite" || name=="image" || name=="table")) ||
     (child.tagName=="TABLE" && name=="table"))
  {
   if(name=="image" && kind!="text") return null;
   if(kind=="nested") return null;
   kind="text"; phase=5; continue;
  }
  return null;
 }
 return kind || null;
}

function StructuralSimpleContent(node, type, onlyLines)
{
 var children=StructuralChildren(node), author=false, lines=0, phase=0;
 if(!children) return false;
 for(var i=0; i<children.length; ++i)
 {
  var child=children[i], name=String(child.className || "").toLowerCase();
  if(type=="stanza")
  {
   if(!onlyLines && child.tagName=="DIV" && name=="title" && phase==0) { phase=1; continue; }
   if(!onlyLines && child.tagName=="P" && name=="subtitle" && phase<=1) { phase=2; continue; }
   if(child.tagName!="P" || name!="") return false;
   ++lines; phase=3;
  }
  else
  {
   if(child.tagName=="P" && name=="text-author") { author=true; continue; }
   if(author || !((child.tagName=="P" && (name=="" || name=="subtitle" || name=="empty-line")) ||
      (child.tagName=="DIV" && (name=="poem" || name=="table")) || (child.tagName=="TABLE" && name=="table"))) return false;
   ++lines;
  }
 }
 return type=="stanza" ? lines>0 : true;
}

function MergeContainers(cp,check)
{
 cp=GetCP(cp);
 if(!cp || (cp.className!="section" && cp.className!="stanza" && cp.className!="cite")) return false;
 var nx=StructuralNextElement(cp);
 if(!nx || nx.tagName!="DIV" || nx.className!=cp.className || nx.parentNode!=cp.parentNode) return false;
 if(StructuralHasExtraAttributes(nx))
  return StructuralFailure("fbe.structure.merge.attributes","The second element has an ID or attributes that cannot be discarded safely.",check);
 var first=StructuralChildren(cp), second=StructuralChildren(nx);
 if(!first || !second)
  return StructuralFailure("fbe.structure.merge.layout","The elements contain unsupported content.",check);
 if(cp.className=="section")
 {
  var firstKind=StructuralSectionKind(cp), secondKind=StructuralSectionKind(nx);
  if(!firstKind || firstKind!=secondKind)
   return StructuralFailure("fbe.structure.merge.layout","The elements contain unsupported content.",check);
  var secondBodySeen=false;
  for(var i=0; i<second.length; ++i)
  {
   var headerName=second[i].className;
   if(second[i].tagName=="DIV" &&
      (headerName=="title" || headerName=="epigraph" || headerName=="annotation" ||
       (headerName=="image" && !secondBodySeen)))
    return StructuralFailure("fbe.structure.merge.header","The second section has a heading or metadata that cannot be merged without changing its meaning.",check);
   if(headerName!="image") secondBodySeen=true;
  }
 }
 else if(cp.className=="stanza")
 {
  if(!StructuralSimpleContent(cp,"stanza",false) || !StructuralSimpleContent(nx,"stanza",true))
   return StructuralFailure("fbe.structure.merge.layout","The elements contain unsupported content.",check);
 }
 else
 {
  if(!StructuralSimpleContent(cp,"cite",false) || !StructuralSimpleContent(nx,"cite",false))
   return StructuralFailure("fbe.structure.merge.layout","The elements contain unsupported content.",check);
  var author=false;
  for(var i=0; i<first.length; ++i) if(first[i].className=="text-author") author=true;
  if(author)
   for(var i=0; i<second.length; ++i) if(second[i].className!="text-author")
    return StructuralFailure("fbe.structure.merge.author","The first quotation has an author line before the second quotation's text.",check);
 }
 if(check) return true;
 var parent=cp.parentNode, parentBefore=StructuralNodeOrder(parent);
 var firstBefore=StructuralNodeOrder(cp), secondBefore=StructuralNodeOrder(nx), failed=false;
 window.external.BeginUndoUnit(document,"merge "+cp.className+"s");
 try
 {
  for(var i=0; i<second.length; ++i) cp.appendChild(second[i]);
  nx.removeNode(true);
 }
 catch(error)
 {
  failed=true;
  try { StructuralRestoreNodeOrder(parent,parentBefore); } catch(rollbackError) {}
  try { StructuralRestoreNodeOrder(nx,secondBefore); } catch(rollbackError) {}
  try { StructuralRestoreNodeOrder(cp,firstBefore); } catch(rollbackError) {}
 }
 finally { window.external.EndUndoUnit(document); }
 if(failed) return StructuralOperationFailure(StructuralNodeOrderMatches(parent,parentBefore) &&
  StructuralNodeOrderMatches(cp,firstBefore) && StructuralNodeOrderMatches(nx,secondBefore));
 GoTo(cp);
 return true;
}

function RemoveOuterContainer(cp,check)
{
 cp=GetCP(cp);
 if(!cp) return false;
 if(cp.className=="body")
  return StructuralFailure("fbe.structure.remove.body","The FB2 body cannot be removed.",check);
 if(cp.className!="section") return false;
 var parent=cp.parentNode;
 if(!parent || parent.tagName!="DIV" || (parent.className!="body" && parent.className!="section")) return false;
 if(StructuralHasExtraAttributes(cp))
  return StructuralFailure("fbe.structure.remove.attributes","The outer section has an ID or attributes that cannot be removed safely.",check);
 var children=StructuralChildren(cp);
 if(!children || !children.length)
  return StructuralFailure("fbe.structure.remove.content","The outer section contains content that would be lost.",check);
 for(var i=0; i<children.length; ++i)
  if(children[i].tagName!="DIV" || children[i].className!="section")
   return StructuralFailure("fbe.structure.remove.content","The outer section contains content that would be lost.",check);
 if(parent.className=="section" && StructuralSectionKind(parent)!="nested")
  return StructuralFailure("fbe.structure.remove.layout","Removing this section would create an invalid FB2 layout.",check);
 if(check) return true;
 var parentBefore=StructuralNodeOrder(parent), outerBefore=StructuralNodeOrder(cp), failed=false;
 window.external.BeginUndoUnit(document,"Remove Outer Section");
 try
 {
  for(var i=0; i<children.length; ++i) cp.insertAdjacentElement("beforeBegin",children[i]);
  cp.removeNode(true);
 }
 catch(error)
 {
  failed=true;
  try { StructuralRestoreNodeOrder(parent,parentBefore); } catch(rollbackError) {}
  try { StructuralRestoreNodeOrder(cp,outerBefore); } catch(rollbackError) {}
 }
 finally { window.external.EndUndoUnit(document); }
 if(failed) return StructuralOperationFailure(StructuralNodeOrderMatches(parent,parentBefore) &&
  StructuralNodeOrderMatches(cp,outerBefore));
 GoTo(children[0]);
 return true;
}
//-----------------------------------------------

function ImgSetURL(image,url)
{
	image.style.width=null;
	image.src=document.urlprefix+url;
}

function SaveBodyScroll()
{
	document.ScrollSave=document.body.scrollTop;
}

function SetExtendedStyle(elem, button_id, change)
{
	/*var desc = document.getElementById("fbw_desc");
	var button = document.getElementById(button_id);

	var ext = window.external.GetExtendedStyle(elem);
	if(change)
		ext= !ext;

	if(!desc)
		return false;

	var all=desc.all;
	for(var i=0; i<all.length; i++)
	{
		if(null != all[i].getAttribute(elem))
			if(ext)
				all[i].style.display="inline-block";
			else
				all[i].style.display="none";
	}

	//button.setAttribute("onclick", "SetExtendedStyle(" + !ext + ",'" + elem + "','" + button_id + "')");
	if(ext)
		button.value = "<<<<";
	else
		button.value = ">>>";

	window.external.SetExtendedStyle(elem, ext);*/
}

function ShowElement(id, show)
{
	if(!id)
		return;
	var desc = document.getElementById("fbw_desc");
	var spans = desc.getElementsByTagName("SPAN");
	for(var i=0; i<spans.length; i++)
	{
		if(spans[i].getAttribute("id") == id)
		{
			if(show)
				spans[i].style.display = "block";
			else
				spans[i].style.display = "none";
			window.external.DescShowElement(id, show);
		}
	}
}

function ShowElementsMenu(button)
{
	var elem_id = window.external.DescShowMenu(button, window.event.screenX, window.event.screenY);
	if(elem_id)
		ShowElement(elem_id, !window.external.GetExtendedStyle(elem_id));
}

function ShowImageOfBin(binDiv,mode)
{
	var inps=binDiv.getElementsByTagName("INPUT");
	for (var i=0;i<inps.length;i++)
		if (inps[i].id=="id")
		{
   			var src=inps[i].value;
   			if (!src || src=="") return;
   			if (mode=="full") ShowFullImage("fbw-internal:#"+src);
   			else if (mode=="prev") ShowPrevImage("fbw-internal:#"+src);
		} 
}
