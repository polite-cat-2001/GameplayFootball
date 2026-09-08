# Handoff: перевод матчинга данных на tm-id (Слой 2 стабильных id)

Дата: 2026-09-09. Одноразовый handoff-пакет для новой сессии. Не редактировать.
Свежее состояние проекта — вики игры, начинать с `docs/wiki/index.md`.

---

## Промпт для новой сессии

Задача: перевести весь конвейер данных на **матчинг по id**, а не по имени.

### Контекст

Единый договор пайплайна — `../GameplayFootball/docs/wiki/пайплайн-данных.md` (в репо
GameplayFootball). Текущая лента:

```
transfermarkt-api (живой скрейпинг TM, эндпоинты по TM-id)
  → transfermarkt_scrapper → data/full/{clubs,national_teams}.json
      (все сущности уже keyed по TM-id: игрок, клуб, лига (GB1/ARM1...), страна, сборная)
  → ratings-generator   (выход keyed по TM-id игрока)
  → kit-generator       (specs keyed по club id, логотипы <id>.png)
  → tm-gf-face-generator(specs keyed по TM-id игрока)
  → tm-gf-import        → БД игры database.sqlite
  → GameplayFootball    (читает БД по внутренним rowid)
```

Проблема: **БД игры не хранит TM-id**, поэтому все матчинги в патч-скриптах идут **по имени**,
а имена ненадёжны. Реальные случаи-эталоны (что уже случилось):
- фантом «Roberto Owono» (GK, Экв. Гвинея) — был в старом срезе TM, сейчас не существует;
  по имени я спутал его с Jesús Owono (tm 631693) — это разные записи;
- спеллинг «Soulisack/Soulisak Souvankham» — один игрок (tm 1427661), TM поменял написание;
- риск коллизий имён (несколько «Henderson» в одной сборной и т.п.).

**Правило на всю задачу: поиск/сравнение игрока/клуба/лиги/сборной делать только по id в
данной категории (TM-id). Имя — только для отображения и логов, никогда для матчинга.**

### Основной объём (tm-gf-import, репо `Desktop/projects/tm-gf-import`)

1. **Схема БД v2** (`schema.py`): добавить колонки `tm_id` в `players`, `teams`, `leagues`,
   `countries` (источник — TM-id из `data/full/*.json`). Внутренние rowid остаются ключами БД,
   `tm_id` — внешний стабильный ключ.
2. `builders/*`: при вставке записывать `tm_id` (игрок — `player["id"]`, клуб — `club["id"]`,
   лига — `league["id"]`, страна — `country["id"]`; сборные — `team["id"]`).
3. `patch_lineups.py` и `patch_nt_stats.py`: переписать матчинг на `tm_id` (сейчас — по имени
   внутри сборной). Имя оставить только для человекочитаемых логов.
4. **Пересобрать БД полностью** (`import.py --db-only` в staging-каталог, бэкап текущей,
   проверка, затем подмена `data/databases/default` и `build/Release/databases/default`).
   Текущая БД не хранит tm-id, поэтому без полной пересборки колонки не появятся.
5. В `builders/lineup.py`, `mapping.py` — проверять, что нигде нет name-сравнений сущностей.

### Субагенты по остальным репозиториям

Запустить по одному субагенту на репозиторий. Для каждого: найти все места, где сущность
ищется/сравнивается **по имени** (grep по `.get("name")`, `name ==`, `.startswith(name)`,
сравнение строк в матчинге/дедупе/слиянии), и заменить на **id-матчинг**, где это матчинг
данных. НЕ трогать поиск как пользовательскую фичу (например `/search/{name}` в API) — это
вход пользователя, не матчинг данных.

- `transfermarkt_scrapper` (`Desktop/projects/transfermarkt_scrapper`, ветка `scraper-v2`):
  `build_pilot_json.py`, `topup.py`, `fetch/*` — проверить, что слияние/индексация идёт по id,
  не по имени; `data/league_id_map.json` — карта id лиг (структура ↔ TM), не по именам.
- `ratings-generator` (`Desktop/projects/ratings-generator`): `generate.py` keyed по TM-id
  игрока; проверить `build_club_strength`, `build_league_economy` — не появилось ли name-сравнений
  клубов/лиг.
- `kit-generator` (`Desktop/projects/kit-generator`): `kits/palette.py`, `export_game.py`,
  `generate_kits.py` — клубы keyed по club id; НО `kits/linked.py` (подбор родителя молодёжной
  команды) — осознанно name-based (имена клубов, не id): НЕ переделывать без отдельного обсуждения
  (это линковка «Under 18 → основной клуб» по имени).
- `tm-gf-face-generator` (`Desktop/projects/tm-gf-face-generator`): keyed по TM-id игрока;
  проверить `faces/data.py`.
- `transfermarkt-api` (`Desktop/projects/transfermarkt-api`): эндпоинты по TM-id;
  `/search/{name}` — пользовательская фича, не трогать.
- `GameplayFootball` (`Desktop/projects/GameplayFootball`): код игры читает БД по внутренним
  rowid — менять не нужно; колонки `tm_id` — метаданные для внешнего матчинга (сеть/карьера/
  апдейты данных). Убедиться, что лишние колонки не ломают SELECT игры (они не мешают — игра
  выбирает конкретные колонки).

### Верификация (обязательно)

- После пересборки БД: у каждой строки `players/teams/leagues/countries` заполнен `tm_id`;
  у сборных — совпадает с `national_teams.json`.
- Эталоны из прошлого бага:
  - Англия (NT): Кейн на CF (индекс 10 в `nationalteamformationorder`), полный 4-2-3-1;
  - Экваториальная Гвинея: ровно 1 GK «Jesús Owono» (tm 631693), НЕТ «Roberto Owono»;
  - Лаос: «Soulisak Souvankham» (tm 1427661), без дублей;
  - пустых `profile_xml` у игроков сборных — 0.
- Сборка игры проходит (`cmake --build`), обычный матч и матч сборной не падают.
- Детерминизм-эталоны, если затронуты — перебазировать (расстановки уже менялись).

### Ограничения

- Не ломать `football_collection` (конвертер канона в Flutter-assets, отдельный репо вне списка).
- Обновить вики после правок: `пайплайн-данных.md`, `данные-из-transfermarkt.md`,
  `база-данных.md`, `открытые-вопросы.md` (страницы игры); ролевые AGENTS-секции репо — если
  меняется роль/формат.
- Коммиты: английский subject, по 1–2 логических на репо; БД и `data/` в git не коммитить.
- Правки в `google_brain`/`windows` ветки игры не вливать.

### Что НЕ в этой задаче (отложено)

- Плоский canon (`data/canon/`) как единый контракт — отдельный милстоун data v2.
- Версионирование данных (manifest, бандлы вне git) — отдельно.
- Пакеты лиг (base + DLC), сеть (LAN), карьера.