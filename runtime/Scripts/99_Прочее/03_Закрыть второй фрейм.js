/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Закрыть второй фрейм",
  "version": "1.1",
  "authors": [
    {
      "name": "Sclex",
      "role": "author"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Удаляет вторую область и возвращает основной контейнер текста.",
  "description": "Находит созданные вторым фреймом контейнеры, заменяет оболочку основным fbw_body и восстанавливает её через InflateIt. Изменения, сделанные только в несинхронизированной копии, не следует считать перенесёнными в основной текст.",
  "scope": "Сеанс и интерфейс редактора.",
  "modifiesDocument": null,
  "requiresSelection": false,
  "usage": [
    "Запустите скрипт в FBE Next и следуйте сообщениям; требования к документу указаны в preconditions.",
    "Запустите команду и ответьте на её диалоги. Тексты и условия проверок приведены ниже."
  ],
  "warnings": [
    "Скрипт содержит операции изменения документа. Перед массовой обработкой сохраните отдельную копию книги."
  ],
  "dependencies": [],
  "dependenciesComplete": false,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "57a99e659ac870f54c16aca95c01c40f91d203167ec70cc646d3d0f1dcca7000",
    "reviewStatus": "generated"
  },
  "tags": [
    "Прочее"
  ],
  "undo": {
    "status": "explicit_calls_found",
    "description": "Вызовы BeginUndoUnit/EndUndoUnit найдены. Это не доказывает корректный откат всех ветвей, внешних файлов и зависимостей."
  }
}
</FBE-SCRIPT-CARD>*/
// Скрипт "Закрыть второй фрейм"
// Версия 1.1
// Автор Sclex

function Run() {
  var fbwBody=document.getElementById("fbw_body");
  var containerEl=document.getElementById("container");
  if (!fbwBody || !containerEl) {
    MsgBox("Произошла ошибка!");
    return;
  }
  window.external.BeginUndoUnit(document,"закрытие второго фрейма");
  fbwBody.style.height="100%";
  fbwBody.style.overflow="auto";
  containerEl.outerHTML=fbwBody.outerHTML;
  InflateIt(containerEl);
  window.external.EndUndoUnit(document);
}
