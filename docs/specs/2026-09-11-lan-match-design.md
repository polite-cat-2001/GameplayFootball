# Матч по локальной сети (LAN): дизайн

Дата: 2026-09-11. Тип: spec (дизайн-документ). Статус: утверждён в обсуждении,
готов к плану реализации. Неизменяемый снимок — правки вести в вики.

Ветка: `lan` (отпочкована от `squads-update`).

## Контекст и цель

Новый режим: матч по локальной сети (Hamachi/Radmin и подобные виртуальные LAN),
вход в матч по IP + порту, как в Minecraft. Матч идёт на компьютере хоста с его
командами/игроками. Хост не должен получать преимущества из-за пинга клиента.
Режим закладывается так, чтобы позже встроить его в карьеру.

Выбрано: **host-authoritative, тонкий клиент**, до N игроков, кросс-платформа
(Win/Linux/macOS), сетевой стек — `boost::asio`.

Опора на существующий код:

- фиксированный тик симуляции 10 мс, не привязанный к стенным часам
  (`Match::Process`, `src/onthepitch/match.cpp:1015`);
- ввод абстрагирован через `IHIDevice` (`src/hid/ihidevice.hpp:57`), контроллеры —
  список в `gamecontext.cpp:88`, привязка человека — `HumanGamer`/`HumanController`;
- рендер-состояние отделено от симуляции фазами Get/Process/PreparePut/FetchPut/Put;
- DB-гейт версии данных (`gamecontext.cpp:234`) — дешёвая проверка совместимости;
- `Gui2EditLine` (`src/utils/gui2/widgets/editline.hpp:14`) — поля IP/порта.

## Почему тонкий клиент, а не lockstep

Кросс-платформенный детерминизм не гарантирован: эталоны `tools/determinism`
различаются по платформам (Win x86/x64, linux, macos-arm64; см.
`docs/wiki/открытые-вопросы.md`). Lockstep требовал бы побитового совпадения
симуляции между разными ОС/сборками. Host-authoritative детерминизм не нужен:
симулирует только хост, клиент — презентация.

## Поток данных рендера

Клиент **не вызывает `Match::Process()`**. Он строит `Match` из `MatchSetup` и
проигрывает снапшоты через тот же Put-пайплайн.

Ключевой шов — поза игрока уже восстанавливается из компактного набора:

- `HumanoidBase::Put` (`humanoidbase.cpp:759`) берёт `fetchedbuf_animApplyBuffer`
  и вызывает `anim->Apply(nodeMap, frameNum, ..., position, orientation, offsets, ...)`;
  структура — `AnimApplyBuffer` (`humanoidbase.hpp:133`). Значит для игрока слать:
  анимацию, `frameNum`, `position`, `orientation`. `offsets` сейчас пусты —
  `CalculateGeomOffsets` заглушен (`humanoidbase.cpp:803`).
- `Ball::Put` (`ball.cpp:597`) применяет `positionBuffer` + `orientationBuffer`.
- Камера считается локально из позиций (`UpdateIngameCamera`), не передаётся.
- Счёт/время/фаза — маленький заголовок; радар и подписи выводятся из позиций.

Снапшот на тик — порядка 0.5 КБ (≈23 игрока × ~20 Б + мяч + заголовок). На
30–50 Гц это десятки КБ/с на клиента.

## MatchSetup (сериализуемое определение матча)

Передаётся хостом клиенту, чтобы клиент построил сцену без своей БД и без
симуляции. Два уровня детализации:

- **Same-data** (v1, обычный матч): у обоих одинаковая `data/` и `database.sqlite`,
  передаются только `teamID`/`playerID`, совместимость — по DB-гейту.
- **Full-setup** (карьера): сериализованные `TeamData`/`PlayerData` по стабильным
  `tm_id` (см. `docs/wiki/пайплайн-данных.md`).

Состав `MatchSetup`:

- handshake: protocol version, build hash, data version, хеш набора анимаций;
- опции матча: `matchDurationFactor`, `matchDifficulty`, стадион, погода (params
  солнца — чтобы картинка совпадала), формы;
- обе команды: `TeamData` (имя, цвета, киты, formation/tactics XML) + `PlayerData`;
- назначение сторон/контроллеров на пиров.

Не передаётся: 3D-модели, анимации, текстуры — это общие ассеты из `data/`.
Кит — ссылка на ассет. Кастомные киты карьеры — вне scope v1.

Реализация: десериализующие конструкторы `MatchData(NetMatchSetup)` → `TeamData`
→ `PlayerData` вместо запросов к sqlite (`matchdata.cpp:7`, `teamdata.cpp:59`,
`playerdata.cpp:13`). Анимации — по имени (`Animation::GetName()`): на handshake
хост шлёт таблицу имён, клиент строит `name→Animation*` и дальше шлёт `animID`.

## Протокол

Топология — звезда: хост авторитет, ретранслирует состояние всем N клиентам.

- **TCP** — control: handshake, лобби, `MatchSetup`, надёжные события (гол/свисток/
  карточка/фаза/пауза/дисконнект/результат).
- **UDP** — realtime: ввод и снапшоты, sequence + дублирование, интерполяция.

Сообщения:

- `C→H ClientHello{proto, build, dataVer, animHash, name}`
- `H→C ServerHello{accept|reject, sessionId, LobbyState}`
- `C→H LobbyAction{setSide | setReady | moveTeamCursor | commitTeam}`
- `H→C LobbyState{...}` (authoritative, c `revision`)
- `H→C MatchSetup{...}`
- `C→H SetupAck / Progress`
- `C→H InputFrame{tick, buttons[], direction, seq}` (UDP)
- `H→C Snapshot{...}` (UDP)
- `H↔C PauseRequest / PauseState`, `Keepalive/Ping`

Сид/RNG синхронизировать не нужно — симулирует только хост.

## Зеркальное лобби

Лобби — одно каноническое состояние на хосте, которое все рисуют одинаково.
Любая правка сразу видна всем: смена стороны, листание списка команд и т.д.

```
LobbyState {
  revision
  phase: SIDES | TEAMS
  players[]: { id, name, side(Home|Away|Spectator), ready, ping, isHost }
  teamId[Home], teamId[Away]
  chooser[Home], chooser[Away]        // владелец выбора команды стороны
  teamCursor[Home], teamCursor[Away]  // живой курсор/подсветка
  listScroll[Home], listScroll[Away]  // позиция списка (зеркальная прокрутка)
}
```

- Клиенты шлют `LobbyAction`, хост применяет и рассылает `LobbyState` с новым
  `revision`. Предсказания нет: ввод → хост → рассылка, задержка ~RTT.
- В лобби **паузы нет**: можно либо выбрать стороны и идти дальше, либо выйти
  назад в меню.
- Хост «Назад» в `noMatch`-контексте: закрыть порт, всех в главное меню,
  матч отменён.

## Стороны, команды, зрители

- Стороны: Home, Away, Spectator. Команду стороны задаёт её chooser: `Home` —
  хост, `Away` — первый человек, вставший на противоположную сторону.
- Конфликт команд невозможен: вторую команду выбирает только первый на стороне.
- Если противоположная сторона пуста — хост выбирает обе команды и играет
  против AI.
- Зритель (Spectator) строит ту же сцену без привязки ввода.
- Если chooser стороны отключился — владение переходит следующему человеку на
  стороне, иначе (людей не осталось) сторона управляется AI.

## Машины состояний

### Хост

| Состояние | Контекст | Действия | Переходы |
|---|---|---|---|
| `H_LOBBY_SIDES` | `noMatch` / `paused` | Порт открыт; игроки подключаются, выбирают Home/Away/Spectator и Ready | `noMatch`: Назад → закрыть порт, всех в меню, матч отменён. `paused`: Назад → `H_PAUSED`. Все Ready → `H_LOBBY_TEAMS` (`noMatch`) / применить и `H_MATCH` (`paused`) |
| `H_LOBBY_TEAMS` | только `noMatch` | chooser[Home] и chooser[Away] выбирают команды; live-курсоры зеркалятся | Все Ready → `H_LOADING` |
| `H_LOADING` | — | Рассылка `MatchSetup`, все строят сцену | готово → `H_MATCH` |
| `H_MATCH` | — | Авторитетная симуляция, ввод, снапшоты, host input-delay | пауза (любой пир) → `H_PAUSED`; конец → `H_POST_MATCH` |
| `H_PAUSED` | — | In-game меню: Продолжить, Выбор сторон, Выйти в лобби; хост может менять формы, погоду, сложность AI | Продолжить → `H_MATCH`; Выбор сторон → `H_LOBBY_SIDES(paused)`; Выйти в лобби → завершить матч → `H_LOBBY_SIDES(noMatch)` |
| `H_POST_MATCH` | — | Результат | Реванш / `H_LOBBY_SIDES(noMatch)` |
| `H_ERROR` | — | Ошибки (bind fail, version mismatch, потеря пира) | → `H_LOBBY_SIDES(noMatch)` / меню |

### Клиент

| Состояние | Действия | Переходы |
|---|---|---|
| `C_CONNECT_INPUT` | IP + порт + имя | → `C_CONNECTING` |
| `C_CONNECTING` | Отмена | таймаут → `C_ERROR`; ок → `C_HANDSHAKE` |
| `C_HANDSHAKE` | — | mismatch → `C_ERROR`; ок → `C_LOBBY_SIDES` |
| `C_LOBBY_SIDES` (ctx) | Сторона, Ready, Выйти | Ready → `C_LOBBY_TEAMS` (`noMatch`) / `C_MATCH` (`paused`); Выйти → меню |
| `C_LOBBY_TEAMS` (ctx, только `noMatch`) | Выбор команды (если chooser), live-курсор, Ready | все Ready → `C_LOADING` |
| `C_LOADING` | Получает `MatchSetup`, строит `Match` | готово → `C_MATCH`; ошибка → `C_ERROR` |
| `C_MATCH` | Шлёт ввод, принимает снапшоты, интерполяция | пауза → `C_PAUSED`; конец → `C_POST_MATCH` |
| `C_PAUSED` | Продолжить, Выбор сторон (зеркальный лобби) | → `C_MATCH` / `C_LOBBY_SIDES(paused)` |
| `C_POST_MATCH` | Результат | → `C_LOBBY_SIDES(noMatch)` |
| `C_ERROR / C_HOST_LOST` | Ок | → главное меню |

## Пауза

- Пауза **равноправна**: любой пир может поставить и снять её. Хост — арбитр-
  ретранслятор, применяет `Match::Pause(bool)` (`match.hpp:130`) и рассылает
  `PauseState`.
- Экран паузы: Продолжить, Выбор сторон, Выйти в лобби. Хост дополнительно может
  менять формы, погоду и сложность AI.
- Выбор сторон в паузе применяется **живой перепривязкой**:
  `Match::UpdateControllerSetup()` (`match.cpp:587`) → `Team::DeleteHumanGamers()`
  (`team.cpp:173`) + `Team::AddHumanGamer` (`team.cpp:162`). Освободившуюся команду
  берёт AI. Рестарт матча не нужен.
- **Смена клуба в паузе заблокирована** (поля команд read-only): составы
  создаются в `Team::InitPlayers` при сборке `Match`. Сменить клуб можно только
  через «Выйти в лобби» → новый матч.
- «Выйти в лобби» = завершение матча и переход к выбору сторон в той
  конфигурации сторон, что была выбрана на этот матч.

## Дисконнект, реконнект, подключение во время матча

- Дисконнект во время матча обрабатывается как отключение геймпада
  (`gametask.cpp:139-180`): хост ставит `Match::Pause(true)` и открывает выбор
  сторон поверх матча («resume on close»). Остальные переназначают стороны,
  освободившуюся команду добивает AI; все Ready → `UpdateControllerSetup()` →
  продолжить.
- **Подключение во время матча**: новичок коннектится → матч ставится на паузу
  и открывается выбор сторон, чтобы он выбрал сторону. Далее как выше.
- **Реконнект поддерживается**, но владение стороной не возвращается:
  переподключившийся считается новым игроком.

## Host input-delay (fairness)

Клиент: нажатие → L (до хоста) → симуляция → L (снапшот обратно) → +B
интерполяция = RTT + B до видимого результата. Хост при немедленном вводе видит
свой результат почти мгновенно. Правило:

```
hostInputDelay = maxClientRTT + интерполяционный буфер B
```

Реализация — `DelayedHIDDevice : IHIDevice` поверх локального устройства (сэмпл
раз в тик, отдаётся кадр возрастом D). Клиентское предсказание в v1 не делаем —
иначе преимущество получит клиент; при добавлении включать симметрично всем.

## Вне scope

- Клиентское предсказание/rollback.
- Интернет-матчмейкинг, NAT traversal (только прямая LAN/Hamachi/Radmin).
- Передача текстур/моделей и кастомных китов карьеры.
- Управление камерой зрителем (зритель смотрит штатную камеру).
- Возврат владения стороной при реконнекте.

## Открытые вопросы

- Точный порог `hostInputDelay` и глубина интерполяции (эмпирически).
- Формат дельта-сжатия снапшотов (v1 — полные).
- Частота рассылки снапшотов (30–50 Гц) и дублирования UDP.
- Судьба матча при отключении хоста у клиентов (сейчас — `C_HOST_LOST`).

## Заметки по реализации

- Новый модуль `src/net/`: `nettypes`, `netbuffer`, `netserver`, `netclient`,
  `nethiddevice`, `matchsnapshot` (capture/apply), интеграция в `GameTask`.
- Сетевой I/O — отдельный `boost::thread`/io_context, обмен через mutex-очереди
  (принято в проекте).
- Меню: новые значения `e_PageID` (`pagefactory.hpp:12`), страницы рядом с
  `SettingsPage`, кейсы в `PageFactory::CreatePage` (`pagefactory.cpp:33`), файлы
  в `menulib` (`CMakeLists.txt`).
- Remote-режим `Match`: публичный сеттер `animApplyBuffer`, метод
  `Match::ApplyRemoteSnapshot(...)`, обход мёртвого состояния в
  `Player::PreparePutBuffers` (`player.cpp:353`).
- `boost::asio` уже доступен через boost; на Windows добить линковку `ws2_32`.

## План по фазам

1. Скелет `src/net` + handshake (version/build/data/anims).
2. Лобби-UI: host/join, IP+порт, зеркальные стороны/команды.
3. Снапшот: capture на хосте → apply на клиенте (визуально одинаковый матч).
4. `NetHIDDevice` + ввод клиентов + назначение сторон.
5. Host input-delay, интерполяция, пауза, дисконнект/реконнект.
6. Карьера: `MatchSetup` full-setup по `tm_id`, репорт результата.
