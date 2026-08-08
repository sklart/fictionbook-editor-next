// Скрипт "Создать эпиграф из полных абзацев (расширенная версия)" для редактора FBE
// version 3.9
// Идея - TaKir
// Реализация - DeepSeek, TaKir
// Основано на скрипте "Создать эпиграф из полных абзацев" уважаемого тов. Sclex

// Скрипт предназначен для создания эпиграфа из выделенных полных абзацев
// в fb2 документах с расширенными настройками.
// Абзац или абзацы могут быть выделены полностью или частично
// или в абзаце может быть установлен курсор.
//
// ОСНОВНЫЕ ВОЗМОЖНОСТИ:
// 1. Преобразование выделенных абзацев в эпиграф (<div class="epigraph">)
// 2. Автоматическое создание строки "автор текста" ("text-author") для последнего абзаца
//    с настраиваемыми условиями (длина, кавычки, ФИО)
// 3. Расформатирование от полного форматирования (удаление внешних тегов <strong>/<em>)
//    с тремя режимами настройки
// 4. Сохранение сносок и частичного форматирования внутри абзацев строк без сносок
// 5. Статистика выполнения (опционально)
//
// АЛГОРИТМ СОЗДАНИЯ СТРОКИ "АВТОР ТЕКСТА" (при authorParagraphMode=2):
// 1. Проверяется разница длины с предыдущим абзацем
// 2. Если последний абзац короче предыдущего на minLengthDiffPercent% или более → АВТОР
// 3. Если НЕ короче: проверяются кавычки в тексте
// 4. Если есть кавычки → АВТОР
// 5. Если нет кавычек: проверяется похожесть на ФИО
// 6. Если похоже на ФИО → АВТОР
// 7. Если длиннее, нет кавычек и не ФИО → НЕ АВТОР

// Строка "автор текста" ВСЕГДА полностью расформатируются от жирности, курсива и индексов.
// Обычные строки расформатируются только при полном форматировании (весь эпиграф в одном теге).
// Частичное форматирование внутри абзацев строк без сносок сохраняется.

// Режим работы: обычный или тихий.
// Поддержка отмены действий (Ctrl+Z).

// !НЕ удается корректно сохранять частичное форматирование в строках со сносками
// при расформатировании основных внешних тэгов!

// В версии 3.9 по сравнению с весрией 3.7 добавлено также опциональное
// расформатирование эпиграфов от верхних и нижних индексов.

// version 3.9, 24.04.2026
//======================================

function Run(cp,check) {

  // =============== НАСТРОЙКИ ===============
  // Настройка: 1 - показывать статистику, 0 - не показывать (тихий режим)
  var showStatistics = 0; // Измените на 1 для показа статистики
  
  // Настройка создания "авторского" абзаца (text-author) для эпиграфа:
  // 0 - Никогда не создавать
  // 1 - Создавать, если последний абзац короче предыдущего
  // 2 - Создавать, если последний абзац содержит кавычки или ФИО
  // 3 - Всегда создавать
  var authorParagraphMode = 2; // По умолчанию: условие 1 ИЛИ 2
  
  // Минимальный процент разницы длины для создания авторского абзаца (0-100)
  // Если последний абзац короче предыдущего на этот процент или более, 
  // то он может стать авторским (при authorParagraphMode=1 или 2)
  var minLengthDiffPercent = 10; // По умолчанию 10%
  
  // Настройка расформатирования создаваемого эпиграфа от исходных жирности, курсива, индексов:
  // 0 - Не расформатировать
  // 1 - Только от полного форматирования (весь абзац в тегах)
  // 2 - Расформатировать всегда (даже при частичном форматировании)
  var reformatMode = 1; // По умолчанию: только полное форматирование
  
  // Максимальная длина абзаца для проверки ФИО (символов)
  var maxNameLength = 100;
  // =========================================
  
  // =============== АЛГОРИТМ СОЗДАНИЯ АВТОРСКОЙ СТРОКИ ===============
  // 1. Проверяется последний абзац в эпиграфе (если абзацев больше 1)
  // 2. При authorParagraphMode=3: всегда создается авторская строка
  // 3. При authorParagraphMode=0: никогда не создается авторская строка
  // 4. При authorParagraphMode=1 или 2:
  //    а) Проверяется разница длины с предыдущим абзацем
  //    б) Если последний абзац короче предыдущего на minLengthDiffPercent% или более → АВТОР
  //    в) Если НЕ короче: проверяются кавычки
  //    г) Если есть кавычки → АВТОР
  //    д) Если нет кавычек: проверяется похожесть на ФИО
  //    е) Если похоже на ФИО → АВТОР
  //    ж) Если длиннее, нет кавычек и не ФИО → НЕ АВТОР
  // 5. Для авторской строки ВСЕГДА удаляются теги форматирования (strong/em)
  // ==================================================================
  
  function GoToEndOfElement(elem) {
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

  // Функция для показа сообщения со статистикой
  function showStats(processedParagraphs, settingsUsed, executionTime) {
      if (!showStatistics) return;
      
      var statsMessage = "Создать эпиграф из полных абзацев (расширенная версия)\n" +
                        "ver. " + "3.9" + "\n" +
                        "---------------------------------------\n" +
                        "Эпиграф успешно создан из " + processedParagraphs + " абзацев.\n\n" +
                        "Настройки:\n" +
                        settingsUsed + "\n" +
                        "---------------------------------------\n" +
                        "Время выполнения: " + executionTime.toFixed(2) + " сек.";
      
      MsgBox(statsMessage);
  }

  // Функция trim (совместимость с IE6)
  function trimStr(str) {
      if (!str) return str;
      return str.replace(/^\s+|\s+$/g, '');
  }

  // Функция для нормализации пробелов
  function normalizeText(text) {
      if (!text) return text;
      return trimStr(text.replace(/\u00A0/g, ' '));
  }

  // Функция для проверки, является ли строка ФИО
  function isFIO(text) {
      if (!text || text.length > maxNameLength) return false;
      
      var normalized = normalizeText(text);
      if (normalized.length === 0) return false;
      
      // Убираем цифры в квадратных скобках (сноски)
      var cleanText = normalized.replace(/\[\d+\]/g, '');
      cleanText = trimStr(cleanText);
      if (cleanText.length === 0) return false;
      
      // Проверяем наличие инициалов с точками
      var hasLetterDot = false;
      var capitalCount = 0;
      var dotCount = 0;
      
      for (var i = 0; i < cleanText.length; i++) {
          var char = cleanText.charAt(i);
          var code = char.charCodeAt(0);
          
          // Заглавная буква (русская или английская)
          var isUpper = (code >= 1040 && code <= 1071) || (code >= 65 && code <= 90);
          
          if (isUpper) {
              capitalCount++;
              if (i < cleanText.length - 1 && cleanText.charAt(i + 1) === '.') {
                  hasLetterDot = true;
              }
          }
          
          if (char === '.') dotCount++;
      }
      
      // Считаем слова
      var words = cleanText.split(/\s+/);
      
      // ФИО должно удовлетворять условиям:
      if (words.length > 4) return false;
      if (capitalCount === 0) return false;
      
      // Варианты признания ФИО:
      var variant1 = hasLetterDot; // есть "буква."
      var variant2 = dotCount >= 2; // есть несколько точек
      var variant3 = capitalCount >= words.length * 0.7; // большинство слов с заглавной
      
      return variant1 || variant2 || variant3;
  }

  // Функция для проверки наличия парных кавычек (без апострофов)
  function hasQuotes(text) {
      if (!text) return false;
      
      var quotePairs = [
          { open: '«', close: '»' },
          { open: '"', close: '"' },
          { open: '„', close: '«' }
      ];
      
      for (var i = 0; i < quotePairs.length; i++) {
          var openChar = quotePairs[i].open;
          var closeChar = quotePairs[i].close;
          var openCount = 0;
          var closeCount = 0;
          
          for (var j = 0; j < text.length; j++) {
              var currentChar = text.charAt(j);
              if (currentChar === openChar) openCount++;
              if (currentChar === closeChar) closeCount++;
          }
          
          // Для одинаковых символов нужно минимум 2
          if (openChar === closeChar) {
              if (openCount >= 2) return true;
          } else {
              // Для разных символов нужно хотя бы по одному
              if (openCount >= 1 && closeCount >= 1) return true;
          }
      }
      
      return false;
  }

  // Функция для получения строки форматирования абзаца (уникальный идентификатор)
  function getFormatString(elem) {
      if (!elem) return '';
      
      var html = elem.innerHTML || '';
      if (!html) return '';
      
      // Удаляем сноски для проверки
      var htmlWithoutNotes = html.replace(/<a\s+[^>]*class\s*=\s*["']?note["']?[^>]*>.*?<\/a>/gi, '');
      
      // Удаляем пробелы в начале и конце для сравнения
      htmlWithoutNotes = trimStr(htmlWithoutNotes);
      
      // Проверяем, полностью ли отформатирован абзац
      // Паттерн: весь текст в тегах форматирования (один или несколько вложенных)
      var pattern = /^<(strong|em|sub|sup)(\s+[^>]*)?>(.*)<\/\1>$/i;
      var match = htmlWithoutNotes.match(pattern);
      
      if (!match) return '';
      
      // Рекурсивно получаем строку форматирования
      var innerContent = match[3];
      var innerFormat = getFormatStringFromHTML(innerContent);
      
      if (innerFormat) {
          // Возвращаем комбинацию: внешний тег + внутреннее форматирование
          return match[1] + '[' + innerFormat + ']';
      } else {
          // Только внешний тег
          return match[1];
      }
  }
  
  // Вспомогательная функция для получения строки форматирования из HTML
  function getFormatStringFromHTML(html) {
      if (!html) return '';
      
      html = trimStr(html);
      
      var pattern = /^<(strong|em|sub|sup)(\s+[^>]*)?>(.*)<\/\1>$/i;
      var match = html.match(pattern);
      
      if (!match) return '';
      
      var innerContent = match[3];
      var innerFormat = getFormatStringFromHTML(innerContent);
      
      if (innerFormat) {
          return match[1] + '[' + innerFormat + ']';
      } else {
          return match[1];
      }
  }

  // Функция для удаления всех тегов форматирования (поддерживает strong/em/sub/sup)
  function removeAllFormatting(elem) {
      if (!elem) return;
      
      var originalHTML = elem.innerHTML;
      if (!originalHTML) return;
      
      var newHTML = originalHTML;
      
      // Удаляем вложенные теги форматирования в несколько проходов
      for (var pass = 0; pass < 3; pass++) {
          newHTML = newHTML.replace(/<(strong|em|sub|sup)(\s+[^>]*)?>([^<]*)<\/\1>/gi, '$3');
          newHTML = newHTML.replace(/<(strong|em|sub|sup)(\s+[^>]*)?>([^<]*(?:<(?!\/?\1)[^>]*>[^<]*)*)<\/\1>/gi, '$3');
      }
      
      try {
          elem.innerHTML = newHTML;
      } catch(e) {
          // Если ошибка - оставляем оригинальный HTML
      }
  }

  // Функция для удаления определенного форматирования из абзаца
  function removeSpecificFormatting(elem, formatString) {
      if (!elem || !formatString) return;
      
      var originalHTML = elem.innerHTML;
      if (!originalHTML) return;
      
      // Сохраняем сноски
      var noteCounter = 0;
      var notesMap = {};
      var htmlWithMarkers = originalHTML.replace(/<a\s+[^>]*class\s*=\s*["']?note["']?[^>]*>.*?<\/a>/gi, function(match) {
          var marker = '<!--NOTE_' + (noteCounter++) + '-->';
          notesMap[marker] = match;
          return marker;
      });
      
      // Функция для рекурсивного удаления тегов по формату
      function removeByFormat(html, fmt) {
          if (!fmt) return html;
          
          // Извлекаем внешний тег и внутренний формат
          var match = fmt.match(/^(strong|em|sub|sup)(?:\[(.*)\])?$/i);
          if (!match) return html;
          
          var outerTag = match[1];
          var innerFmt = match[2] || '';
          
          // Паттерн для удаления внешнего тега
          var pattern = new RegExp('^\\s*<' + outerTag + '(\\s+[^>]*)?>(.*)<\\/' + outerTag + '>\\s*$', 'i');
          var tagMatch = html.match(pattern);
          
          if (tagMatch) {
              var innerContent = tagMatch[2];
              // Рекурсивно обрабатываем внутреннее содержимое
              innerContent = removeByFormat(innerContent, innerFmt);
              return innerContent;
          }
          
          return html;
      }
      
      var newHTML = removeByFormat(htmlWithMarkers, formatString);
      
      // Восстанавливаем сноски
      for (var marker in notesMap) {
          newHTML = newHTML.replace(marker, notesMap[marker]);
      }
      
      try {
          elem.innerHTML = newHTML;
      } catch(e) {
          // Если ошибка - оставляем оригинальный HTML
      }
  }

  // Функция для расформатирования эпиграфа целиком (режим 1)
  function reformatEpigraph(epigraphLines, authorIndex) {
      if (!epigraphLines || epigraphLines.length === 0) return;
      
      // Собираем форматирование не-авторских строк
      var formats = [];
      var linesToReformat = [];
      
      for (var i = 0; i < epigraphLines.length; i++) {
          // Пропускаем авторскую строку
          if (i === authorIndex) continue;
          
          var format = getFormatString(epigraphLines[i]);
          if (format) {
              formats.push(format);
              linesToReformat.push(epigraphLines[i]);
          }
      }
      
      // Если нет строк для расформатирования - выходим
      if (linesToReformat.length === 0) return;
      
      // Проверяем, что все форматирования одинаковые
      var firstFormat = formats[0];
      var allSame = true;
      for (var i = 1; i < formats.length; i++) {
          if (formats[i] !== firstFormat) {
              allSame = false;
              break;
          }
      }
      
      // Если все форматирования одинаковые - удаляем их
      if (allSame) {
          for (var i = 0; i < linesToReformat.length; i++) {
              removeSpecificFormatting(linesToReformat[i], firstFormat);
          }
      }
  }

  // Исправляем проблему с курсором внутри тега форматирования
  var tr=document.selection.createRange();
  
  function findParentParagraph(element) {
      while (element && element.nodeName != "P" && element.nodeName != "DIV") {
          element = element.parentElement;
      }
      return element;
  }
  
  var rngStart=tr.duplicate();
  rngStart.collapse(true);
  var startEl=findParentParagraph(rngStart.parentElement());
  
  var rngEnd=tr.duplicate();
  rngEnd.collapse(false);
  var endEl=findParentParagraph(rngEnd.parentElement());
  
  var rng2=document.body.createTextRange();
  rng2.moveToElementText(startEl);
  tr.setEndPoint("StartToStart",rng2);
  rng2=document.body.createTextRange();
  rng2.moveToElementText(endEl);
  tr.setEndPoint("EndToEnd",rng2);
  
  var cp=tr.parentElement();
  
  cp = GetCP(cp);
  if(!cp) return;
  
  if(cp.className != "body" && cp.className != "section" && cp.className != "poem") return;

  var pp=cp.firstChild;
  if(cp.className == "body")
    pp = SkipOver(pp, "title", "image", "epigraph");
  else
    pp = SkipOver(pp, "title", "epigraph", null);

  if(check) return true;

  if (document.selection.type && document.selection.type=="Control") {
   MsgBox("Вы используете не тот тип выделения, с которым работает вставка эпиграфа. Выделяйте текст для будущего эпиграфа не кликом по картинке, а движением мыши слева направо или справа налево. Либо задайте выделение, используя клавиатуру.");
   return;
  }

  var rng = tr.duplicate();
  
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

  // Включаем таймер
  var startTime = new Date().getTime();
  
  window.external.BeginUndoUnit(document,"создание эпиграфа из полных абзацев (расширенная версия)");
  var ep=document.createElement("DIV");
  ep.className="epigraph";
  
  // Массив для статистики
  var processedParagraphs = 0;
  var authorParagraphs = 0;
  var reformattedParagraphs = 0;
  
  // Сохраняем созданные строки для последующего расформатирования
  var createdLines = [];
  var authorLineIndex = -1;
  
  if(txt != "")
  {
    var pwt = document.createElement("P");
    pwt.innerHTML = txt;
    processedParagraphs = 1;
    ep.appendChild(pwt);
    createdLines.push(pwt);
  }
  else if(pps && pps.length > 0)
  {
    processedParagraphs = pps.length;
    
    for(i = 0; i < pps.length; ++i)
    {
      var pwt = document.createElement("P");
      pwt.innerHTML = pps[i].innerHTML;
      
      // Проверяем, нужно ли сделать этот абзац авторским
      var makeAuthor = false;
      
      if (i === pps.length - 1 && pps.length > 1) {
          var currentText = pps[i].innerText || pps[i].textContent || '';
          var prevText = pps[i-1].innerText || pps[i-1].textContent || '';
          
          var current = normalizeText(currentText);
          var prev = normalizeText(prevText);
          
          if (current.length > 0 && prev.length > 0) {
              var lengthDiffPercent = 0;
              if (prev.length > 0) {
                  lengthDiffPercent = ((prev.length - current.length) / prev.length) * 100;
              }
              
              var isShorterByPercent = lengthDiffPercent >= minLengthDiffPercent;
              
              switch(authorParagraphMode) {
                  case 0:
                      makeAuthor = false;
                      break;
                  case 1:
                      makeAuthor = isShorterByPercent;
                      break;
                  case 2:
                      if (isShorterByPercent) {
                          makeAuthor = true;
                      } else {
                          var hasQuotesInText = hasQuotes(current);
                          if (hasQuotesInText) {
                              makeAuthor = true;
                          } else {
                              var isFIOtext = isFIO(current);
                              makeAuthor = isFIOtext;
                          }
                      }
                      break;
                  case 3:
                      makeAuthor = true;
                      break;
                  default:
                      if (isShorterByPercent) {
                          makeAuthor = true;
                      } else {
                          var hasQuotesInText = hasQuotes(current);
                          if (hasQuotesInText) {
                              makeAuthor = true;
                          } else {
                              var isFIOtext = isFIO(current);
                              makeAuthor = isFIOtext;
                          }
                      }
                      break;
              }
          }
          
          if (makeAuthor) {
              pwt.className = "text-author";
              authorParagraphs++;
              authorLineIndex = createdLines.length;
          }
      }
      
      ep.appendChild(pwt);
      createdLines.push(pwt);
    }
  }
  else
    ep.appendChild(document.createElement("P"));

  // Расформатирование согласно режиму
  if (reformatMode === 1) {
      // Режим 1: расформатировать эпиграф целиком (при одинаковом форматировании строк)
      reformatEpigraph(createdLines, authorLineIndex);
      reformattedParagraphs = createdLines.length;
  } else if (reformatMode === 2) {
      // Режим 2: удаляем все форматирование у всех строк
      for (var i = 0; i < createdLines.length; i++) {
          removeAllFormatting(createdLines[i]);
      }
      reformattedParagraphs = createdLines.length;
  }
  
  // Отдельно обрабатываем авторскую строку (если есть) - всегда полностью расформатируем
  if (authorLineIndex !== -1 && reformatMode !== 0) {
      removeAllFormatting(createdLines[authorLineIndex]);
  }

  var el=startEl;
  var psArr=[];
  psArr.push(el);
  while (el!=endEl) {
   if (el.firstChild)
    el=el.firstChild;
  else {
    while (el.nextSibling==null)
     el=el.parentNode;                                                 
    el=el.nextSibling;
   }
   if (el.nodeName=="P") psArr.push(el);
  }

  InsBefore(cp, pp, ep);
  InflateIt(ep);

  var el2, parentOfEl2;
  while (psArr.length>0) {
   el2=psArr.pop();
   parentOfEl2=el2.parentNode;
   if (el2 && (el2.nextSibling || el2.parentNode.className!="section")) {
    el2.removeNode(true);
   }
   else {
    el2.innerHTML="";
    InflateIt(el2);
   }
   if (parentOfEl2.nodeName=="DIV" && 
       (parentOfEl2.className=="title" || parentOfEl2.className=="epigraph" || parentOfEl2.className=="cite") &&
       !parentOfEl2.firstChild)
    parentOfEl2.removeNode(true);
  }
  
  window.external.EndUndoUnit(document);

  GoToEndOfElement(ep);
  
  var endTime = new Date().getTime();
  var executionTime = (endTime - startTime) / 1000;
  
  var settingsText = "";
  if (showStatistics) {
      var authorModeText = "";
      switch(authorParagraphMode) {
          case 0: authorModeText = "Никогда не делать"; break;
          case 1: authorModeText = "Если короче предыдущего (" + minLengthDiffPercent + "%+)"; break;
          case 2: authorModeText = "Если короче (" + minLengthDiffPercent + "%+), кавычки или ФИО"; break;
          case 3: authorModeText = "Всегда делать"; break;
          default: authorModeText = "По умолчанию (2)"; break;
      }
      
      var reformatModeText = "";
      switch(reformatMode) {
          case 0: reformatModeText = "Не расформатировать"; break;
          case 1: reformatModeText = "Расформатировать эпиграф целиком (при одинаковом форматировании)"; break;
          case 2: reformatModeText = "Всегда расформатировать все строки"; break;
          default: reformatModeText = "Не расформатировать"; break;
      }
      
      settingsText = "Создавать автора текста: " + authorModeText + "\n" +
                    "Расформатирование: " + reformatModeText + "\n" +
                    "Авторских абзацев создано: " + authorParagraphs + "\n" +
                    "Абзацев расформатировано: " + reformattedParagraphs + "\n" +
                    "Примечание: авторские строки всегда полностью расформатируются";
  }
  
  if (showStatistics) {
      showStats(processedParagraphs, settingsText, executionTime);
  }
}
