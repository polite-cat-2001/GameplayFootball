# Handoff: LAN — развязка gamepad-оверлея и единый экран выбора сторон

Дата: 2026-09-11. Одноразовый пакет для свежей сессии; устаревает по природе.
Состояние на конец прошлой сессии: ветка `lan`, `origin/lan` @ `c8d30c5`.

## Промпт для новой сессии (скопировать целиком)

> Продолжаем LAN-матч в репозитории GameplayFootball, ветка `lan` (от `squads-update`).
> Модель — host-authoritative, тонкий клиент: клиент не симулирует, а проигрывает
> снапшоты; лобби зеркальное.
>
> Сначала прочитай:
> - `docs/specs/2026-09-11-lan-match-design.md` — дизайн (снимок);
> - `docs/plans/2026-09-11-lan-match.md` — план задач;
> - `docs/wiki/сеть.md` — текущее состояние подсистемы;
> - хвост `log.md` — что уже сделано.
>
> **Уже сделано:** Task 1–7 целиком (транспорт/handshake/сериализация, зеркальное
> лобби, remote-презентация снапшотами, сетевой ввод, fairness input-delay, пауза,
> голосование за продолжение, дисконнект/join, живая смена сторон, хост-опции
> паузы). Архитектурно: сетевая логика матча вынесена из `GameTask` в
> `src/net/netmatchsession.*` (`NetMatchSession` + `e_NetMatchPhaseState`),
> оверлей выбора сторон открывает меню-слой (`MenuTask::UpdateNetworkOverlay`).
> Есть автотесты: `tools/nettest` (протокол, 43 проверки) и `tools/lanmatchtest`
> (host-интеграция, 26 проверок).
>
> **Задача на сессию — два пункта:**
>
> 1. **Убрать последний GUI-вызов из `GameTask`.** В `GameTask::ProcessPhase`
>    есть блок «gamepad-missing» (`src/gametask.cpp`, ~строки 170–211): при
>    отключении геймпада в матче он ставит паузу и создаёт `ControllerSelectPage`
>    через page factory. Перенести создание оверлея в меню-слой (аналогично
>    `MenuTask::UpdateNetworkOverlay`). Учесть тайминг: `RefreshGamepads()` зовётся
>    в `GameTask::ProcessPhase`, а `MenuTask::Process` идёт **до** `GameTask::Process`
>    (см. `src/main.cpp`, регистрация `gameSequence`), т.е. в меню видно состояние
>    на кадр раньше — допустимо (проверка всё равно rate-limited 1 с), либо вынести
>    флаг «геймпады изменились» и/или сам рескан. `GameTask` после этого не должен
>    трогать окно/фабрику страниц вообще.
>
> 2. **Объединить два интерфейса выбора сторон в один экран с бэкендами.**
>    Сейчас есть локальный `ControllerSelectPage` (`src/menu/controllerselect.*`:
>    участники — локальные `IHIDevice`, состояние — `SideSelection`, применяется
>    `MenuTask::SetControllerSetup` + `Match::UpdateControllerSetup`) и сетевой
>    `NetworkLobbyPage` (`src/menu/network/networklobby.*`: участники — пиры
>    `NetLobbyPlayer`, состояние — `NetLobbyState`, применяется хостом через
>    `Team::AddHumanGamer`/`NetMatchSession::SetupControllers`). Нужно сделать один
>    `SideSelectPage` поверх интерфейса-бэкенда:
>    - `Participant { kind: LocalDevice|RemotePeer, id, label, device, side, ready }`;
>    - `SideSelectBackend { GetParticipants(); SetSide(); SetReady(); CanControl();
>      IsHost(); Commit(); }`;
>    - `LocalSideSelectBackend` поверх `GetControllers()`/`SetControllerSetup`;
>    - `NetworkSideSelectBackend` поверх `LobbyState`/lobby-actions.
>    Бонус цели: поддержать смешанный режим (хост + локальные игроки + удалённые
>    пиры), которого сейчас нет ни в одном UI. Фазу выбора **команд** (`Teams`)
>    сетевого лобби трогать не обязательно — можно оставить в `NetworkLobbyPage`
>    или тоже вынести, но не смешивать с выбором сторон.
>
> Поведенческие ожидания (сохранить!): выбор сторон в матче — общий и требует
> подтверждения всех; выход из выбора сторон возвращает в пауза-меню (матч остаётся
> на паузе); возобновление матча — отдельное голосование Continue (`X/N`).
>
> Проверка после изменений (обязательно):
> - сборка: `cmake --build build --config Release --parallel` (перед сборкой закрыть
>   запущенные `gameplayfootball.exe`, иначе LNK1104);
> - из `build\Release`: `.\nettest.exe` → `PASS`; `.\lanmatchtest.exe` → `PASS`;
> - из `build\Release`: `.\determinism_runner.exe check 7134def2c0863d4978bb18742b1f173358e4bf66`
>   → exit 0 (не должен сдвинуться).
>
> Ручной тест (два инстанса `build\Release\gameplayfootball.exe`, рабочий каталог
> `build\Release`): Network → Host game (порт 27015), второй Join
> (`127.0.0.1:27015`); клавиатуру получает только окно в фокусе. Проверить: матч,
> пауза, выбор сторон у обоих, выход из выбора сторон в паузу, Continue-счётчик,
> дисконнект/join, персональные камера/визуал у клиента, локальный матч (в паузе
> «controller select», не сетевой лобби).
>
> Правила проекта: не коммитить без явной просьбы; после содержательной правки
> обновлять `docs/wiki/` (не создавать датированный документ); код/комментарии на
> английском, вики на русском; константы — в `src/gamedefines.hpp`/
> `docs/wiki/константы.md`; тик симуляции 10 мс, сетевые частоты — в
> `src/net/nettypes.hpp`. Вести `log.md` (append-only).

## Ключевые точки кода

- GUI-развязка:
  - `src/gametask.cpp` `GameTask::ProcessPhase` — gamepad-missing блок (создать
    страницу через `topPage->CreatePage`); `src/gametask.hpp` — `GetNetSession()`.
  - `src/menu/menutask.cpp` `MenuTask::UpdateNetworkOverlay()` — образец того, как
    меню-слой открывает оверлей по состоянию (каждый тик в `ProcessPhase`).
  - `src/main.cpp` (~190–207) — порядок задач `gameSequence`: `MenuTask`
    Get/Process/Put, затем `GameTask` Get/Process.
- Локальный выбор сторон:
  - `src/menu/controllerselect.cpp/.hpp` (`ControllerSelectPage`): `inGame`,
    `resumeOnClose`, `CheckAllConfirmed`, `ExitControllerSelectPage`,
    `GetControllerSetup()`.
  - `src/menu/menutask.hpp` `SideSelection`, `SetControllerSetup`/`GetControllerSetup`.
  - `src/onthepitch/match.cpp` `Match::UpdateControllerSetup` (DeleteHumanGamers +
    AddHumanGamer), `src/onthepitch/team.cpp`.
- Сетевой выбор сторон:
  - `src/menu/network/networklobby.cpp/.hpp` (`NetworkLobbyPage`): фазы Sides/Teams,
    `resumeOnClose`, `GetState`, `SendAction`.
  - `src/net/netmessages.hpp` `NetLobbyState`/`NetLobbyPlayer`/`NetLobbyAction`
    (`SetSide`/`SetReady`/`RequestSideSelect`/`SetResumeReady`/`SetDevice`).
  - `src/net/netmatchsession.cpp` `SetupControllers`/`RebindControllers`,
    `GetState()`; `src/net/netserver.*` `ApplyLobbyAction`, `SetSideSelectMode`.
- Страницы/навигация:
  - `src/utils/gui2/page.cpp` `Gui2Page::GoBack` — уже обновляет
    `mostRecentlyCreatedPage` (фикс UAF). Помни: `PreQuitPage::GoMenu` всё ещё
    удаляет себя без обновления (тот же класс бага).
  - `src/menu/pagefactory.cpp` — `e_PageID_*`, создание страниц.

## Ловушки

- Запущена игра → `gameplayfootball.exe` занят, линковка падает `LNK1104`.

## Что уже сделано (для контекста)

- `src/net/netmatchsession.*` — сетевая логика матча, `e_NetMatchPhaseState`
  (`Playing`/`Paused`/`SideSelect`), выводится из `Match::GetPause()` +
  `LobbyState.sideSelect`.
- Снапшот несёт подпись гол/рефери (`message`/`messageTime_ms`/`messageCounter`) —
  клиент показывает её через `SpamMessage`.
- Гигиена: `NetClient` геттеры возвращают копии под `stateMutex`; сигналы
  `sig_OnHandshake`/`sig_OnLobbyState` удалены; `net_protocolVersion = 2`.
- Тесты: `tools/nettest`, `tools/lanmatchtest` (см. `docs/wiki/сеть.md`, раздел
  «Инструменты»).

## Проверка

```bat
cmake --build build --config Release --parallel
cd build\Release
.\nettest.exe
.\lanmatchtest.exe
.\determinism_runner.exe check 7134def2c0863d4978bb18742b1f173358e4bf66
```
