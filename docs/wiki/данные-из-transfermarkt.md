# Данные из Transfermarkt

## Что это

Конвейер поставки актуальных футбольных данных (составы, клубы, сборные, стоимости) из
Transfermarkt в две игры: GameplayFootball и football_collection. Источник — форк
[transfermarkt-api](https://github.com/packee-dev/transfermarkt-api) (FastAPI-обёртка над
скрейпингом transfermarkt.com, дополнена сборными и тренерами). Единый **канонический JSON на
TM-id** — источник истины; конвертеры производят из него GF SQLite и Flutter-assets.

Ветка разработки: `develop` (фичи — из `squads-update`). См. также [[база-данных]].

## Схема конвейера

```
transfermarkt.com
   │  (HTML + внутренние JSON-API)
   ▼
transfermarkt-api  (локальный FastAPI, :8000)
   │  HTML 1.5 rps / CDN 10 rps (probe 2026-08-29), resume-кэш, curl_cffi
   ▼
скрейпер (transfermarkt_scrapper)
   │
   ▼
канон-JSON на TM-id  (единый источник истины)
   ├──► конвертер 1 → GF SQLite (teams/players/leagues + киты, логотипы)
   └──► конвертер 2 → Flutter assets (prepared_tm_*.json) для football_collection
```

Скрейпер ходит **не на transfermarkt напрямую**, а на локальный transfermarkt-api, который уже
скрейпит TM. Лимит ≤2 запроса/сек к TM обязателен — при частых запросах TM банит IP надолго.

## Канонический формат

Ключ всех сущностей — строковый **TM-id**. Никакой GF-специфики в каноне: `base_stat`,
`profile_xml`, формации, цвета в формате GF считает конвертер.

```
data/
  meta.json                          # version, snapshot_date, tm_season, counts
  competitions.json                  # лиги/кубки: id, name, type, country{id,name}, tier, logo
  clubs.json                         # id, name, colors[] (HEX), stadium{name,seats}, league_id,
                                     #   logo, founded_on
  national_teams.json                # id, name, coach_id, flag, emblem
  players.json                       # id, name, first_name, last_name, birth_date, height, weight,
                                     #   foot, citizenship[], position{main,other}, club_id,
                                     #   national_team_id, shirt_number, market_value,
                                     #   max_market_value, outfitter, is_retired
  players_market_value.json          # id, history[{age,date,clubId,clubName,marketValue}]
  coaches.json                       # id, name, citizenship, current_club_id
```

### Источники полей (проверено живыми запросами 2026-08-29)

| Поле канона | Источник | Примечание |
|---|---|---|
| `role`, `height`, `foot`, `birth_date`, `marketValue`, `citizenship`, `position` | `GET /clubs/{id}/players` | состав клуба — 1 запрос на команду |
| `national_team_id` | `GET /national-teams/{id}/players` | **только здесь** — профиль игрока сборную не даёт |
| `shirt_number` (в сборной) | `GET /national-teams/{id}/players` → `shirtNumber` | поле добавлено в форк |
| `coach_id` | прямой парсинг `/mitarbeiter/verein/{id}` | **API сборных тренера не отдаёт** |
| `colors`, `stadium`, `league` | `GET /clubs/{id}/profile` | |
| `max_market_value` | `GET /players/{id}/market_value` (история) | нужен только коллекционке |
| `weight` | **отсутствует в TM** | эвристика в конвертере (рост + позиция) |
| `shortname` (3 буквы для GF) | **отсутствует в TM** | выводить в конвертере |

### Стратегия сбора

Основной источник — **состав клуба** (`clubs/{id}/players`): для ~220 команд Tier-1 это ~220
запросов вместо одного на игрока. Индивидуальные профили (`/players/{id}/profile`) — точечно:
`outfitter`, `imageUrl`, multi-позиции для игроков, нужных коллекционке. История стоимости —
отдельный запрос на игрока, только для коллекционки.

## Маппинг в GF SQLite

| GF-поле | Источник / логика |
|---|---|
| `players.role` | маппинг TM-позиции → GF-токен: Centre-Forward→ST, Left Winger→AM L, Defensive Midfield→DM, Left-Back→D/WB L…; `position.other` — доп. позиции через `/` |
| `players.base_stat` | формула из `market_value` (лог → 0..1), возрастной корректир делает `CalculateStat` в игре |
| `players.profile_xml` | 22 стата по роли, тот же алгоритм, что в GF (`GetDefaultProfile`, `src/utils.cpp`); исследование-прототип с калибровкой по FIFA — см. «Калибровка 22 статов по FIFA» на этой странице |
| `players.height` | см → м (1.87) |
| `players.weight` | эвристика (рост + позиция), TM вес не даёт |
| `players.formationorder` | стартовые 11 (лучшие по `base_stat` + роль), слот формации `p(i+1)` ↔ игрок `[i]` |
| `players.nationalteam_id` | из состава сборной; `nationalteamformationorder` — аналогично |
| `teams.formation_xml` | шаблон из схем (4-2-3-1 / 4-3-3…), дефолт GF в `mainmenu.cpp:492` |
| `teams.tactics_xml` | дефолт GF или случайные стили (possession/counter/…) |
| `teams.color1/color2` | `clubs.colors` HEX → «R, G, B» |
| `teams.shortname` | первые 3 буквы (или ручной маппинг топ-клубов) |
| `teams.kit_url` | `images_teams/<лига>/<клуб>` + генератор китов по `template_kit.png` |
| `teams.logo_url` | скачивание с TM + ресайз 128×128 |
| `leagues`, `countries`, `regions` | из `competitions.json` + страны |

### Киты

Кит в GF — PNG 1024×1024 с UV-раскладкой (торс = color1, рукава/шорты/носки = color2, нижняя
четверть текстуры не используется). Генератор красит регионы по `template_kit.png`
(`data/databases/default/template_kit.png`). Текущие 8 команд сохраняют реальные PNG.

## Калибровка 22 статов по FIFA (fifagc)

Исследование-прототип во вьювере данных (каталог `data_max` на машине, `http://localhost:9090`,
120K игроков, 279 лиг): 22 стата GF считаются не «формулой из воздуха» (OVR + позиция + шум),
а из реальных FIFA-атрибутов. OVR калибруется по fifagc, теперь — и распределение статов.

### Источник и скрейпер

- `fetch_fifa_pages.py` — скрейпер страниц игроков fifagc.ru (resume-кэш `fifa_pages.json`,
  ~1.1 rps). Со страницы: 29-34 атрибута (Acceleration, Finishing, Standing Tackle…),
  **Потенциал**, слабая нога, особые приёмы, рост/вес, мульти-позиции. У вратарей +5 своих
  атрибутов (Diving, Handling, Kicking, Reflexes, Positioning).
- 1864 из 120K игроков совпали с fifagc-страницами (матчинг по slug через `fifa_matches.json`);
  11 ошибочных матчингов (страница вратаря-однофамильца на полевого) отсеяны по признаку
  «есть Diving ↔ позиция не GK».

### Маппинг 29-34 атрибутов → 22 стата (`fifa_mapping.py`)

Часть статов — прямые копии, часть — взвешенные смеси. Примеры:
`technical_shot = 0.5·Finishing + 0.25·Long Shots + 0.15·Free Kick + 0.1·Penalties`,
`technical_highpass = 0.7·Long Pass + 0.3·Crossing`, `mental_workrate = 0.5·Stamina + 0.5·Aggression`.
У вратарей reaction/balance/defpos считаются с их атрибутов (Reflexes/Diving/Handling/Positioning).

### Совпавшие vs архетипы (`build_stats_v3.py`)

- **1864 совпавших** — реальные атрибуты со страницы.
- **118K остальных** — архетип позиции: для каждой из 8 групп (GK/CB/FB/DM/CM/AM/W/CF) из
  реальных данных посчитано среднее каждого атрибута и наклон по OVR (OLS cov/var). Стат =
  `среднее_группы + (OVR − средний_OVR_группы) · наклон` + детерминированный разброс ±3 по хэшу
  pid. Профили позиций (что у кого выше/ниже нормы) — `analyze_positions.py`.

### POT / вес / слабая нога / приёмы

- **POT**: у совпавших реальный; у остальных — модель дельты `pot − ovr ~ (ovr, возраст, позиция)`,
  pot никогда не ниже ovr.
- **Вес**: у совпавших реальный; у остальных — регрессия `вес = −41.5 + 0.671·рост(см) + поправка
  позиции` (MAE 3.6 кг).
- **Слабая нога / приёмы**: у совпавших реальные (1-5); у остальных — среднее по позиции
  (слабая нога ≈ 3 везде; приёмы GK=1, CB=2, FB/DM/CM/AM/CF=3, W=4, буст OVR≥85→4, OVR≥90→5).

### Масштаб

Статы во вьювере — **шкала FIFA 0-99**, а GF хранит статы как 0-1 (множители в `match.cpp`,
`GetDefaultProfile`). При переносе в `profile_xml` нужна нормализация (÷100 или через
GF `CalculateStat`).

## Изменения в коде GF

1. `teams.national INTEGER` — колонка; добавить `teams.national` в SELECT в `teamdata.cpp:61`
   (сейчас проверка `national` на строке 74 никогда не срабатывает).
2. Сборные — строки в `teams` (league_id → синтетическая лига «International»), игроки — с
   `nationalteam_id` + `nationalteamformationorder` (запрос в `teamdata.cpp:251` это уже умеет).
3. Реальные имена — осознанное решение (личный проект; в `master` остаются фейковые).

## Ловушки

- **Лимит запросов**: HTML 1.5 rps к TM (безопасное значение по probe 2026-08-29: чисто на 2.0 rps
  sustained, 3.0 rps burst; CDN-картинки не ограничены — 10 rps). Resume-кэш обязателен.
- **`extract_from_url`** в transfermarkt-api должен срезать домен (`https://www...`) и ловить
  `AttributeError` — иначе канонические URL ломают парсинг (починено в форке).
- **Возраст в составе сборной** брать со второго `td.zentriert` (`[2]`), не с первого (номер).
- **Схема парсит int из строк**: `parse_str_to_int` в `schemas/base.py` ожидает строку, int уже
  передавать нельзя.