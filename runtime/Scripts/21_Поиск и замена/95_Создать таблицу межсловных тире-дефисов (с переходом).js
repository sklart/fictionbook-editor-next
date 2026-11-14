// Скрипт «Создать таблицу межсловных тире- дефисов (с переходом)».
// v.1.0
// автор - stokber+DeepSpeek (июль, 2026)
function Run() {
    var scriptName = "Создать таблицу межсловных тире- дефисов (с переходом)";
    var scriptVersion = "1.0";

    // НАСТРОЙКИ
    var headerBgColor = "orange";
    var singleHeader = true;
    // var singleHeader = false;
    
    // ========== НАСТРОЙКИ ОКНА РЕЗУЛЬТАТОВ ==========
    var windowPosition = "left";   // "left" - левая половина экрана, "right" - правая половина
    var windowWidthPercent = 0.49;  // доля ширины экрана (0.5 = половина)
    var windowHeightPercent = 0.95; // доля высоты экрана (0.5 = половина)
    // ================================================

    var helpText = "<small><strong>СПРАВКА:</strong></small><br><small>При большом количестве совпадений, открытие таблицы может занять некоторое время.<br>Для перехода к совпадению кликните по соответствующей ячейке <strong>Позиция:Длина</strong>.</small>";

    var nbspEntity = "&nbsp;";
    try {
        var nbspChar = window.external.GetNBSP();
        if (nbspChar.charCodeAt(0) != 160)
            nbspEntity = nbspChar;
    } catch (e) {
        nbspChar = String.fromCharCode(160);
    }

    var sel = document.getElementById("fbw_body");
    if (!sel) {
        alert("Не найден элемент fbw_body");
        return;
    }

   // правка кода для таблицы:
    var fromHTML = sel.innerHTML;
    fromHTML = fromHTML.replace(new RegExp("<SPAN onresizestart=[^>]+?><IMG\\b.*?></SPAN>", "gi"), "░░░");
    fromHTML = fromHTML.replace(/<IMG\b.*?>/gi, "▓▓▓");

    var fromText = extractTextWithNewlines(fromHTML);
    fromText = fromText.replace(/^ $/igm, "");
    fromText = fromText.replace(/▓▓▓(?=\n▓▓▓)/gm, "▓▓");
    fromText = fromText.replace(/░░░$/gm, "░░");

    // начало блока шаблонов регекспов, их заголовков и некоторых параметров:
    var rg1_name = "слово-ДЕФИС-слово"; // заголовок списка совпадений.
    var rg1 = new RegExp("([a-zа-яё]+[-])+[a-zа-яё]+", "ig"); // регексп для поиска совпадений.
    var rg1_onlyCount = false; // true — показывать ТОЛЬКО количество совпадений; false — показывать и совпадения и их количество.
    var rg1_skipIfEmpty = true; // если совпадений нет, то true — не показывать заголовок группы совпадений, true — показывать.
    
    var rg2_name = "слово-пробел-ДЕФИС-пробел-слово";
    var rg2 = new RegExp("[a-zа-яё]+("+nbspChar+"|\\s)+[-]("+nbspChar+"|\\s)+[a-zа-яё]+", "ig");
    var rg2_onlyCount = false; // 
    var rg2_skipIfEmpty = true; // 
    
    var rg3_name = "слово-пробел-ДЕФИС-слово";
    var rg3 = new RegExp("[a-zа-яё]+("+nbspChar+"|\\s)+[-][a-zа-яё]+", "ig");
    var rg3_onlyCount = false; // 
    var rg3_skipIfEmpty = true; // 
    
    var rg4_name = "слово-ДЕФИС-пробел-слово";
    var rg4 = new RegExp("[a-zа-яё]+[-]("+nbspChar+"|\\s)+[a-zа-яё]+", "ig");
    var rg4_onlyCount = false; // 
    var rg4_skipIfEmpty = true; // 
//=========
    var rg5_name = "слово-пробел-ТИРЕ-пробел-слово";
    var rg5 = new RegExp("[a-zа-яё]+("+nbspChar+"|\\s)+[—]("+nbspChar+"|\\s)+[a-zа-яё]+", "ig");
    var rg5_onlyCount = false; 
    var rg5_skipIfEmpty = true;
    
    var rg6_name = "слово-ТИРЕ-слово";
    var rg6 = new RegExp("[a-zа-яё]+[—][a-zа-яё]+", "ig");
    var rg6_onlyCount = false; 
    var rg6_skipIfEmpty = true;
    
    var rg7_name = "слово-пробел-ТИРЕ-слово";
    var rg7 = new RegExp("[a-zа-яё]+("+nbspChar+"|\\s)+[—][a-zа-яё]+", "ig");
    var rg7_onlyCount = false; 
    var rg7_skipIfEmpty = true;
    
    var rg8_name = "слово-ТИРЕ-пробел-слово";
    var rg8 = new RegExp("[a-zа-яё]+[—]("+nbspChar+"|\\s)+[a-zа-яё]+", "ig");
    var rg8_onlyCount = false; 
    var rg8_skipIfEmpty = true;

    var rg9_name = "слово-КОРОТКОЕ ТИРЕ-слово";
    var rg9 = new RegExp("[a-zа-яё]+[–][a-zа-яё]+", "ig");
    var rg9_onlyCount = false;
    var rg9_skipIfEmpty = true;

    var rg10_name = "слово-пробел-КОРОТКОЕ ТИРЕ-пробел-слово";
    var rg10 = new RegExp("[a-zа-яё]+("+nbspChar+"|\\s)+[–]("+nbspChar+"|\\s)+[a-zа-яё]+", "ig");
    var rg10_onlyCount = false;
    var rg10_skipIfEmpty = true;
    
    var rg11_name = "слово-пробел-КОРОТКОЕ ТИРЕ-слово";
    var rg11 = new RegExp("[a-zа-яё]+("+nbspChar+"|\\s)+[–][a-zа-яё]+", "ig");
    var rg11_onlyCount = false;
    var rg11_skipIfEmpty = true;
    
    var rg12_name = "слово-КОРОТКОЕ ТИРЕ-пробел-слово";
    var rg12 = new RegExp("[a-zа-яё]+[–]("+nbspChar+"|\\s)+[a-zа-яё]+", "ig");
    var rg12_onlyCount = false;
    var rg12_skipIfEmpty = true;

	var rg13_name = "число-ДЕФИС-число";
    var rg13 = new RegExp("([0-9]+[-])+[0-9]+", "ig");
    var rg13_onlyCount = false; 
    var rg13_skipIfEmpty = true;
	
	var rg14_name = "число-ДЕФИС-слово";
    var rg14 = new RegExp("([0-9]+[-])+[a-zа-яё]+", "ig");
    var rg14_onlyCount = false; 
    var rg14_skipIfEmpty = true;
	
	var rg15_name = "слово-ДЕФИС-число";
    var rg15 = new RegExp("([a-zа-яё]+[-])+[0-9]+", "ig");
    var rg15_onlyCount = false; 
    var rg15_skipIfEmpty = true;
	
	var rg16_name = "число-пробел-ДЕФИС-пробел-число";
    var rg16 = new RegExp("[0-9]+("+nbspChar+"|\\s)+[-]("+nbspChar+"|\\s)+[0-9]+", "ig");
    var rg16_onlyCount = false; // 
    var rg16_skipIfEmpty = true; //
	
	var rg17_name = "число-пробел-ДЕФИС-пробел-слово";
    var rg17 = new RegExp("[0-9]+("+nbspChar+"|\\s)+[-]("+nbspChar+"|\\s)+[a-zа-яё]+", "ig");
    var rg17_onlyCount = false; // 
    var rg17_skipIfEmpty = true; //
	
	var rg18_name = "слово-пробел-ДЕФИС-пробел-число";
    var rg18 = new RegExp("[a-zа-яё]+("+nbspChar+"|\\s)+[-]("+nbspChar+"|\\s)+[0-9]+", "ig");
    var rg18_onlyCount = false; // 
    var rg18_skipIfEmpty = true; //

	var rg19_name = "число-пробел-ДЕФИС-число";
    var rg19 = new RegExp("[0-9]+("+nbspChar+"|\\s)+[-][0-9]+", "ig");
    var rg19_onlyCount = false; // 
    var rg19_skipIfEmpty = true; // 
	
	var rg20_name = "число-пробел-ДЕФИС-слово";
    var rg20 = new RegExp("[0-9]+("+nbspChar+"|\\s)+[-][a-zа-яё]+", "ig");
    var rg20_onlyCount = false; // 
    var rg20_skipIfEmpty = true; //
	
	var rg21_name = "слово-пробел-ДЕФИС-число";
    var rg21 = new RegExp("[a-zа-яё]+("+nbspChar+"|\\s)+[-][0-9]+", "ig");
    var rg21_onlyCount = false; // 
    var rg21_skipIfEmpty = true; //
    
    var rg22_name = "число-ДЕФИС-пробел-число";
    var rg22 = new RegExp("[0-9]+[-]("+nbspChar+"|\\s)+[0-9]+", "ig");
    var rg22_onlyCount = false; // 
    var rg22_skipIfEmpty = true; // 
	
	var rg23_name = "число-ДЕФИС-пробел-слово";
    var rg23 = new RegExp("[0-9]+[-]("+nbspChar+"|\\s)+[a-zа-яё]+", "ig");
    var rg23_onlyCount = false; // 
    var rg23_skipIfEmpty = true; //
	
	var rg24_name = "слово-ДЕФИС-пробел-число";
    var rg24 = new RegExp("[a-zа-яё]+[-]("+nbspChar+"|\\s)+[0-9]+", "ig");
    var rg24_onlyCount = false; // 
    var rg24_skipIfEmpty = true; //

	var rg25_name = "число-ТИРЕ-число";
    var rg25 = new RegExp("([0-9]+[—])+[0-9]+", "ig");
    var rg25_onlyCount = false; 
    var rg25_skipIfEmpty = true;
	
	var rg26_name = "число-ТИРЕ-слово";
    var rg26 = new RegExp("([0-9]+[—])+[a-zа-яё]+", "ig");
    var rg26_onlyCount = false; 
    var rg26_skipIfEmpty = true;
	
	var rg27_name = "слово-ТИРЕ-число";
    var rg27 = new RegExp("([a-zа-яё]+[—])+[0-9]+", "ig");
    var rg27_onlyCount = false; 
    var rg27_skipIfEmpty = true;
	
	var rg28_name = "число-пробел-ТИРЕ-пробел-число";
    var rg28 = new RegExp("[0-9]+("+nbspChar+"|\\s)+[—]("+nbspChar+"|\\s)+[0-9]+", "ig");
    var rg28_onlyCount = false; // 
    var rg28_skipIfEmpty = true; //
	
	var rg29_name = "число-пробел-ТИРЕ-пробел-слово";
    var rg29 = new RegExp("[0-9]+("+nbspChar+"|\\s)+[—]("+nbspChar+"|\\s)+[a-zа-яё]+", "ig");
    var rg29_onlyCount = false; // 
    var rg29_skipIfEmpty = true; //
	
	var rg30_name = "слово-пробел-ТИРЕ-пробел-число";
    var rg30 = new RegExp("[a-zа-яё]+("+nbspChar+"|\\s)+[—]("+nbspChar+"|\\s)+[0-9]+", "ig");
    var rg30_onlyCount = false; // 
    var rg30_skipIfEmpty = true; //

	var rg31_name = "число-пробел-ТИРЕ-число";
    var rg31 = new RegExp("[0-9]+("+nbspChar+"|\\s)+[—][0-9]+", "ig");
    var rg31_onlyCount = false; // 
    var rg31_skipIfEmpty = true; // 
	
	var rg32_name = "число-пробел-ТИРЕ-слово";
    var rg32 = new RegExp("[0-9]+("+nbspChar+"|\\s)+[—][a-zа-яё]+", "ig");
    var rg32_onlyCount = false; // 
    var rg32_skipIfEmpty = true; //
	
	var rg33_name = "слово-пробел-ТИРЕ-число";
    var rg33 = new RegExp("[a-zа-яё]+("+nbspChar+"|\\s)+[—][0-9]+", "ig");
    var rg33_onlyCount = false; // 
    var rg33_skipIfEmpty = true; //
    
    var rg34_name = "число-ТИРЕ-пробел-число";
    var rg34 = new RegExp("[0-9]+[—]("+nbspChar+"|\\s)+[0-9]+", "ig");
    var rg34_onlyCount = false; // 
    var rg34_skipIfEmpty = true; // 
	
	var rg35_name = "число-ТИРЕ-пробел-слово";
    var rg35 = new RegExp("[0-9]+[—]("+nbspChar+"|\\s)+[a-zа-яё]+", "ig");
    var rg35_onlyCount = false; // 
    var rg35_skipIfEmpty = true; //
	
	var rg36_name = "слово-ТИРЕ-пробел-число";
    var rg36 = new RegExp("[a-zа-яё]+[—]("+nbspChar+"|\\s)+[0-9]+", "ig");
    var rg36_onlyCount = false; // 
    var rg36_skipIfEmpty = true; //

	var rg37_name = "число-КОРОТКОЕ ТИРЕ-число";
    var rg37 = new RegExp("([0-9]+[–])+[0-9]+", "ig");
    var rg37_onlyCount = false; 
    var rg37_skipIfEmpty = true;
	
	var rg38_name = "число-КОРОТКОЕ ТИРЕ-слово";
    var rg38 = new RegExp("([0-9]+[–])+[a-zа-яё]+", "ig");
    var rg38_onlyCount = false; 
    var rg38_skipIfEmpty = true;
	
	var rg39_name = "слово-КОРОТКОЕ ТИРЕ-число";
    var rg39 = new RegExp("([a-zа-яё]+[–])+[0-9]+", "ig");
    var rg39_onlyCount = false; 
    var rg39_skipIfEmpty = true;
	
	var rg40_name = "число-пробел-КОРОТКОЕ ТИРЕ-пробел-число";
    var rg40 = new RegExp("[0-9]+("+nbspChar+"|\\s)+[–]("+nbspChar+"|\\s)+[0-9]+", "ig");
    var rg40_onlyCount = false; // 
    var rg40_skipIfEmpty = true; //
	
	var rg41_name = "число-пробел-КОРОТКОЕ ТИРЕ-пробел-слово";
    var rg41= new RegExp("[0-9]+("+nbspChar+"|\\s)+[–]("+nbspChar+"|\\s)+[a-zа-яё]+", "ig");
    var rg41_onlyCount = false; // 
    var rg41_skipIfEmpty = true; //
	
	var rg42_name = "слово-пробел-КОРОТКОЕ ТИРЕ-пробел-число";
    var rg42 = new RegExp("[a-zа-яё]+("+nbspChar+"|\\s)+[–]("+nbspChar+"|\\s)+[0-9]+", "ig");
    var rg42_onlyCount = false; // 
    var rg42_skipIfEmpty = true; //

	var rg43_name = "число-пробел-КОРОТКОЕ ТИРЕ-число";
    var rg43 = new RegExp("[0-9]+("+nbspChar+"|\\s)+[–][0-9]+", "ig");
    var rg43_onlyCount = false; // 
    var rg43_skipIfEmpty = true; // 
	
	var rg44_name = "число-пробел-КОРОТКОЕ ТИРЕ-слово";
    var rg44 = new RegExp("[0-9]+("+nbspChar+"|\\s)+[–][a-zа-яё]+", "ig");
    var rg44_onlyCount = false; // 
    var rg44_skipIfEmpty = true; //
	
	var rg45_name = "слово-пробел-КОРОТКОЕ ТИРЕ-число";
    var rg45 = new RegExp("[a-zа-яё]+("+nbspChar+"|\\s)+[–][0-9]+", "ig");
    var rg45_onlyCount = false; // 
    var rg45_skipIfEmpty = true; //
    
    var rg46_name = "число-КОРОТКОЕ ТИРЕ-пробел-число";
    var rg46 = new RegExp("[0-9]+[–]("+nbspChar+"|\\s)+[0-9]+", "ig");
    var rg46_onlyCount = false; // 
    var rg46_skipIfEmpty = true; // 
	
	var rg47_name = "число-КОРОТКОЕ ТИРЕ-пробел-слово";
    var rg47 = new RegExp("[0-9]+[–]("+nbspChar+"|\\s)+[a-zа-яё]+", "ig");
    var rg47_onlyCount = false; // 
    var rg47_skipIfEmpty = true; //
	
	var rg48_name = "слово-КОРОТКОЕ ТИРЕ-пробел-число";
    var rg48 = new RegExp("[a-zа-яё]+[–]("+nbspChar+"|\\s)+[0-9]+", "ig");
    var rg48_onlyCount = false; // 
    var rg48_skipIfEmpty = true; //

	var rg49_name = "начало строки-ДЕФИС-текст";
    var rg49 = new RegExp("^("+nbspChar+"|\\s)*[-][^"+nbspChar+"\\s—–-]", "ig");
    var rg49_onlyCount = false; 
    var rg49_skipIfEmpty = true;
	
	var rg50_name = "начало строки-ДЕФИС-пробел";
    var rg50 = new RegExp("^("+nbspChar+"|\\s)*[-]["+nbspChar+"\\s]+(?![-—–])", "ig");
    var rg50_onlyCount = false; 
    var rg50_skipIfEmpty = true;
	
	var rg51_name = "начало строки-ТИРЕ-текст";
    var rg51 = new RegExp("^("+nbspChar+"|\\s)*[—][^"+nbspChar+"\\s—–-]", "ig");
    var rg51_onlyCount = false; 
    var rg51_skipIfEmpty = true;
	
	var rg52_name = "начало строки-ТИРЕ-пробел";
    var rg52 = new RegExp("^("+nbspChar+"|\\s)*[—]["+nbspChar+"\\s]+(?![-—–])", "ig");
    var rg52_onlyCount = false; 
    var rg52_skipIfEmpty = true;
	
	var rg53_name = "начало строки-КОРОТКОЕ ТИРЕ-текст";
    var rg53 = new RegExp("^("+nbspChar+"|\\s)*[–][^"+nbspChar+"\\s—–-]", "ig");
    var rg53_onlyCount = false; 
    var rg53_skipIfEmpty = true;
	
	var rg54_name = "начало строки-КОРОТКОЕ ТИРЕ-пробел";
    var rg54 = new RegExp("^("+nbspChar+"|\\s)*[–]["+nbspChar+"\\s]+(?![-—–])", "ig");
    var rg54_onlyCount = false; 
    var rg54_skipIfEmpty = true;

	var rg55_name = "текст-ДЕФИС-конец строки";
    var rg55 = new RegExp("[^"+nbspChar+"\\s—–-][-]["+nbspChar+"\\s]*$", "ig");
    var rg55_onlyCount = false; 
    var rg55_skipIfEmpty = true;
	
	var rg56_name = "пробел-ДЕФИС-конец строки";
    var rg56 = new RegExp("[^—–-]("+nbspChar+"|\\s)+[-]["+nbspChar+"\\s]*$", "ig");
    var rg56_onlyCount = false; 
    var rg56_skipIfEmpty = true;
	
	var rg57_name = "текст-ТИРЕ-конец строки";
    var rg57 = new RegExp("[^"+nbspChar+"\\s—–-][—]["+nbspChar+"\\s]*$", "ig");
    var rg57_onlyCount = false; 
    var rg57_skipIfEmpty = true;
	
	var rg58_name = "пробел-ТИРЕ-конец строки";
    var rg58 = new RegExp("[^—–-]("+nbspChar+"|\\s)+[—]["+nbspChar+"\\s]*$", "ig");
    var rg58_onlyCount = false; 
    var rg58_skipIfEmpty = true;
	
	var rg59_name = "текст-КОРОТКОЕ ТИРЕ-конец строки";
    var rg59 = new RegExp("[^"+nbspChar+"\\s—–-][–]["+nbspChar+"\\s]*$", "ig");
    var rg59_onlyCount = false; 
    var rg59_skipIfEmpty = true;
	
	var rg60_name = "пробел-КОРОТКОЕ ТИРЕ-конец строки";
    var rg60 = new RegExp("[^—–-]("+nbspChar+"|\\s)+[–]["+nbspChar+"\\s]*$", "ig");
    var rg60_onlyCount = false; 
    var rg60_skipIfEmpty = true;

	var rg100_name = "ДЕФИС или (КОРОТКОЕ) ТИРЕ в пустой строке";
    var rg100 = new RegExp("^("+nbspChar+"|\\s)*[-—–]("+nbspChar+"|\\s)*$", "ig");
    var rg100_onlyCount = false;
    var rg100_skipIfEmpty = true;
	
	var rg101_name = "Группы дефисов-тире подряд";
    var rg101 = new RegExp("[-—–](("+nbspChar+"|\\s)*[-—–])+", "ig");
    var rg101_onlyCount = false;
    var rg101_skipIfEmpty = true;

    // ---- При добавлении нового регекспа (rg4) не забудьте добавить его в массив regexGroups ниже ----
    // ---- а также определить переменные rg4_name, rg4, rg4_onlyCount, rg4_skipIfEmpty ----

    // конец блока шаблонов регекспов и их заголовков.
    // ========================================

    // Массив групп регекспов (пользователь должен добавлять сюда новые элементы):
    var regexGroups = [
        { name: rg1_name, regex: rg1, onlyCount: rg1_onlyCount, skipIfEmpty: rg1_skipIfEmpty },
        { name: rg2_name, regex: rg2, onlyCount: rg2_onlyCount, skipIfEmpty: rg2_skipIfEmpty },
        { name: rg3_name, regex: rg3, onlyCount: rg3_onlyCount, skipIfEmpty: rg3_skipIfEmpty },
        { name: rg4_name, regex: rg4, onlyCount: rg4_onlyCount, skipIfEmpty: rg4_skipIfEmpty },
        { name: rg5_name, regex: rg5, onlyCount: rg5_onlyCount, skipIfEmpty: rg5_skipIfEmpty },
        { name: rg6_name, regex: rg6, onlyCount: rg6_onlyCount, skipIfEmpty: rg6_skipIfEmpty },
        { name: rg7_name, regex: rg7, onlyCount: rg7_onlyCount, skipIfEmpty: rg7_skipIfEmpty },
        { name: rg8_name, regex: rg8, onlyCount: rg8_onlyCount, skipIfEmpty: rg8_skipIfEmpty },
        { name: rg9_name, regex: rg9, onlyCount: rg9_onlyCount, skipIfEmpty: rg9_skipIfEmpty },
        { name: rg10_name, regex: rg10, onlyCount: rg10_onlyCount, skipIfEmpty: rg10_skipIfEmpty },
        { name: rg11_name, regex: rg11, onlyCount: rg11_onlyCount, skipIfEmpty: rg11_skipIfEmpty },
        { name: rg12_name, regex: rg12, onlyCount: rg12_onlyCount, skipIfEmpty: rg12_skipIfEmpty },
        { name: rg13_name, regex: rg13, onlyCount: rg13_onlyCount, skipIfEmpty: rg13_skipIfEmpty },
        { name: rg14_name, regex: rg14, onlyCount: rg14_onlyCount, skipIfEmpty: rg14_skipIfEmpty },
        { name: rg15_name, regex: rg15, onlyCount: rg15_onlyCount, skipIfEmpty: rg15_skipIfEmpty },
		{ name: rg16_name, regex: rg16, onlyCount: rg16_onlyCount, skipIfEmpty: rg16_skipIfEmpty },
        { name: rg17_name, regex: rg17, onlyCount: rg17_onlyCount, skipIfEmpty: rg17_skipIfEmpty },
        { name: rg18_name, regex: rg18, onlyCount: rg18_onlyCount, skipIfEmpty: rg18_skipIfEmpty },
		{ name: rg19_name, regex: rg19, onlyCount: rg19_onlyCount, skipIfEmpty: rg19_skipIfEmpty },
        { name: rg20_name, regex: rg20, onlyCount: rg20_onlyCount, skipIfEmpty: rg20_skipIfEmpty },
        { name: rg21_name, regex: rg21, onlyCount: rg21_onlyCount, skipIfEmpty: rg21_skipIfEmpty },
        { name: rg22_name, regex: rg22, onlyCount: rg22_onlyCount, skipIfEmpty: rg22_skipIfEmpty },
        { name: rg23_name, regex: rg23, onlyCount: rg23_onlyCount, skipIfEmpty: rg23_skipIfEmpty },
        { name: rg24_name, regex: rg24, onlyCount: rg24_onlyCount, skipIfEmpty: rg24_skipIfEmpty },
        { name: rg25_name, regex: rg25, onlyCount: rg25_onlyCount, skipIfEmpty: rg25_skipIfEmpty },
		{ name: rg26_name, regex: rg26, onlyCount: rg26_onlyCount, skipIfEmpty: rg26_skipIfEmpty },
        { name: rg27_name, regex: rg27, onlyCount: rg27_onlyCount, skipIfEmpty: rg27_skipIfEmpty },
        { name: rg28_name, regex: rg28, onlyCount: rg28_onlyCount, skipIfEmpty: rg28_skipIfEmpty },
		{ name: rg29_name, regex: rg29, onlyCount: rg29_onlyCount, skipIfEmpty: rg29_skipIfEmpty },
        { name: rg30_name, regex: rg30, onlyCount: rg30_onlyCount, skipIfEmpty: rg30_skipIfEmpty },
        { name: rg31_name, regex: rg31, onlyCount: rg31_onlyCount, skipIfEmpty: rg31_skipIfEmpty },
        { name: rg32_name, regex: rg32, onlyCount: rg32_onlyCount, skipIfEmpty: rg32_skipIfEmpty },
        { name: rg33_name, regex: rg33, onlyCount: rg33_onlyCount, skipIfEmpty: rg33_skipIfEmpty },
        { name: rg34_name, regex: rg34, onlyCount: rg34_onlyCount, skipIfEmpty: rg34_skipIfEmpty },
		{ name: rg35_name, regex: rg35, onlyCount: rg35_onlyCount, skipIfEmpty: rg35_skipIfEmpty },
		{ name: rg36_name, regex: rg36, onlyCount: rg36_onlyCount, skipIfEmpty: rg36_skipIfEmpty },
        { name: rg37_name, regex: rg37, onlyCount: rg37_onlyCount, skipIfEmpty: rg37_skipIfEmpty },
        { name: rg38_name, regex: rg38, onlyCount: rg38_onlyCount, skipIfEmpty: rg38_skipIfEmpty },
		{ name: rg39_name, regex: rg39, onlyCount: rg39_onlyCount, skipIfEmpty: rg39_skipIfEmpty },
        { name: rg40_name, regex: rg40, onlyCount: rg40_onlyCount, skipIfEmpty: rg40_skipIfEmpty },
        { name: rg41_name, regex: rg41, onlyCount: rg41_onlyCount, skipIfEmpty: rg41_skipIfEmpty },
        { name: rg42_name, regex: rg42, onlyCount: rg42_onlyCount, skipIfEmpty: rg42_skipIfEmpty },
        { name: rg43_name, regex: rg43, onlyCount: rg43_onlyCount, skipIfEmpty: rg43_skipIfEmpty },
        { name: rg44_name, regex: rg44, onlyCount: rg44_onlyCount, skipIfEmpty: rg44_skipIfEmpty },
		{ name: rg45_name, regex: rg45, onlyCount: rg45_onlyCount, skipIfEmpty: rg45_skipIfEmpty },
		{ name: rg46_name, regex: rg46, onlyCount: rg46_onlyCount, skipIfEmpty: rg46_skipIfEmpty },
        { name: rg47_name, regex: rg47, onlyCount: rg47_onlyCount, skipIfEmpty: rg47_skipIfEmpty },
        { name: rg48_name, regex: rg48, onlyCount: rg48_onlyCount, skipIfEmpty: rg48_skipIfEmpty },
		{ name: rg49_name, regex: rg49, onlyCount: rg49_onlyCount, skipIfEmpty: rg49_skipIfEmpty },
		{ name: rg50_name, regex: rg50, onlyCount: rg50_onlyCount, skipIfEmpty: rg50_skipIfEmpty },
        { name: rg51_name, regex: rg51, onlyCount: rg51_onlyCount, skipIfEmpty: rg51_skipIfEmpty },
        { name: rg52_name, regex: rg52, onlyCount: rg52_onlyCount, skipIfEmpty: rg52_skipIfEmpty },
        { name: rg53_name, regex: rg53, onlyCount: rg53_onlyCount, skipIfEmpty: rg53_skipIfEmpty },
        { name: rg54_name, regex: rg54, onlyCount: rg54_onlyCount, skipIfEmpty: rg54_skipIfEmpty },
		{ name: rg55_name, regex: rg55, onlyCount: rg55_onlyCount, skipIfEmpty: rg55_skipIfEmpty },
		{ name: rg56_name, regex: rg56, onlyCount: rg56_onlyCount, skipIfEmpty: rg56_skipIfEmpty },
        { name: rg57_name, regex: rg57, onlyCount: rg57_onlyCount, skipIfEmpty: rg57_skipIfEmpty },
        { name: rg58_name, regex: rg58, onlyCount: rg58_onlyCount, skipIfEmpty: rg58_skipIfEmpty },
		{ name: rg59_name, regex: rg59, onlyCount: rg59_onlyCount, skipIfEmpty: rg59_skipIfEmpty },
		{ name: rg60_name, regex: rg60, onlyCount: rg60_onlyCount, skipIfEmpty: rg60_skipIfEmpty },
        { name: rg100_name, regex: rg100, onlyCount: rg100_onlyCount, skipIfEmpty: rg100_skipIfEmpty },
        { name: rg101_name, regex: rg101, onlyCount: rg101_onlyCount, skipIfEmpty: rg101_skipIfEmpty }
    ];
    
    // конец блока регекспов.
    // =====================
    
    // Разбиение на абзацы
    var paragraphs = [];
    var paragraphStart = 0;
    for (var i = 0; i < fromText.length; i++) {
        if (fromText.charAt(i) === '\n') {
            paragraphs.push({
                text: fromText.substring(paragraphStart, i),
                start: paragraphStart,
                end: i
            });
            paragraphStart = i + 1;
        }
    }
    if (paragraphStart < fromText.length) {
        paragraphs.push({
            text: fromText.substring(paragraphStart),
            start: paragraphStart,
            end: fromText.length
        });
    }

    function findMatches(regex, paragraphs) {
        var matches = [];
        for (var p = 0; p < paragraphs.length; p++) {
            var paragraph = paragraphs[p];
            var hasContent = false;
            for (var j = 0; j < paragraph.text.length; j++) {
                var ch = paragraph.text.charAt(j);
                if (ch !== ' ' && ch !== '\t' && ch !== '\n' && ch !== '\r') {
                    hasContent = true;
                    break;
                }
            }
            if (!hasContent) continue;

            regex.lastIndex = 0;
            var match;
            while ((match = regex.exec(paragraph.text)) !== null) {
                var absoluteIndex = paragraph.start + match.index;
                var contextStartInPara = Math.max(0, match.index - 30);
                var contextEndInPara = Math.min(paragraph.text.length, match.index + match[0].length + 30);
                var context = paragraph.text.substring(contextStartInPara, contextEndInPara);
                matches.push({
                    text: match[0],
                    index: absoluteIndex,
                    length: match[0].length,
                    context: context,
                    contextOffset: match.index - contextStartInPara
                });
            }
        }
        return matches;
    }

    function escapeHtml(str) {
        if (!str) return "";
        return str.replace(/&/g, "&amp;")
                  .replace(/</g, "&lt;")
                  .replace(/>/g, "&gt;");
    }

    var groupsInfo = [];
    for (var g = 0; g < regexGroups.length; g++) {
        var group = regexGroups[g];
        var matches = findMatches(group.regex, paragraphs);
        groupsInfo.push({
            name: group.name,
            count: matches.length,
            onlyCount: group.onlyCount,
            skipIfEmpty: group.skipIfEmpty,
            matches: matches
        });
    }

    // Вычисляем размеры и положение окна результатов
    var screenWidth = window.screen.availWidth || window.screen.width;
    var screenHeight = window.screen.availHeight || window.screen.height;
    var winWidth = Math.floor(screenWidth * windowWidthPercent);
    var winHeight = Math.floor(screenHeight * windowHeightPercent);
    var winLeft = 0;
    var winTop = 0;
    
    if (windowPosition === "right") {
        winLeft = screenWidth - winWidth;
    } else {
        winLeft = 0;
    }
    // Можно также центрировать по вертикали, но оставим winTop = 0
    
    var winParams = "width=" + winWidth + 
                    ",height=" + winHeight + 
                    ",left=" + winLeft + 
                    ",top=" + winTop + 
                    ",resizable=yes,scrollbars=yes";
    
    var win = window.open("", "_blank", winParams);
    if (!win) {
        alert("Не удалось открыть окно. Разрешите всплывающие окна.");
        return;
    }

    win.document.write('<html><head>' +
        '<meta http-equiv="Content-Type" content="text/html; charset=utf-8">' +
        '<title>' + scriptName + ' v' + scriptVersion + '</title>' +
        '<style>' +
        'body { font-family: Arial, sans-serif; margin: 20px; }' +
        'table { border-collapse: collapse; width: 100%; margin-bottom: 20px; }' +
        'th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }' +
        'th { background-color: #f2f2f2; }' +
        '.match { background-color: #ffcccc; padding: 2px; }' +
        '.context { max-width: 500px; word-wrap: break-word; }' +
        '.pos-length { text-align: center; width: 120px; cursor: pointer; background-color: #eef; }' +
        '.pos-length:hover { background-color: #ccf; }' +
        'h3 { color: #333; margin-top: 25px; }' +
        'h4 { color: #555; margin: 15px 0 5px 0; padding: 5px; background-color: ' + headerBgColor + '; }' +
        '.context-container { background-color: #f9f9f9; margin: 2px; padding: 3px; white-space: pre-wrap; }' +
        '.summary-list { margin: 10px 0 20px 0; padding: 10px; background-color: #f0f0f0; border-left: 4px solid #ccc; }' +
        '.summary-list ul { margin: 0; padding-left: 20px; }' +
        '.summary-list li { margin: 5px 0; }' +
        '</style>' +
        '</head><body>');

    // Вывод справки
    if (helpText && helpText.length > 0) {
        win.document.write('<div class="summary-list">' + helpText + '</div>');
    }

    // Сводка по группам
    var nonEmptyGroups = [];
    for (var i = 0; i < groupsInfo.length; i++) {
        var info = groupsInfo[i];
        if (!(info.count === 0 && info.skipIfEmpty)) {
            nonEmptyGroups.push(info);
        }
    }
    if (nonEmptyGroups.length > 1) {
        win.document.write('<div class="summary-list">');
        win.document.write('<strong>Сводка по группам (кликните для перехода;<br> для возврата в Сводку — кликните клавишу HOME):</strong><ul>');
        for (var i = 0; i < nonEmptyGroups.length; i++) {
            var origIndex = -1;
            for (var j = 0; j < groupsInfo.length; j++) {
                if (groupsInfo[j] === nonEmptyGroups[i]) {
                    origIndex = j;
                    break;
                }
            }
            win.document.write('<li><a href="#group_' + origIndex + '">' + nonEmptyGroups[i].name + ' (' + nonEmptyGroups[i].count + ')</a></li>');
        }
        win.document.write('</ul></div>');
    }
    
    
    if (singleHeader) {
            win.document.write('<table><tr><th>Контекст (совпадение выделено красным)</th><th class="pos-length">Позиция:Длина</th></tr></table>');
        }
    
    

    var totalMatchesAll = 0;
    for (var i = 0; i < groupsInfo.length; i++) {
        var info = groupsInfo[i];
        totalMatchesAll += info.count;
        if (info.count === 0 && info.skipIfEmpty) continue;

        win.document.write('<a name="group_' + i + '"></a>');
        win.document.write('<h4>' + info.name + ' (всего ' + info.count + ')</h4>');
        if (info.onlyCount) continue;
        if (info.count === 0) {
            win.document.write('<p>Совпадений не найдено.</p>');
            continue;
        }

        // Открываем таблицу (правильный тег <tr>)
            win.document.write('<table>');


        // Заголовок таблицы (только если singleHeader == false)
        if (!singleHeader) {
            win.document.write('<tr><th>Контекст (выделено красным)</th><th class="pos-length">Позиция:Длина</th></tr>');
        }

        for (var m = 0; m < info.matches.length; m++) {
            var match = info.matches[m];
            var beforeRaw = match.context.substring(0, match.contextOffset);
            var matchRaw = match.text;
            var afterRaw = match.context.substring(match.contextOffset + match.length);

            var beforeEsc = escapeHtml(beforeRaw);
            var matchEsc = escapeHtml(matchRaw);
            var afterEsc = escapeHtml(afterRaw);

            var contextHtml = '<div class="context-container">' +
                beforeEsc + '<span class="match">' + matchEsc + '</span>' + afterEsc + '</div>';
            var pos = match.index;
            var len = match.length;
            var posLength = pos + ':' + len;

            // Исправленный onclick с правильной высотой окна
            var onclickCode = 
                "var txt = this.innerText || this.textContent; var parts = txt.split(':'); if(parts.length==2) { " +
                "var p = parseInt(parts[0], 10); var l = parseInt(parts[1], 10); " +
                // "alert('Позиция: ' + p + '\\nДлина: ' + l); " +
                "if(window.opener) { window.opener.focus(); " +
                "try { " +
                "var fbwBody = window.opener.document.getElementById('fbw_body'); " +
                "if(fbwBody) { " +
                "var tr = window.opener.document.body.createTextRange(); " +
                "tr.moveToElementText(fbwBody); " +
                "tr.collapse(true); " +
                "tr.select(); " +
                "window.opener.scrollTo(0,0); " +
                "var myRange = window.opener.document.selection.createRange(); " +
                "myRange.moveStart('character', p); " +
                "myRange.moveEnd('character', l); " +
                "myRange.select(); " +
                "myRange.scrollIntoView(); " +
                "var rect = myRange.getBoundingClientRect ? myRange.getBoundingClientRect() : null; " +
                "if(rect && (window.opener.document.documentElement.clientHeight - rect.bottom) < 20) { " +
                "window.opener.scrollBy(0, 50); " +
                "} " +
                "} else { alert('Не найден элемент fbw_body'); } " +
                "} catch(e) { alert('Ошибка выделения: ' + e.message); } " +
                "} else { alert('Нет доступа к окну редактора'); } " +
                "} else { alert('Ошибка формата: ' + txt); }";

            win.document.write('<tr>' +
                '<td class="context">' + contextHtml + '</td>' +
                '<td class="pos-length" onclick="' + onclickCode.replace(/"/g, '&quot;') + '">' + posLength + '</td>' +
                '</tr>');
        }
        win.document.write('</table>');
    }

    if (totalMatchesAll === 0) {
        win.document.write('<p>Совпадений не найдено ни по одному из регекспов.</p>');
    }

    win.document.write('</body></html>');
    win.document.close();

    function extractTextWithNewlines(html) {
        var tempDiv = document.createElement("div");
        tempDiv.innerHTML = html;
        var result = "";
        function traverse(node) {
            if (node.nodeType === 3) {
                result += node.nodeValue;
            } else if (node.nodeType === 1) {
                var tagName = node.tagName.toLowerCase();
                var addNewline = false;
                if (tagName === 'p') addNewline = true;
                else if (tagName === 'div') {
                    var className = node.className || "";
                    if (className.indexOf('image') !== -1) addNewline = true;
                }
                for (var i = 0; i < node.childNodes.length; i++) traverse(node.childNodes[i]);
                if (addNewline) result += "\n";
            }
        }
        traverse(tempDiv);
        return result;
    }
}
