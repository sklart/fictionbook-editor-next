// Appended only to an isolated C4 diagnostic runtime main.js.
var _c4_phase_times={};
(function(){
 var names=['apiLoadFB2','LoadFromDOM','TransformXML','PutBinaries','apiAddBinary','BuildBinaryControls','RebuildImagesInfo','FillLists'];
 for(var i=0;i<names.length;i++)(function(name){
  var original=window[name];_c4_phase_times[name]={calls:0,ms:0};
  window[name]=function(){var started=new Date().getTime();try{return original.apply(this,arguments);}finally{var row=_c4_phase_times[name];row.calls++;row.ms+=new Date().getTime()-started;}};
 })(names[i]);
})();
function _c4_profile_json(){var rows=[];for(var name in _c4_phase_times){if(!_c4_phase_times.hasOwnProperty(name))continue;var row=_c4_phase_times[name];rows.push('"'+name+'":{"calls":'+row.calls+',"ms":'+row.ms+'}');}return '{'+rows.join(',')+'}';}
