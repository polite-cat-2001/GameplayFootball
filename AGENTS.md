# AGENTS.md

This file provides guidance to agents working with code in this repository.

## Что это

3D-футбольная игра на C++17 (CMake 3.16+), форк заброшенного GameplayFootball
([BazkieBumpercar](https://github.com/BazkieBumpercar/GameplayFootball)): обновлён до SDL3/OpenGL/OpenAL/Boost/SQLite3
и собран по изменениям Google Brain (ветка `google_brain`). Цель репозитория — собираться и запускаться на как
можно большем числе платформ. Движок «Blunted2» лежит в репозитории (`src/base`, `src/types`, `src/scene`,
`src/systems`, `src/managers`, `src/framework`); игра поверх него — `src/onthepitch`, `src/menu`, `src/league`, `src/data`.
Windows: локально x86 (Win32). macOS: запускается (подтверждено 2026-08-13 на
MacBook Air M2): окно/GL-контекст и прокачка событий в main thread, шедулер — во
вспомогательном `boost::thread`. Готовый `.app`-бандл собирается
`tools/release/package_macos.sh` — см. [[релиз]].

**Актуальная картина проекта живёт в вики: `docs/wiki/index.md` — начинать оттуда.** Этот файл
несёт только то, что сломаешь, *не зная, где посмотреть*; предметные детали — в вики, а
`docs/wiki/константы.md` — единственный дом для всех настраиваемых чисел.

## Ядро архитектуры

`main()` → конфиг `football.config` → `Database` (sqlite, путь `databases/default/database.sqlite`) →
`SystemManager` (GraphicsSystem + AudioSystem) → сцены `Scene2D`/`Scene3D` → `Scheduler` с двумя
`TaskSequence` → `Run()`.

Две последовательности задач (см. `src/main.cpp:188`, `src/main.cpp:209`):

- **`game`** — тик 10 мс (`physics_frametime_ms`): `MenuTask` и `GameTask` в фазах Get/Process;
- **`graphics`** — непрерывный: `GameTask::Put` + `GraphicsSystem` Get/Process/Put (рендер в отдельном потоке).

Игровая симуляция — `src/onthepitch` (Match, Team, Player, Ball, судьи, AI): класс `Match` с фазами
Get/Process/Put и буферами (см. `docs/wiki/матч`). AI игроков — `ElizaController` (`docs/wiki/глоссарий`).
Схема данных — `docs/wiki/база-данных`.

### Ловушки (каждая ломается молча)

- **Физика (ODE) НЕ компилируется.** Исходники `src/systems/physics/*` не включены в сборку
  (`CMakeLists.txt`, комментарий: «not compiling physics, as not used by gameplayfootball»).
  Правки там не соберутся. `tools/animator` — единственное место, где физика подключается.
- **Запуск — из каталога сборки с копией `data/`.** Все пути относительные CWD: база
  `databases/default/database.sqlite`, конфиг `football.config`, `media/...`. Без копии `data/` игра
  падает с `Log(e_FatalError, ... "Could not open database")`.
- **`football.config` в репо пустой** — всё работает на дефолтах кода. Ключи конфига и дефолты —
  в `docs/wiki/константы.md`.
- **Фазы Get/Process/Put и мьютексы.** Игровая логика (GameTask) и рендер (GraphicsSystem) живут в
  разных потоках; `matchPutBufferMutex`, `matchLifetimeMutex`, `getPhaseMutex` защищают границы.
  Ломать блокировки — сегфолт на выходе (починен в #3, не возвращай).
- **`time_ms` — глобальный extern** (`src/gamedefines.hpp:16`). Источник истины времени —
  `EnvironmentManager::GetTime_ms()`; не заводи параллельное время.
- **Boost-идиомы вместо std**: `boost::intrusive_ptr`/`boost::shared_ptr`, `boost::signals2`,
  `boost::thread`. Не заменяй на std:: — код написан под boost.
- **Windows**: инклюды вида `#include <SDL3/SDL.h>`; vcpkg-триплеты строго `x86-windows`;
  CMake — `CMAKE_GENERATOR_PLATFORM=Win32`; для MinGW в `main.cpp` стоит `#undef main`.
  Игра — GUI (`WIN32`-подсистема) через опцию `GAMEPLAYFOOTBALL_WINDOWS_SUBSYSTEM` (по умолчанию ON;
  OFF — консоль для дебага): в `src/main.cpp` собственная WinMain-заглушка (SDL3 не даёт SDLmain).
- **`google_brain` и `windows` — чужие ветки.** Это форк Google Brain RL и отдельная Windows-линия;
  не вливай их целиком в `master` без разбора.
- **`dataSetSortable`** (закомментирован в `src/gamedefines.hpp:70`) меняет тип `DataSet`
  (std::list ↔ std::deque) — код под обоими собран не был.
- **Упаковка macOS — только под bash 4.** `tools/release/package_macos.sh` использует
  `declare -A`, а системный bash на macOS — 3.2; запускать через
  `/opt/homebrew/bin/bash tools/release/package_macos.sh <ver>`. Игра резолвит пути
  относительно CWD, а macOS стартует .app из `/`, поэтому в бандле настоящий бинарник
  — `Contents/MacOS/gameplayfootball-bin`, а `gameplayfootball` — launcher, chdir'ящий в
  `Contents/Resources/data` (иначе из Finder фатальная ошибка `file not found or empty:
  football.config`). Детали и ловушки — см. [[релиз]].

## Сборка / запуск

Команды взяты из `README.md` и `CMakeLists.txt`.

**Linux** (deps: `apt-get install ... libsdl3-dev libsdl3-image-dev libsdl3-ttf-dev
libopenal-dev libboost-all-dev libsqlite3-dev`; пакеты SDL3 есть с Ubuntu 26.04):

```bash
cp -R data/. build          # данные обязаны лежать рядом с бинарником
cd build
cmake ..
make -j$(nproc)
./gameplayfootball
```

**Windows** (vcpkg, **manifest mode** через `vcpkg.json` в корне; триплет **обязательно**
`x86-windows`, генератор `Win32`). Отдельный `vcpkg install` не нужен — зависимости ставятся
при конфигурации CMake автоматически:

```bat
cd build
cmake .. -DGAMEPLAYFOOTBALL_WINDOWS_SUBSYSTEM=ON -DVCPKG_TARGET_TRIPLET=x86-windows -DCMAKE_GENERATOR_PLATFORM=Win32 -DCMAKE_TOOLCHAIN_FILE=C:/dev/vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_WINDOWS_EXPORT_ALL_SYMBOLS=TRUE
cmake --build . --parallel --config Release
:: data/ копируется рядом с exe автоматически (POST_BUILD); запуск: gameplayfootball.exe внутри build\Release
:: GAMEPLAYFOOTBALL_WINDOWS_SUBSYSTEM=OFF — собрать с консолью для дебага
```

**macOS**: собирается через brew (sdl3 sdl3_image sdl3_ttf boost openal-soft). Запускается
(подтверждено 2026-08-13 на MacBook Air M2): окно/GL-контекст и прокачка событий в main
thread, шедулер — во вспомогательном `boost::thread` (см. `src/main.cpp`). Готовый .app:
`/opt/homebrew/bin/bash tools/release/package_macos.sh <ver>` → `dist/GameplayFootball-v<ver>-macos-arm64.zip`
(нужен bash 4, см. [[релиз]]).

Проверка регрессии геймплея — вручную через `tools/determinism` (эталоны см.
`docs/wiki/открытые-вопросы`). CI в GitHub нет.

Тестов и линта в проекте нет — проверка компиляцией: `cmake --build` в каталоге сборки.

## Устройство документации — вики как живой слой

- `docs/wiki/` — **текущее состояние**, одна страница на подсистему, кросс-ссылки вида
  `[[имя-страницы]]`, каталог в `docs/wiki/index.md`. Ты её поддерживаешь: после правки кода
  обнови затронутые страницы вики — не создавай новый датированный документ.
- `docs/wiki/глоссарий.md` — **имена предметной области** и запрещённые синонимы. Термин берётся
  отсюда для заголовков задач, имён тестов и формулировок гипотез; разрешил новый термин —
  допиши сюда. `CONTEXT.md` в корне — только указатель на эту страницу.
- `log.md` (корень репозитория) — **append-only хронология** (паттерн LLM-Wiki Карпатого): вехи,
  деплои, опровергнутые гипотезы, решения. Записи начинаются `## [YYYY-MM-DD] тип | описание`
  (парсится: `grep "^## \[" log.md | tail -5`); типы перечислены в его шапке. Дописывать в конец,
  старые записи не редактировать и не удалять. Завершая сессию, допиши запись `session` и обнови
  `открытые-вопросы.md` — незакрытые хвосты проекта живут там, а не в датированном файле.
- `docs/reports/`, `docs/specs/`, `docs/plans/` — **датированные снимки, неизменяемые**.
  Никогда не редактировать; они фиксируют, что было верно или запланировано на дату. Сюда же
  кладётся handoff-пакет, когда нужно продолжить работу в свежей сессии: он одноразовый и
  устаревает по природе, поэтому живёт среди снимков, а не в вики.

**Правило: после любого содержательного изменения обнови соответствующую страницу вики — не
создавай новый датированный документ.** Новый файл в `docs/reports/` появляется только для
отчёта или аудита, адресованного человеку на конкретную дату. Всё, что описывает, *как система
устроена сейчас*, живёт в вики и заменяет собой то, что там было.

Когда задача закончена:
1. Обнови страницу(ы) вики для затронутой подсистемы — перепиши устаревшие утверждения, не
   дописывай «UPDATE:».
2. Изменился порог — обнови `docs/wiki/константы.md` (страница заявляет, что сверена с кодом:
   это заявление должно оставаться истинным).
3. Что-то стало подтверждено или наоборот — обнови `docs/wiki/открытые-вопросы.md`. Закрытые
   пункты **удаляются**, а не помечаются сделанными.
4. Новая страница → строка в `docs/wiki/index.md` и ссылка на неё хотя бы с одной существующей
   страницы.
5. Задача была вехой (деплой, полевой тест, опровергнутая гипотеза, смена направления) — допиши
   запись в `log.md`.

**Доверяй вики больше, чем замороженным отчётам и этому файлу, но коду — больше, чем всем трём.**
Расхождение документации с кодом — находка: почини страницу тем же изменением.

Периодически, или когда просят «проверь вики» / «lint the wiki»: запусти скилл `wiki-lint` —
битые `[[ссылки]]`, страницы-сироты, разошедшиеся с кодом константы, противоречия между
страницами.

Три хука (скрипты в `.agent/hooks/`, обвязка под инструмент — для Claude Code в
`.claude/settings.json`, для opencode плагин в `.opencode/plugins/`) поддерживают контур
механически: оглавление вики на старте сессии, подсказка «прочитай эту страницу» по пути
правимого файла, и проверка в конце работы, возвращающая один раз за сессию, если код менялся,
а `docs/wiki/` — нет. Добавил подсистему — впиши её отображение файл→страница в `wiki-hint.sh`.

## Workflow

- Where a task matches an installed skill, use that skill — but only when it's genuinely appropriate to the task, not by default. Don't force-fit a skill onto work it wasn't meant for.
- Long-running processes (long builds, model training, long test runs, migrations) must not block the main conversation: launch them via a background agent or background process if the tool supports it (e.g. a background agent with a `run_in_background`-style flag), rather than a blocking call; have the process log progress periodically rather than only at the end, and continue other work or respond to the user while it runs.

## Working principles

## 1. Think Before Coding
Don't assume. Don't hide confusion. Surface tradeoffs.

Before implementing:

State your assumptions explicitly. If uncertain, ask.
If multiple interpretations exist, present them - don't pick silently.
If a simpler approach exists, say so. Push back when warranted.
If something is unclear, stop. Name what's confusing. Ask.

## 2. Simplicity First
Minimum code that solves the problem. Nothing speculative.

No features beyond what was asked.
No abstractions for single-use code.
No "flexibility" or "configurability" that wasn't requested.
No error handling for impossible scenarios.
If you write 200 lines and it could be 50, rewrite it.

Ask yourself: "Would a senior engineer say this is overcomplicated?" If yes, simplify.

## 3. Surgical Changes
Touch only what you must. Clean up only your own mess.

When editing existing code:

Don't "improve" adjacent code, comments, or formatting.
Don't refactor things that aren't broken.
Match existing style, even if you'd do it differently.
If you notice unrelated dead code, mention it - don't delete it.

When your changes create orphans:

Remove imports/variables/functions that YOUR changes made unused.
Don't remove pre-existing dead code unless asked.

The test: Every changed line should trace directly to the user's request.

## 4. Goal-Driven Execution
Define success criteria. Loop until verified.

Transform tasks into verifiable goals:

"Add validation" → "Write tests for invalid inputs, then make them pass"
"Fix the bug" → "Write a test that reproduces it, then make it pass"
"Refactor X" → "Ensure tests pass before and after"

For multi-step tasks, state a brief plan:

1. [Step] → verify: [check]
2. [Step] → verify: [check]
3. [Step] → verify: [check]

Strong success criteria let you loop independently. Weak criteria ("make it work") require constant clarification.

## Conventions

- Код и комментарии — на английском; вики и этот файл — на русском (язык общения проекта).
- Весь код движка в namespace `blunted`; `using namespace blunted;` в исходниках игры.
- `real` — алиас `float` (`src/defines.hpp:37`). Координаты и скорости — метры/метры-в-секунду, время — мс.
- Константы игровой логики — в `src/gamedefines.hpp` (и нигде больше; единственный дом —
  `docs/wiki/константы.md`).
- **«Назад» в UI — только Escape (клавиатура) и B (геймпад). Backspace не используется в игре
  нигде** (ни как «назад», ни как отладочная клавиша) — не заводить его заново.
- Git: комментарии коммитов — английские, Conventional Commits не обязательны, но осмысленный
  subject в одну строку — да. Ветки с незавершённой работой (`google_brain`, `windows`) не вливать.
- `.gitignore`: `*build*/` и `.idea/`. Каталог `build/` игнорируется целиком, в том числе от
  локального коммита; данные в `build/` не коммитить.
