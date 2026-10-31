# Контрольные входы тестов

Это действующие входы tools/tests/test-audit-script-cards.py, tools/tests/test-note-creation-evidence.py и генератора аудита, включая связанные результаты runtime. Пути и исходные SHA сохранены; изменение пробелов, кодировки или EOL также меняет контракт. Сохранённый результат подтверждает указанную в нём историческую ревизию, а не новый прогон.

| Файл | SHA-256 исходных байтов |
| --- | --- |
| [note-creation-lint-metadata-results.json](../note-creation-lint-metadata-results.json) | `24acc193b77ed40ae0666002098f1666afdabb1df53a337a6ff32d946331fc69` |
| [note-creation-undo-stage1-results.json](../note-creation-undo-stage1-results.json) | `41eca965617b16b9357f17476ee7b4f6ed07e2f2ce2a83fa2c825c3b6c427e3f` |
| [script-card-code-baseline.json](../script-card-code-baseline.json) | `d299a7e2593f4f9e82716cca9095b9359f22b16540fd39efc13bac09ec7aabd9` |
| [script-card-reviewed-proposals.json](../script-card-reviewed-proposals.json) | `c10aa540eea838847a33724dc258fc5e22c2e20bb25532b9b8e8f71548a19b1b` |
| [script-card-undo-classification.json](../script-card-undo-classification.json) | `781e63b7209ff7e26f99ae8f11a640b8f5abb655dfcb6b83bbffcd2c40d8d579` |
| [script-card-undo-defects.md](../script-card-undo-defects.md) | `7d1a5efe4cc90970051db020e2c5b7d78a685e0ceaa226b52b0def26dab1701d` |
| [script64-runtime-results.json](../script64-runtime-results.json) | `af57389d24dbcdc52ba4c8ee4bf124299971b534154ce9056944fba34ca369b0` |

Классификация Undo проверяет sourceSha256 файла script-card-undo-defects.md; baseline и reviewed proposals связывают утверждения с конкретным исполняемым кодом. Расхождение текущего harness с историческим SHA разбирается отдельно и не исправляется автоматическим пересчётом. Workflow документации не выполняет общую проверку этих SHA.

## Новые редакции контрольной базы

Исторический SHA baseline в таблице сохранён. Редакция после разрешённого исправления двух уборок в `c0f33532` имеет SHA-256 `ce7446f629154ef242514186e40ace7bcea71512f26b0140651c55f04dd2c7e0`. Следующая редакция, учитывающая 12 известных цепочек примечаний и комментариев, имеет SHA-256 `5205462f7ecdf991edfb5c76e9ebd75d82334fa811ede23076f53d8e0e94454a`. Каждая промежуточная и текущая ревизия, коммиты и пределы подтверждения описаны в [цепочке ревизий](../community-feedback-note-revisions.md). Файл исторических предложений ревью не изменялся; его SHA в таблице остаётся действительным. Исторические результаты не подтверждают запуск старого набора сценариев на новых редакциях.
