// Скрипт "Первую букву каждого слова в верхний регистр (с исключениями)" для редактора FBE
// version 2.1
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Основано на скрипте: "Капитализация выделения" v1.3 уважаемого тов. Sclex.

// Скрипт предназначен для преобразования регистра слов в выделенном фрагменте fb2 документа.
// Если выделено одно слово ---> обрабатывается только это слово.
// Если выделено несколько слов ---> обрабатываются выделенные слова.
// Первая буква каждого слова становится ЗАГЛАВНОЙ, остальные - строчными.
// Редактируемые списки слов-исключений:
// - Короткие слова (из списка) могут оставаться строчными (кроме начала текста).
// - Римские цифры по умолчанию остаются неизменными.
// - Аббревиатуры также по умолчанию остаются неизменными.

// Режим работы: обычный и тихий.
// Поддержка отмены действий (Ctrl+Z).

// version 2.1, 26.02.2026
//======================================

function Run() {
// Название и версия для сообщений
var scriptName = "Первую букву каждого слова в верхний регистр (с исключениями)";
var version = "2.1";

// ==================================================
// НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
// ==================================================

// Настройка: 1 - показывать статистику, 0 - не показывать (тихий режим)
var showStatistics = 0; // Измените на 1 для показа статистики

// Обработка римских цифр
var keepRomanNumerals = 1; // 0 - преобразовывать, 1 - оставить как есть

// Обработка аббревиатур из списка
var keepAbbreviations = 1; // 0 - преобразовывать, 1 - оставить как есть

// Обработка коротких слов из списка (делать с маленькой буквы, если не в начале)
var keepShortWords = 1; // 0 - делать с заглавной, 1 - делать с маленькой (кроме начала)

// Имя тега для маркеров
var MyTagName = "B";

// ========== СПИСКИ ДЛЯ ПРОВЕРКИ ==========

// Список русских коротких слов-исключений
var russianShortWords = "а|б|без|бы|в|вне|во|да|для|до|ж|же|за|и|из|из-за|из-под|или|им.|к|ко|ли|на|над|не|ни|но|о|об|обо|от|по|под|при|про|с|со|у";

// Список английских коротких слов-исключений
var englishShortWords = "a|an|and|at|but|by|for|from|in|nor|of|on|or|the|to|with|without";

// Список немецких коротких слов-исключений
var germanShortWords = "aber|an|auf|aus|bei|beim|bis|das|dass|dem|den|denn|der|die|durch|ein|eine|einem|einen|einer|eines|für|gegen|hinter|im|in|mit|nach|neben|ob|oder|ohne|seit|sondern|über|um|und|unter|von|vor|weil|wenn|zu|zum|zur|zwischen";

// Объединяем все короткие слова в массив
var shortWordsArray = (russianShortWords + "|" + englishShortWords + "|" + germanShortWords).split("|");

// Список римских цифр
var romanNumeralsArray = [
    "I","II","III","IV","V","VI","VII","VIII","IX","X",
    "XI","XII","XIII","XIV","XV","XVI","XVII","XVIII","XIX","XX",
    "XXI","XXII","XXIII","XXIV","XXV","XXVI","XXVII","XXVIII","XXIX","XXX",
    "XXXI","XXXII","XXXIII","XXXIV","XXXV","XXXVI","XXXVII","XXXVIII","XXXIX","XL",
    "XLI","XLII","XLIII","XLIV","XLV","XLVI","XLVII","XLVIII","XLIX","L",
    "LI","LII","LIII","LIV","LV","LVI","LVII","LVIII","LIX","LX",
    "LXI","LXII","LXIII","LXIV","LXV","LXVI","LXVII","LXVIII","LXIX","LXX",
    "LXXI","LXXII","LXXIII","LXXIV","LXXV","LXXVI","LXXVII","LXXVIII","LXXIX","LXXX",
    "LXXXI","LXXXII","LXXXIII","LXXXIV","LXXXV","LXXXVI","LXXXVII","LXXXVIII","LXXXIX","XC",
    "XCI","XCII","XCIII","XCIV","XCV","XCVI","XCVII","XCVIII","XCIX","C"
];

// Список русских аббревиатур
var russianAbbreviations = [
    "АБС","АЗЛК","АЗС","АКБ","АКМ","АНБ","АССР","АСТ","АЭС","БАД","БАМ","ББК","БИК","БМП","БП","БПЛА","БССР","БТР",
    "ВАЗ","ВАСХНИЛ","ВВП","ВВС","ВГИК","ВДВ","ВДНХ","ВИЧ","ВКС","ВЛКСМ","ВМС","ВМФ","ВНД","ВОЗ","ВПК","ВС","ВУЗ","ВЦИК","ВЦСПС","ВШЭ","ВЭД",
    "ГАЗ","ГАИ","ГДР","ГИБДД","ГК","ГЛОНАСС","ГОСТ","ГТО","ГУЛАГ","ГЭС",
    "ДВС","ДНК","ДОСААФ","ДЮСШ","ЕГЭ","ЕС","ЕЭС","ЖБИ","ЖК","ЖКХ","ЖКТ","ЖЭК",
    "ЗАГС","ЗАО","ЗИЛ","ЗКС","ЗРК",
    "ИБП","ИВЛ","ИГИЛ","ИЖС","ИИ","ИНН","ИП","ИФНС",
    "КАМАЗ","КАСКО","КГБ","КНДР","КНР","КоАП","КПД","КПП","КПРФ","КПСС","КТ",
    "ЛАТР","ЛГБТ","ЛДПР","ЛПУ","ЛЭП",
    "МАГАТЭ","МАЗ","МБР","МВД","МВФ","МГИМО","МГУ","МДФ","МИД","МИСИ","МИФИ","МКАД","МКС","ММВБ","МО","МРТ","МТС","МФТИ","МФЦ","МЧС",
    "НАМИ","НАТО","НВП","НДС","НДФЛ","НИИ","НИОКР","НИИЦ","НКВД","НКО","НЛП","НПЗ","НПО","НШ","НЭП",
    "ОБСЕ","ОБХСС","ОБЭП","ОКАТО","ОКВЭД","ОКД","ОКП","ОКПО","ОМОН","ООН","ООО","ОПГ","ОСАГО","ОТК",
    "ПВО","ПВХ","ПДД","ПК","ПНД","ППС","ПТС","ПТУ","ПУЭ","ПФР",
    "РАМН","РАН","РБК","РВСН","РГБ","РЖД","РККА","РКН","РНК","РПГ","РПЦ","РСДРП","РСФСР","РУВД","РУДН","РФ","РЭБ","РЭР",
    "СВ","СВР","СВЧ","СЗ","СИЗ","СИЗО","СКА","СМЕРШ","СМИ","СНГ","СНиП","СНТ","СПбГУ","СС","ССО","ССР","СССР","СТС","США","СЭВ","СЭС",
    "ТАСС","ТВД","ТН","ТПУ","ТТН","ТТХ","ТЭЦ",
    "УАЗ","УВД","УДК","УЗИ","УЗО","УПК","УССР",
    "ФБР","ФМС","ФНС","ФРГ","ФСБ","ФСИН","ФСО","ФССП",
    "ХВ","ХЗ","ХЛ","ХО","ХР",
    "ЦЕРН","ЦК","ЦНС","ЦРУ","ЦСКА",
    "ЧК","ЧМ","ЧП",
    "ЭВМ","ЭКГ","ЭЭГ",
    "ЮАР","ЮВ","ЮЗ","ЮНЕСКО",
    "ЯБЧ","ЯВ","ЯМЗ","ЯО"
];

// Список латинских аббревиатур
var latinAbbreviations = [
    "BBC","BMW","CEO","CFO","CERN","CNN","COO","CTO","DOI","ESA","FIFA","GE","GM","GMC","HBO","HDMI","HP","IBM","ISBN","ISSN","KPI",
    "L","LAN","LCD","LED","LG","M","MTV","NASA","NATO","NBC","NEC","OLED","PC","R&D","S","SAP","TV","UAV","UN","UNESCO","USB","UEFA","VPN","VW","WAN","WHO","WI-FI","WTO","XL","XS","XXL","XXXL"
];

// Объединяем оба списка аббревиатур
var abbreviationsArray = [];
for (var i = 0; i < russianAbbreviations.length; i++) {
    abbreviationsArray.push(russianAbbreviations[i]);
}
for (var i = 0; i < latinAbbreviations.length; i++) {
    abbreviationsArray.push(latinAbbreviations[i]);
}

// Неразрывный пробел из настроек FBE
var nbspChar = String.fromCharCode(160);
try {
    nbspChar = window.external.GetNBSP();
} catch(e) {
    nbspChar = String.fromCharCode(160);
}

// ==================================================
// ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
// ==================================================

// Проверка, является ли символ буквой
function isLetter(ch) {
    if (!ch || ch.length == 0) return false;
    var lettersRE = new RegExp("[а-яёa-zÀÁÂÃÄÅÆÇÈÉÊËÌÍÏÐÑÒÓÔÕÖØÙÚÛÜÝÞßàáâãäåæçèéêëìíîïðñòóôõöøùúûüýþÿĀāăĄąĆćĈĉĊċČčĎďĐđĒēĔĕĖėĘęĚěĜĝĞğĠġĢģĤĥĦħĨĩĪīĬĭĮįİıĲĳĴĵĶķĸĹĺĻļĽľĿŀŁłŃńŅņŇňŉŊŋŌōŎŏŐőŒœŔŕŖŗŘřŚśŜŝŞşŠšŢţŤťŦŧŨũŪūŬŭŮůŰűŲųŴŵŶŷŸŹźŻżŽžſƏƒƠơƯưƷǤǥǦǧǨǩǪǫǮǯǺǻǼǽǾǿȘșȚțȨȩəʒ" +
        "ΆΈΉΊΌΎΐΑΒΓΔΕΖΗΘΙΚΛΜΝΞΟΠΡΣΤΥΦΧΨΩΪΫάέήίΰαβγδεζηθικλμνξοπρςστυφχψωϊϋόύώЀЁЂЃЄЅІЇЈЉЊЋЌЍЎЏ" +
        "ѐђѓєѕіїјљњћќѝўџҐґҒғҖҗҚқҜҝҢңҮүҰұҲҳҸҹҺһӘәӨө" +
        "ḀḁḂḃḄḅḆḇḈḉḊḋḌḍḎḏḐḑḒḓḔḕḖḗḘḙḚḛḜḝḞḟḠḡḢḣḤḥḦḧḨḩḪḫḬḭḮḯḰḱḲḳḴḵḶḷḸḹḺḻḼḽḾḿṀṁṂṃṄṅṆṇṈṉṊṋṌṍṎṏṐṑṒṓṔṕṖṗṘṙṚṛṜṝṞṟṠṡṢṣṤṥṦṧṨṩṪṫṬṭṮṯṰṱṲṳṴṵṶṷṸṹṺṻṼṽṾṿẀẁẂẃẄẅẆẇẈẉẊẋẌẍẎẏẐẑẒẓẔẕẖẗẘẙẚẛẠạẢảẤấẦầẨẩẪẫẬậẮắẰằẲẳẴẵẶặẸẹẺẻẼẽẾếỀềỂểỄễỆệỈỉỊịỌọỎỏỐốỒồỔổỖỗỘộỚớỜờỞởỠỡỢợỤụỦủỨứỪừỬửỮữỰựỲỳỴỵỶỷỸỹἀἁἂἃἄἅἆἇἈἉἊἋἌἍἎἏἐἑἒἓἔἕἘἙἚἛἜἝἠἡἢἣἤἥἦἧἨἩἪἫἬἭἮἯἰἱἲἳἴἵἶἷἸἹἺἻἼἽἾἿὀὁὂὃὄὅὈὉὊὋὌὍὐὑὒὓὔὕὖὗὙὛὝὟὠὡὢὣὤὥὦὧἨὩὪὫὬὭὮὯὰάὲέὴήὶίὸόὺύὼώᾀᾁᾂᾃᾄᾅᾆᾇᾈᾉᾊᾋᾌᾍᾎᾏᾐᾑᾒᾓᾔᾕᾖᾗᾘᾙᾚᾛᾜᾝᾞᾟᾠᾡᾢᾣᾤᾥᾦᾧᾨᾩᾪᾫᾬᾭᾮᾯᾰᾱᾲᾳᾴᾶᾷᾸᾹᾺΆᾼ" +
        "ῂῃῄῆῇῈΈῊΉῌ" +
        "ῐῑῒΐῖῗῘῙῚΊῠῡῢΰῤῥῦῧῨῩῪΎῬῲῳῴῶῷῸΌῺΏῼµ]", "i");
    return lettersRE.test(ch);
}

// Проверка, является ли символ пробельным
function isWhitespace(ch) {
    return (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == nbspChar);
}

// Проверка, является ли символ дефисом/тире
function isDash(ch) {
    return (ch == '-' || ch == '—' || ch == '–');
}

// Проверка, является ли символ открывающей кавычкой
function isOpeningQuote(ch) {
    return (ch == '"' || ch == '«' || ch == '„' || ch == '“');
}

// Очистка слова от знаков препинания для проверки по спискам
function cleanWordForCheck(word) {
    var start = 0;
    var end = word.length - 1;
    
    while (start <= end && !isLetter(word.charAt(start))) {
        start++;
    }
    
    while (end >= start && !isLetter(word.charAt(end))) {
        end--;
    }
    
    if (start > end) return "";
    return word.substring(start, end + 1);
}

// Проверка, является ли слово римской цифрой
function isRomanNumeral(word) {
    if (!word) return false;
    var cleanWord = cleanWordForCheck(word);
    if (!cleanWord) return false;
    
    var upperWord = cleanWord.toUpperCase();
    for (var i = 0; i < romanNumeralsArray.length; i++) {
        if (romanNumeralsArray[i] === upperWord) {
            return true;
        }
    }
    return false;
}

// Проверка, является ли слово аббревиатурой
function isAbbreviation(word) {
    if (!word) return false;
    var cleanWord = cleanWordForCheck(word);
    if (!cleanWord) return false;
    
    var upperWord = cleanWord.toUpperCase();
    for (var i = 0; i < abbreviationsArray.length; i++) {
        if (abbreviationsArray[i] === upperWord) {
            return true;
        }
    }
    return false;
}

// Проверка, является ли слово коротким словом-исключением
function isShortWord(word) {
    if (!word) return false;
    var cleanWord = cleanWordForCheck(word);
    if (!cleanWord) return false;
    
    var lowerWord = cleanWord.toLowerCase();
    for (var i = 0; i < shortWordsArray.length; i++) {
        if (shortWordsArray[i] === lowerWord) {
            return true;
        }
    }
    return false;
}

// Функция для определения, является ли слово началом предложения
function isSentenceStart(text, offset) {
    if (offset <= 0) return true;
    
    var pos = offset - 1;
    
    // Пропускаем пробелы
    while (pos >= 0 && isWhitespace(text.charAt(pos))) {
        pos--;
    }
    
    if (pos < 0) return true;
    
    var ch = text.charAt(pos);
    
    // Если перед словом тире - это начало диалога
    if (isDash(ch)) return true;
    
    // Если перед словом открывающая кавычка
    if (isOpeningQuote(ch)) return true;
    
    // Если перед словом знак конца предложения
    if (ch == '.' || ch == '!' || ch == '?' || ch == '…') return true;
    
    return false;
}

// Глобальная переменная для отслеживания последнего символа
var lastSymbolLetter = false;

// Наша функция обработки слова
function replaceFunc(full_match, offset_of_match, string_we_search_in) {
    // Логика из оригинального скрипта Sclex
    if (lastSymbolLetter && offset_of_match == 0) {
        lastSymbolLetter = false;
        return full_match.toLowerCase();
    } else {
        lastSymbolLetter = false;
        
        // Проверяем исключения
        var wordIsRoman = isRomanNumeral(full_match);
        var wordIsAbbrev = isAbbreviation(full_match);
        var wordIsShort = isShortWord(full_match);
        
        // Защищенные аббревиатуры и римские цифры
        if ((wordIsRoman && keepRomanNumerals == 1) || (wordIsAbbrev && keepAbbreviations == 1)) {
            return full_match;
        }
        
        // Короткие слова
        if (wordIsShort && keepShortWords == 1) {
            // Проверяем, является ли это слово началом предложения/диалога
            if (isSentenceStart(string_we_search_in, offset_of_match)) {
                return full_match.substr(0, 1).toUpperCase() + full_match.substr(1).toLowerCase();
            } else {
                return full_match.toLowerCase();
            }
        }
        
        // Обычное слово: первая буква заглавная, остальные строчные
        return full_match.substr(0, 1).toUpperCase() + full_match.substr(1).toLowerCase();
    }
}

// ==================================================
// ОСНОВНАЯ ЛОГИКА (ПОЛНОСТЬЮ ИЗ СКРИПТА SCLEX)
// ==================================================

var lettersRE = new RegExp("[а-яёa-zÀÁÂÃÄÅÆÇÈÉÊËÌÍÏÐÑÒÓÔÕÖØÙÚÛÜÝÞßàáâãäåæçèéêëìíîïðñòóôõöøùúûüýþÿĀāăĄąĆćĈĉĊċČčĎďĐđĒēĔĕĖėĘęĚěĜĝĞğĠġĢģĤĥĦħĨĩĪīĬĭĮįİıĲĳĴĵĶķĸĹĺĻļĽľĿŀŁłŃńŅņŇňŉŊŋŌōŎŏŐőŒœŔŕŖŗŘřŚśŜŝŞşŠšŢţŤťŦŧŨũŪūŬŭŮůŰűŲųŴŵŶŷŸŹźŻżŽžſƏƒƠơƯưƷǤǥǦǧǨǩǪǫǮǯǺǻǼǽǾǿȘșȚțȨȩəʒ" +
    "ΆΈΉΊΌΎΐΑΒΓΔΕΖΗΘΙΚΛΜΝΞΟΠΡΣΤΥΦΧΨΩΪΫάέήίΰαβγδεζηθικλμνξοπρςστυφχψωϊϋόύώЀЁЂЃЄЅІЇЈЉЊЋЌЍЎЏ" +
    "ѐђѓєѕіїјљњћќѝўџҐґҒғҖҗҚқҜҝҢңҮүҰұҲҳҸҹҺһӘәӨө" +
    "ḀḁḂḃḄḅḆḇḈḉḊḋḌḍḎḏḐḑḒḓḔḕḖḗḘḙḚḛḜḝḞḟḠḡḢḣḤḥḦḧḨḩḪḫḬḭḮḯḰḱḲḳḴḵḶḷḸḹḺḻḼḽḾḿṀṁṂṃṄṅṆṇṈṉṊṋṌṍṎṏṐṑṒṓṔṕṖṗṘṙṚṛṜṝṞṟṠṡṢṣṤṥṦṧṨṩṪṫṬṭṮṯṰṱṲṳṴṵṶṷṸṹṺṻṼṽṾṿẀẁẂẃẄẅẆẇẈẉẊẋẌẍẎẏẐẑẒẓẔẕẖẗẘẙẚẛẠạẢảẤấẦầẨẩẪẫẬậẮắẰằẲẳẴẵẶặẸẹẺẻẼẽẾếỀềỂểỄễỆệỈỉỊịỌọỎỏỐốỒồỔổỖỗỘộỚớỜờỞởỠỡỢợỤụỦủỨứỪừỬửỮữỰựỲỳỴỵỶỷỸỹἀἁἂἃἄἅἆἇἈἉἊἋἌἍἎἏἐἑἒἓἔἕἘἙἚἛἜἝἠἡἢἣἤἥἦἧἨἩἪἫἬἭἮἯἰἱἲἳἴἵἶἷἸἹἺἻἼἽἾἿὀὁὂὃὄὅὈὉὊὋὌὍὐὑὒὓὔὕὖὗὙὛὝὟὠὡὢὣὤὥὦὧἨὩὪὫὬὭὮὯὰάὲέὴήὶίὸόὺύὼώᾀᾁᾂᾃᾄᾅᾆᾇᾈᾉᾊᾋᾌᾍᾎᾏᾐᾑᾒᾓᾔᾕᾖᾗᾘᾙᾚᾛᾜᾝᾞᾟᾠᾡᾢᾣᾤᾥᾦᾧᾨᾩᾪᾫᾬᾭᾮᾯᾰᾱᾲᾳᾴᾶᾷᾸᾹᾺΆᾼ" +
    "ῂῃῄῆῇῈΈῊΉῌ" +
    "ῐῑῒΐῖῗῘῙῚΊῠῡῢΰῤῥῦῧῨῩῪΎῬῲῳῴῶῷῸΌῺΏῼµ]+", "ig");

var Ts = new Date().getTime();
var stats = { changed: false };

var tr;
var errMsg = "Нет выделения.\n\nПеред запуском скрипта нужно выделить текст.";

tr = document.selection.createRange();
if (!tr) {
    MsgBox(scriptName + "\nver. " + version + "\n\n" + errMsg);
    return;
}

if (tr.compareEndPoints("StartToEnd", tr) == 0) {
    MsgBox(scriptName + "\nver. " + version + "\n\nНет выделенного текста!\n\nСкрипт работает только с выделенным фрагментом.");
    return;
}

window.external.BeginUndoUnit(document, scriptName);

if (tr.parentElement().nodeName == "TEXTAREA") {
    var tr1 = document.body.createTextRange();
    tr1.moveToElementText(tr.parentElement());
    tr1.setEndPoint("EndToStart", tr);
    var tr2 = document.body.createTextRange();
    tr2.moveToElementText(tr.parentElement());
    tr2.setEndPoint("StartToEnd", tr);
    var s = tr.text;
    
    s = s.replace(lettersRE, replaceFunc);
    
    tr.parentElement().value = tr1.text + s + tr2.text;
    stats.changed = true;
} else if (tr.parentElement().nodeName != "INPUT") {
    var body = document.getElementById("fbw_body");
    var coll = tr.getClientRects();
    var ttr1 = body.document.selection.createRange();
    var el = body.document.elementFromPoint(coll[0].left, coll[0].top);
    
    var cursorPos = null;
    if (tr.compareEndPoints("StartToEnd", tr) == 0) {
        var el2 = document.getElementById("CursorPosition");
        if (el2) el2.removeAttribute("id");
        ttr1.pasteHTML("<" + MyTagName + " id=CursorPosition></" + MyTagName + ">");
        cursorPos = document.getElementById("CursorPosition");
        ttr1.expand("word");
    }
    
    tr = ttr1.duplicate();
    tr.collapse();
    tr.pasteHTML("<" + MyTagName + " id=BlockStart></" + MyTagName + ">");
    tr = ttr1.duplicate();
    tr.collapse(false);
    tr.pasteHTML("<" + MyTagName + " id=BlockEnd></" + MyTagName + ">");
    
    while (el && el.nodeName != "DIV" && el.nodeName != "P") {
        el = el.parentNode;
    }
    
    var InsideP = false;
    var InsideSelection = false;
    var ProcessingEnded = false;
    var ptr = el;
    lastSymbolLetter = false;
    
    while (!ProcessingEnded) {
        if (ptr.nodeType == 1 && ptr.nodeName == "P") {
            InsideP = true;
            lastSymbolLetter = false;
        }
        
        if (ptr.nodeType == 1 && ptr.nodeName == MyTagName &&
            ptr.getAttribute("id") == "BlockStart") {
            InsideSelection = true;
            var BlockStartNode = ptr;
        }
        
        if (ptr.nodeType == 1 && ptr.nodeName == MyTagName &&
            ptr.getAttribute("id") == "BlockEnd") {
            InsideSelection = false;
            ProcessingEnded = true;
            var BlockEndNode = ptr;
        }
        
        if (ptr.nodeType == 3 && InsideP && InsideSelection) {
            var s = ptr.nodeValue;
            s = s.replace(lettersRE, replaceFunc);
            
            if (s.length > 0 && s.substr(s.length - 1).search(lettersRE) >= 0) {
                lastSymbolLetter = true;
            } else {
                lastSymbolLetter = false;
            }
            
            ptr.nodeValue = s;
            stats.changed = true;
        }
        
        if (ptr.firstChild != null) {
            ptr = ptr.firstChild;
        } else {
            while (ptr.nextSibling == null) {
                ptr = ptr.parentNode;
                if (ptr && ptr.nodeType == 1 && ptr.nodeName == "P") {
                    InsideP = false;
                }
            }
            ptr = ptr.nextSibling;
        }
    }
    
    var tr1 = document.body.createTextRange();
    if (!cursorPos) {
        tr1.moveToElementText(BlockStartNode);
        var tr2 = document.body.createTextRange();
        tr2.moveToElementText(BlockEndNode);
        tr1.setEndPoint("StartToStart", tr2);
        tr1.select();
    } else {
        tr1.moveToElementText(cursorPos);
        tr1.select();
    }
    
    if (BlockStartNode && BlockStartNode.parentNode) {
        BlockStartNode.parentNode.removeChild(BlockStartNode);
    }
    if (BlockEndNode && BlockEndNode.parentNode) {
        BlockEndNode.parentNode.removeChild(BlockEndNode);
    }
}

window.external.EndUndoUnit(document);

var Tf = new Date().getTime();
var timeSec = Math.round((Tf - Ts) / 10) / 100;

if (showStatistics == 1) {
    var msg = scriptName + "\nver. " + version + "\n\n";
    
    if (stats.changed) {
        msg += "✓ Текст успешно обработан!\n\n";
    } else {
        msg += "Изменений не требуется.\n\n";
    }
    
    msg += "Время выполнения: " + timeSec + " сек\n";
    msg += "==============================";
    
    MsgBox(msg);
}

}
