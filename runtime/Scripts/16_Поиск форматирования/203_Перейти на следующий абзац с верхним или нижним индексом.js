// Скрипт "Перейти на следующий абзац с верхним или нижним индексом" для редактора FBE
// version 1.0
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для поиска и выделения абзацев, содержащих форматирование
// верхним или нижним индексом вниз по тексту fb2 документа.
// В настройках можно задать либо установку курсора, либо выделение всего найденного абзаца.

// На основе скриптов
// Перейти на следующий абзац с верхним индексом, Перейти на следующий абзац с нижним индексом
// уважаемого тов. Sclex.

// version 1.0, 05.03.2026
//======================================

function Run() {
// Название и версия для сообщений
var scriptName = "Перейти на следующий абзац с верхним или нижним индексом";
var version = "1.0";
var versionStr = scriptName + " v" + version + ".";

// ==================================================
// НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
// ==================================================

// Действие при обнаружении индекса:
// 0 - поставить курсор на найденный индекс
// 1 - выделить весь абзац с найденным индексом (по умолчанию)
var actionOnFound = 1; // Измените на 0, чтобы курсор ставился на индекс

// Направление поиска (всегда вперед)
var scriptDirection = "forward";

// Функция проверки, является ли тег искомым (верхний ИЛИ нижний индекс)
var isItTagWeLookingFor = function(t) {
if (t.nodeName == "SUP" || t.nodeName == "SUB") return true;
else return false;
};

// Режим поиска по абзацам (всегда true для данного скрипта)
var paragraphIndent = true;

// ==================================================
// НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
// ==================================================

var randomNum, beginMarkerId, endMarkerId, beginMarkerEl, endMarkerEl, range1, range2, el, el2, el3, placeWhereToStop, fbw_body, tagWeLookingFor_count;

// Начинаем блок отмены действий
window.external.BeginUndoUnit(document, versionStr);

// Функция прокрутки, чтобы найденный элемент был виден
function scrollIfItNeeds() {
var selection = document.selection;
if (selection) {
var range = selection.createRange();
var rect = range.getBoundingClientRect();

// Проверяем, находится ли выделение менее чем в 20 пикселях от нижнего края окна
if (document.documentElement.clientHeight - rect.bottom < 20) {
// Прокручиваем документ на 50 пикселей вниз
window.scrollBy(0, 50);
}
}
}

// Функция получения следующего узла в заданном направлении с учетом подсчета вложенности
function getNextNodeInScriptDirection(a) {
if (scriptDirection == "forward") {
if (a.firstChild) {
if (isItTagWeLookingFor(a)) tagWeLookingFor_count++;
a = a.firstChild;
} else {
while (a != fbw_body && a != placeWhereToStop && a.nextSibling == null) {
if (isItTagWeLookingFor(a) && tagWeLookingFor_count > 0) tagWeLookingFor_count--;
a = a.parentNode;
}
if (a != fbw_body && a != placeWhereToStop) {
if (isItTagWeLookingFor(a) && tagWeLookingFor_count > 0) tagWeLookingFor_count--;
a = a.nextSibling;
}
}
} else {
if (a.lastChild) {
if (isItTagWeLookingFor(a)) tagWeLookingFor_count++;
a = a.lastChild;
} else {
while (a != fbw_body && a != placeWhereToStop && a.previousSibling == null) {
if (isItTagWeLookingFor(a) && tagWeLookingFor_count > 0) tagWeLookingFor_count--;
a = a.parentNode;
}
if (a != fbw_body && a != placeWhereToStop) {
if (isItTagWeLookingFor(a) && tagWeLookingFor_count > 0) tagWeLookingFor_count--;
a = a.previousSibling;
}
}
}
return a;
}

// Функция проверки, что тег содержит непустой текст
function nonEmptyTag(f) {
function getNextNodeInScriptDirection2(t) {
var a = t;
if (scriptDirection == "forward") {
if (a.firstChild) a = a.firstChild;
else {
while (a != f && a.nextSibling == null) a = a.parentNode;
if (a != f) a = a.nextSibling;
}
} else {
if (a.lastChild) a = a.lastChild;
else {
while (a != f && a.previousSibling == null) a = a.parentNode;
if (a != f) a = a.previousSibling;
}
}
return a;
}

if (!f.hasChildNodes()) return false;
var el3 = getNextNodeInScriptDirection2(f);
while (el3 != f && el3.nodeType != 3) el3 = getNextNodeInScriptDirection2(el3);
if (el3.nodeType == 3) return true;
else return false;
}

// Основная функция поиска
function mySearch() {
// Проверим, сколько раз находится маркер, от которого начинаем движение, в теге искомого типа
tagWeLookingFor_count = 0;
el2 = el;

if (paragraphIndent) {
// Поднимаемся до родительского блока (абзаца или секции)
while (el.parentNode.nodeName != "BODY" && el.parentNode.nodeName != "DIV") el = el.parentNode;

// Переходим к следующему/предыдущему блоку
if ((scriptDirection == "forward" ? el.nextSibling : el.previousSibling) == null) {
while (el != fbw_body && (scriptDirection == "forward" ? el.nextSibling : el.previousSibling) == null) el = el.parentNode;
if (el != fbw_body) el = (scriptDirection == "forward" ? el.nextSibling : el.previousSibling);
} else {
el = scriptDirection == "forward" ? el.nextSibling : el.previousSibling;
}

// Если достигли границы документа, спрашиваем, продолжать ли с другого конца
if (el == fbw_body) {
if (confirm("Достигнут " + (scriptDirection == "forward" ? "конец" : "начало") + " документа.\nПродолжить поиск с " + (scriptDirection == "forward" ? "начала" : "конца") + " документа?")) {
el = fbw_body;
} else {
return;
}
}
} else {
// Подсчитываем вложенность искомых тегов в текущей позиции
while (el2 != fbw_body) {
if (isItTagWeLookingFor(el2)) tagWeLookingFor_count++;
el2 = el2.parentNode;
}

// Пропускаем текущий узел, если он не подходит для начала поиска
while (!((el.nodeType == 3 && tagWeLookingFor_count == 0) || (el.nodeName == "P" && window.external.inflateBlock(el) == true) || el.nodeName == "IMG")) {
el = getNextNodeInScriptDirection(el);
if (el == placeWhereToStop) {
alert("Ничего не найдено, поиск завершен.");
return;
}
if (el == fbw_body) {
if (confirm("Достигнут " + (scriptDirection == "forward" ? "конец" : "начало") + " документа.\nПродолжить поиск с " + (scriptDirection == "forward" ? "начала" : "конца") + " документа?")) {
el = fbw_body;
} else {
return;
}
}
}
}

// Теперь ищем новый индекс (SUP или SUB)
while (true) {
el = getNextNodeInScriptDirection(el);
if (el == placeWhereToStop) {
alert("Ничего не найдено, поиск завершен.");
return;
}
if (el == fbw_body) {
if (confirm("Достигнут " + (scriptDirection == "forward" ? "конец" : "начало") + " документа.\nПродолжить поиск с " + (scriptDirection == "forward" ? "начала" : "конца") + " документа?")) {
el = fbw_body;
} else {
return;
}
}

// Если нашли искомый тег (SUP или SUB) и он не пустой
if (isItTagWeLookingFor(el) && nonEmptyTag(el)) {
el2 = el;

// Находим первый дочерний текстовый узел
while (el2.hasChildNodes() && el2 != placeWhereToStop) {
el2 = getNextNodeInScriptDirection(el2);
}
if (el2 == placeWhereToStop) {
alert("Ничего не найдено, поиск завершен.");
return;
}

// В зависимости от настройки actionOnFound выполняем нужное действие
if (actionOnFound == 0) {
// Поставить курсор на найденный индекс
range1.moveToElementText(el);
range1.collapse(scriptDirection == "forward" ? true : false);

// Коррекция для IE (чтобы курсор встал точно в начало/конец тега)
if (scriptDirection == "forward") {
if (range1.parentElement() !== el && range1.move("character", 1) == 1) range1.move("character", -1);
} else {
if (range1.parentElement() !== el && range1.move("character", -1) == 1) range1.move("character", 1);
}
} else {
// Выделить весь абзац с найденным индексом
var paragraph = el;
// Поднимаемся до тега P (абзаца)
while (paragraph && paragraph.nodeName != "P" && paragraph != fbw_body) {
paragraph = paragraph.parentNode;
}
if (paragraph && paragraph.nodeName == "P") {
range1.moveToElementText(paragraph);
} else {
// Если не нашли P, выделяем сам индекс
range1.moveToElementText(el);
}
// Не схлопываем диапазон, оставляем выделение
}

range1.select();
scrollIfItNeeds();
return "Found";
}
}
}

// Проверка типа выделения (не должно быть выделения картинок)
if (document.selection.type == "Control") {
alert("Ошибка.\n\n" + versionStr + "\n\nВы используете тип выделения,\nкоторый не поддерживается этим скриптом –\nвыделение картинок, а не текста.\nПожалуйста, используйте выделение текста.");
return;
}

// Получаем основной элемент body документа
fbw_body = document.getElementById("fbw_body");

// Генерируем случайное число для создания уникальных ID маркеров
randomNum = Math.floor((Math.random() * 9)).toString() +
Math.floor((Math.random() * 9)).toString() +
Math.floor((Math.random() * 9)).toString() +
Math.floor((Math.random() * 9)).toString() +
Math.floor((Math.random() * 9)).toString() +
Math.floor((Math.random() * 9)).toString();

beginMarkerId = "GoToIndexBeginMarker" + randomNum;
endMarkerId = "GoToIndexEndMarker" + randomNum;

// Создаем маркеры начала и конца текущего выделения
var range = document.selection.createRange();
range1 = range.duplicate();
range1.collapse(true);
range1.pasteHTML("<B id=" + beginMarkerId + "></B>");

beginMarkerEl = document.getElementById(beginMarkerId);

range2 = range.duplicate();
range2.collapse(false);
range2.pasteHTML("<B id=" + endMarkerId + "></B>");
endMarkerEl = document.getElementById(endMarkerId);

// Устанавливаем начальную позицию поиска (после текущего выделения)
el = (scriptDirection == "forward") ? endMarkerEl : beginMarkerEl;
placeWhereToStop = (scriptDirection == "forward") ? beginMarkerEl : endMarkerEl;

// Запускаем поиск с обработкой ошибок
try {
var scriptResult = mySearch();
} catch(e) {
alert(versionStr + "\n\nПроизошла какая-то ошибка.");
}

// Удаляем маркеры
beginMarkerEl.removeNode(true);
endMarkerEl.removeNode(true);

// Завершаем блок отмены действий
window.external.EndUndoUnit(document);

// Возвращаем результат
if (scriptResult == "Found") return scriptResult;
return "NotFound";
}
