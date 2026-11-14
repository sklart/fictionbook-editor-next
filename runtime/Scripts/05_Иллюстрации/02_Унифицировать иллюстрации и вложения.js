// Скрипт «Унифицировать иллюстрации и вложения»
// Автор Sclex (v1.6.3 Stable: исправлена кросс-валидация, DOM-Reflow, защита от пустых href и битых объектов метаданных)
// Модификация v1.6.3: авторасширения + автообложка + безопасный запрос на очистку мусора
// Фикс совместимости: Каскадный Reflow (автоматическое восстановление видимости графики от Windows XP до Windows 11)

function Run() {
 // Открываем транзакцию отмены (Ctrl+Z) в редакторе FictionBook Editor
 window.external.BeginUndoUnit(document,"binaries and images unification");
 
 var Ts=new Date().getTime();
 var versionNumber="1.6.3 (Stable-Reflow)";

 // --- НАСТРОЙКИ СКРИПТА ---
 // Автоматически делать изображение обложкой, если оно всего одно в книге? (true - да, false - нет)
 var AutoSetSingleCover = true; 
 
 // Шаблон id картинок и вложений (вместо %N будет подставлен номер картинки)
 var strconst1 = "i_%N";
 // Как обозвать вложение-обложку
 var strconst2 = "cover";
 // Как обозвать вложение-обложку оригинального издания
 var strconst3 = "cover_src";
 // Префикс к id вложений, на которые нет ссылки
 var strconst4 = "unused_";
 // Префикс к ссылкам картинок, для которых нет вложений
 var strconst5 = "nobin_";
 // Префикс для невалидных форматов
 var strconst6 = "nonjpegpnggif_";
 // Количество цифр, до которого будет дополняться номер картинки
 var SymbolsNum = 3;
 // Количество цифр во временном имени бинарника
 var DigitsInTempName = 10;
 // Если true, выводить списки в столбик
 var ColumnView=true;

 // Функция для правильного склонения существительных и числительных в финальном отчете
 function units(num, cases) {
     num = Math.abs(num);
     var word = '';
     if (num.toString().indexOf('.') > -1) {
         word = cases.gen;
     } else { 
         word = (
             num % 10 == 1 && num % 100 != 11 
                 ? cases.nom
                 : num % 10 >= 2 && num % 10 <= 4 && (num % 100 < 10 || num % 100 >= 20) 
                     ? cases.gen
                     : cases.plu
         );
     }
     return word;
 }

 // Функция генерации порядкового номера по шаблону (например, 1 -> "001")
 function PoShablonu(s,n) {
  var ttt3 = new RegExp("\%N");
  var n1=n.toString();
  while (n1.length<SymbolsNum) n1="0"+n1;
  return(s.replace(ttt3,n1));
 }

 // Извлечение локального ID из атрибута href (отсекает внешние ссылки и служебные маркеры main.html)
 function GetLocalHref(name) {
  if (!name) return '"';
  var name1=name;
  if (name1.indexOf("#")!=0) {return('"')}
  var thg=new RegExp("main\.html\#","i");
  var srch10=name1.search(thg);
  if (srch10==-1) {
   name1 = name1.substring(1,name1.length);
  } else {
   name1 = name1.substring(srch10+10,name1.length);
  }
  return(name1);
 }

 // Generator случайных чисел для временного переименования файлов во избежание коллизий
 function GetRandomNum(n) {
  var s="";
  for (var i=1;i<=n;i++) {
    s+=Math.floor(Math.random()*10);
  }
  return s;
 }

 // Синхронизация изменений путей в глобальном внутреннем массиве информации об изображениях FBE
 function ChangeSrcInImageInfo(id,newId) {
  if (ImgInfoNumById[id]==null) return;
  ImagesInfo[ImgInfoNumById[id]].src="fbw-internal:#"+newId;
  ImgInfoNumById[newId]=ImgInfoNumById[id];
  ImgInfoNumById[id]=null;
 }
 
 // Принудительное обновление путей у элементов IMG в документе
 function updateImages() {
  var imgs=document.getElementsByTagName("IMG");
  var myImg,pic_id;
  for (var i=imgs.length-1; i>=0; i--) {
   myImg=imgs[i];
   pic_id=myImg.src;
   myImg.src="";
   myImg.src=pic_id;
  }
  FillCoverList();
 }

 // Проверяем наличие тела документа
 var body=document.getElementById("fbw_body");
 if (!body) return;
 var imgs=body.getElementsByTagName("IMG");
 var bins=document.all.binobj.getElementsByTagName("DIV");
 var i,id,type,j,len;
 var ImgsById={};
 // Создаем пустой клон контейнера бинарников для минимизации DOM-Reflow при сборке
 var NewBinobj=document.all.binobj.cloneNode(false);
 var ImgCntById={};
 var BinById={};
 var ImgByNum={};
 var NonJpegPngImgs={};
 NonJpegPngImgs["0"]=0;
 var ImgInfoNumById={};
 var BinCnt=0;
 var CoversCnt=0;
 var ImgsWithoutBin_list="";
 var BinsWithoutImg_list="";
 var NonLocalImgs_list="";
 var NonJpegPngImgs_list="";
 var SavedUnusedCover=null;
 var SavedUnusedSrcCover=null;
 var DeleteUnused = false; // Значение по умолчанию для очистки мусора

 // Заполняем карту индексов существующей информации об изображениях
 for (i=0, len=ImagesInfo.length; i<len; i++) {
  if (ImagesInfo[i] && ImagesInfo[i].src) {
   ImgInfoNumById[ImagesInfo[i].src.replace(/^fbw-internal:#/i,"")]=i;
  }
 } 

 // ПЕРВЫЙ ПРОХОД ПО ВЛОЖЕНИЯМ (BINARY): нормализация типов и исправление расширений
 for (i=0, len=bins.length; i<len; i++) {
  id = bins[i].all.id.value.replace(/^\s+|\s+$/g, "");
  bins[i].all.id.value = id; // Записываем очищенный ID обратно в поле редактора
  
  if (BinById[id] == null || BinById[id] == "undefined") {
   type = bins[i].all.type.value;
   var lowerId = id.toLowerCase();
   
   // Корректируем устаревший или ошибочный MIME-тип
   if (type == "image/jpg") {
      type = "image/jpeg";
      bins[i].all.type.value = "image/jpeg";
   }
   if (lowerId.substring(lowerId.length - 4) == ".png" && type == "image/jpeg") {
      type = "image/png";
      bins[i].all.type.value = "image/png";
   }
   if (lowerId.substring(lowerId.length - 4) == ".gif" && type == "image/jpeg") {
      type = "image/gif";
      bins[i].all.type.value = "image/gif";
   }

   // Автодобавление расширения к ID бинарника, если автор забыл его указать
   var endsWithExt = (lowerId.substring(lowerId.length - 4) == ".png" || 
                      lowerId.substring(lowerId.length - 4) == ".gif" || 
                      lowerId.substring(lowerId.length - 4) == ".jpg" || 
                      lowerId.substring(lowerId.length - 5) == ".jpeg");
                      
   if (!endsWithExt) {
      if (type == "image/jpeg") id += ".jpg";
      else if (type == "image/png") id += ".png";
      else if (type == "image/gif") id += ".gif";
      bins[i].all.id.value = id;
   }
   BinCnt++;
   BinById[id] = bins[i];
   // Проверяем на соответствие спецификации FB2 (разрешены только png, jpeg, gif)
   if (type != "image/png" && type != "image/jpeg" && type != "image/gif") {
    NonJpegPngImgs["0"]++;
    NonJpegPngImgs[NonJpegPngImgs["0"]] = bins[i];
   }
  }
  else { MsgBox("Ошибка. Два вложения имеют одинаковый id: " + id); return; }
 }
 var href;
 var BinaryNotPresent=0;
 var NonLocalAddress=0;
 var imgDiv;
 var ImgCnt=0;

 // ВТОРОЙ ПРОХОД ПО ТЕКСТУ: валидация и синхронизация ссылок на изображения
 for (i=0, len=imgs.length; i<len; i++) {
  imgDiv=imgs[i].parentNode;
  if ((imgDiv.nodeName=="DIV" || imgDiv.nodeName=="SPAN") && imgDiv.className=="image") {
   var rawHref = imgDiv.getAttribute("href");
   if (rawHref) {
    // ИСПРАВЛЕНО: Принудительно очищаем от пробелов ссылку, извлеченную из текста
    href=GetLocalHref(rawHref).replace(/^\s+|\s+$/g, "");
    
    if (href!='"') {
     // ИСПРАВЛЕНО: Перед проверками зачищаем пробелы во всех возможных комбинациях поиска
     if (BinById[href] == null && href.indexOf(strconst5) != 0) {
        var cleanJpg = href.replace(/\s+/g, "") + ".jpg";
        var cleanPng = href.replace(/\s+/g, "") + ".png";
        var cleanGif = href.replace(/\s+/g, "") + ".gif";

        if (BinById[href + ".jpg"]) href += ".jpg";
        else if (BinById[href + ".png"]) href += ".png";
        else if (BinById[href + ".gif"]) href += ".gif";
        else if (BinById[cleanJpg]) href = cleanJpg;
        else if (BinById[cleanPng]) href = cleanPng;
        else if (BinById[cleanGif]) href = cleanGif;

        imgDiv.setAttribute("href", "#" + href);
        var insideImg = imgDiv.getElementsByTagName("IMG");
        if (insideImg && insideImg.length > 0) insideImg[0].setAttribute("src", "fbw-internal:#" + href);
     }

     if (BinById[href]!=null) {
      ImgCnt++;
      ImgByNum[ImgCnt]=imgs[i];
      if (ImgCntById[href]==null || ImgCntById[href]=="undefined") {ImgCntById[href]=1}
      else {ImgCntById[href]++;}
      ImgsById[href+'"'+ImgCntById[href]]=imgDiv;
     }
     else {
      // Маркируем "битую" ссылку префиксом nobin_
      var NewId;
      var alreadyMarked = (href.indexOf(strconst5) == 0);
      
      if (alreadyMarked) NewId = href;
      else NewId = strconst5 + href;
      
      imgDiv.setAttribute("href", "#" + NewId);
      var innerImg = imgDiv.getElementsByTagName("IMG");
      if (innerImg && innerImg.length > 0) innerImg[0].setAttribute("src", "fbw-internal:#" + NewId);
      
      if (!alreadyMarked) {
       BinaryNotPresent++;
       if (!ColumnView) ImgsWithoutBin_list+=' "'+NewId+'"';
       else ImgsWithoutBin_list+='\n   "'+NewId+'"';
      }
     }
    }
    else {
     // Фиксируем внешние интернет-адреса, неподдерживаемые локально в FB2
     if (!ColumnView) NonLocalImgs_list+=' "'+imgDiv.getAttribute("href")+'"';
     else NonLocalImgs_list+='\n   "'+imgDiv.getAttribute("href")+'"';
     NonLocalAddress++;
    }
   }
  }
 }

 // Копируем служебные текстовые узлы и комментарии из старого блока binobj в новый
 var ptr=document.all.binobj.firstChild;
 var GoMore=true;
 var tmp_node;
 while (GoMore) {
  if (ptr) {
   if (ptr.nodeName!="DIV") {
    tmp_node=ptr.cloneNode(true);
    NewBinobj.appendChild(tmp_node);
    ptr=ptr.nextSibling;
   }
   else GoMore=false;
  } else GoMore=false;
 }

 // Инициализация параметров временного именования для безопасного ренейминга
 var RandomNum=GetRandomNum(DigitsInTempName);
 var tempPrefixLength = ("img_" + RandomNum + "_").length;
 var IdUsed={};
 var NewTiCover="";
 var NewStiCover="";
 
 // Считываем текущую обложку книги из интерфейса метаданных FBE
 var lists1=document.all.tiCover.getElementsByTagName("select");
 id="";
 var targetIndex1 = -1; 
 for (var xyz1=0, len1=lists1.length; xyz1<len1; xyz1++) {
  if (lists1[xyz1].id=='href') {
   id=lists1[xyz1].value;
   if (id!="") id=GetLocalHref(id);
   targetIndex1 = xyz1; 
   break;
  }
 }
 
 // Если обложка не задана, пытаемся определить её автоматически по зарезервированным именам
 if (id=="") {
  if (BinById[strconst2+".png"]) { id=strconst2+".png"; NewTiCover=id; }
  else if (BinById[strconst2+".jpg"]) { id=strconst2+".jpg"; NewTiCover=id;}
  else if (BinById[strconst2+".jpeg"]) { id=strconst2+".jpeg"; NewTiCover=id;}
  else if (BinById[strconst2+".gif"]) { id=strconst2+".gif"; NewTiCover=id;} 
  // Модификация v1.6.3: если картинка одна на всю книгу, принудительно назначаем обложкой
  else if (AutoSetSingleCover && bins.length == 1) {
     id = bins.all.id.value;
     NewTiCover = id;
  }
 }

 // Считываем текущую обложку оригинального издания из метаданных FBE
 var id_="";
 var targetIndex2 = -1; 
 if (document.all.stiCover!=null) {
  var lists2=document.all.stiCover.getElementsByTagName("select");
  for (var xyz2=0, len2=lists2.length; xyz2<len2; xyz2++) {
   if (lists2[xyz2].id=='href') {
    id_=lists2[xyz2].value;
    if (id_!="") id_=GetLocalHref(id_);
    targetIndex2 = xyz2; 
    break;
   }
  }
 }
 
 // Пробуем автоматически определить оригинальную обложку по зарезервированным именам
 if (id_=="") {
  if (BinById[strconst3+".png"]) { id_=strconst3+".png"; NewStiCover=id_;}
  else if (BinById[strconst3+".jpg"]) { id_=strconst3+".jpg"; NewStiCover=id_;}
  else if (BinById[strconst3+".jpeg"]) { id_=strconst3+".jpeg"; NewStiCover=id_;}
  else if (BinById[strconst3+".gif"]) { id_=strconst3+".gif"; NewStiCover=id_;} 
 }

 // --- БЕЗОПАСНЫЙ ЗАПРОС ОЧИСТКИ МУСОРА (Модификация v1.6.3) ---
 var HasUnusedBins = false;
 for (var check_i = 0, check_len = bins.length; check_i < check_len; check_i++) {
  var check_id = bins[check_i].all.id.value;
  if (check_id != id && check_id != id_ && (ImgCntById[check_id] == null || ImgCntById[check_id] == "undefined")) {
   if (bins[check_i].all.type.value == "image/png" || bins[check_i].all.type.value == "image/jpeg" || bins[check_i].all.type.value == "image/gif") {
    HasUnusedBins = true;
    break; 
   }
  }
 }

 // Если обнаружен мусор, интерактивно спрашиваем пользователя, нужно ли его вычищать
 if (HasUnusedBins) {
  DeleteUnused = window.confirm("В книге обнаружены неиспользуемые изображения.\nУдалить их?");
 }

 // Защита от логической ошибки структуры FB2
 if (id!='' && id==id_) {
  MsgBox("Ошибка.\nОбложка книги и обложка оригинального издания ссылаются на одно вложение.\nТакая ситуация не обрабатывается.");
  return;
 }
 // ОБРАБОТКА И ВЫДЕЛЕНИЕ ГЛАВНОЙ ОБЛОЖКИ КНИГИ
 if (id!="" && BinById[id]!=null) {
  var coverNewId=strconst2;
  type=BinById[id].all.type.value;
  if (type=="image/png") {coverNewId+=".png";}
  else if (type=="image/jpeg") {coverNewId+=".jpg";} 
  else if (type=="image/gif") {coverNewId+=".gif";} 
  else {
    coverNewId=id;
    if (!ColumnView) NonJpegPngImgs_list+=' "' + strconst6 + coverNewId+'" (обложка)';
    else NonJpegPngImgs_list+='\n   "' + strconst6 + coverNewId+'" (обложка)';
   }
  if (id!=coverNewId) {
   if (BinById[coverNewId]) {
    for (j=1;j<=ImgCntById[coverNewId];j++) {
     ImgsById[strconst4+coverNewId+'"'+j]=ImgsById[coverNewId+'"'+j];
     ImgsById[coverNewId+'"'+j]=null;
     ImgsById[strconst4+coverNewId+'"'+j].setAttribute("href","#"+strconst4+coverNewId);
     var cImg = ImgsById[strconst4+coverNewId+'"'+j].getElementsByTagName("IMG");
     if (cImg && cImg.length > 0) cImg.setAttribute("src","fbw-internal:#"+strconst4+coverNewId);
    }
    ChangeSrcInImageInfo(coverNewId,strconst4+coverNewId);
    BinById[strconst4+coverNewId]=BinById[coverNewId];
    BinById[coverNewId]=null;
    BinById[strconst4+coverNewId].all.id.value=strconst4+coverNewId;
    ImgCntById[strconst4+coverNewId]=ImgCntById[coverNewId];
    ImgCntById[coverNewId]=null;    
    IdUsed[strconst4+coverNewId]=1;
    SavedUnusedCover=BinById[strconst4+coverNewId].cloneNode(true);
    if (DeleteUnused && !ColumnView) BinsWithoutImg_list+=' "'+strconst4+coverNewId+'"';
    else if (DeleteUnused && ColumnView) BinsWithoutImg_list+='\n   "'+strconst4+coverNewId+'"';
   }
   for (j=1;j<=ImgCntById[id];j++) {
    ImgsById[coverNewId+'"'+j]=ImgsById[id+'"'+j];
    ImgsById[id+'"'+j]=null;
    ImgsById[coverNewId+'"'+j].setAttribute("href","#"+coverNewId);
    var cImg2 = ImgsById[coverNewId+'"'+j].getElementsByTagName("IMG");
    if (cImg2 && cImg2.length > 0) cImg2.setAttribute("src","fbw-internal:#"+coverNewId);
   }
   ChangeSrcInImageInfo(id,coverNewId);
   BinById[coverNewId]=BinById[id];
   BinById[id]=null;
   BinById[coverNewId].all.id.value=coverNewId;
   IdUsed[coverNewId]=1;
   tmp_node=BinById[coverNewId].cloneNode(true);
   NewBinobj.appendChild(tmp_node);
   NewTiCover=coverNewId;
   CoversCnt++;
  } else {
   tmp_node=BinById[id].cloneNode(true);
   NewBinobj.appendChild(tmp_node);
   NewTiCover=id;
   IdUsed[id]=1;
  }
 }
 id=id_;

 // ОБРАБОТКА И ВЫДЕЛЕНИЕ ОБЛОЖКИ ОРИГИНАЛЬНОГО ИЗДАНИЯ
 if (id!="" && BinById[id]!=null) {
  var srcCoverNewId=strconst3;
  type=BinById[id].all.type.value;
  if (type=="image/png") {srcCoverNewId+=".png";}
  else if (type=="image/jpeg") {srcCoverNewId+=".jpg";} 
  else if (type=="image/gif") {srcCoverNewId+=".gif";} 
   else {
    srcCoverNewId=id;
    if (!ColumnView) NonJpegPngImgs_list+=' "' + strconst6 + srcCoverNewId+'" (обложка ориг. изд.)';
    else NonJpegPngImgs_list+='\n   "' + strconst6 + srcCoverNewId+'" (обложка ориг. изд.)';
   }
  if (id!=srcCoverNewId) {
   if (BinById[srcCoverNewId]) {
    for (j=1;j<=ImgCntById[srcCoverNewId];j++) {
     ImgsById[strconst4+srcCoverNewId+'"'+j]=ImgsById[srcCoverNewId+'"'+j];
     ImgsById[srcCoverNewId+'"'+j]=null;
     ImgsById[strconst4+srcCoverNewId+'"'+j].setAttribute("href","#"+strconst4+srcCoverNewId);
     var scImg = ImgsById[strconst4+srcCoverNewId+'"'+j].getElementsByTagName("IMG");
     if (scImg && scImg.length > 0) scImg.setAttribute("src","fbw-internal:#"+strconst4+srcCoverNewId);
    }
    ChangeSrcInImageInfo(srcCoverNewId,strconst4+srcCoverNewId);
    BinById[strconst4+srcCoverNewId]=BinById[srcCoverNewId];
    BinById[coverNewId]=null; // Очистка старой ссылки
    BinById[strconst4+srcCoverNewId].all.id.value=strconst4+srcCoverNewId;
    ImgCntById[strconst4+srcCoverNewId]=ImgCntById[srcCoverNewId];
    ImgCntById[srcCoverNewId]=null;    
    IdUsed[strconst4+srcCoverNewId]=1;
    SavedUnusedSrcCover=BinById[strconst4+srcCoverNewId].cloneNode(true);
    if (DeleteUnused && !ColumnView) BinsWithoutImg_list+=' "'+strconst4+srcCoverNewId+'"';
    else if (DeleteUnused && ColumnView) BinsWithoutImg_list+='\n   "'+strconst4+srcCoverNewId+'"';
   }
   for (j=1;j<=ImgCntById[id];j++) {
    ImgsById[srcCoverNewId+'"'+j]=ImgsById[id+'"'+j];
    ImgsById[id+'"'+j]=null;
    ImgsById[srcCoverNewId+'"'+j].setAttribute("href","#"+srcCoverNewId);
    var scImg2 = ImgsById[srcCoverNewId+'"'+j].getElementsByTagName("IMG");
    if (scImg2 && scImg2.length > 0) scImg2.setAttribute("src","fbw-internal:#"+srcCoverNewId);
   }
   ChangeSrcInImageInfo(id,srcCoverNewId);
   var Bin=BinById[id];
   BinById[srcCoverNewId]=Bin;
   BinById[id]=null;
   Bin.all.id.value=srcCoverNewId;
   IdUsed[srcCoverNewId]=1;
   tmp_node=BinById[srcCoverNewId].cloneNode(true);
   NewBinobj.appendChild(tmp_node);
   NewStiCover=srcCoverNewId;
   CoversCnt++;
  } else {
   tmp_node=BinById[id].cloneNode(true);
   NewBinobj.appendChild(tmp_node);
   NewStiCover=id;
   IdUsed[id]=1;
  }
 }

 // ВРЕМЕННОЕ ПЕРЕИМЕНОВАНИЕ ОСТАВШИХСЯ ИЛЛЮСТРАЦИЙ ВО ИЗБЕЖАНИЕ НАЛОЖЕНИЯ ИМЕН
 var NonUsedBinary = 0;
 var ImgCnt2;
 var currentBin;
 for (i=0, len=bins.length; i<len; i++) {
  id=bins[i].all.id.value;
  var tempNewId="img_"+RandomNum+"_"+id;
  if (IdUsed[id]==null || IdUsed[id]==undefined) {
   ImgCnt2=ImgCntById[id];
   if (ImgCnt2!=null && ImgCnt2!="undefined") {
    for (j=1;j<=ImgCnt2;j++) {
     ImgsById[tempNewId+'"'+j]=ImgsById[id+'"'+j];
     ImgsById[id+'"'+j]=null;
     ImgsById[tempNewId+'"'+j].setAttribute("href","#"+tempNewId);
     var tImg = ImgsById[tempNewId+'"'+j].getElementsByTagName("IMG");
     if (nonVImg && nonVImg.length > 0) nonVImg[0].setAttribute("src","fbw-internal:#"+nonValidNewId);
    }
   } else { 
    IdUsed[id]++; 
    NonUsedBinary++; 
   }
   ChangeSrcInImageInfo(id,tempNewId);
   currentBin=BinById[id];
   if (currentBin) { 
    BinById[tempNewId]=currentBin;
    BinById[id]=null;
    currentBin.all.id.value=tempNewId;
    ImgCntById[tempNewId]=ImgCntById[id];
    ImgCntById[id]=null;
   }
  }
 }

 // ОКОНЧАТЕЛЬНОЕ СИСТЕМНОЕ ПЕРЕИМЕНОВАНИЕ КАРТИНОК ПО ПОРЯДКУ (i_001, i_002...)
 var cnt=0;
 if (typeof tempPrefixLength == "undefined" && typeof RandomNum != "undefined") {
    var tempPrefixLength = ("img_" + RandomNum + "_").length;
 }

 for (i=1; i<=ImgCnt; i++) {
  imgDiv=ImgByNum[i].parentNode;
  if ((imgDiv.nodeName=="DIV" || imgDiv.nodeName=="SPAN") && imgDiv.className=="image") {
   id=GetLocalHref(imgDiv.getAttribute("href"));
   if (id!='"' && BinById[id] &&
       (BinById[id].all.type.value=="image/png" ||
        BinById[id].all.type.value=="image/jpeg" ||
        BinById[id].all.type.value=="image/gif")) { 
    if (ImgCntById[id]!=null) {
     if (IdUsed[id]==null || IdUsed[id]=="undefined") {
      cnt++;
      var innerType=BinById[id].all.type.value; 
      var finalNewId=PoShablonu(strconst1,cnt);
      if (innerType=="image/png") finalNewId+=".png";
      else if (innerType=="image/jpeg") finalNewId+=".jpg";
      else if (innerType=="image/gif") finalNewId+=".gif"; 
      ChangeSrcInImageInfo(id,finalNewId);
      var finalBin=BinById[id];
      BinById[finalNewId]=finalBin;
      finalBin.all.id.value=finalNewId;
      for (j=1;j<=ImgCntById[id];j++) {
       ImgsById[finalNewId+'"'+j]=ImgsById[id+'"'+j];
       ImgsById[id+'"'+j]=null;
       var innerImg = ImgsById[finalNewId+'"'+j].getElementsByTagName("IMG");
       if (innerImg && innerImg.length > 0) {
         innerImg.src="";
         ImgsById[finalNewId+'"'+j].setAttribute("href","#"+finalNewId);
         innerImg.src="fbw-internal:#"+finalNewId;
       }
      }
      ImgCntById[finalNewId]=ImgCntById[id];
      ImgCntById[id]=null;
      tmp_node=BinById[finalNewId].cloneNode(true);
      NewBinobj.appendChild(tmp_node);
      IdUsed[finalNewId]=1;
     }
     else { IdUsed[id]++; }
    }
   }
  }
 }
 if (!DeleteUnused) {
    if (SavedUnusedCover) NewBinobj.appendChild(SavedUnusedCover);
    if (SavedUnusedSrcCover) NewBinobj.appendChild(SavedUnusedSrcCover);
 } else {
    if (SavedUnusedCover) NonUsedBinary++;
    if (SavedUnusedSrcCover) NonUsedBinary++;
 }

 // ИЗОЛЯЦИЯ И ПЕРЕИМЕНОВАНИЕ НЕИСПОЛЬЗУЕМЫХ В ТЕКСТЕ ИЗОБРАЖЕНИЙ (ПРЕФИКС unused_)
 var n;
 for (var k=0, len=bins.length; k<len; k++) {
  id=bins[k].all.id.value;
  
  if (id == NewTiCover || id == NewStiCover || id == strconst2 + ".jpg" || id == strconst2 + ".png" || id == strconst2 + ".gif") {
     continue; 
  }

  if (IdUsed[id]==null || IdUsed[id]=="undefined") {
   if (bins[k].all.type.value=="image/png" || bins[k].all.type.value=="image/jpeg" || bins[k].all.type.value=="image/gif") { 
    var unusedNewId;
    if (id.substr(tempPrefixLength,strconst4.length)==strconst4) unusedNewId=id.substr(tempPrefixLength);
    else {
     var baseUnusedId=strconst4+id.substr(tempPrefixLength);
     unusedNewId=baseUnusedId;
     n=0;
     while (BinById[unusedNewId] || BinById["img_"+RandomNum+"_"+unusedNewId]) {
      unusedNewId=baseUnusedId+"_"+n;
      n++;
     }
    }
    
    NonUsedBinary++;
    if (!ColumnView) BinsWithoutImg_list+=' "'+unusedNewId+'"';
    else BinsWithoutImg_list+='\n   "'+unusedNewId+'"';

    if (!DeleteUnused) {
       ChangeSrcInImageInfo(id,unusedNewId);
       BinById[unusedNewId]=bins[k];
       bins[k].all.id.value=unusedNewId;
       tmp_node=bins[k].cloneNode(true);
       NewBinobj.appendChild(tmp_node);
       IdUsed[unusedNewId]=1;
    }
   }
  }
 }

 // МАРКИРОВКА ВЛОЖЕНИЙ С НЕПОДДЕРЖИВАЕМЫМИ ФОРМАТАМИ (ПРЕФИКС nonjpegpnggif_)
 for (var m=1; m<=NonJpegPngImgs["0"]; m++) {
  id=NonJpegPngImgs[m].all.id.value;
  
  if (id == NewTiCover || id == NewStiCover) continue;

  if (IdUsed[id]==null || IdUsed[id]=="undefined") {
   var nonValidNewId;
   if (id.substr(tempPrefixLength,strconst6.length)==strconst6) {
    nonValidNewId=id.substr(tempPrefixLength);
   }
   else {
    var baseNonValidId=strconst6+id.substr(tempPrefixLength);
    nonValidNewId=baseNonValidId;
    n=0;
    while (BinById[nonValidNewId] || BinById["img_"+RandomNum+"_"+nonValidNewId]) {
     nonValidNewId=baseNonValidId+"_"+n;
     n++;
    }
   }
   
   if (!ColumnView) NonJpegPngImgs_list+=' "'+nonValidNewId+'"';
   else NonJpegPngImgs_list+='\n   "'+nonValidNewId+'"';

   if (!DeleteUnused) {
      if (ImgCntById[id] != null && ImgCntById[id] != "undefined") { 
         for (j=1; j<=ImgCntById[id]; j++) {
          ImgsById[nonValidNewId+'"'+j]=ImgsById[id+'"'+j];
          ImgsById[id+'"'+j]=null;
          if (ImgsById[nonValidNewId+'"'+j]) {
             ImgsById[nonValidNewId+'"'+j].setAttribute("href","#"+nonValidNewId);
             var nonVImg = ImgsById[nonValidNewId+'"'+j].getElementsByTagName("IMG");
             if (nonVImg && nonVImg.length > 0) nonVImg.setAttribute("src","fbw-internal:#"+nonValidNewId);
          }
         }
      }
      ChangeSrcInImageInfo(id, nonValidNewId); 
      NonJpegPngImgs[m].all.id.value=nonValidNewId;
      tmp_node=NonJpegPngImgs[m].cloneNode(true);
      NewBinobj.appendChild(tmp_node);
   }
  }
 }
 
 document.all.binobj.parentNode.replaceChild(NewBinobj, document.all.binobj);
 
 PutSpacers(document.all.binobj);
 FillCoverList();

 if (NewTiCover != "" && targetIndex1 != -1) {
    lists1[targetIndex1].value = "#" + NewTiCover;
 }
 if (NewStiCover != "" && targetIndex2 != -1) {
    lists2[targetIndex2].value = "#" + NewStiCover;
 }
 
 // =========================================================================
 // БЛОК ИСПРАВЛЕНИЯ КЭША ИМЕДЖЕЙ (КАСКАДНЫЙ REFLOW: Windows XP – Windows 11)
 // =========================================================================
 var fbw_body = document.getElementById("fbw_body");
 if (fbw_body) {
    try {
       var originalDisplay = fbw_body.style.display || "block";
       fbw_body.style.display = "none";
       
       var forceReflowGeometry = fbw_body.offsetHeight;
       
       fbw_body.style.display = originalDisplay;
       
       var oldClass = fbw_body.className;
       fbw_body.className = oldClass + " _reflow_bugfix";
       var forceReflowClass = fbw_body.offsetWidth;
       fbw_body.className = oldClass;
    } catch(e) {}
 }

 updateImages();
 updateImages();
 // =========================================================================
 
 var Tf=new Date().getTime();
 var Tmin = Math.floor((Tf-Ts)/60000);
 var Tsec = Math.ceil((Tf-Ts)/1000-Tmin*60);
 var Tsek = Math.ceil(10*((Tf-Ts)/1000-Tmin*60))/10;
 var Tssek = Math.ceil(100*((Tf-Ts)/1000-Tmin*60))/100;
 var Tsssek = Math.ceil(1000*((Tf-Ts)/1000-Tmin*60))/1000;
 var TimeStr = "";
 
 if (Tmin >= 1) {
    TimeStr = Tmin + " мин " + Tsec + " с";
 } else if (Tssek < 1) {
    TimeStr = Tsssek + " с";
 } else {
    TimeStr = Tsek + " с";
 }

 var st2="";
 if (BinsWithoutImg_list!="") {
    st2 += '\n' + (DeleteUnused ? 'Удаленные' : 'Неиспользуемые') + ' бинарные объекты:' + BinsWithoutImg_list;
 }
 if (ImgsWithoutBin_list!="") {st2+='\nБезбинарные иллюстрации:'+ImgsWithoutBin_list;}
 if (NonLocalImgs_list!="") {st2+='\nИзображения с нелокальными адресами:'+NonLocalImgs_list;}
 if (NonJpegPngImgs_list!="") {st2+='\nВложения не image/jpeg, не image/png, не image/gif:'+NonJpegPngImgs_list;}
 if (st2!="") st2="\n"+st2;
 
 window.external.EndUndoUnit(document);

 var resultWordUnused = DeleteUnused 
    ? {nom: 'Удален', gen: 'Удалены', plu: 'Удалено'} 
    : {nom: 'Обнаружен', gen: 'Обнаружены', plu: 'Обнаружено'};
    
 var resultNameUnused = DeleteUnused
    ? {nom: 'неиспользуемый объект', gen: 'неиспользуемых объекта', plu: 'неиспользуемых объектов'}
    : {nom: 'неиспользуемый бинарный объект', gen: 'неиспользуемых бинарных объекта', plu: 'неиспользуемых бинарных объектов'};

 MsgBox('                   –= Sclex Script =– \n'+
        '  "Унифицировать иллюстрации и вложения"\n'+
        '                       v' + versionNumber + '\n\n'+
        units(cnt,{nom: 'Переименована', gen: 'Переименованы', plu: 'Переименовано'})+' '+cnt+' '+units(cnt,{nom: 'иллюстрация', gen: 'иллюстрации', plu: 'иллюстраций'})+'.\n'+ 
        units(CoversCnt,{nom: 'Переименована', gen: 'Переименованы', plu: 'Переименовано'})+' '+CoversCnt+' '+units(CoversCnt,{nom: 'обложка', gen: 'обложки', plu: 'обложек'})+'.\n'+
        units(NonLocalAddress,{nom: 'Обнаружено', gen: 'Обнаружены', plu: 'Обнаружено'})+' '+NonLocalAddress+' '+units(NonLocalAddress,{nom: 'иллюстрация', gen: 'иллюстрации', plu: 'иллюстраций'})+' с нелокальным адресом.\n'+
        units(NonUsedBinary, resultWordUnused)+' '+NonUsedBinary+' '+units(NonUsedBinary, resultNameUnused)+'.\n'+
        units(BinaryNotPresent,{nom: 'Обнаружена', gen: 'Обнаружены', plu: 'Обнаружено'})+' '+BinaryNotPresent+' '+units(BinaryNotPresent,{nom: 'безбинарная иллюстрация', gen: 'безбинарные иллюстрации', plu: 'безбинарных иллюстраций'})+'.\n'+
        units(NonJpegPngImgs["0"],{nom: 'Обнаружена', gen: 'Обнаружены', plu: 'Обнаружено'})+' '+NonJpegPngImgs["0"]+' '+units(NonJpegPngImgs["0"],{nom: 'иллюстрация', gen: 'иллюстрации', plu: 'иллюстраций'})+' с типом, не корректным для FB2.\n\n'+
        'Время выполнения: '+TimeStr+'.'+st2);
}
