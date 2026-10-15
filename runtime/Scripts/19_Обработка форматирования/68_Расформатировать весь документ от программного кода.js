/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Расформатировать весь документ от программного кода",
  "version": "1.3",
  "authors": [
    {
      "name": "TaKir",
      "role": "author"
    },
    {
      "name": "DeepSeek",
      "role": "ai_assistance"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Снимает оформление программного кода (SPAN.code) во всех включённых частях книги.",
  "description": "Обходит абзацы P внутри fbw_body, находит вложенные SPAN с точным классом code и удаляет их, сохраняя содержимое и остальные теги. По умолчанию включает основной текст, сноски и комментарии; другие типы body исключены. Выделение и курсор не требуются, подтверждения перед правкой нет. После обработки показывает количество найденных абзацев по разделам и время. Внутренний параметр обработки блочных элементов заявлен, но текущий рекурсивный обход посещает их при обоих значениях.",
  "scope": "Все P в body основного текста, сносок и комментариев внутри fbw_body; последние две части можно исключить параметрами. Иные body пропускаются. Блочные DIV внутри включённых частей посещаются независимо от processBlockElements.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "При необходимости измените в начале JS включение сносок и комментариев, а также показ итогового сообщения.",
    "Запустите команду в открытой книге: подтверждение не запрашивается, все найденные SPAN.code в разрешённых частях удаляются сразу.",
    "Проверьте места с программным кодом и итоговое число абзацев; при ошибке обработки это число может отражать найденные, а не успешно изменённые абзацы."
  ],
  "warnings": [
    "Снимается оформление кода по всей книге, включая сноски и комментарии по умолчанию; выборочного режима по выделению нет.",
    "processBlockElements=0 не исключает блочные элементы: рекурсия проходит через них в общей ветке.",
    "При ошибке обработки скрипт продолжает показывать число заранее найденных абзацев как число расформатированных.",
    "Завершение операции отмены стоит после обработчика ошибки, без finally; если сообщение об ошибке само не откроется, завершение может не выполниться."
  ],
  "dependencies": [],
  "dependenciesComplete": true,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "6674f4aee88a4873831e8640266bcb705148ffc61d672970d87e3487472127da",
    "reviewStatus": "generated"
  },
  "tags": [
    "Обработка форматирования",
    "параметры"
  ],
  "undo": {
    "status": "explicit_calls_found",
    "description": "Одна операция отмены начинается после проверки наличия абзацев. Обычные досрочные выходы расположены до неё. Обработка находится в try/catch, а завершение после catch; исключение в сообщении об ошибке может помешать закрытию операции. Откат следует проверять."
  },
  "settings": [
    {
      "name": "showStatistics",
      "value": 1,
      "expression": "1",
      "description": "0 — скрыть итоговое сообщение, 1 — показать итог и число найденных абзацев по разделам. Предварительного запроса подтверждения нет."
    },
    {
      "name": "processNotesSection",
      "value": 1,
      "expression": "1",
      "description": "1 — включать раздел сносок (по умолчанию); 0 — пропускать."
    },
    {
      "name": "processCommentsSection",
      "value": 1,
      "expression": "1",
      "description": "1 — включать раздел комментариев (по умолчанию); 0 — пропускать."
    },
    {
      "name": "processBlockElements",
      "value": 1,
      "expression": "1",
      "description": "Исходное значение 1. Код обходит дочерние элементы и при 0, поэтому настройка сейчас не исключает блочные элементы."
    }
  ]
}
</FBE-SCRIPT-CARD>*/
// Скрипт "Расформатировать весь документ от программного кода" для редактора FBE
// version 1.3
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для удаления тегов кода (SPAN class=code) fb2 документах.
// Скрипт работает сразу со всем документом.
// По умолчанию обрабатываются сразу все разделы документа, включая разделы сносок и комментариев.
// При обработке удаляются ТОЛЬКО теги кода, остальное форматирование 
// (жирность, курсив, зачёркнутость, индексы и т.д.) сохраняется в полном объёме.
// Отображается подробная статистика по обработанным абзацам.
// Режим работы: обычный и тихий.
// Поддержка отмены действий (Ctrl+Z).

// На основе скрипта "Расформатировать абзац(ы) от программного кода" уважаемого тов. Sclex.

// version 1.3, 05.03.2026
//======================================

function Run() {

// ==================================================
// НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
// ==================================================

// Настройка режима отображения:
// 0 - не показывать ничего (только ошибки)
// 1 - показывать анализ и статистику
var showStatistics = 1;

// Обрабатывать раздел сносок (примечаний)
var processNotesSection = 1; // 0 - нет, 1 - да

// Обрабатывать раздел комментариев
var processCommentsSection = 1; // 0 - нет, 1 - да

// Обрабатывать ли уже размеченные "блочные" элементы
// (poem, stanza, cite, table, subtitle) - программный код в fb2, кроме обычных абзацев, допускается только в этих элементах!
var processBlockElements = 1; // 0 - нет, 1 - да

// ==================================================
// НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
// ==================================================

// Название и версия для сообщений
var scriptName = "Расформатировать весь документ от программного кода";
var version = "1.3";

// Получаем символ неразрывного пробела из настроек FBE
try {
var nbspChar = window.external.GetNBSP();
var nbspEntity;
if (nbspChar.charCodeAt(0) == 160)
nbspEntity = " ";
else
nbspEntity = nbspChar;
}
catch(e) {
var nbspChar = String.fromCharCode(160);
var nbspEntity = " ";
}

// Массивы для сбора статистики
var processedParagraphs = []; // Будем хранить информацию об обработанных абзацах

// Структура для хранения информации об абзаце
function ParagraphInfo(paragraph, bodyType, fbname) {
this.element = paragraph;
this.bodyType = bodyType; // "main", "notes", "comments"
this.fbname = fbname;
}

// ==================================================
// ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
// ==================================================

// Функция проверки, нужно ли обрабатывать данный body
function shouldProcessBody(bodyElement) {
var fbname = bodyElement.getAttribute("fbname") || "";

if (fbname == "") {
return true; // Основное тело обрабатываем всегда
}
else if (fbname == "notes") {
return (processNotesSection == 1);
}
else if (fbname == "comments") {
return (processCommentsSection == 1);
}
else {
return false; // Другие типы body не обрабатываем
}
}

// Функция проверки, является ли элемент "блочным" и нужно ли его обрабатывать
function isBlockElement(element) {
if (!element || element.nodeType != 1) return false;

var tagName = element.nodeName;
var className = element.className || "";

// Список блочных элементов, где может встречаться код
if (processBlockElements == 1) {
if (tagName == "DIV") {
if (className == "poem" ||
className == "stanza" ||
className == "cite" ||
className == "table" ||
className == "subtitle") {
return true;
}
}
}

return false;
}

// Функция рекурсивного обхода и сбора абзацев для обработки
function collectParagraphs(node, bodyType, fbname) {
if (!node) return;

// Обрабатываем все дочерние узлы
for (var i = 0; i < node.childNodes.length; i++) {
var child = node.childNodes[i];

// Если это элемент
if (child.nodeType == 1) {

// Проверяем, нужно ли обрабатывать этот элемент
var newBodyType = bodyType;
var newFbname = fbname;

// Если это body, проверяем его fbname
if (child.nodeName == "DIV" && child.className == "body") {
var childFbname = child.getAttribute("fbname") || "";
if (childFbname == "") {
newBodyType = "main";
newFbname = "";
}
else if (childFbname == "notes") {
newBodyType = "notes";
newFbname = "notes";
}
else if (childFbname == "comments") {
newBodyType = "comments";
newFbname = "comments";
}
else {
newBodyType = "other";
newFbname = childFbname;
}

// Если этот body нужно обрабатывать, рекурсивно обходим его
if (shouldProcessBody(child)) {
collectParagraphs(child, newBodyType, newFbname);
}
}
// Если это абзац
else if (child.nodeName == "P") {
// Проверяем, есть ли внутри span class="code"
var hasCode = false;
var spans = child.getElementsByTagName("SPAN");
for (var j = 0; j < spans.length; j++) {
if (spans[j].className == "code") {
hasCode = true;
break;
}
}

if (hasCode) {
processedParagraphs.push(new ParagraphInfo(child, bodyType, fbname));
}

// Рекурсивно обходим содержимое абзаца на всякий случай
collectParagraphs(child, bodyType, fbname);
}
// Если это блочный элемент, который может содержать код
else if (isBlockElement(child)) {
// Рекурсивно обходим его содержимое
collectParagraphs(child, bodyType, fbname);
}
else {
// Для всех остальных элементов рекурсивно обходим их содержимое
collectParagraphs(child, bodyType, fbname);
}
}
}
}

// Функция обработки одного абзаца
function processParagraph(p) {
if (!p) return;

// Находим все SPAN с class="code" внутри абзаца
var spans = p.getElementsByTagName("SPAN");

// Идем с конца, чтобы не сбивать индексы при удалении
for (var i = spans.length - 1; i >= 0; i--) {
if (spans[i].className == "code") {
var span = spans[i];
var parent = span.parentNode;

// Переносим всех детей спана на его место перед удалением
// (клонируем узлы, чтобы они не потерялись при удалении)
var children = span.childNodes;
var childrenCopy = [];

// Сначала собираем копии всех дочерних узлов
for (var j = 0; j < children.length; j++) {
childrenCopy.push(children[j].cloneNode(true));
}

// Вставляем копии перед спаноm
for (var j = 0; j < childrenCopy.length; j++) {
parent.insertBefore(childrenCopy[j], span);
}

// Удаляем span
span.removeNode(true);
}
}
}

// ==================================================
// ОСНОВНАЯ ЧАСТЬ СКРИПТА
// ==================================================

// Проверяем, что документ загружен
var fbwBody = document.getElementById("fbw_body");
if (!fbwBody) {
MsgBox(scriptName + "\nver. " + version + "\n\nОшибка: не найден контейнер fbw_body!");
return;
}

// Запускаем сбор абзацев со всего документа
collectParagraphs(fbwBody, "main", "");

// Проверяем, нашли ли что-нибудь
if (processedParagraphs.length == 0) {
var msg = scriptName + "\nver. " + version + "\n\n";
msg += "Абзацев с программным кодом (согласно настройкам) не найдено.";
MsgBox(msg);
return;
}

// Засекаем время начала выполнения (после сбора данных, перед обработкой)
var startTime = new Date();

// Фаза 2: Обработка (запись) с отменой действий
window.external.BeginUndoUnit(document, "Расформатировать весь документ от программного кода");

try {
// Обрабатываем каждый найденный абзац
for (var i = 0; i < processedParagraphs.length; i++) {
processParagraph(processedParagraphs[i].element);
}
}
catch (e) {
MsgBox(scriptName + "\nver. " + version + "\n\nОшибка при обработке:\n" + e.description);
}

window.external.EndUndoUnit(document);

// ==================================================
// СТАТИСТИКА
// ==================================================

// Вычисляем время выполнения
var endTime = new Date();
var elapsedSeconds = (endTime - startTime) / 1000;
// Форматируем время с тремя знаками после запятой (заменяем точку на запятую)
var elapsedStr = elapsedSeconds.toFixed(3).replace(".", ",");

// Если режим не тихий (0 - только ошибки)
if (showStatistics > 0) {

// Подсчитываем статистику по разделам
var mainCount = 0;
var notesCount = 0;
var commentsCount = 0;

for (var i = 0; i < processedParagraphs.length; i++) {
var info = processedParagraphs[i];
if (info.bodyType == "main") {
mainCount++;
}
else if (info.bodyType == "notes") {
notesCount++;
}
else if (info.bodyType == "comments") {
commentsCount++;
}
}

// Формируем сообщение
var msg = "";
msg += scriptName + "\n";
msg += "ver. " + version + "\n";
msg += "========================\n";
msg += "✓ Расформатировано абзацев: " + processedParagraphs.length + "\n\n";
msg += " • в основном разделе: " + mainCount + "\n";

if (processNotesSection == 1) {
msg += " • в разделе сносок (примечаний): " + notesCount + "\n";
} else {
msg += " • в разделе сносок (примечаний): " + notesCount + " (раздел исключён настройками)\n";
}

if (processCommentsSection == 1) {
msg += " • в разделе комментариев: " + commentsCount + "\n";
} else {
msg += " • в разделе комментариев: " + commentsCount + " (раздел исключён настройками)\n";
}

if (processBlockElements == 1) {
msg += "(Блочные элементы обрабатывались)\n\n";
} else {
msg += "(Блочные элементы не обрабатывались)\n\n";
}

msg += "✓ Время обработки: " + elapsedStr + " сек\n";

MsgBox(msg);
}
}
