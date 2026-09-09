# Handoff: плоский canon как единый контракт + бандлы данных (data v2, милстоун 2)

Дата: 2026-09-09. Одноразовый handoff-пакет для новой сессии. Не редактировать.
Свежее состояние проекта — вики игры, начинать с `docs/wiki/index.md`.

---

## Промпт для новой сессии

Задача: сделать **плоский canon единым контрактом** всех звеньев конвейера (вместо вложенного
`data/full/*.json`) и наладить **распространение данных** (бандлы + `data-versions.json`).

### Контекст

Единый договор пайплайна — `docs/wiki/пайплайн-данных.md` (в репо GameplayFootball).
Сделано к этому моменту (2026-09-09): схема БД v2 (колонки `tm_id`), весь матчинг на TM-id,
дедуп дублей клубов перенесён в скрейпер (`build_pilot_json.py` держит один клуб на id в лиге),
`PRAGMA user_version` + `manifest.json` рядом с БД, игра жёстко проверяет схему и логирует
`data_version` на старте. Детерминизм x86 перебазирован (`60380de0...`, см.
[[открытые-вопросы]]); linux/x64/macos-эталоны требуют пересъёмки — **после** стабилизации
данных этой задачей.

Текущая лента (все репо — `Desktop/projects/<имя>`):

```
transfermarkt_scrapper (ветка scraper-v2)
   ├── data/full/clubs.json + national_teams.json   ← прод-формат «v1» (вложенный, причуды)
   ├── data/canon/*.json                            ← плоский canon НЕ в проде (нет на диске)
   ├── data/images/*
   ▼
ratings-generator / kit-generator / tm-gf-face-generator  ← читают data/full/*
   ▼
tm-gf-import → БД (tm_id, user_version=2, manifest.json) → GameplayFootball
```

**Плоский canon** реализован в скрейпере, но не в проде: `build_canon.py` (библиотека, вызывается
из `crawl.py`) + `canon_schema.json`. Вход — «сырые» records fetch-модулей (`club_rosters`,
`club_profiles`, `nt_rosters`, `nt_profiles`, `coach_map`, `coach_profiles`, `player_profiles`,
`market_values`); выход — 7 файлов `data/canon/`: `meta`, `competitions`, `clubs`,
`national_teams`, `players`, `players_market_value`, `coaches`; валидация против `canon_schema.json`.
Один рекорд на игрока: клубная и сборная записи сливаются в один объект
(`club_id` + `national_team_id` + `shirt_number`).

Известные пробелы canon (по [[данные-из-transfermarkt]] и коду):

- `competitions[].logo` — всегда `None` (в `_build_competitions`), нужен URL
  (`LEAGUE_LOGO_URL = "https://img.a.transfermarkt.technology/logo/header/{lid}.png"` есть в `topup.py`);
- `national_teams[].flag` — всегда `None`, и у сборных **нет `colors`** (в `_build_national_teams`
  и в схеме `canon_schema.json` поля `colors` нет вообще — его надо добавить в обе стороны;
  источник — `nt_profiles[].colors`, как в `data/full`);
- в `players.json` нет `age`/`imageUrl` — не нужны потребителям (age выводится из `birth_date`,
  фото keyed по id на диске);
- прод-генерации канона нет: `data/full` собирают `build_pilot_json.py` (из resume-кэша) +
  `topup.py` (докачка), canon из кэша не производится.

### Правило (остаётся в силе)

Поиск/сравнение игрока/клуба/лиги/сборной — **только по TM-id** в данной категории. Имя —
только отображение/логи. Пользовательские `/search/{name}` в api и осознанный name-based
`kits/linked.py` не трогать.

### Основной объём

#### 1. transfermarkt_scrapper: canon — прод-выход

1. Доделать пробелы canon (см. выше): `competitions.logo`, `national_teams.flag` +
   добавить `colors` в схему и в `_build_national_teams`.
2. **Прод-генерация из resume-кэша**: путь «cache → canon» без живых запросов к TM, идемпотентно,
   с валидацией `canon_schema.json`. Ориентиры: `build_pilot_json.py` уже поднимает из кэша
   `comp_clubs/club_profiles/club_rosters/nt_profiles/nt_rosters` — те же records нужны
   `build_canon`. `topup`-логика (докачка недостающих клубов из `data/clubs.json` по
   `tournament_structure.json`) должна работать в canonical-режиме (докачал → пересобрал canon).
3. Решить судьбу `data/full/*.json`: либо продолжать как промежуточный слой на время миграции
   потребителей, либо выпилить из прод-договора. **Договор: canon — единственный прод-выход
   скрейпера** (фиксируется в [[пайплайн-данных]]).

#### 2. Миграция четырёх потребителей на canon

Каждый репо: заменить чтение `data/full/*.json` на `data/canon/*.json`. Выход **не должен
измениться** — canon несёт те же данные (плоский контракт), поэтому артефакты потребителей и
БД должны получиться бит-в-бит теми же.

- **ratings-generator** (`generate.py`): клубные игроки — `canon/players.json` (рекорды с
  `club_id`), лиги/страны/экономика — `canon/competitions.json` + `canon/clubs.json`
  (`league_id` → лига → страна), NT-only — рекорды с `national_team_id` и пустым `club_id`.
  Учесть: `market_value` берётся из canon (`players.market_value`, фолбэк `players_market_value`).
- **kit-generator** (`kits/palette.py` `load_clubs`/`load_national_teams`, `generate_kits.py`,
  `export_game.py`): клубы — `canon/clubs.json` + `competitions.json` (для `_league_id`/`_league`/
  `_country`), сборные — `canon/national_teams.json` (добавить `colors`!).
  `kits/linked.py` — не трогать (name-based по дизайну).
- **tm-gf-face-generator** (`faces/data.py`): `canon/players.json` (один рекорд на игрока —
  дедупликация по id из `data.py` больше не нужна), NT-only — рекорды с `national_team_id`.
- **tm-gf-import**: читать canon вместо `data/full`:
  `competitions.json` (страны+лиги), `clubs.json` (клубы, `league_id`), `national_teams.json`
  (сборные+`colors`), `players.json` (игроки, `club_id`/`national_team_id`). Схема БД, колонки
  `tm_id`, `user_version`, `manifest.json`, индексы — без изменений. Матчинг — по TM-id.

#### 3. GameplayFootball: бандлы + `data-versions.json`

На базе готового `manifest.json`:

1. Скрипт упаковки (рядом с `tools/release/`): `GameplayFootball-data-<ver>.zip` из
   `data/databases/default` (БД + `images_*` + `template_kit.png`), версия из манифеста
   (`data_version`/`snapshot_id`), с проверкой `user_version`/schema.
2. `data-versions.json` в репо игры — реестр: каждая строка = `{version, snapshot_id, sha256,
   game_version, date}`. См. [[пайплайн-данных]] («Реестр версий»).
3. Связать с релизным процессом ([[релиз]]); игра уже логирует версию и проверяет схему.

### Верификация (обязательно)

- `data/canon/*` валидируется `canon_schema.json` (7 файлов, `build_canon._validate`).
- **Артефакты потребителей совпадают** с теми, что из `data/full`:
  - `ratings/output.json` — те же (player_id → overall/stats);
  - kit `specs.json`/PNG — те же (club_id → palette/киты), у сборных появились `colors`;
  - face `specs.json` — те же;
  - БД tm-gf-import — те же counts (4652 teams = 4406 клубов + 246 NT, 126843 players),
    те же эталоны (Англия — Кейн на CF слот 10; Экв. Гвинея — 1 GK Jesús Owono tm 631693,
    нет Roberto; Лаос — Soulisak Souvankham tm 1427661; пустых `profile_xml` у сборных — 0),
    `user_version=2`, `manifest.json` рядом.
- **Детерминизм x86**: если не сдвинулся — эталоны других платформ не переснимать (только
  подтвердить на linux/x64/macos). Если сдвинулся — понять причину, перебазировать x86 и
  переснять остальные (см. [[открытые-вопросы]]).
- Бандл собирается, распаковывается в чистый каталог, игра запускается из него
  (`determinism_runner` против распакованных данных), матч клубный и сборной не падает.

### Ограничения

- Не ломать `football_collection` (конвертер канона в Flutter-assets, отдельный репо; читает
  `prepared_tm_*.json`, которые скрейпер собирает отдельно от canon — проверить, что миграция
  их не трогает).
- Не трогать `/search/{name}` в api и `kits/linked.py`.
- Коммиты: английский subject, по 1–2 логических на репо; БД/`data/` в git не коммитить.
- Обновить вики после правок: `пайплайн-данных.md`, `данные-из-transfermarkt.md`,
  `база-данных.md`, `открытые-вопросы.md` (страницы игры) и ролевые AGENTS-секции репо.

### Что НЕ в этой задаче (отложено)

- Киты `pick_match_kits` в `team.cpp`/меню (отдельная игровая задача).
- Модернизация навигации, LAN, карьера, пакеты лиг (DLC).