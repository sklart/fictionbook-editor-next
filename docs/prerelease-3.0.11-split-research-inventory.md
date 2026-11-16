# Инвентаризация исследования Split/Undo и Recovery

Срез: `master` `2e94edb7cb110ed1071fa9cff3a1c1fbd01941ed`, `codex/split-undo-resumption-20261104` `11317d0636c22b2a375aa675533baef3ce618488`. Перечислены все 128 непоглощённых немержевых коммитов этой ветки на данном срезе. Подсчёт выполнен по истории Git; адаптированные отдельно изменения других веток не становятся предками исходной исследовательской истории.

Все изменённые пути этих коммитов находятся в `docs/`, `src/fbe/testing/` или `tools/tests/`; производственные и сборочные пути отсутствуют. Тесты и свидетельства сохранены в исходной ветке. Исходный сценарий Split/Undo — **KNOWN FAILURE / DEFERRED**. Наблюдения Recovery, в том числе потеря Redo и гонки публикации, не считаются исправлением и учитываются отдельно в [блокерах](prerelease-3.0.11-blockers.md).

Проверка содержимого общих тестовых файлов показала их связь со Split/Recovery: `RuntimeTestArchiveAndLifecycle.inl` добавляет проверку исследовательского снимка, `RuntimeTestScenarios.inl` подключает исследовательский заголовок, изменения `RuntimeTestPortableState.inl` сопровождают сценарии Split. Самостоятельный тест файлового протокола прямо описывает себя как эксперимент, не интегрированный с Recovery FBE; его нельзя засчитывать как приёмку редактора. Перенос отдельных будущих тестов потребует проверки зависимостей, очистки и актуального oracle.

Таблица фиксирует происхождение и тематику по заголовкам коммитов и путям, а не утверждает построчную семантическую проверку всех прототипов или приёмку итогового SHA. История и свидетельства не удаляются.

| Коммит | Назначение по исходному сообщению | Пути |
|---|---|---|
| `11317d06` | Research caret pointer variants before links | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `cee23aad` | Research keyboard caret affinity at first link | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `ce670ae0` | Research caret positions on both sides of link | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `d9373dcf` | Research display caret affinity across Split history | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `27eeca28` | Research caret affinity at inline links | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `7ecbce65` | Research caret affinity at formatting boundary | документация/свидетельства, тестовые инструменты |
| `439ca5a8` | Research validated recovery publication and history preservation | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `be20ca2b` | Expose validated detached candidate substitution | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `2f26919c` | Probe exclusive detached recovery candidate creation | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `7bd3b9e5` | Check concurrent recovery target during publication | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `5a611e48` | Verify recovery restart after publication boundary termination | документация/свидетельства, тестовые инструменты |
| `7fa741b4` | Observe FBE termination at recovery publication | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `853b0ea2` | Probe actual detached recovery write failure | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `e9ddb82f` | Reproduce detached candidate race in FBE | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `262c4b2d` | Probe interruption around recovery publication | документация/свидетельства, тестовые инструменты |
| `ab032d89` | Isolate native recovery redo loss and file races | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `67ea87d9` | Verify recovery refusal caret and redo history | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `42766cb3` | Research detached recovery with preserved redo | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `763f18ed` | Research recovery snapshot after long history rollback | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `a8aa71f4` | Investigate relative checkpoint and recovery snapshot | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `a0a0a4ff` | Research file baseline opening race and fingerprint limits | документация/свидетельства, тестовые инструменты |
| `09ccef4e` | Research late locked file baseline after rollback | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `54eb7dec` | Research independent saved file baseline for rollback | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `cd0c1821` | Research deferred savepoint reconciliation after rollback | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `435621b3` | Test dirty history savepoint guard after rollback | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `1031a98f` | Investigate dirty state after long history rollback | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `3095fddb` | Research ID fault rollback with long redo history | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `93f5a9d6` | Research split preparation with long native redo history | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `cc162c07` | Reject incomplete split history observations | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `3cd27692` | Check selected note links with isolated keyboard | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `a8451d7e` | Research selected note links and persistence | документация/свидетельства, тестовые инструменты |
| `604872fa` | Research selected text across inline links | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `a565b94f` | Prioritize split research safety for selected text | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `af4eb18f` | Study retaining caret input redo across persistence | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `f0cff386` | Check persistence after caret retry | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `46ef7ab8` | Study caret retry after partial selection changes | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `65795171` | Verify caret retry with isolated SendInput | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `3f3bc04f` | Research retrying transient split caret restoration | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `8b48ae24` | Guard split research across multiple FB2 bodies | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `7e63f7e0` | Guard nested split plans across the visual body | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `6e5fa827` | Reject unverified neighboring sections in split research | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `d6b56188` | Investigate recovery without live link normalization | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `7cde55dd` | Study persistence with a retained redo unit | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `3fecee49` | Research retaining an additional redo unit | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `0ba2b570` | Research rebuild failures with two prior units | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `71ef498c` | Research rebuilding two prior edits | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `385aa677` | Research persistence after history reconstruction | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `400eac92` | Research journaled prior insertion recovery | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `53c28d7c` | Research rebuilding prior text undo | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `e8e44d8e` | Research late Redo compensation and prior Undo | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `16baf493` | Research rollback during Redo reconstruction | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `fb5bc208` | Research rebuilding failed Split redo | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `e367c921` | Research guarded recovery snapshot replacement | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `e2399adb` | Research detached recovery validation | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `5a7ec9b9` | Probe persistence after late Split redo failure | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `844393ac` | Probe Save after failed Split redo | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `3910c8d8` | Probe recovery after failed Split redo | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `18954d2c` | Probe prior history after failed Split redo | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `93dd57d4` | Research failed Redo unit recovery | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `8b45b885` | Research redo unit failures in split history | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `d83b0e88` | Research save without live link mutation | документация/свидетельства, тестовые инструменты |
| `9133fc8b` | Research live link normalization and redo loss | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `bdf65236` | Research analysis snapshot with retained redo | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `2829a72c` | Research save and recovery after undo compensation | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `d1f84368` | Research plan based ID compensation guard | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `ea508689` | Research guarded ID compensation after failed undo | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `84d1fbb2` | Research failed undo during split rollback | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `841050bb` | Research COM attach failure after split ID change | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `ff11be30` | Research dirty split rollback with two redo units | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `b710ad22` | Research BR caret and undo after save | документация/свидетельства, тестовые инструменты |
| `8f5a5925` | Research marked BR serialization and recovery | документация/свидетельства, тестовые инструменты |
| `f83bee60` | Research isolated SendInput with BR tail | документация/свидетельства, тестовые инструменты |
| `5c8190e1` | Research grouped caret recovery | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `bbb4297f` | Research empty split input and undo group | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `b1c36bd4` | Investigate empty split caret services | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `a2e3b0da` | Research split at paragraph start | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `24ee5ada` | Exercise isolated split keyboard input | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `47c3e3fa` | Verify split history after saving | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `ef4d73ef` | Research full split edit history save | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `febe9f47` | Research collapsed selection split staging | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `4002b1ad` | Guard experimental split caret failures | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `cc467c8e` | Research automatic split caret restoration | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `ad4a833b` | Research same-implementation MSHTML staging | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `053899bd` | Investigate MSHTML cross document import semantics | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `d9899be9` | Probe separate document title staging and refusal | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `10cc3a16` | Research live partial title split and recovery | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `6fc6d557` | Probe title selection boundaries in shadow | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `9c15509f` | Research selected title fragments in shadow document | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `e1221238` | Probe rollback with multiple prior redo units | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `7dd1d780` | Research nested split paths and save semantics | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `788a1a76` | Probe split save and recovery with image fixture | документация/свидетельства, тестовые инструменты |
| `b1a6e502` | Test nonbreaking placeholder for empty split tail | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `94eabafa` | Investigate caret and serialization for empty split tail | документация/свидетельства, тестовый код FBE |
| `0123d80b` | Research caret restoration across split history | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `ee337a06` | Trace dirty document history after split failure | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `81d17d54` | Probe savepoint recovery after split failure | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `2aed97fe` | Research split planning and rollback history | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `14541f13` | Probe selection-based split and guard prior redo | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `4215dc6a` | Probe split caret recovery and targeted redo discard | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `34bebb35` | Exercise S2d on guarded desktop and probe failure history | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `f54f8b47` | Probe isolated desktop and reassess S2d caret | документация/свидетельства, тестовые инструменты |
| `91f35bec` | Report split probe caret and focus outcomes separately | документация/свидетельства, тестовые инструменты |
| `70ea75b4` | Exercise Split undo with multiple following nodes | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `68f47268` | Research Split undo with formatted ranges | документация/свидетельства, тестовый код FBE, тестовые инструменты |
| `ec7c29f1` | Complete Split Undo research assessment | документация/свидетельства |
| `772bd74e` | Record production Split redo input failure | документация/свидетельства |
| `dd95d81c` | Trace production Split redo and subsequent input | тестовый код FBE, тестовые инструменты |
| `29bb4bf4` | Record current Split failure redo evidence | документация/свидетельства |
| `2435550e` | Trace current Split failure history | тестовый код FBE |
| `8b84fccd` | Record dangling links after Split failure redo | документация/свидетельства, тестовые инструменты |
| `12d39cdf` | Record clean-state Split failure history | документация/свидетельства, тестовые инструменты |
| `27ddcd13` | Record partial Split failure redo behavior | документация/свидетельства |
| `7f7703f5` | Record controlled production Split failures | документация/свидетельства |
| `4b39f428` | Record unrelated edit S2 history results | документация/свидетельства |
| `d5c25afb` | Record plain typing savepoint control | документация/свидетельства |
| `38780352` | Record S2 saved edit undo failure | документация/свидетельства |
| `0c4f0fd0` | Record S2 clean-start savepoint failure | документация/свидетельства |
| `d87011d4` | Record tree descriptor version transition | документация/свидетельства |
| `af483643` | Record startup HTML comparison | документация/свидетельства |
| `80b24d25` | Record S2 startup version traces | документация/свидетельства |
| `057be066` | Record spelling disabled control | документация/свидетельства |
| `4fa81ad6` | Record document attach version observations | документация/свидетельства |
| `d44c31bf` | Record isolated S2 recovery evidence | документация/свидетельства |
| `9e841a53` | Record initial savepoint version mismatch | документация/свидетельства |
| `66986d19` | Record S2 savepoint and reopen evidence | документация/свидетельства |
| `bc725a94` | Record S2 Windows keyboard input evidence | документация/свидетельства |
| `8e5e74ef` | Record S2 input across undo states | документация/свидетельства |
| `0b0e075c` | Record resumed Split undo experiments | документация/свидетельства, тестовые инструменты |

## Независимая часть второй S2-ветки

Срез: `master` `c40979c39c9b5cac99f485e69560d07c9c8e05d6`, `codex/split-undo-s2-resumption-20261104` `d69266aff446cf0115d103385b05e3f9c56b65c2`. Исключены коммиты, достижимые из `master`, `codex/fbe-undo-architecture-feasibility` (`bcc9435b07f8916e1a23d53d7fb5e47b4a769ede`) и `codex/split-undo-resumption-20261104` (`11317d0636c22b2a375aa675533baef3ce618488`). Остаются следующие 18 непоглощённых немержевых коммитов; исключение общей истории не означает её полной семантической приёмки.

Четыре изменения производственных файлов (`a7beaa6e`, `d6387ecd`, `be9c70e0`, `18181237`) добавляют инструментирование запуска, дерева и состояния документа для S2, а не готовое исправление. Их следует сохранить в исследовательской ветке. Снимок `RuntimeTestIsolation.ps1` в `f7286b15` побайтово совпадает с историческим blob из уже включённого `e2e53fdc`; нынешний файл дополнительно учитывает ограничения диска и уборку, поэтому подмена старым снимком недопустима. Остальные изменения относятся к инструментам и сценариям исследования. Перенос целиком не выполнялся.

| Коммит | Назначение по исходному сообщению | Пути |
|---|---|---|
| `d69266af` | Probe S2 ID transfer and formatted fragment history | тестовый код FBE, тестовые инструменты |
| `91830aa0` | Trace history after controlled Split failure | тестовый код FBE |
| `44fdf8b0` | Probe S2 history with edits in another section | тестовый код FBE, тестовые инструменты |
| `d4599c68` | Compare plain text savepoint without S2 | тестовый код FBE, тестовые инструменты |
| `3ca21788` | Probe S2 edits after saved state | тестовый код FBE, тестовые инструменты |
| `50625862` | Test S2 savepoint without startup tree script | тестовые инструменты |
| `18181237` | Trace S2 tree descriptor initialization | инструментирование редактора |
| `be9c70e0` | Compare S2 startup HTML at version transition | инструментирование редактора |
| `d6387ecd` | Trace S2 startup version checkpoints | тестовый код FBE, инструментирование редактора |
| `c9d7f52a` | Compare S2 startup with spelling disabled | тестовые инструменты |
| `a7beaa6e` | Trace S2 document attach version transitions | инструментирование редактора |
| `6e5721dc` | Exercise S2 recovery in isolated profile | тестовый код FBE, тестовые инструменты |
| `4266cc96` | Trace initial S2 savepoint version | тестовый код FBE |
| `3453234f` | Observe S2 savepoint and reopen state | тестовый код FBE, тестовые инструменты |
| `5d608fca` | Exercise mixed S2 history through Windows input | тестовый код FBE, тестовые инструменты |
| `f7286b15` | Research S2 keyboard input route | тестовый код FBE, тестовые инструменты |
| `071b4f68` | Exercise S2 input in additional undo states | тестовый код FBE, тестовые инструменты |
| `28125135` | Research S2 caret navigation and mixed undo history | тестовый код FBE, тестовые инструменты |
