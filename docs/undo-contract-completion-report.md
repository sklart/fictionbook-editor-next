# Завершение контракта Undo

Дата: 2026-10-18. База: master, f3b46cf275c78f86a0e6af577d983116236e9547.

## Изменения

- RestoreRedo::Do(nullptr) возвращает S_OK и не исполняет удержанные записи,
  не регистрирует обратные записи и не меняет состав/порядок истории.
  Путь с менеджером сохраняет прежний порядок вызовов Add.
- ImportedBinaryUndoUnit::Do направляет std::bad_alloc, CAtlException
  и неизвестные C++-исключения после изменения DOM через существующую
  компенсацию, как и COM-ошибки. После успешного отката возвращается исходный
  HRESULT (E_OUTOFMEMORY для нехватки памяти, E_UNEXPECTED для неизвестного
  исключения); обратная операция и новое направление не регистрируются.
- Ошибка или исключение при компенсации возвращает E_ABORT. Ошибки до
  изменения документа также преобразуются в HRESULT, исключения не выходят
  через COM-метод. Постоянный отказ самого DOM/менеджера не считается
  доказанно восстановленным.
- Публичные COM/JavaScript API, зависимости и минимальная версия Windows
  не изменены.

## Проверки

Целевой сценарий undo-atomic-failures дополнен проверками:

- Do(nullptr) с пустым и заполненным набором удержанных COM-единиц:
  повторный вызов, неизменность обеих историй, их порядка и документа;
- std::bad_alloc, COM E_OUTOFMEMORY, ATL E_OUTOFMEMORY и неизвестное исключение
  после удаления/вставки binary; std::bad_alloc после перестроения обложек;
- оба направления Undo/Redo, ошибочный HRESULT команды, полный HTML, поля формы, обложки,
  ID, типы, байты binary и ссылки, идентичность/порядок истории, каретка,
  dirty/savepoint и закрытие групп;
- успешный повтор Undo/Redo после отказов, предыдущие нативные записи,
  Validate и полное сравнение XML после Save → Reopen.

| Проверка | Результат |
| --- | --- |
| Release Win32, v143 / VC Tools 14.44.35207, FbeStrictWarnings=true | PASS, без предупреждений |
| test-fbe-image-document-inserter-contract.ps1 | PASS |
| test-fbe-image-document-inserter-boundary.ps1 | PASS |
| undo-atomic-failures: Do(nullptr), исключения, сохранность/повтор Undo/Redo | PASS |
| Validate и полное сравнение XML/binary/ссылок Save → Reopen | PASS |
| git diff --check | PASS |

Выполнена одна полная сборка FBE.vcxproj и затем одна инкрементальная сборка
тестового mainfrm.cpp после исправления ожидания HRESULT в проверке.
Первый диагностический запуск показал сохранность всех состояний, но неверно
ожидал исходный HRESULT дочерней единицы от команды MSHTML. В действительности
MSHTML возвращает OLECMDERR_E_CANCELED (0x80040104). Итоговая проверка требует
ошибку команды и полное совпадение состояний; прямое сравнение исходного
HRESULT бинарной единицы этим маршрутом не заявляется.

Команды:

~~~powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe' src/fbe/FBE.vcxproj /t:Build /p:Configuration=Release /p:Platform=Win32 /p:PlatformToolset=v143 /p:VCToolsVersion=14.44.35207 /p:FbeStrictWarnings=true /m /nologo /v:minimal
.\tools\tests\test-fbe-image-document-inserter-contract.ps1
.\tools\tests\test-fbe-image-document-inserter-boundary.ps1
.\tools\tests\test-fbe-legacy-todo-runtime.ps1 -Scenarios undo-atomic-failures -TimeoutSeconds 90 -KeepArtifacts
~~~

Логи и итоговый TSV сохранены в out/tests/undo-contract-completion;
первый диагностический отчёт отделён в development. Временные portable-копии
после завершения проверок удалены. Действующих тестовых экземпляров нет.

SHA256 итогового out/Release/FBE.exe:
FAAA44F977521EF93F6BAFBD003F1B0D2DB26EEC45508989B350B6A1457BE0C8.
Бинарный файл собран до коммита; встроенный build stamp не является SHA
итогового коммита исходников.

## FAIL вне области задачи

Ранее зафиксированные internal-history и plain-middle обычной HTML-вставки
сохраняются отдельной задачей. Код их исправления не включён; повторный запуск
этих сценариев в этом этапе не выполнялся. Исходные результаты приведены
в undo-atomic-failures-report.md.

## NOT RUN

- Windows 7 и матрица пользовательских книг.
- Настоящий перенос мышью, длительные E2E, полный verify-release.ps1.
- Реальное системное исчерпание памяти и одновременный/постоянный отказ
  компенсационных COM-вызовов. Использованы одноразовые искусственные отказы;
  успешное восстановление при постоянном отказе не заявляется.
