/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Текст документа продублировать во втором фрейме",
  "version": "1.1",
  "authors": [
    {
      "name": "Sclex",
      "role": "author"
    },
    {
      "name": "DeepSeek",
      "role": "ai_assistance"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Создаёт под основным текстом вторую редактируемую копию.",
  "description": "Заменяет оболочку основного текста контейнером с разделителем и вторым DIV, содержащим копию HTML книги без атрибутов id. По умолчанию редактирование копии разрешено; её содержимое не синхронизируется автоматически с основным текстом. Разделитель можно перетаскивать.",
  "scope": "Визуальный DOM текущей книги и её интерфейс; скрипт запускается один раз, пока второй контейнер закрыт.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Откройте книгу и запустите команду для создания второго поля под основным текстом.",
    "Перетащите разделитель для изменения высоты полей.",
    "Для возврата к одному полю используйте отдельную команду закрытия; изменения в копии не считайте автоматически перенесёнными в книгу."
  ],
  "warnings": [
    "Второе поле — отдельная копия HTML; изменения в нём не синхронизируются с основным текстом.",
    "Копия создаётся без атрибутов id, поэтому внутренние ссылки и элементы с идентификаторами в ней могут работать иначе.",
    "Если основной текст не удастся найти после перестройки DOM, скрипт выйдет до закрытия группы отмены."
  ],
  "dependencies": [],
  "dependenciesComplete": true,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "8c5ed24cf6a038d584f78a36d10f0f97b32180f4e75d0ed958bc2fa0aefdebf1",
    "reviewStatus": "generated"
  },
  "tags": [
    "Прочее",
    "параметры"
  ],
  "undo": {
    "status": "explicit_calls_found",
    "description": "Если после перестройки текста основной раздел книги не найден, скрипт выходит с незакрытой группой отмены."
  },
  "settings": [
    {
      "name": "allowEditingInSecondFrame",
      "title": "Редактирование второго поля",
      "value": 1,
      "expression": "1",
      "description": "1 — разрешить редактирование копии; 0 — показать её только для чтения.",
      "source": "script"
    }
  ]
}
</FBE-SCRIPT-CARD>*/
// Скрипт "Текст документа продублировать во втором фрейме"
// Версия 1.1
// Автор Sclex (также при создании скрипта использовался ИИ DeepSeek)

// Настройка allowEditingInSecondFrame либо разрешает, либо запрещает редактирование
// текста во втором (дополнительном) фрейме.
// Значение 0 — запретить редактирование
// Значение 1 — разрешить редактирование
var
  allowEditingInSecondFrame=1;

var SplitManager = {
    container: null,
    topDiv: null,
    bottomDiv: null,
    divider: null,
    isDragging: false,
    startY: 0,
    startTopHeight: 0,
    minHeight: 50,
    dividerHeight: 6,

    init: function() {
        this.container = document.getElementById('container');
        this.topDiv = document.getElementById('fbw_body');
        this.bottomDiv = document.getElementById('fbw_body_2');
        this.divider = document.getElementById('divider');

        this.setupEvents();
        this.updateLayout();
    },

    setupEvents: function() {
        var self = this;

        // Для Internet Explorer (attachEvent)
        if (this.divider.attachEvent) {
            this.divider.attachEvent('onmousedown', function() {
                self.startDrag(window.event);
                self.divider.setCapture();
                return false;
            });

            this.divider.attachEvent('onmousemove', function() {
                if (self.isDragging) {
                    self.doDrag(window.event);
                }
                return false;
            });

            this.divider.attachEvent('onmouseup', function() {
                self.stopDrag();
                self.divider.releaseCapture();
                return false;
            });

            window.attachEvent('onresize', function() {
                self.updateLayout();
            });
        } else {
            // Для современных браузеров
            this.divider.onmousedown = function(e) {
                self.startDrag(e || window.event);
                return false;
            };

            document.onmousemove = function(e) {
                self.doDrag(e || window.event);
            };

            document.onmouseup = function() {
                self.stopDrag();
            };

            window.addEventListener('resize', function() {
                self.updateLayout();
            }, false);
        }
    },

    startDrag: function(e) {
        this.isDragging = true;
        this.startY = e.clientY;
        this.startTopHeight = this.topDiv.offsetHeight;
        document.body.style.cursor = 'n-resize';

        if (document.body.setCapture) {
            document.body.setCapture();
        }
    },

    doDrag: function(e) {
        if (!this.isDragging) return;

        var containerHeight = this.container.offsetHeight;
        var deltaY = e.clientY - this.startY;
        var newTopHeight = this.startTopHeight + deltaY;

        if (newTopHeight < this.minHeight) {
            newTopHeight = this.minHeight;
        }

        var maxTopHeight = containerHeight - this.minHeight - this.dividerHeight;
        if (newTopHeight > maxTopHeight) {
            newTopHeight = maxTopHeight;
        }

        this.topDiv.style.height = newTopHeight + 'px';
        this.updateLayout();
    },

    stopDrag: function() {
        if (this.isDragging) {
            this.isDragging = false;
            document.body.style.cursor = '';

            if (document.body.releaseCapture) {
                document.body.releaseCapture();
            }
        }
    },

    updateLayout: function() {
        var topHeight = this.topDiv.offsetHeight;
        
        this.divider.style.top = topHeight + 'px';
        this.bottomDiv.style.top = (topHeight + this.dividerHeight) + 'px';
        
        this.updateIndicators();
    },

    updateIndicators: function() {
        var containerHeight = this.container.offsetHeight;
        var topHeight = this.topDiv.offsetHeight;
        var bottomHeight = containerHeight - topHeight - this.dividerHeight;

        var topPercent = Math.round((topHeight / containerHeight) * 100);
        var bottomPercent = Math.round((bottomHeight / containerHeight) * 100);

        var topInd = document.getElementById('topIndicator');
        var bottomInd = document.getElementById('bottomIndicator');

        if (topInd) topInd.innerHTML = topPercent + '%';
        if (bottomInd) bottomInd.innerHTML = bottomPercent + '%';
    },

    setSplit: function(percent) {
        var containerHeight = this.container.offsetHeight;
        var newHeight = Math.round((containerHeight * percent) / 100);

        if (newHeight < this.minHeight) newHeight = this.minHeight;
        if (newHeight > containerHeight - this.minHeight - this.dividerHeight) {
            newHeight = containerHeight - this.minHeight - this.dividerHeight;
        }

        this.topDiv.style.height = newHeight + 'px';
        this.updateLayout();
    }
};

function setSplit(percent) {
    SplitManager.setSplit(percent);
}

function Run() {
  if (allowEditingInSecondFrame!=0 && allowEditingInSecondFrame!=1) {
    MsgBox("Ошибка.\n\nПеременная allowEditingInSecondFrame имеет значение не 0 и не 1.");
    return;
  }

  var fbwBody2=document.getElementById("fbw_body_2");
  var containerEl=document.getElementById("container");
  if (fbwBody2 || containerEl) {
    MsgBox("Вы уже открыли второй фрейм раньше — до того как сейчас запустили данный скрипт.");
    return;
  }
  
  var fbwBody=document.getElementById("fbw_body");
  if (!fbwBody) return;
  
  window.external.BeginUndoUnit(document,"добавление второго фрейма");
  fbwBody.style.height="50%";
  //fbwBody2.innerHTML=fbwBody.innerHTML;

  var re0=new RegExp(" +id=[^ <>]+(?=>| [^<>]*>)","gi");
  var re0_="";
  fbwBody.outerHTML="<DIV id='container'>"+fbwBody.outerHTML+
                    "<DIV id='divider' style='display: block;'></DIV>"+
                    "<DIV id='fbw_body_2' contentEditable='"+(allowEditingInSecondFrame==1?true:false)+"' style='display: block;'>"+fbwBody.innerHTML.replace(re0,re0_)+"</DIV>"+
                    "</DIV>";
                    
  var fbwBody=document.getElementById("fbw_body");
  if (!fbwBody) return;
  fbwBody.style.height="50%";
  fbwBody.style.overflow="auto";
  
  SplitManager.init();
  window.external.EndUndoUnit(document);
   
  MsgBox("Второй фрейм был успешно создан.");
}
