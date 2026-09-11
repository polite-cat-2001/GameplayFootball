# Handoff: LAN-матч (ветка `lan`) — продолжение в новой сессии

Дата: 2026-09-11. Одноразовый пакет для свежей сессии; устаревает по природе.

## Промпт для новой сессии (скопировать целиком)

> Продолжаем разработку матча по локальной сети в репозитории GameplayFootball, ветка `lan`
> (отпочкована от `squads-update`). Модель — host-authoritative, тонкий клиент; клиент не
> симулирует, а проигрывает снапшоты рендер-состояния; лобби зеркальное.
>
> Сначала прочитай (в этом порядке):
> - `docs/specs/2026-09-11-lan-match-design.md` — полный дизайн (неизменяемый снимок);
> - `docs/plans/2026-09-11-lan-match.md` — план задач 1–7;
> - `docs/wiki/сеть.md` — текущее состояние подсистемы;
> - хвост `log.md` — что уже сделано.
>
> Сделаны задачи 1–4: `src/net` (boost::asio, TCP control), handshake (protocol/build/data/
> animation хеши), сериализация `TeamDataRaw`/`PlayerDataRaw` + `QueryTeamCatalog`,
> зеркальное лобби с UI (host/join, экран выбора сторон как `ControllerSelectPage`,
> фаза команд страна→турнир→команда как `TeamSelectPage`, кнопки Ready, live-зеркалирование,
> определение устройства ввода, ограничение ввода выбранным устройством, сброс в лобби при
> потере устройства/выходе игрока). **Старт матча ещё не реализован** — после Ready ничего
> не происходит; это Task 5.
>
> Задача на сессию — **Task 5: remote-режим `Match` + снапшоты рендер-состояния** (и, если
> успеешь, начало Task 6 — ввод). Конкретно:
> 1. Таблица анимаций на handshake: хост шлёт имена `AnimCollection`, клиент строит
>    `name→Animation*`, дальше `animID`.
> 2. Remote-режим `Match`: публичный сеттер `animApplyBuffer` в `HumanoidBase`, метод
>    `Match::ApplyRemoteSnapshot(...)`, клиент не вызывает `Match::Process`, а применяет
>    позы игроков (`animApplyBuffer`: анимация + frameNum + position + orientation) и мяч
>    (`positionBuffer`/`orientationBuffer`) + заголовок (время/счёт/фаза), затем штатный
>    `PreparePutBuffers/FetchPutBuffers/Put`.
> 3. Старт: когда обе стороны Ready (`LobbyState.teamReady`) и команды выбраны — хост
>    стартует матч на своих командах, клиент строит `Match` из той же БД (handshake уже
>    требует одинаковый `data_hash`, т.е. в v1 БД идентична) и переходит в remote-режим.
> 4. Не забыть guards: `Player::PreparePutBuffers` не должен зависеть от мёртвого
>    possession-состояния на клиенте; реплеи/неттинг на клиенте выключить.
>
> Проверка после изменений (обязательно):
> - сборка: `cmake --build build --config Release --parallel`;
> - smoke сети: запустить `build\Release\nettest.exe` → `PASS`;
> - детерминизм: из `build\Release` выполнить
>   `.\determinism_runner.exe check 7134def2c0863d4978bb18742b1f173358e4bf66` → exit 0
>   (эталон не должен сдвинуться).
>
> Ручной тест: запустить два инстанса `build\Release\gameplayfootball.exe` (рабочий каталог —
> `build\Release`, там копия `data/`), в одном Network → Host game (порт 27015), во втором
> Join game (127.0.0.1:27015). Клавиатуру получает только окно в фокусе.
>
> Правила проекта: не коммитить без явной просьбы; после содержательной правки обновлять
> `docs/wiki/` (не создавать датированный документ); комментарии/код на английском, вики на
> русском; константы — в `src/gamedefines.hpp`/`docs/wiki/константы.md`. Обновляй прогресс
> в плане/вики и веди `log.md` (append-only).

## Ключевые точки кода

- Транспорт: `src/net/netserver.*`, `src/net/netclient.*`, `src/net/netbuffer.*`,
  `src/net/netmessages.*`, `src/net/netassets.*`, `src/net/netdata.*`.
- Данные: `src/data/playerdata.*`, `src/data/teamdata.*`, `src/data/teamcatalog.*`.
- Лобби-UI: `src/menu/network/network.*`, `src/menu/network/networklobby.*`,
  регистрация в `src/menu/pagefactory.*`, кнопка в `src/menu/mainmenu.cpp`.
- Сессия живёт в `MenuTask` (`GetNetServer`/`GetNetClient`), `src/menu/menutask.hpp`.
- Шов рендера для снапшота: `HumanoidBase::Put` (`humanoidbase.cpp`), `AnimApplyBuffer`
  (`humanoidbase.hpp`), `Ball::Put` (`ball.cpp`).

## Ловушки

- Windows: `winsock2.h` обязан идти до `windows.h` (сделано в `defines.hpp` и файлах с
  прямым `windows.h`); иначе asio падает с «WinSock.h has already been included».
- Запуск — из каталога сборки с копией `data/` (POST_BUILD копирует). GUI-подсистема без
  консоли; для логов собирать с `-DGAMEPLAYFOOTBALL_WINDOWS_SUBSYSTEM=OFF`.
- `net_protocolVersion`/`net_defaultPort`/лимиты — в `src/net/nettypes.hpp`.
- `data_hash` в handshake = sha256 из `databases/default/manifest.json`; поэтому клиент
  использует свою БД для списков команд (стрим каталога — задел под карьеру).
