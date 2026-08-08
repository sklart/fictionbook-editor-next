// Скрипт "Текстовая статистика" для редактора FBE
// version 3.1
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для отображения максимально подробной текстовой
// и лингвистической статистики в fb2 документе или выделенном фрагменте.
// Лингвистическую статистику скрипт производит, в основном, для русского языка.
// Никаких изменений в документе скрипт не производит.

// Изменения по сравнению с версией 2.2:
// Добавлена статистика для выделенного фрагмента.
// Добавлен подсчет блочных и инлайн иллюстраций.
// Добавлена более подробная статистика по различным абзацам.

// version 3.1, 22.04.2026
//======================================

function Run() {
    var ScriptName = "Текстовая статистика";
    var NumerusVersion = "3.1";
    
    var nbspEntity = "&nbsp;";
    try { 
        var nbspChar = window.external.GetNBSP();  
        if (nbspChar.charCodeAt(0) != 160) nbspEntity = nbspChar;
    } catch(e) { 
        var nbspChar = String.fromCharCode(160); 
    }

    // Новые счетчики
    var totalParagraphsAll = 0;      // Абзацев всего (любые)
    var textParagraphs = 0;           // Текстовых абзацев
    var symbolOnlyParagraphs = 0;     // Абзацев только с символами (без букв или цифр)
    var emptyParagraphs = 0;          // Абзацев-пустых строк
    var blockImages = 0;              // Блочных иллюстраций (DIV class="image")
    var inlineImages = 0;             // Инлайн иллюстраций (span class="image")
    
    // Основные счетчики
    var totalCharsWithSpaces = 0;
    var totalCharsWithoutSpaces = 0;
    var paragraphCount = 0;
    var wordCount = 0;
    var uniqueWords = {};
    var duplicateWords = 0;
    var vocabulary = {};
    
    // Счетчики языков
    var languageStats = {
        "russian": 0, "english": 0, "french": 0, "german": 0, 
        "spanish": 0, "italian": 0, "other": 0
    };
    
    // Счетчики частей речи
    var partsOfSpeech = {
        "nouns": 0, "adjectives": 0, "verbs": 0, "adverbs": 0,
        "pronouns": 0, "numerals": 0, "prepositions": 0, "conjunctions": 0,
        "arabic_numbers": 0, "roman_numbers": 0
    };

    // Регулярные выражения как в скрипте "ОБЩАЯ СТАТИСТИКА"
    var reWordLing = /[А-Яа-яA-Za-zЁё0-9]+/g;  // для лингвистического анализа
    var reWordOrig = new RegExp("(^|\\s|"+nbspChar+").{0,}?[А-яA-Za-zЁё].{0,}?(?=\\s|"+nbspChar+"|$)","g");  // как в скрипте "ОБЩАЯ СТАТИСТИКА"
    var reRoman = /^[IVXLCDMivxlcdm]+$/;
    var reArabic = /^[0-9]+$/;
    var reSpaces = /^\s*$/;
    var reHasLetter = /[А-Яа-яA-Za-zЁё0-9]/;  // проверка наличия букв ИЛИ цифр
    
    // [ФУНКЦИИ getLemma, detectLanguage, detectPartOfSpeech БЕЗ ИЗМЕНЕНИЙ]
    // Функция лемматизации
    function getLemma(word) {
        var lemma = word.toLowerCase();
        if (lemma.length <= 2 || reArabic.test(lemma) || reRoman.test(lemma)) return lemma;
        
        if (lemma.length > 3) {
            if (/(а|я|у|ю|ом|ем|ой|ей|ами|ями|ах|ях)$/.test(lemma)) {
                lemma = lemma.replace(/(а|я|у|ю|ом|ем|ой|ей|ами|ями|ах|ях)$/, "");
            }
        }
        
        if (/(ет|ют|ут|ят|ат|ите|йте|ла|ло|ли|лся|лась|лось|лись)$/.test(lemma)) {
            lemma = lemma.replace(/(ет|ют|ут|ят|ат|ите|йте|ла|ло|ли|лся|лась|лось|лись)$/, "ть");
        }
        
        if (/(ый|ий|ой|ая|яя|ое|ее|ые|ие)$/.test(lemma)) {
            lemma = lemma.replace(/(ый|ий|ой|ая|яя|ое|ее|ые|ие)$/, "ый");
        }
        
        return lemma;
    }
    
    // Функция определения языка
    function detectLanguage(word) {
        if (/[àâäèéêëîïôöùûüÿçœæ]/.test(word) || 
            /^(le|la|les|de|des|du|et|est|que|dans|une|il)$/.test(word)) {
            languageStats.french++;
            return;
        }
        
        if (/[äöüß]/.test(word) || 
            /^(der|die|das|den|dem|des|ein|eine|und|ist|sind|war)$/.test(word)) {
            languageStats.german++;
            return;
        }
        
        if (/[ñáéíóúü¿¡]/.test(word) || 
            /^(el|la|los|las|un|una|y|o|pero|es|soy|eres)$/.test(word)) {
            languageStats.spanish++;
            return;
        }
        
        if (/[àèéìíîòóù]/.test(word) ||
            /^(il|lo|la|i|gli|le|un|uno|una|e|o|ma)$/.test(word)) {
            languageStats.italian++;
            return;
        }
        
        if (/[а-яё]/.test(word)) {
            languageStats.russian++;
            return;
        }
        
        if (/^[a-z]+$/.test(word)) {
            languageStats.english++;
            return;
        }
        
        languageStats.other++;
    }
    
// УЛУЧШЕННАЯ ФУНКЦИЯ ОПРЕДЕЛЕНИЯ ЧАСТЕЙ РЕЧИ
function detectPartOfSpeech(word) {
    // 1. Сначала проверяем самые короткие и частые слова
    if (word.length <= 3) {
        // Местоимения
        if (/(^я$|^ты$|^он$|^она$|^оно$|^мы$|^вы$|^они$|^мой$|^твой$|^свой$|^наш$|^ваш$|^его$|^её$|^их$|^кто$|^что$|^чей$|^все$|^сам$)/.test(word)) {
            partsOfSpeech.pronouns++;
            return;
        }
        // Предлоги
        if (/(^в$|^на$|^за$|^под$|^над$|^от$|^до$|^из$|^у$|^о$|^об$|^со$|^по$|^про$|^при$|^без$|^для$|^к$|^с$|^перед$|^через$|^между$|^среди$)/.test(word)) {
            partsOfSpeech.prepositions++;
            return;
        }
        // Союзы
        if (/(^и$|^а$|^но$|^или$|^что$|^чтобы$|^как$|^когда$|^если$|^да$|^же$|^ли$|^либо$|^ни$|^не$|^нет$|^то$|^ведь$|^хотя$)/.test(word)) {
            partsOfSpeech.conjunctions++;
            return;
        }
        // Числительные
        if (/(^один$|^два$|^три$|^раз$|^пять$|^шесть$|^семь$|^восемь$|^девять$|^десять$|^сто$|^тысяча$|^первый$|^второй$|^третий$)/.test(word)) {
            partsOfSpeech.numerals++;
            return;
        }
    }
    
    // 2. Глаголы (более точные окончания)
    if (/(ть$|ти$|ться$|тся$|лся$|лась$|лось$|лись$|ет$|ут$|ют$|ат$|ят$|ите$|йте$|ал$|ыл$|ыла$|ыло$|ыли$|ил$|ила$|ило$|или$|ала$|ало$|али$|ить$|ыть$|ать$|ять$)/.test(word)) {
        partsOfSpeech.verbs++;
        return;
    }
    
    // 3. Прилагательные (расширенные окончания)
    if (/(ый$|ий$|ой$|ая$|яя$|ое$|ее$|ые$|ие$|ого$|ему$|ой$|ую$|ым$|ом$|ская$|ское$|ской$|еский$|еская$|еское$)/.test(word)) {
        partsOfSpeech.adjectives++;
        return;
    }
    
    // 4. Наречия (расширенные окончания)
    if (/(о$|е$|ко$|но$|то$|где$|куда$|когда$|почему$|зачем$|как$|сколько$|хорошо$|плохо$|быстро$|медленно$|здесь$|там$|сейчас$|потом$)/.test(word)) {
        partsOfSpeech.adverbs++;
        return;
    }
    
    // 5. Всё остальное - существительные
    partsOfSpeech.nouns++;
}
    
    // Функция для обхода узлов (из скрипта "Удалить пустые строки в выделении")
    function getNextNode(el) {
        if (el.firstChild && el.nodeName!="P")
            el=el.firstChild;
        else {
            while (el && !el.nextSibling)
                el=el.parentNode;
            if (el && el.nextSibling) el=el.nextSibling; 
        }
        return el;
    }
    
    // Функция для получения ближайшего блочного элемента (P или DIV.image)
    function getNearestBlockElement(el) {
        if (!el) return null;
        // Если это уже P или DIV.image - возвращаем его
        if (el.nodeName == "P") return el;
        if (el.nodeName == "DIV" && el.className == "image") return el;
        // Поднимаемся вверх по родителям, пока не найдём P или DIV.image
        var current = el;
        while (current && current.nodeName != "BODY") {
            if (current.nodeName == "P") return current;
            if (current.nodeName == "DIV" && current.className == "image") return current;
            current = current.parentNode;
        }
        return null;
    }
    
    // Функция для проверки: является ли абзац пустым
    function isEmptyParagraph(pElement) {
        var text = pElement.innerText || pElement.textContent;
        if (!text) return true;
        var cleanText = text.replace(/^\s+|\s+$/g, "");
        return cleanText.length == 0;
    }
    
    // Функция для проверки: содержит ли абзац только символы (без букв и цифр)
    function isSymbolOnlyParagraph(pElement) {
        var text = pElement.innerText || pElement.textContent;
        if (!text) return false;
        return !reHasLetter.test(text);
    }
    
    // Функция для подсчета инлайн картинок внутри абзаца (для всего документа)
    function countInlineImagesInParagraph(pElement) {
        var spans = pElement.getElementsByTagName("SPAN");
        var count = 0;
        for (var i = 0; i < spans.length; i++) {
            if (spans[i].className == "image") {
                count++;
            }
        }
        return count;
    }
    
    // Функция для проверки, находится ли span внутри выделения
    function isSpanInSelection(spanElement, selectionRange) {
        try {
            // Создаем диапазон для span
            var spanRange = document.body.createTextRange();
            spanRange.moveToElementText(spanElement);
            // Проверяем пересечение с выделением
            return selectionRange.inRange(spanRange) || selectionRange.isEqual(spanRange);
        } catch(e) {
            return false;
        }
    }
    
    // Определяем, есть ли выделение
    var isSelection = false;
    var selectionStartEl = null;
    var selectionEndEl = null;
    var elementsToProcess = [];
    var paragraphsToProcess = [];
    var blockImagesToProcess = [];
    var selectedText = "";  // Текст выделения
    var selectionRange = null;  // Диапазон выделения
    
    try {
        var sel = document.selection;
        if (sel) {
            var range = sel.createRange();
            if (range && range.parentElement().nodeName != "TEXTAREA" && range.parentElement().nodeName != "INPUT") {
                var tempRange = sel.createRange();
                if (tempRange.compareEndPoints("StartToEnd", tempRange) != 0) {
                    isSelection = true;
                    selectionRange = sel.createRange();
                    selectedText = selectionRange.text;
                    
                    var startRange = sel.createRange();
                    startRange.collapse(true);
                    var startEl = startRange.parentElement();
                    
                    var endRange = sel.createRange();
                    endRange.collapse(false);
                    var endEl = endRange.parentElement();
                    
                    // Поднимаемся до блочных элементов (P или DIV.image)
                    startEl = getNearestBlockElement(startEl);
                    endEl = getNearestBlockElement(endEl);
                    
                    // Функция для проверки, находится ли элемент el1 перед el2 в документе
                    function isBefore(el1, el2) {
                        if (el1 == el2) return true;
                        var ptr = el1;
                        var maxIterations = 10000;
                        var iter = 0;
                        var fbwBody_local = document.getElementById("fbw_body");
                        if (!fbwBody_local) return false;
                        while (ptr && ptr != el2 && iter < maxIterations) {
                            // Проверяем, что ptr - это элемент (nodeType == 1), прежде чем использовать contains
                            if (ptr.nodeType == 1 && !fbwBody_local.contains(ptr)) {
                                return false;
                            }
                            ptr = getNextNode(ptr);
                            iter++;
                        }
                        return (ptr == el2);
                    }
                    
                    // Определяем реальное начало и конец
                    if (startEl && endEl) {
                        if (isBefore(startEl, endEl)) {
                            selectionStartEl = startEl;
                            selectionEndEl = endEl;
                        } else {
                            selectionStartEl = endEl;
                            selectionEndEl = startEl;
                        }
                    }
                }
            }
        }
    } catch(e) {
        isSelection = false;
    }
    
    // Если есть выделение - собираем элементы в его пределах
    if (isSelection && selectionStartEl && selectionEndEl && selectionRange) {
        var fbwBody = document.getElementById("fbw_body");
        if (fbwBody) {
            var ptr = selectionStartEl;
            while (ptr && fbwBody.contains(ptr)) {
                // Собираем абзацы и блочные картинки
                if (ptr.nodeName == "P") {
                    elementsToProcess.push(ptr);
                } else if (ptr.nodeName == "DIV" && ptr.className == "image") {
                    elementsToProcess.push(ptr);
                }
                
                if (ptr === selectionEndEl) break;
                ptr = getNextNode(ptr);
            }
        }
        
        // Разделяем собранные элементы на абзацы и блочные картинки
        for (var i = 0; i < elementsToProcess.length; i++) {
            var el = elementsToProcess[i];
            if (el.nodeName == "P") {
                paragraphsToProcess.push(el);
            } else if (el.nodeName == "DIV" && el.className == "image") {
                blockImagesToProcess.push(el);
            }
        }
        
        // Считаем блочные картинки
        blockImages = blockImagesToProcess.length;
        
        // Обрабатываем каждый абзац в выделении (только для подсчёта абзацев)
        for (var i = 0; i < paragraphsToProcess.length; i++) {
            var paragraph = paragraphsToProcess[i];
            var text = paragraph.innerText || paragraph.textContent;
            
            totalParagraphsAll++;
            
            // Проверка на пустой абзац
            if (isEmptyParagraph(paragraph)) {
                emptyParagraphs++;
            }
            
            // Проверка на абзац только с символами
            if (isSymbolOnlyParagraph(paragraph)) {
                symbolOnlyParagraphs++;
            }
            
            // Текстовый абзац - если не пустой И не "только символы"
            if (!isEmptyParagraph(paragraph) && !isSymbolOnlyParagraph(paragraph)) {
                textParagraphs++;
            }
            
            // Считаем инлайн картинки внутри абзаца, которые попали в выделение
            var spans = paragraph.getElementsByTagName("SPAN");
            for (var j = 0; j < spans.length; j++) {
                if (spans[j].className == "image" && isSpanInSelection(spans[j], selectionRange)) {
                    inlineImages++;
                }
            }
        }
        
        // Подсчёт символов и слов только в выделенном тексте
        if (selectedText && !selectedText.match(reSpaces)) {
            var cleanSelectedText = selectedText.replace(/^\s+|\s+$/g, "");
            
            // Подсчет символов
            totalCharsWithSpaces += selectedText.length;
            totalCharsWithoutSpaces += cleanSelectedText.replace(/\s/g, "").length;
            
            // ПОДСЧЕТ СЛОВ
            if (selectedText.search(reWordOrig) != -1) {
                wordCount += selectedText.match(reWordOrig).length;
            }
            
            // Лингвистический анализ (отдельно)
            var words = selectedText.match(reWordLing);
            if (words) {
                for (var j = 0; j < words.length; j++) {
                    var originalWord = words[j];
                    var word = originalWord.toLowerCase();
                    
                    // Сначала определяем язык для ВСЕХ слов
                    detectLanguage(word);
                    
                    // Обрабатываем цифры
                    if (reArabic.test(word)) {
                        partsOfSpeech.arabic_numbers++;
                        partsOfSpeech.numerals++;
                        continue;
                    }
                    
                    if (reRoman.test(word)) {
                        partsOfSpeech.roman_numbers++;
                        partsOfSpeech.numerals++;
                        continue;
                    }
                    
                    // Обычные слова
                    if (uniqueWords[word]) {
                        uniqueWords[word]++;
                        if (uniqueWords[word] === 2) {
                            duplicateWords++;
                        }
                    } else {
                        uniqueWords[word] = 1;
                    }
                    
                    var lemma = getLemma(word);
                    if (vocabulary[lemma]) {
                        vocabulary[lemma]++;
                    } else {
                        vocabulary[lemma] = 1;
                    }
                    
                    detectPartOfSpeech(word);
                }
            }
        }
        
        paragraphCount = paragraphsToProcess.length;
        
    } else {
        // НЕТ ВЫДЕЛЕНИЯ - обрабатываем весь документ как раньше
        var fbwBody = document.getElementById("fbw_body");
        var allParagraphs = fbwBody.getElementsByTagName("P");
        var allBlockImages = fbwBody.getElementsByTagName("DIV");
        
        // Считаем блочные картинки
        for (var i = 0; i < allBlockImages.length; i++) {
            if (allBlockImages[i].className == "image") {
                blockImages++;
            }
        }
        
        // Обрабатываем каждый параграф как в скрипте "ОБЩАЯ СТАТИСТИКА"
        for (var i = 0; i < allParagraphs.length; i++) {
            var paragraph = allParagraphs[i];
            var text = paragraph.innerText || paragraph.textContent;
            
            totalParagraphsAll++;
            
            // Проверка на пустой абзац
            if (isEmptyParagraph(paragraph)) {
                emptyParagraphs++;
            }
            
            // Проверка на абзац только с символами
            if (isSymbolOnlyParagraph(paragraph)) {
                symbolOnlyParagraphs++;
            }
            
            // Текстовый абзац - если не пустой И не "только символы"
            if (!isEmptyParagraph(paragraph) && !isSymbolOnlyParagraph(paragraph)) {
                textParagraphs++;
            }
            
            // Считаем инлайн картинки внутри абзаца
            inlineImages += countInlineImagesInParagraph(paragraph);
            
            if (!text || text.match(reSpaces)) continue;
            
            var cleanText = text.replace(/^\s+|\s+$/g, "");
            
            // Подсчет символов
            totalCharsWithSpaces += text.length;
            totalCharsWithoutSpaces += cleanText.replace(/\s/g, "").length;
            
            // ПОДСЧЕТ СЛОВ как в скрипте "ОБЩАЯ СТАТИСТИКА"
            if (text.search(reWordOrig) != -1) {
                wordCount += text.match(reWordOrig).length;
            }
            
            // Лингвистический анализ (отдельно)
            var words = text.match(reWordLing);
            if (words) {
                for (var j = 0; j < words.length; j++) {
                    var originalWord = words[j];
                    var word = originalWord.toLowerCase();
                    
                    // Сначала определяем язык для ВСЕХ слов
                    detectLanguage(word);
                    
                    // Обрабатываем цифры
                    if (reArabic.test(word)) {
                        partsOfSpeech.arabic_numbers++;
                        partsOfSpeech.numerals++;
                        continue;
                    }
                    
                    if (reRoman.test(word)) {
                        partsOfSpeech.roman_numbers++;
                        partsOfSpeech.numerals++;
                        continue;
                    }
                    
                    // Обычные слова
                    if (uniqueWords[word]) {
                        uniqueWords[word]++;
                        if (uniqueWords[word] === 2) {
                            duplicateWords++;
                        }
                    } else {
                        uniqueWords[word] = 1;
                    }
                    
                    var lemma = getLemma(word);
                    if (vocabulary[lemma]) {
                        vocabulary[lemma]++;
                    } else {
                        vocabulary[lemma] = 1;
                    }
                    
                    detectPartOfSpeech(word);
                }
            }
        }
        
        paragraphCount = allParagraphs.length;
    }
    
    // [ПОДГОТОВКА РЕЗУЛЬТАТОВ И ФОРМАТИРОВАНИЕ КАК В ПРЕДЫДУЩЕЙ ВЕРСИИ]
    // Подготовка результатов
    var uniqueWordCount = 0;
    for (var word in uniqueWords) {
        if (uniqueWords.hasOwnProperty(word)) {
            uniqueWordCount++;
        }
    }
    
    var vocabularySize = 0;
    for (var lemma in vocabulary) {
        if (vocabulary.hasOwnProperty(lemma)) {
            vocabularySize++;
        }
    }
    
    // Расчет процентов языков
    var totalWordsForLang = languageStats.russian + languageStats.english + languageStats.french + 
                           languageStats.german + languageStats.spanish + languageStats.italian + languageStats.other;
    
    var russianPercent = totalWordsForLang > 0 ? Math.round((languageStats.russian / totalWordsForLang) * 1000) / 10 : 0;
    var englishPercent = totalWordsForLang > 0 ? Math.round((languageStats.english / totalWordsForLang) * 1000) / 10 : 0;
    var frenchPercent = totalWordsForLang > 0 ? Math.round((languageStats.french / totalWordsForLang) * 1000) / 10 : 0;
    var germanPercent = totalWordsForLang > 0 ? Math.round((languageStats.german / totalWordsForLang) * 1000) / 10 : 0;
    var spanishPercent = totalWordsForLang > 0 ? Math.round((languageStats.spanish / totalWordsForLang) * 1000) / 10 : 0;
    var italianPercent = totalWordsForLang > 0 ? Math.round((languageStats.italian / totalWordsForLang) * 1000) / 10 : 0;
    var otherPercent = totalWordsForLang > 0 ? Math.round((languageStats.other / totalWordsForLang) * 1000) / 10 : 0;
    
    // Функция форматирования чисел (единая для всех скриптов)
    function formatNumber(num) {
        num = num + "";
        if (num.length > 3) return num.replace(/(\d)(?=(\d{3})+$)/g, "$1 ");
        return num;
    }
    
    // Применяем форматирование ко всем числам
    totalCharsWithSpaces = formatNumber(totalCharsWithSpaces);
    totalCharsWithoutSpaces = formatNumber(totalCharsWithoutSpaces);
    wordCount = formatNumber(wordCount);
    uniqueWordCount = formatNumber(uniqueWordCount);
    vocabularySize = formatNumber(vocabularySize);
    duplicateWords = formatNumber(duplicateWords);
    
    // Форматируем новые счетчики
    var totalParagraphsAllFormatted = formatNumber(totalParagraphsAll);
    var textParagraphsFormatted = formatNumber(textParagraphs);
    var symbolOnlyParagraphsFormatted = formatNumber(symbolOnlyParagraphs);
    var emptyParagraphsFormatted = formatNumber(emptyParagraphs);
    var blockImagesFormatted = formatNumber(blockImages);
    var inlineImagesFormatted = formatNumber(inlineImages);
    
    // Форматируем все счетчики
    var nounsFormatted = formatNumber(partsOfSpeech.nouns);
    var adjectivesFormatted = formatNumber(partsOfSpeech.adjectives);
    var verbsFormatted = formatNumber(partsOfSpeech.verbs);
    var adverbsFormatted = formatNumber(partsOfSpeech.adverbs);
    var pronounsFormatted = formatNumber(partsOfSpeech.pronouns);
    var numeralsFormatted = formatNumber(partsOfSpeech.numerals);
    var prepositionsFormatted = formatNumber(partsOfSpeech.prepositions);
    var conjunctionsFormatted = formatNumber(partsOfSpeech.conjunctions);
    var arabicNumbersFormatted = formatNumber(partsOfSpeech.arabic_numbers);
    var romanNumbersFormatted = formatNumber(partsOfSpeech.roman_numbers);
    
    var russianWordsFormatted = formatNumber(languageStats.russian);
    var englishWordsFormatted = formatNumber(languageStats.english);
    var frenchWordsFormatted = formatNumber(languageStats.french);
    var germanWordsFormatted = formatNumber(languageStats.german);
    var spanishWordsFormatted = formatNumber(languageStats.spanish);
    var italianWordsFormatted = formatNumber(languageStats.italian);
    var otherWordsFormatted = formatNumber(languageStats.other);
    
    // ФУНКЦИЯ ПРАВИЛЬНОГО СКЛОНЕНИЯ
    function pluralize(number, one, two, five) {
        number = Math.abs(number);
        if (number > 10 && number < 20) return five;
        var lastDigit = number % 10;
        if (lastDigit === 1) return one;
        if (lastDigit >= 2 && lastDigit <= 4) return two;
        return five;
    }
    
    // Формирование сообщения
    var resultMessage = ScriptName + " v." + NumerusVersion + "\n";
    resultMessage += "---------------------------------------------------\n";
    
    // Выбор заголовка в зависимости от наличия выделения
    if (isSelection) {
        resultMessage += "Статистика текста ВЫДЕЛЕННОГО ФРАГМЕНТА:\n\n";
    } else {
        resultMessage += "Статистика текста ВСЕГО ДОКУМЕНТА:\n\n";
    }
    
    resultMessage += "Символов (с пробелами)  - " + totalCharsWithSpaces + "\n";
    resultMessage += "Символов (без пробелов) - " + totalCharsWithoutSpaces + "\n\n";
    resultMessage += "Абзацев всего (любые): " + totalParagraphsAllFormatted + "\n";
    resultMessage += "Текстовых абзацев: " + textParagraphsFormatted + "\n";
    resultMessage += "Абзацев только с символами: " + symbolOnlyParagraphsFormatted + "\n";
    resultMessage += "Абзацев-пустых строк: " + emptyParagraphsFormatted + "\n\n";
    resultMessage += "Слов - " + wordCount + "\n";
    resultMessage += "Уникальных слов - " + uniqueWordCount + "\n";
    resultMessage += "Словарный запас - " + vocabularySize + "\n";
    resultMessage += "Повторяющихся слов - " + duplicateWords + "\n\n";
    resultMessage += "Блочных иллюстраций: " + blockImagesFormatted + "\n";
    resultMessage += "Инлайн иллюстраций: " + inlineImagesFormatted + "\n\n";

// РАСЧЕТ АВТОРСКИХ И ПЕЧАТНЫХ ЛИСТОВ
    var charsWithoutSpacesNum = 0;
    var charsWithSpacesNum = 0;
    
    // Преобразуем форматированные строки обратно в числа
    var tempWithout = totalCharsWithoutSpaces.replace(/\s/g, "");
    var tempWith = totalCharsWithSpaces.replace(/\s/g, "");
    charsWithoutSpacesNum = parseInt(tempWithout, 10);
    charsWithSpacesNum = parseInt(tempWith, 10);
    
    var authorSheets = Math.round(charsWithoutSpacesNum / 40000 * 100) / 100;
    var printedSheets = Math.round(charsWithoutSpacesNum / 30000 * 100) / 100;
    var typewrittenSheets = Math.round(charsWithSpacesNum / 1860 * 100) / 100;
    var a4Pages = Math.round(charsWithSpacesNum / 2500 * 100) / 100;  // стандартная страница А4
    var a5Pages = Math.round(charsWithSpacesNum / 1600 * 100) / 100;  // стандартная страница А5
    
    resultMessage += "Объем издания:\n";
    resultMessage += "Авторских листов - " + authorSheets + "\n";
    resultMessage += "Условных печатных листов - " + printedSheets + "\n";
    resultMessage += "Машинописных листов - " + typewrittenSheets + "\n";
    resultMessage += "Страниц А4 - " + a4Pages + "\n";
    resultMessage += "Страниц А5 - " + a5Pages + "\n\n";
    
    resultMessage += "Используемые языки:\n";
    resultMessage += "- русский - " + russianPercent + "% (" + russianWordsFormatted + " " + pluralize(languageStats.russian, "слово", "слова", "слов") + ")\n";
    resultMessage += "- английский - " + englishPercent + "% (" + englishWordsFormatted + " " + pluralize(languageStats.english, "слово", "слова", "слов") + ")\n";
    if (languageStats.french > 0) resultMessage += "- французский - " + frenchPercent + "% (" + frenchWordsFormatted + " " + pluralize(languageStats.french, "слово", "слова", "слов") + ")\n";
    if (languageStats.german > 0) resultMessage += "- немецкий - " + germanPercent + "% (" + germanWordsFormatted + " " + pluralize(languageStats.german, "слово", "слова", "слов") + ")\n";
    if (languageStats.spanish > 0) resultMessage += "- испанский - " + spanishPercent + "% (" + spanishWordsFormatted + " " + pluralize(languageStats.spanish, "слово", "слова", "слов") + ")\n";
    if (languageStats.italian > 0) resultMessage += "- итальянский - " + italianPercent + "% (" + italianWordsFormatted + " " + pluralize(languageStats.italian, "слово", "слова", "слов") + ")\n";
    if (languageStats.other > 0) resultMessage += "- другие - " + otherPercent + "% (" + otherWordsFormatted + " " + pluralize(languageStats.other, "слово", "слова", "слов") + ")\n";
    resultMessage += "\n";
    
    resultMessage += "Основные части речи:\n";
    resultMessage += "Существительные - " + nounsFormatted + "\n";
    resultMessage += "Прилагательные - " + adjectivesFormatted + "\n";
    resultMessage += "Глаголы - " + verbsFormatted + "\n";
    resultMessage += "Наречия - " + adverbsFormatted + "\n";
    resultMessage += "Местоимения - " + pronounsFormatted + "\n";
    resultMessage += "Числительные - " + numeralsFormatted + "\n";
    resultMessage += "Предлоги - " + prepositionsFormatted + "\n";
    resultMessage += "Союзы - " + conjunctionsFormatted + "\n";
    resultMessage += "Арабские цифры, числа - " + arabicNumbersFormatted + "\n";
    resultMessage += "Римские цифры, числа - " + romanNumbersFormatted + "\n";
    
    MsgBox(resultMessage);
}
