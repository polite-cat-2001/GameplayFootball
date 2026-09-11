# Handoff: LAN-матч, Task 7 (fairness, пауза-опции, дисконнект) — новая сессия

Дата: 2026-09-11. Одноразовый пакет для свежей сессии; устаревает по природе.

## Промпт для новой сессии (скопировать целиком)

> Продолжаем LAN-матч в репозитории GameplayFootball, ветка `lan` (от `squads-update`).
> Модель — host-authoritative, тонкий клиент: клиент не симулирует, а проигрывает
> снапшоты; лобби зеркальное.
>
> Сначала прочитай (в этом порядке):
> - `docs/specs/2026-09-11-lan-match-design.md` — полный дизайн (неизменяемый снимок);
> - `docs/plans/2026-09-11-lan-match.md` — план задач 1–8;
> - `docs/wiki/сеть.md` — текущее состояние подсистемы;
> - хвост `log.md` — что уже сделано.
>
> **Сделаны Task 1–6 и часть Task 7.** Именно:
> - транспорт (boost::asio, TCP control), handshake (protocol/build/data/animation хеши),
>   сериализация `TeamDataRaw`/`PlayerDataRaw`, зеркальное лобби с UI;
> - старт матча из лобби (`NetMatchSetup`), remote-презентация: снапшоты рендер-состояния
>   (позы из `animApplyBuffer`, мяч, судьи, заголовок), таблица анимаций;
> - сетевой ввод (`NetHIDDevice`), владелец игрока (`ownerId`) для подсветки/подписей;
> - **равноправная пауза**: `PauseRequest`/`PauseState`, пауза-меню открывается/закрывается
>   у всех (`Match::GetPauseMenuRequested`);
> - **камера считается хостом** и передаётся снапшотом (общий ракурс, слежение за мячом);
> - **синхронизация гол-повторов**: `goalScored`/`goalScoredTimer` в снапшоте, клиент пишет
>   свои кадры повтора, `ReplayStop` закрывает повтор у всех;
> - снапшоты/ввод идут по TCP на 100 Гц (`net_snapshotRate_hz`, `net_inputRate_hz`).
>   Полевые тесты двух инстансов пройдены: матч, управление, пауза, повторы, камера — ок.
>
> **Задача на сессию — оставшийся Task 7** (и, если успеешь, Task 8):
> 1. **Fairness / host input-delay:** оценка односторонней задержки `U` из ping;
>    `DelayedHIDDevice : IHIDevice` поверх локального устройства хоста; клиент
>    задерживает свой ввод на `2U - u_i`, хост — на `2U`. Правило: `hostInputDelay =
>    maxClientRTT + интерполяционный буфер B`. Сейчас ввод по TCP с заметной задержкой.
> 2. **Интерполяция снапшотов** (`B`): плавный рендер между снапшотами (сейчас удержание
>    последнего; при 100 Гц ок, но нужно для UDP/потерь). Решить, делать ли UDP-канал
>    ввода/снапшотов (сейчас всё по TCP; в спеке UDP для realtime).
> 3. **Пауза: смена сторон/команд живьём** через `Match::UpdateControllerSetup()` +
>    `Team::DeleteHumanGamers`/`AddHumanGamer`; хост-опции паузы (формы, погода, сложность
>    AI). Смена клуба в паузе заблокирована (составы создаются в `Team::InitPlayers`).
> 4. **Дисконнект/реконнект/join во время матча** → `Match::Pause(true)` + экран выбора
>    сторон поверх матча (образец — обработка отключения геймпада, `gametask.cpp`
>    gamepad-missing блок). Keepalive 500 мс, таймаут без пакетов 5 с = отключён.
>    Реконнект поддержан, но владение стороной не возвращается (новый игрок).
> 5. **Task 8 (позже):** карьера — full-setup по `tm_id`, стрим кастомных китов, репорт
>    результата в сейв хоста.
>
> Проверка после изменений (обязательно):
> - сборка: `cmake --build build --config Release --parallel`;
> - smoke сети: `build\Release\nettest.exe` → `PASS`;
> - детерминизм: из `build\Release` — `.\determinism_runner.exe check 7134def2c0863d4978bb18742b1f173358e4bf66`
>   → exit 0 (не должен сдвинуться).
>
> Ручной тест: два инстанса `build\Release\gameplayfootball.exe` (рабочий каталог —
> `build\Release`), в одном Network → Host game (порт 27015), во втором Join game
> (`127.0.0.1:27015`). Клавиатуру получает только окно в фокусе.
>
> Правила проекта: не коммитить без явной просьбы; после содержательной правки обновлять
> `docs/wiki/` (не создавать датированный документ); код/комментарии на английском, вики
> на русском; константы — в `src/gamedefines.hpp`/`docs/wiki/константы.md`. Тик
> симуляции 10 мс, сетевые частоты — в `src/net/nettypes.hpp`. Веди `log.md` (append-only).

## Что уже сделано (Task 5–6 + часть Task 7)

- Транспорт/handshake/сериализация данных/зеркальное лобби — см. `docs/wiki/сеть.md`.
- Старт матча: `NetworkLobbyPage::StartHostMatch` → `NetMatchSetup` + `LoadingMatchPage`;
  клиент строит `MatchData` из своей БД (одинаковый `data_hash`).
- `src/net/matchsnapshot.*` (компилируется в `gamelib`): захват/применение снапшота.
  Сущности — `team`/`slot`, не глобальный `PlayerBase::id`.
- Remote-режим `Match` (`remotePresentation`): `HumanoidBase::SetRemotePose`,
  `Ball::SetRemoteState`, `Match::ApplyRemoteSnapshot`; `CalculateGeomOffsets` не зовётся.
- `src/net/nethiddevice.*`: ввод хоста из InputFrame; владелец (`ownerId`) в снапшоте;
  подсветка/подписи локальные (`Player::PreparePutBuffers`).
- Пауза: `Match::Pause` сетевой, `pauseMenuRequested`; `GamePage`/`IngamePage`.
- Камера: хост кладёт `camera*` в снапшот, клиент применяет (при `autoUpdateIngameCamera`).
- Гол-повторы: `goalScored`/`goalScoredTimer` в снапшоте, `CaptureReplayFrame` у всех,
  `ReplayStop` через `NetServer`/`NetClient`, обработка в `ReplayPage`.
- `MatchEnvironment` (солнце) зеркалится.
- Частоты: `net_snapshotRate_hz = net_inputRate_hz = 100`.

## Ключевые точки кода

- Транспорт: `src/net/netserver.*`, `netclient.*`, `netbuffer.*`, `netmessages.*`,
  `netdata.*`, `netassets.*`, `nethiddevice.*`.
- Снапшот: `src/net/matchsnapshot.*`; шов рендера — `HumanoidBase::Put`,
  `AnimApplyBuffer`, `Ball::Put`.
- Remote-режим `Match`: `ApplyRemoteSnapshot`/`ResolveRemoteAnimTable`/`CaptureRemoteSnapshot`
  (`src/onthepitch/match.cpp`), `Match::Pause`, `UpdateIngameCameraStartEffect`.
- Ввод/биндинг: `GameTask::SetupNetworkControllers`, `FindLocalDevice`,
  `GetLocalDeviceType` (`src/gametask.cpp`), `Team::AddHumanGamer`/`GetControllingPeerId`.
- Пауза-меню: `src/menu/ingame/gamepage.cpp`, `ingame.cpp`.
- Реплеи: `src/menu/ingame/replaymenu.cpp` (`Process`/`OnClose`).
- Сессия живёт в `MenuTask` (`GetNetServer`/`GetNetClient`), `src/menu/menutask.hpp`.

## Ловушки

- Windows: `winsock2.h` до `windows.h` (asio иначе падает). CMake — `Win32`/`x86-windows`.
- Запуск — из каталога сборки с копией `data/` (POST_BUILD копирует).
- **Не вызывать `CalculateGeomOffsets()` и прочий simulation-derived код на клиенте** —
  `currentMentalImage`/`mentalImages` там нет (это был краш `GetBallPrediction`).
- Клиентская камера теперь host-driven; в реплее — локальная (`autoUpdateIngameCamera`).
- Снапшот-формат меняется — при правках сохранять симметрию Write/Read (`matchsnapshot.cpp`)
  и `netmessages.*`.
- Диагностика из прошлой сессии удалена (crash-хендлер, `netdiag_*`, `/MAP`) — не возвращай
  без нужды; для отладки можно временно вернуть crash-хендлер с RVA.
- `playerCount` (`PlayerBase`) процесс-глобален — поэтому в сети адресация по `team`/`slot`,
  а не по id.
