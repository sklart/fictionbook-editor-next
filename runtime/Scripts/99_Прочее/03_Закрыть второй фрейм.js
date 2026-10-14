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
  "summary": "Удаляет второе поле и оставляет основной текст.",
  "description": "Берёт текущий основной fbw_body из двухпанельного контейнера и заменяет весь контейнер его HTML. Второе поле и разделитель удаляются; изменения, сделанные только в копии, не переносятся в основной текст.",
  "scope": "Текущий двухпанельный DOM после запуска команды создания второго поля.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "После работы с двумя полями запустите эту команду для возврата к одному основному полю.",
    "Проверьте основной текст: содержимое нижней копии в него не переносится."
  ],
  "warnings": [
    "Содержимое нижней копии удаляется вместе с контейнером и не объединяется с основным текстом.",
    "После замены контейнера скрипт вызывает восстановление отображения на старом узле; эффект этого вызова для нового основного поля не подтверждён."
  ],
  "dependencies": [],
  "dependenciesComplete": true,
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
    "description": "Штатная замена контейнера находится внутри группы отмены. Корректное восстановление двухполосного интерфейса и содержимого нижней копии через отмену не проверено."
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
