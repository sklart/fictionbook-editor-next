// Isolated C4 JS call-site profile; not copied into production runtime.
var _c4_phase_times={};
function _c4_record_phase(name,ms,entries){var row=_c4_phase_times[name];if(!row||typeof row.calls!='number')row=_c4_phase_times[name]={calls:0,ms:0,entries:0};row.calls++;row.ms+=ms;row.entries+=entries||0;}
function _c4_binary_enumeration(){var started=new Date().getTime();var items=document.all.binobj.getElementsByTagName("DIV");_c4_record_phase('dom_binary_enumeration',new Date().getTime()-started,items.length);return items;}
(function(){var names=['apiLoadFB2','LoadFromDOM','TransformXML','PutBinaries','apiAddBinary','BuildBinaryControls','RebuildImagesInfo','FillLists'];
 for(var i=0;i<names.length;i++)(function(name){var original=window[name];if(typeof original!='function')return;_c4_phase_times[name]={calls:0,ms:0,entries:0};
 window[name]=function(){var started=new Date().getTime();try{return original.apply(this,arguments);}finally{_c4_record_phase(name,new Date().getTime()-started,0);}};
 })(names[i]);
})();
function _c4_profile_json(){var rows=[];for(var name in _c4_phase_times){if(!_c4_phase_times.hasOwnProperty(name))continue;var row=_c4_phase_times[name];rows.push('"'+name+'":{"calls":'+row.calls+',"ms":'+row.ms+',"entries":'+row.entries+'}');}return '{'+rows.join(',')+'}';}
