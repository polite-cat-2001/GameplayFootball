# log.md — хронология проекта

Append-only журнал по методу Карпатого (LLM Wiki): **что произошло и когда** — вехи, деплои,
опровергнутые гипотезы, решения. Дополняет вики: `docs/wiki/` отвечает на «как устроено сейчас»,
лог — на «как мы сюда пришли». Записи только добавляются в конец, никогда не редактируются и не
удаляются (опечатки — новой записью-поправкой).

Формат записи (парсится unix-утилитами, `grep "^## \[" log.md | tail -5` — последние 5 событий):

```
## [YYYY-MM-DD] тип | краткое описание
```

Типы: `feat` (новая механика), `fix`, `deploy` (выкатка), `docs`, `decision` (выбор направления,
отказ от идеи), `lint` (сверка вики/констант), `session` (итог рабочей сессии, если не покрыт
другими типами).

Лог **заменяет `handoff.md`**: при завершении сессии — запись `session` здесь плюс обновление
`docs/wiki/открытые-вопросы.md`.

---

## [2020-07-17] docs | Начата модернизация: Blunted2 в репозитории, единая сборка
Движок Blunted2 добавлен в репозиторий (`af00e69`), источники сведены в одну сборку (`081d919`).
Позже (2020-07-21) переезд на SDL2, удаление SGE/Glew; добавлены дефолтные данные.

## [2020-07-31] decision | macOS: цель — компилироваться, но не запускаться
Рендеринг обязан идти в main thread, а рендерер стартует в отдельном потоке — macOS собирается,
но не работает. Направление зафиксировано, решение не найдено (см. `docs/wiki/открытые-вопросы`).

## [2020-11-06] docs | Инструкции для Windows и macOS
README дополнен сборкой для Windows (vcpkg, триплеты `x86-windows`) и macOS (brew).

## [2021-04-21] feat | SDL2: получение доступных display modes
`80ef590` — запрос доступных режимов дисплея через SDL2 вместо жёстких дефолтов.

## [2021-07-20] fix | Сегфолт на выходе
`1415a33` — исправлен сегфолт при завершении игры (защита границ Get/Process/Put мьютексами).
Правило из ловушек AGENTS.md: не возвращать.

## [2021-07-20] decision | SQLite из системного пакета, а не из репозитория
`68159a2` — вендорные sqlite3-исходники удалены, сборка использует системный пакет SQLite3.

## [2026-08-09] docs | Развёрнут контур документации
AGENTS.md, `docs/wiki/` (архитектура, матч, база-данных, константы, открытые вопросы, глоссарий),
этот лог, корневой `CONTEXT.md`-указатель, хуки поддержки вики (`.agent/hooks/`, opencode-плагин)
и скилл `wiki-lint`. Затравка лога восстановлена из git-истории (вехи 2020-2021 по коммитам).

## [2026-08-09] deploy | Windows-сборка подтверждена end-to-end (MSVC + vcpkg)
`gameplayfootball.exe` собран на master в VS2022 Build Tools (MSVC 14.44, платформа Win32),
зависимости vcpkg `x86-windows`, системный SQLite3. Запуск проверен — процесс жив и рендерит.
В master из ветки `windows` перенесены точечные MSVC-фиксы: `NOMINMAX` в `defines.hpp`/`main.cpp`,
`<SDL2/SDL_opengl_glext.h>` вместо `wingdi.h` в рендерере, замена VLA на `std::vector`
(`aseloader.cpp`, `grid.cpp`, `humanoid.cpp`, `match.cpp`, `proceduralpitch.cpp`,
`teamAIcontroller.cpp`, `animcollection.cpp`, `opengl_renderer3d.cpp`), `Stat{...}` вместо
`(Stat){...}` в `utils.cpp`, сигнатура `main(int, char**)`, условное `dl`/`m` только под UNIX
в `CMakeLists.txt`. Ветку `windows` целиком не вливаем (проверено её содержимое: устаревший
вендорный sqlite3 и лишние правки).

## [2026-08-10] feat | Ворота №1: детерминированный headless-раннер (ветка `determinism-gate`)
Перенесены из GRF (ветка `google-brain` = GRF v2.10.1) только архитектурные механизмы, без
изменения геймплея: `EnvState`-сериализатор (`defines.hpp`), отвязка времени в `Match::Process`
(фиксированный шаг 10 мс вместо реальных часов), `randomize(seed)` (srand + boost + fastrandom),
`ProcessState` для ядра Match/Ball/Player/PlayerBase (только совпадающие поля, без
AI/humanoid-внутренностей). Вынесен игровой контекст из `main.cpp` в `src/gamecontext.*`.
Добавлен `tools/determinism` (режимы `run`/`check`) с эталоном `reference.txt`, `MockRenderer3D`
для headless-рендера (`graphics3d_renderer=mock`). При отладке найден и исправлен баг
неинициализированной памяти: `tacticalSituation.forwardRating` и `dynamicFormationEntry`
запасных игроков не инициализировались в ctor — это и был источник недетерминизма.
Оценка геймплея GRF: играется похоже, но Google меняли ощущения (автопилот, переключение,
пас) — игровую логику GRF в master не переносим.

## [2026-08-10] feat | Модернизация сборки: CMake 3.16+/C++17, SDL2→SDL3, CI (ветка `build-modernization`)
CMake поднят до 3.16, включён C++17 (`c3bdb61`); источники сведены в единый статический `blunted2`,
`sources.cmake` удалён (`858f09b`); SDL2→SDL3 в инклюдах и API, SDL_gfx убран полностью
(в vcpkg порта sdl3-gfx нет; заменён на `SDL_ScaleSurface`) (`e138cff`). Эталон детерминизма сделан
воспроизводимым между пересборками (`fb3a90f`): `determinism_runner` в начале фиксирует ключи конфига
(`graphics3d_renderer=mock`, `match_difficulty=0.8`, `match_duration=1.0`). Добавлен CI
`.github/workflows/build.yml` (4 job: linux сборка+check, windows-x86 check, windows-x64 capture,
macos сборка) и эталоны на платформу: x86 `reference.txt`=`372c4bbd...`, x64
`reference-windows-x64.txt`=`4e9ddb72...` (снят локально; отличается от x86 — эталон платформозависим),
`reference-linux.txt` снимается первым CI-прогоном (TODO, Task 6). Linux-job привязан к `ubuntu-26.04`
(apt-пакеты SDL3 есть только с 26.04; `ubuntu-latest` всё ещё 24.04). Обновлены вики, AGENTS.md,
README. Первый прогон CI — после push (Task 6).

## [2026-08-10] session | CI на GitHub Actions: Windows подтверждён, linux/macos отложены
Ветка `build-modernization` запушена, workflow запущен. Windows x86+x64 подтверждены end-to-end:
сборка на VS2026 + новый boost, determinism check/capture дают ровно локальные эталоны
(x86 `372c4bbd...`, x64 `4e9ddb72...`) — оба портативны между MSVC 2022 и 2026. По пути починены
три реальных бага: (1) дистрибутивный Boost (Ubuntu/Homebrew, b2) не ставит компонентные
CMake-конфиги и в 1.90+ не собирает `libboost_system` (header-only с 1.69) — в CMakeLists
`Boost_NO_BOOST_CMAKE=ON`, компоненты `thread filesystem`; (2) `MockAudioRenderer`
(`audio_renderer=mock`, `src/systems/audio/rendering/mock_audiorenderer.hpp`) — на CI-раннерах нет
аудио-устройства, `OpenALRenderer::CreateContext` падал фатально; (3) баг кавычек в PowerShell-шаге
capture (`$refFile = ..\..\...` без кавычек → `$null`). Linux/macos в CI нестабильны:
preview-раннер `ubuntu-26.04` гасится посреди сборки, контейнер `ubuntu:26.04` висит на teardown,
сборка SDL3 из исходников упёрлась в нехватку `libasound2-dev`, macOS build-only висит часами.
Linux/macos вынесены из CI, проверка переносится на локальные девайсы (см.
`docs/wiki/открытые-вопросы.md`). `reference-linux.txt` — TODO (снять на Linux-устройстве).
Малый boost-набор в vcpkg (component-порты) ускорил Windows-джобы с ~57 до ~10-15 мин.

## [2026-08-10] decision | GitHub Actions CI убран, проверка детерминизма — вручную
Windows x86+x64 собрались и подтвердили оба эталона на CI (VS2026: x86 `372c4bbd...`, x64
`4e9ddb72...`), но linux/macos-джобы оказались нестабильны: preview-раннер `ubuntu-26.04` гасится
посреди сборки, контейнер `ubuntu:26.04` зависает на teardown, сборка SDL3 из исходников упёрлась
в нехватку `libasound2-dev`, macOS build-only висит часами. Решение: `.github/workflows` удалён,
проверка детерминизма — вручную через `tools/determinism` на локальных машинах (у автора есть
Windows/Linux/Mac-девайсы). Эталоны остаются локальным инструментом; `reference-linux.txt` — TODO
(снять на Linux-девайсе). Полезные фиксы из CI-итераций остались в коде: `Boost_NO_BOOST_CMAKE` +
компоненты `thread filesystem` (дистрибутивный boost не даёт компонентных CMake-конфигов и не
собирает `libboost_system` с 1.69), `MockAudioRenderer` (`audio_renderer=mock`) для headless-запуска
без аудио-устройства.

## [2026-08-10] session | Linux (WSL2 Ubuntu 26.04): сборка зелёная, эталон снят, найдены порт-фиксы
На этом ПК поднят WSL2 с Ubuntu 26.04 LTS, ветка `build-modernization` собрана. Всплыли четыре
бага портируемости, которые MSVC не ловил: (1) циклический инклюд `defines.hpp`↔`log.hpp` скрывал
`blunted::Log` при включении `log.hpp` первым (gcc, two-phase lookup) — в `defines.hpp` добавлен
хелпер `blunted::EnvStateFatal()`, определён в `envstate.cpp`; (2) `settings.cpp` держал SDL2-код
перечисления display modes в не-Windows ветке (`SDL_GetNumDisplayModes`/`SDL_GetDisplayMode` убраны
из SDL3) — переведено на `SDL_GetDisplays` + `SDL_GetDesktopDisplayMode`/`SDL_GetCurrentDisplayMode`;
(3) `SDL_GL_GetProcAddress` в SDL3 возвращает `SDL_FunctionPointer`, а не `void*` — добавлен
`reinterpret_cast<void*>` в макрос `SDL_PROC` (`opengl_renderer3d.cpp`); (4) статические игровые
библиотеки с циклическими ссылками не линкуются на GNU ld — в CMakeLists добавлен
`-Wl,--start-group/-end-group` (UNIX AND NOT APPLE). После фиксов: Linux-сборка зелёная, игра
запускается в WSLg и корректно завершается, linux-эталон `reference-linux.txt` =
`a672aa0b81d6275e60a6469b1c41dfdf9acff138` (воспроизводим, check → 0). Windows-хэши не изменились
(x86 `372c4bbd...`, x64 `4e9ddb72...`).

## [2026-08-10] session | macOS-девайс: сборка собралась, игра не завелась; ветка влита в master
На MacBook Air M2 (arm64, 8 ГБ) ветка `build-modernization` собралась после порт-фиксов
(brew sdl3/sdl3_image/sdl3_ttf/boost/openal-soft; сборка без параллелизма — 8 ГБ RAM). Запуск
подтвердил известный блокер: окно/GL-контекст создаются в отдельном потоке
(`GraphicsSystem::Initialize` → `OpenGLRenderer3D::Run`), а AppKit требует main thread → игра не
стартует. Фикс (создание окна/контекста в main thread) отложен; статус перенесён в
«Отложено осознанно» в вики. Детерминизм на macOS не снимался (игра не работает, эталон не нужен).
Порт-фиксы, найденные на Linux/WSL и подтвердившиеся на macOS: `EnvStateFatal()` вместо
`blunted::Log` в шаблоне (циклический инклюд defines↔log), SDL3 display-mode API в `settings.cpp`,
`reinterpret_cast<void*>` для `SDL_GL_GetProcAddress`, `-Wl,--start-group` для GNU ld.
Ветка `build-modernization` влита в `master`.

## [2026-08-11] feat | Ворота №3: рендер на OpenGL 3.2 core profile (ветка `render-modernization`)
Рендерер `OpenGLRenderer3D` переведён с legacy (compatibility) контекста на **core profile 3.2**:
в `CreateContext` включены `SDL_GL_CONTEXT_MAJOR_VERSION=3`, `MINOR=2`,
`SDL_GL_CONTEXT_PROFILE_CORE`. Деferred-конвейер (GBuffer → accumulation → postprocess) был уже
шейдерным (`#version 150`) и VAO/VBO-ориентированным; единственные активные fixed-function-вызовы
сидели в мёртвых методах интерфейса `Renderer3D`. Убраны legacy-методы и их реализации:
`SetColor` (`glColor4f`), `SetTextureMode`, `RenderAABB`×2 (`glBegin/glEnd`, тело уже в комментарии),
`SetLight` (`glLightfv`, тело уже в комментарии), `SetClientTextureUnit` (`glClientActiveTexture`),
`PushAttribute`/`PopAttribute` (`glPushAttrib`/`glPopAttrib`), `SetColorMask`, HDR-захват яркости;
удалены `drawSphere` и point-light-ветка `RenderLights` (light.type всегда 0). `sdl_glfuncs.h`:
28 deprecated-функций переведены `SDL_PROC`→`SDL_PROC_UNUSED` — под core profile
`SDL_GL_GetProcAddress` вернул бы NULL и лоадер упал бы с `exit(1)`. Проверки: полная сборка MSVC
(Win32) зелёная, `determinism_runner check 372c4bbd...` → 0, запуск игры подтверждает
`Using OpenGL version 3.2 ... Core Profile Context` без ошибок/предупреждений (меню рендерится,
FBO complete). GLES-перевод шейдеров (`#version 300 es`) — отдельная задача, в
`docs/wiki/открытые-вопросы`. План: `docs/plans/2026-08-11-render-modernization.md`. Ветка в master
не влита.
## [2026-08-12] feat | Ввод с геймпада: SDL3 SDL_Gamepad (semantic indices), фикс instance ID (Xbox Series), пресеты PES/FIFA на выборе сторон, GUI-навигация стик+крестовина, hot-plug (пауза+выбор сторон при отключении в матче), удалена калибровка джойстика. Эталон determinism 372c4bbd... не сдвинулся. Спека docs/specs/2026-08-12-gamepad-input-design.md, план docs/plans/2026-08-12-gamepad-input.md. Ручная проверка на Xbox Series отложена (геймпада не было на машине).
## [2026-08-12] fix | Ввод с геймпада (ревью на Xbox Series): подтверждение сторон на выборе сторон (A/Enter, зелёная галочка, все девайсы со стороной обязаны подтвердить, B/Esc двухшаговый выход), циклический PES/FIFA на LB/RB, пауза переехала на Options (в пресете были перепутаны Select/Start), исправлен знак крестовины в GUI, hot-plug пересобирает слоты стабильно (краш при старте матча из-за пересоздания геймпада каждый тик). Стороны сохраняются в потоке выбора матча, сбрасываются в главном меню.
## [2026-08-12] fix | Ввод с геймпада (hot-plug в матче, ревью): краш при отключении геймпада (dangling HIDGamepad) починен перепривязкой human-геймеров при изменении состава (RefreshGamepads возвращает changed); окно выбора сторон при отключении открывается через topPage->CreatePage (не копится в root), resumeOnClose возобновляет матч только если паузу ставил GameTask; повторное открытие окна не дублирует (проверка верхней страницы стека вместо флага). Попытка декларативной навигации (NavigateTo/CloseTopPage, по аналогии с go_router) откачена — не совпадал стек меню под матчем; кейс зафиксирован в docs/wiki/открытые-вопросы.md как причина модернизации навигации.
## [2026-08-12] docs | Зафиксирован роадмап: 1) запуск на macOS (окно/GL-контекст в main thread), 2) модернизация навигации (декларативные роуты вместо императивного стека), 3) LAN-матчи. GLES-перевод шейдеров отложен на будущее (до цели «мобилки/веб»). Порядок — в docs/wiki/открытые-вопросы.md.
## [2026-08-13] fix | macOS: окно/GL-контекст и прокачка событий перенесены в main thread
Причина нестарта: `GraphicsSystem::Initialize` стартовал рендерер в отдельном потоке и создавал
окно/контекст сообщением внутри него, а AppKit требует main thread (то же для `SDL_PollEvent`).
Фикс: на `__APPLE__` `GraphicsSystem::Initialize` создаёт контекст синхронно на вызывающем (main)
потоке и не стартует поток рендерера; `main()` запускает шедулер (`Run()`) во вспомогательном
`boost::thread`, а рендер-цикл `renderer3DTask->operator()()` — на main thread; по завершении
шедулер шлёт рендер-циклу `Message_Shutdown`. `GraphicsSystem::Exit` на macOS поток рендерера не
останавливает (его нет). Проверки: Windows x86 (MSVC, Win32) и Linux (gcc, WSL2 Ubuntu 26.04)
пересобраны, эталоны детерминизма совпали (`372c4bbd...` и `a672aa0b...`). Запуск на устройстве
(MacBook Air M2, сборка без параллелизма) не подтверждён — первый пункт роадмапа открыт до
проверки на маке (docs/wiki/открытые-вопросы.md).
## [2026-08-13] fix | macOS подтверждён на устройстве: запуск работает, но найден визуальный дефект рендера
Запуск на MacBook Air M2 (ветка macos-main-thread, сборка --parallel 1): окно открывается,
меню/матч запускаются, игроки бегают, звук есть, выход чистый (F12/закрытие окна — без краша,
в логе штатное "Shutting down OpenGLRenderer3D thread"). Открыт новый дефект: поле, стадион и
часть 2D-интерфейса чёрные — не рендерятся текстуры (игроки на вершинных цветах видны). В логе
ошибок нет. Гипотеза: GetGLPixelFormatFromSurface возвращает GL_ABGR_EXT для SDL3 RGBA8888,
который в core profile может не поддерживаться. По пути закрыты три блокера macOS:
1) GL_ABGR_EXT не компилировался (SDL_opengl_glext.h был под #ifdef WIN32) — инклюд расширен на __APPLE__;
2) шейдеры simple/lighting/ambient/postprocess использовали texture2D (GLSL 1.30), недоступный в
#version 150 core на Apple GL — заменены на texture();
3) дедлок старта: InitGameContext грузил ресурсы через Command::Wait() до запуска render loop —
на macOS системы/окно создаются на main thread (новый InitGameSystems), вся игра (InitGameContext +
sequence setup + Run) — во вспомогательном boost::thread, render loop — на main thread.
Windows/Linux не затронуты (тот же код не-macOS-ветки main(), шейдеры texture() валидны в 150).
## [2026-08-13] fix | macOS: чёрные текстуры (поле/UI) устранены
Причина: SDL_PIXELFORMAT_RGBA8888 на little-endian хранит байты A,B,G,R (Rmask=0xFF000000),
GetGLPixelFormatFromSurface мапил его на GL_ABGR_EXT, а core profile его не принимает
(glTexImage2D даёт GL_INVALID_ENUM, текстура чёрная). Все поверхности, создаваемые кодом
(CreateSDLSurface: поле, UI, HUD, текст), были чёрными; файловые текстуры (RGB24/ABGR8888,
Rmask=0x000000FF -> GL_RGBA) работали — потому поле/интерфейс не отображались, а стадион/часть
текстур были видны. Фикс: CreateSDLSurface и pow2-поверхность текста переведены на
SDL_PIXELFORMAT_RGBA32 (= ABGR8888 на LE, байты R,G,B,A == GL_RGBA) + glPixelStorei(
GL_UNPACK_ALIGNMENT, 1) для тугоупакованных RGB24. Побочно починен sdl_alphablit /
sdl_setsurfacealpha (читали байты как R,G,B,A — для RGBA8888 это было неверно). Проверено на
устройстве (MacBook Air M2): поле, стадион, интерфейс и текст отображаются, ошибок в логе нет.
## [2026-08-13] deploy | macOS-релиз v0.3.0: .app-бандл собран, упакован и проверен
Собрано на MacBook Air M2 (тег v0.3.0, detached HEAD, `cmake --build --parallel 1`, депсы brew:
sdl3 3.4.14, sdl3_image, sdl3_ttf, boost 1.90, openal-soft; без параллелизма — комп виснет).
`tools/release/package_macos.sh` при первом прогоне был неработоспособен, вскрыты и починены четыре
бага (только скрипт упаковки, код игры не тронут): 1) `declare -A` требует bash 4 — macOS-шебанга
`/usr/bin/env bash` даёт 3.2, запуск через brew bash 5; 2) `codesign "$APP"` — относительный путь,
а .app живёт в mktemp-каталоге, и `$OLDPWD` в свежем shell не задан (set -e убивал скрипт до
`mkdir dist`); 3) `cp -L` падал на r--r--r-- dylib (brew) при дублях в closure — `cp -Lf`;
4) главное: `collect_deps` отбрасывал ссылки `@rpath`, из бандла выпадала `libjxl_cms.0.12.dylib`
и не переписывались `@rpath`-зависимости между dylib — на машине без brew приложение не загрузилось
бы; теперь @rpath/@loader_path резолвятся через brew lib, 35 dylib в бандле, id переписаны
безусловно. Файндер-запуск не работал: игра резолвит пути относительно CWD, а macOS стартует .app
из `/` (fatal `file not found or empty: football.config`) — в скрипт добавлен launcher: бинарник
переименован в `gameplayfootball-bin`, на его месте шелл-скрипт, chdir'ящий в
`Contents/Resources/data`. Проверка пакета: `otool -L` — только `@executable_path/../Frameworks`
и системные, без /opt/homebrew; `codesign --verify --deep --strict` OK (adhoc, не нотаризован);
запуск через `open` — окно/меню работают, ошибок в логе 0, закрытие окна — штатное
`Shutting down OpenGLRenderer3D thread`. Артефакт `dist/GameplayFootball-v0.3.0-macos-arm64.zip`
(~23 МБ). Остаток: пакет не нотаризован (нет Apple Developer); бандл пишет log.txt/saves внутрь
Resources/data (см. docs/wiki/открытые-вопросы.md).
## [2026-08-13] session | macOS-релиз v0.3.0 упакован, загружен на GitHub, бандл проверен на устройстве
Сессия: сборка тега v0.3.0 на MacBook Air M2 (без параллелизма), починка tools/release/package_macos.sh
(четыре бага, см. запись deploy), launcher для двойнокликабельного .app, проверка пакета
(otool чист, codesign OK, запуск через open, игра, закрытие окна — штатный выход без ошибок),
загрузка GameplayFootball-v0.3.0-macos-arm64.zip на release v0.3.0, обновление
docs/wiki/открытые-вопросы.md. Незакоммиченные изменения на выходе: tools/release/package_macos.sh,
docs/wiki/открытые-вопросы.md, log.md. Ветка — detached HEAD на v0.3.0.

## [2026-08-13] feat | Windows-сборка на vcpkg manifest mode
Windows переведён с классического `vcpkg install` на manifest mode (`vcpkg.json` в корне: sdl3,
sdl3-image[jpeg,png], sdl3-ttf, boost, openal-soft, sqlite3; отдельный install не нужен —
зависимости ставятся при конфигурации CMake). Под Windows `find_package` работает в CONFIG-режиме
(SDL3/SDL3_image/SDL3_ttf/Boost/OpenAL/unofficial-sqlite3), под остальными ОС сохранён модульный
режим и `Boost_NO_BOOST_CMAKE=ON` (дистрибутивный Boost b2 не даёт компонентных конфигов).
Добавлены: POST_BUILD-копия `data/` рядом с exe (ручные `xcopy`/`cp` больше не нужны),
опция `GAMEPLAYFOOTBALL_WINDOWS_SUBSYSTEM` (по умолчанию ON — GUI; OFF — консоль для дебага),
убран форс-сегфолт при `e_FatalError` в debug-сборке (`src/base/log.cpp`). `package_windows.ps1`
берёт DLL из `build/vcpkg_installed/<triplet>/bin`. Проверено: Windows x86 (MSVC 14.44, VS2022
Build Tools, триплет `x86-windows`) — конфигурация, сборка, запуск до главного меню, оба состояния
subsystem-опции; `determinism_runner check 372c4bbd...` → 0; Linux (gcc, Ubuntu 26.04 через WSL2) —
сборка и `determinism_runner check a672aa0b...` → 0.

## [2026-08-13] session | vcpkg manifest + WSL-плейтест + фиксы геймпада в GUI
Сессия: (1) Windows-сборка переведена на vcpkg manifest mode (см. запись feat выше), апстрим
подтянут (релиз.md + macOS-упаковка), конфликт log.md разрешён; (2) Linux собран и запущен на WSL2
через WSLg: вскрыт краш Xwayland на AMD-драйвере (`amdxc64.so`, segfault в `/mnt/wslg/stderr.log`)
— workaround `[wsl2] gpuSupport=false` в `C:\Users\User\.wslconfig` (окно рендерится в COPY MODE с
рамкой msrdc, это нормально), звук работает через PulseAudio WSLg (OpenAL pulse-бэкенд); (3) фиксы
геймпада в GUI: дефолты подтверждение/назад в `guitask.cpp` были `(1,1)` = B/B, из-за чего с одним
геймпадом B «выбирал», а назад не работал — теперь A/SOUTH = подтвердить, B/EAST = назад;
горячее подключение/отключение: `GAMEPAD_ADDED/REMOVED` обрабатывались только при фокусе окна,
пропущенное отключение навсегда оставляло «мёртвый» геймпад (экран выбора сторон зависал) —
теперь события устройств обрабатываются всегда. По пути откачены две собственные ошибочные правки
(`main.cpp` использовал `controllers.at(0)` = клавиатура вместо `at(1)`, что давало UB в
SetEventJoyButtons и ломало всю GUI-навигацию; `settings.cpp` аналогично). Проверено вручную:
главное меню/пауза с геймпадом, hot-plug на выборе сторон; детерминизм Windows `372c4bbd...` → 0.

## [2026-08-13] fix | macOS: починен headless determinism_runner, снят эталон arm64
`determinism_runner run` на macOS падал на «Generating pitch» с
`FATAL [ResourceManagerPool::GetManager]: ld not find manager for type` (EXIT=139) — хэш не
снимался. Причина: `InitGameContext` (`gamecontext.cpp`) на `__APPLE__` не вызывает
`InitGameSystems` (создание graphics/audio вынесено в `src/main.cpp`, main-thread), а раннер идёт
без `main()` — менеджеры `Texture`/`VertexBuffer`/`AudioSoundBuffer` не регистрировались.
Фикс в `tools/determinism/main.cpp` под `#ifdef __APPLE__`: раннер сам вызывает `InitGameSystems`
и запускает mock-рендерер в потоке (`graphicsSystem->GetRenderer3D()->Run()`). Windows/Linux
не затронуты (правка изолирована Apple-веткой). Эталон `reference-macos-arm64.txt` =
`7b1c49832d6961d27992141d86f00dc710b67016` (MacBook Air M2, AppleClang), воспроизводим между
запусками и пересборками. Побочно вскрыт косметический баг `resourcemanagerpool.hpp:41` —
`"..." + resourceType` сдвигает указатель вместо конкатенации (печатает мусор в FATAL-логе).

## [2026-08-13] session | Проверка ветки build-and-input-fixes на macOS (M2)
Подтверждено на MacBook Air M2, сборка Release, AppleClang, `--parallel 1`:
- **Сборка зелёная** после перевода find_package в модульный режим на всех платформах
  (`CMakeLists.txt`: FindOpenAL/FindSQLite3; SDL3* — fallback на CONFIG; платформенными остались
  Boost и SQLite). Предупреждений об ошибках нет; единственный warning — CMake author-warning о
  deprecated-имени `SQLite::SQLite3` (цель просит переименование в `SQLite3::SQLite3`) — не блокер.
- **POST_BUILD-копия data/ работает**: в `build/` автоматически появились `databases/`,
  `football.config`, `media/` — ручной `cp -R data/. build` больше не нужен (проверено
  пересборкой на месте, данные до/после в `build/`).
- **Детерминизм**: `cd build && ./determinism_runner check 7b1c49832d6961d27992141d86f00dc710b67016`
  → хэш совпал, код выхода 0. Эталон arm64 не изменился после реструктуризации сборки —
  «геймплей не изменился» подтверждено (обновлена `docs/wiki/открытые-вопросы.md`).
- **Запуск**: окно 1280×752, GL 4.1 (Metal), GraphicsSystem/AudioSystem/MenuScene, рендер идёт,
  `Framebuffer state #36053` в норме, в stdout только безобидный GL debug-шум Metal
  («GLD_TEXTURE_INDEX_2D … unloadable»). Чистый выход подтверждён: штатный путь
  Exit→`QuitGame`→`SignalQuit` останавливает scheduler, main шлёт `Message_Shutdown`, рендер
  завершается с `Shutting down OpenGLRenderer3D thread`; деструкторы WorkerThread, blunted::Exit
  cleanup, без [ERROR]/FATAL/сегфолтов.
- **Нюанс выхода на macOS** (существующее поведение, не из ветки): одно закрытие окна
  (`SDL_EVENT_QUIT`) останавливает только рендер-цикл — scheduler продолжает жить и
  `schedulerThread.join()` висит; полное завершение идёт только через Exit в меню (`SignalQuit`).
- **Геймпад (пункт 5) физически не проверен** — контроллер к машине не подключён. Код-ревью
  подтверждает: дефолты confirm/back в `guitask.cpp` — `SDL_GAMEPAD_BUTTON_SOUTH/EAST` (A=B);
  `GAMEPAD_ADDED/REMOVED` обрабатываются вне фокуса окна (`opengl_renderer3d.cpp:2025`). Запуск
  без геймпада при этом не сломан (проверено запуском выше).

## [2026-08-13] deploy | macOS-релиз v0.3.1: .app-бандл собран, проверен, загружен
Собрано на MacBook Air M2 с тега v0.3.1 (detached HEAD, рабочее дерево чистое), Release,
`cmake --build build --parallel 1`, депсы brew (sdl3 sdl3_image sdl3_ttf boost openal-soft).
`cmake -B build` — только author-warning о deprecated-имени `SQLite::SQLite3` (не блокер);
POST_BUILD-копия работает: в `build/` `media/`, `databases/`, `football.config` без ручного cp.
Детерминизм: `./determinism_runner check 7b1c49832d6961d27992141d86f00dc710b67016` — хэш совпал
с эталоном arm64, код выхода 0. Смоук: старт, окно (GL 4.1 Metal), MenuScene отрисован; выход
по SIGTERM (SDL переводит в SDL_EVENT_QUIT) — штатное `Shutting down OpenGLRenderer3D thread`,
без [ERROR]/FATAL; зависание scheduler'а после этого — уже документированный нюанс macOS
(полный выход — только Exit в меню, см. запись от 2026-08-13). Упаковка
`/opt/homebrew/bin/bash tools/release/package_macos.sh 0.3.1 build` (bash 5, `declare -A`):
35 dylib в `Contents/Frameworks`, launcher + `-bin`. Проверка пакета из распакованного zip:
`otool -L` — только `@executable_path/../Frameworks` + системные, без /opt/homebrew и @rpath
во всех dylib; `codesign --verify --deep --strict` OK (adhoc, не нотаризован); запуск .app через
launcher из чистой распаковки — меню работает, лог пишется внутрь бандла, ошибок нет.
Загружен `gh release upload v0.3.1 dist/GameplayFootball-v0.3.1-macos-arm64.zip`
(23 772 063 байт), `gh release view` — ассет на месте (macOS артефакт v0.3.1 стал доступен,
Windows x86/x64 и Linux были уже загружены). Код не менялся — вики не затронута.

## [2026-08-29] session | Конвейер данных из Transfermarkt: схема, API-фиксы, вики
Спроектирован конвейер данных для обеих игр (GF + football_collection): единый канон-JSON на
TM-id, конвертеры в GF SQLite и Flutter-assets. Схема и решения — docs/wiki/данные-из-transfermarkt.md,
экосистема — docs/wiki/смежные-проекты.md.

- В transfermarkt-api починены 500 у эндпоинтов тренеров и состава сборных (extract_from_url резал
  домен, re.match → None), исправлен возраст в составе сборной (брался игровой номер) и добавлено
  поле shirtNumber. Коммит e53ff69.
- Стратегия сбора: состав клуба (clubs/{id}/players) — 1 запрос на команду вместо профиля на
  игрока; национальная принадлежность игрока — только из состава сборной; coach_id — только через
  /mitarbeiter/ (в API его нет); weight в TM отсутствует.
- GF: ветка develop — дом изменений поверх master (README-заметка в master, 8b9df5c), рабочая
  ветка squads-update. Добавлены вики-страницы и глоссарий.
- Во всех трёх проектах данных (football_collection, transfermarkt-api, transfermarkt_scrapper)
  инициализирован контур вики/AGENTS.md/хуков/плагина opencode.
- Скрейпер: только дизайн (переписывание на базе ветки fix: лимит 2 rps, resume-кэш, curl_cffi,
  идемпотентность) — docs/wiki/конвейер.md в скрейпере.

## [2026-09-02] session | Калибровка 22 статов по FIFA (fifagc) во вьювере данных
Прототип в `data_max` (вне репо, вьювер :9090, 120K игроков): 22 стата GF пересчитаны из реальных
FIFA-атрибутов вместо «формулы из воздуха» (OVR + позиция + детерминированный шум давали
белиберду — у Неймара отбор 87 и ловкость 64).

- Скреп 1838 страниц fifagc (`fetch_fifa_pages.py`, resume-кэш `fifa_pages.json`, ~1.1 rps):
  29-34 атрибута (у вратарей +Diving/Handling/Kicking/Reflexes/Positioning), POT, слабая нога,
  особые приёмы, рост/вес, мульти-позиции.
- 1864 из 120K совпали с fifagc — реальные статы (маппинг 29-34 → 22 через `fifa_mapping.py`,
  часть прямые копии, часть смеси: shot = 0.5·Finishing + 0.25·Long Shots + …). 11 ошибочных
  матчингов (вратарь-однофамилец) отсеяны по признаку «есть Diving ↔ позиция не GK».
- 118K остальных — архетипы позиций (GK/CB/FB/DM/CM/AM/W/CF): среднее атрибута + наклон по OVR
  (OLS), +детерминированный разброс ±3. Профили позиций — `analyze_positions.py`.
- POT: реальный у совпавших, модель дельты pot−ovr ~ (ovr, возраст, позиция) у остальных
  (pot не ниже ovr). Вес: реальный / регрессия `−41.5 + 0.671·рост + поправка позиции` (MAE 3.6 кг).
  Слабая нога/приёмы: реальные / средние по позиции.
- Неймар (архетип AM): отбор 87→64, ловкость 64→86, дриблинг 87, видение 90, pot 85, вес 69.
- Масштаб во вьювере — FIFA 0-99; GF хранит статы 0-1, для `profile_xml` нужна нормализация.
- OVR не менялись (остались калиброванными по fifagc); бэкап оригинала — `ratings_output.bak.json`.

## [2026-09-03] feat | генератор китов: standalone kit-generator (скрипт + редактор)

Собран standalone-инструмент kit-generator (Desktop/projects/kit-generator), который
из данных TM (цвета профиля + логотипы) генерирует комплекты формы для GF: палитра клуба,
раскладка по частям (футболка/шорты/гетры) и комплектам (home/away/gk), узоры футболки.

- Регионы 	emplate_kit.png выводятся программно: футболка y 0–418, шорты y 419–582,
  гетры y 583–766; швы/фон чёрные. Второй регион (ранее «шорты+носки» слиты) разбит по
  координатам — части формы красятся раздельно. Затенение = зелёный канал шаблона
  (трикотаж): цвет_региона × (G/база).
- Палитра: профильные TM-цвета → из логотипа (ресайз, игнор прозрачности, квантование) →
  нейтрали. Автоподбор по контрасту: home = primary, away = третий/контрастный нейтрал,
  gk = максимально отличный. Проверено: 0 клубов с совпадающими home/away, 0 с тремя
  одинаковыми комплектами.
- Покрытие: 4406 клубов (палитра у 4401, у 5 белый/чёрный из-за отсутствия логотипа),
  13218 PNG (199 МБ) + specs.json.
- Web-редактор editor/server.py: клуб за клубом, логотип + цвета, назначение цветов
  частям формы и узор, live-превью, правки в specs.json.
- Экспорт export_game.py → images_teams/<league_id>/<club_id>_kit_01/02/gk.png.
  Per-team GK требует правки 	eam.cpp (см. NOTES.md инструмента).

## [2026-09-05] session | восстановлена vcpkg/MSVC-сборка на Windows; MinGW откатан

После импорта данных (БД 184 МБ, 67 212 фото игроков в `faces/`) игра не запускалась:
`build/` был переконфигурирован с vcpkg/MSVC на MSYS2/MinGW (`C:/msys64/mingw64`), а в
`CMakeLists.txt` (незакоммиченно) добавлена поддержка MinGW (`if(WIN32 AND NOT MINGW)`,
`--start-group`). MinGW-бинарник динамически тянет `libwinpthread-1.dll`, `libgcc_s_seh-1.dll`,
`libstdc++-6.dll` из `C:\msys64\mingw64\bin`, которого нет в PATH — отсюда ошибка запуска.

Решение:
- MinGW-правки в `CMakeLists.txt` откачены к HEAD (чистая vcpkg/MSVC-линия).
- MSVC на машине отсутствовал — установлен VS Build Tools 2022 с workload VCTools.
- `build/` пересоздан под `Visual Studio 17 2022` / `-A Win32`, vcpkg triplet `x86-windows`
  (первичная установка зависимостей vcpkg заняла ~48 мин).
- Сборка Release прошла; данные (включая `faces/` — 67 212 файлов) копируются
  `copy_data_post_build` в `build/Release` автоматически.
- Игра запускается и работает. Вывод: метод сборки на Windows — только vcpkg/MSVC
  (`x86-windows`, Win32), как в AGENTS.md; MinGW на этой машине не нужен.

## [2026-09-05] session | починка выбора команд, дедуп данных, выбор страны

После импорта данных игра «зависала» на экране выбора команд. Причины и фиксы:

1. **Краш на битом PNG**: `nationalteams.png` был 0 байт (конвейер записал пустым);
   `Gui2Image::LoadImage` разыменовывал NULL от `IMG_Load` → сегфолт (чёрный фон успевал
   отрисоваться — отсюда «просто фон»). Фикс: валидный `CNAT.png` из скрейпера + NULL-гард в
   `LoadImage` (любой битый файл больше не роняет игру).
2. **Дубли данных**: источник `clubs.json` содержит один и тот же клуб 2–3× внутри лиги.
   БД дедуплицирована: команда на (league_id, name), игрок на (team_id, firstname, lastname,
   role) → 4967→4653 команд, 135898→126826 игроков. Скрейперу чинить источник.
3. **Коллизия имён**: лига 1 «Premier League» была Арменией; переименована в
   «Armenian Premier League» (английская — id 139). Лига 280 «National Teams» → `country_id NULL`.
4. **Выбор страны**: TeamSelectPage стал трёхступенчатым (страна → лига → команда);
   «National Teams» — спецпункт на первом этапе, сборная выбирается без ступени лиги.
5. **Производительность**: убран per-entry `Redraw()` в `Gui2IconSelector::AddEntry`
   (O(n²)); загрузка стала быстрой. Добавлены `SetSelectedEntry`/`SetSelectable`.
6. **Фото игроков не копируются**: `faces/` (67 тыс.) в игре не используется; исключён из
   `copy_data_post_build` (`tools/copy_data.cmake`, PATTERN faces EXCLUDE).

Проверено: навигация страна→лига→команда и путь сборных проходят без краша, доходит до
MatchOptions. Сборка vcpkg/MSVC, POST_BUILD копирование данных теперь быстрое.

## [2026-09-05] session | матч и выбор команд: данные и UI

После ввода стран/лиг/команд пользователь нашёл ещё два блока:

1. **Матч виснул после розыгрыша, на поле не хватало игроков**. Причина в данных:
   `formation_xml`/`tactics_xml` были NULL у ВСЕХ команд, а `formationorder` импорт записал
   как индекс группы ролей (0=GK, 1=защита, 2=полузащита, 3=нападение), а не слот XI.
   Фикс: залит дефолт 4-2-3-1 и дефолтные тактики; `formationorder`/`nationalteamformationorder`
   пересобраны скриптом (XI = 1 GK + 4 DEF + 5 MID + 1 CF по base_stat, бенч 11+);
   команды с <11 игроками отфильтрованы из выбора команд (иначе assert/freeze).
2. **UI**: при смене страны логотипы клубов «проносились» в углу (иконки позиционировались
   при AddEntry до Redraw) — теперь иконки скрыты за экраном до Redraw и показываются только
   на своих местах; панель игрока 2 строится лениво (вход в выбор команд быстрый); флаги
   стран из скрейпера (маппинг TM-id → game-id по имени) вместо оранжевых квадратов.

Проверено: выбор страны→лиги→команды, путь сборных, запуск матча — игра работает,
зависаний нет.

## [2026-09-07] fix | асинхронное создание GUI-текстур: экран выбора команд ~750 мс → ~40 мс

Жалоба: экран выбора команд после выбора сторон грузился 1–2 с. Причина (замерено
инструментацией): каждый `CreateImage2D` на гейм-потоке делал синхронный круг до
рендер-потока — `Texture::CreateTexture` шлёт `Renderer3DMessage_CreateTexture` и
блокируется на `Wait()`, а рендер-поток занят vsync-кадром (~16 мс), т.е. ~10–30 мс на
текстуру. Страница создаёт ~70–80 текстур (6 икон-селекторов × пул из 9 икон + фоны +
подписи + кнопки).

Фикс: 2D-оверлей-текстуры (GUI) создаются асинхронно — `Texture::CreateTextureAsync`
без `Wait()`, id записывается в ресурс на рендер-потоке; создание+первичная заливка в
одном сообщении; `Renderer3DMessage_UpdateTexture` резолвит id из ресурса на
рендер-потоке (FIFO очереди гарантирует порядок). Синхронный путь оставлен для текстур
геометрии/поля/теней (скрыт экраном загрузки матча). Конструктор `TeamSelectPage`:
~750 мс → ~43 мс. Логотипы/флаги/эмблемы отображаются корректно.

## [2026-09-07] fix | имя над игроком пропадало после обновления составов

Жалоба: в матче не над всеми игроками имя — появляется/исчезает по игрокам в разных командах
и сборных; нет имени даже при ручном выборе игрока. После обновления составов в БД ~2.1 тыс.
игроков (844 клуба, 26 сборных) имеют пустую фамилию (`lastname = ''`, TM хранит их под одним
именем: Endrick, Cézar, Nené…). Подпись над игроком и все UI показывают только
`GetLastName()` (`player.cpp:104,383`) → пустая строка, «нет имени».

Фикс: `PlayerData::GetLastName()` (`src/data/playerdata.hpp:23`) при пустой фамилии возвращает
имя (`firstName`); чинятся подписи над игроками, меню расстановки, planmap и сообщения о голах.
Проверено сборкой (MSVC 2022, Win32/Release).

## [2026-09-07] fix | подпись над принимающим пас

После фикса пустых фамилий жалоба осталась («у сборной Бразилии почти все не подписаны»).
Диагностика (`log.txt`, временный Log в `SelectPlayer`/`SetSelectedPlayerID`) подтвердила: имена
в данных на месте и выбор/переключение работают; проблема была в том, что подпись показывалась
только над управляемым игроком — по одному имени на команду в момент.

Фикс: подпись теперь показывается над **управляемым** игроком **и над принимающим пас**
(`Team::GetHumanPassTarget`, ставится в момент касания паса в `humanoid.cpp:507`, висит до приёма
мяча/касания соперника/таймаута `humanPassTargetTimeout_ms` = 2000 мс,
`src/gamedefines.hpp:70`). `buf_nameCaptionShowCondition` расширен в
`src/onthepitch/player/player.cpp:353`; логика очистки — `src/onthepitch/team.cpp:330`.
Проверено сборкой (MSVC 2022, Win32/Release).

## [2026-09-07] decision | подпись над всеми активными игроками

Промежуточные условия («управляемый + владеющий + цель паса») каждый раз оставляли игрока без
имени (после Бразилии — Барселона: «один из двух ЦЗ не подписан»). Диагностика показала:
у владеющего мячом ЦЗ `humanCtl=0` (ведёт ИИ), `desig=this:1` — он владеющий, но подпись не
показывалась, потому что ветка `GetDesignatedTeamPossessionPlayer()` выполнялась только для
команд без людей.

Решение: `buf_nameCaptionShowCondition = true` для всех активных игроков
(`src/onthepitch/player/player.cpp:358`). Механизм `Team::GetHumanPassTarget` стал не нужен и
удалён (обратно: `humanoid.cpp:507`, `team.hpp`, `team.cpp`, `gamedefines.hpp`).
`PlayerData::GetLastName()`-фолбэк остаётся (у одноимённых игроков подпись = имя).
Проверено сборкой (MSVC 2022, Win32/Release).

## [2026-09-07] decision | откат «имя над всеми» — возврат к исходной логике

Решение «показывать имя над всеми активными игроками» оказалось ошибочным: задача была чинить
существующую логику (имя над **выбранным** игроком), а не менять её. Настоящим багом был пустой
`lastname` после импорта составов — его чинит `PlayerData::GetLastName()`-фолбэк.

Откат: `buf_nameCaptionShowCondition` снова = `IsHumanControlled(id)`, а для команд без людей —
`GetDesignatedTeamPossessionPlayer() == this` (`src/onthepitch/player/player.cpp:358`).
Итоговое изменение — только фолбэк имени (`src/data/playerdata.hpp:23`). Проверено сборкой
(MSVC 2022, Win32/Release).

## [2026-09-07] decision | подпись над всеми игроками команды человека

«Только над выбранным» снова дал жалобы («у второго ЦЗ, Ольмо, Рафиньи нет надписи»).
Диагностика по выбранным игрокам показала: каждый выбранный получает `cond=1` и имя, но игроки,
которых игра сама не переключает (Рафинья в логе не появлялась ни разу), остаются под ИИ и без
имени — человек воспринимает их как «своих» и ждёт подпись.

Итог: подпись над **всеми активными игроками команды человека** + над владеющим у полностью
ИИ-команды (`src/onthepitch/player/player.cpp:358`). `PlayerData::GetLastName()`-фолбэк остаётся.
Проверено сборкой (MSVC 2022, Win32/Release).

## [2026-09-07] decision | подпись: выбранный + владеющий мячом

Диагностика подтвердила: у выбранного игрока подпись всегда рисуется (логика `cond=1` + позиция
в границах экрана, `OFFSCREEN` записей нет) — бага в цепочке отображения нет. Игроки «без имени»
были просто не выбранными (ИИ-подконтрольные партнёры / владеющие у ИИ-команды).

По выбору пользователя итоговое правило (`src/onthepitch/player/player.cpp:358`):
`buf_nameCaptionShowCondition = IsHumanControlled(id) || GetDesignatedTeamPossessionPlayer() == this`
— подписываются **выбранный** и **владеющий мячом** (у команды человека оба, у полностью
ИИ-команды — только владеющий). Единственная правка данных — фолбэк имени при пустой фамилии
(`src/data/playerdata.hpp:23`). Проверено сборкой (MSVC 2022, Win32/Release).

## [2026-09-07] fix | подпись не отображалась у игроков с акцентированными фамилиями

Жалоба: подписи есть только у Родри и Рафиньи (чистый ASCII через фолбэк имени), у остальных
(Cubarsí, García, Koundé, бразильцы Éderson/Cézar…) подпись при выбранности/владении не видна.
Диагностика: логика (`cond=1`) и позиция в кадре корректны — проблема в финальной отрисовке.

Причина: `Gui2Caption::SetCaption` (`src/utils/gui2/widgets/caption.cpp:125`) прогонял имя через
`std::transform(::toupper)` **побайтово**. Для UTF-8-символов с акцентами (байты ≥0x80, на
MSVC `char` знаковый → `toupper` на отрицательном значении = UB) байты последовательности
ломались, SDL_ttf не мог отрисовать строку → пустая подпись.

Фикс: верхний регистр только для ASCII (`< 128`), байты UTF-8 не трогаются
(`src/utils/gui2/widgets/caption.cpp:119`). Вместе с фолбэком имени при пустой фамилии
(`src/data/playerdata.hpp:23`) и правилом «выбранный + владеющий»
(`src/onthepitch/player/player.cpp:358`) подписи теперь корректны у всех игроков.
Проверено сборкой (MSVC 2022, Win32/Release).

## [2026-09-07] fix | подпись не отображалась у части игроков: гонка асинхронного создания текстуры

Жалоба нестабильная: у части игроков (меняется от запуска к запуску: то Гарсия/Кунде, то ЦЗ/Ямаль)
подпись не видна даже когда они владеют мячом. Диагностика (DBG3–DBG7) показала: логика
(`cond=1`), позиция в кадре, видимость (`vis=1`) и включение картинки в 2D-сцену (`imgEnabled=1`)
всегда корректны — рвётся финальная текстура.

Причина — гонка асинхронного создания 2D-текстур (`Texture::CreateTextureAsync`, commit
«async 2D texture creation»). Подпись создаётся при старте матча, `textureID = -1` до обработки
CreateTexture рендер-потоком, а конструктор сразу ресайзит картинку (текст шире 1%×2% исходника).
`Texture::ResizeTexture` использовал `textureID` напрямую (`assert(textureID != -1)`):
при -1 ресайз молча проваливался, текстура оставалась крошечной с пустой поверхностью, а
`UpdateTexture` (`glTexSubImage2D`) размер не меняет → подпись невидима навсегда для этого игрока.
Работало только если рендер-поток успевал создать текстуру до ресайза — отсюда нестабильность.

Фикс: `Renderer3DMessage_ResizeTexture` теперь принимает `Resource<Texture>` и резолвит GL id в
`Execute` на рендер-потоке (как уже делал `UpdateTexture`); FIFO-очередь гарантирует, что
CreateTexture (который пишет id в ресурс) обработан раньше
(`src/systems/graphics/rendering/r3d_messages.hpp`, `resources/texture.cpp`,
`objects/graphics_overlay2d.cpp`). Затрагивает все 2D-картинки, не только подписи. Проверено
сборкой (MSVC 2022, Win32/Release).

## [2026-09-07] decision | закрепление: реальная причина и откат побочных правок

Подтверждено пользователем: подписи работают стабильно. Реальная причина бага — гонка
асинхронного создания текстуры (`ResizeTexture` с `textureID == -1`), фикс выше.

Приведение в порядок:
- **Откат**: изменение логики подписей «выбранный + владеющий» (`player.cpp`) — это была попытка
  чинить количество подписей, а не баг; возвращена исходная логика (команда с человеком —
  выбранный игрок, полностью ИИ — владеющий).
- **Оставлено** (настоящие фиксы, без которых подпись снова сломается):
  - `PlayerData::GetLastName()`-фолбэк при пустой фамилии (`playerdata.hpp`);
  - верхний регистр только для ASCII в `Gui2Caption::SetCaption` (`caption.cpp`);
  - отложенный ресайз текстуры через ресурс (`r3d_messages.hpp`, `texture.cpp/.hpp`,
    `graphics_overlay2d.cpp`).
- Вики `матч.md` приведена к фактическому поведению и фиксам.

## [2026-09-08] session | фиксация пайплайна данных: договор, Слой 0, программа data v2

Решение: до первого публичного релиза делаем **data v2** — «лучшую версию» без поломки текущей
игры (сборка в staging, подмена `databases/default` в конце). Состав: плоский canon становится
единым контрактом вместо вложенного `data/full/*`, схема БД v2 (`tm_id` + `PRAGMA user_version` +
`manifest.json`), данные версионируются бандлами вне git + реестр `data-versions.json` в репо игры.
Программа и реальный статус звеньев зафиксированы в `docs/wiki/открытые-вопросы.md`.

Слой 0 (единый договор):
- Новая вики-страница `docs/wiki/пайплайн-данных.md` — звенья, границы форматов, порядок прогона,
  идентичность (rowid — не контракт, ключ — TM-id), версионирование. Обновлены `index.md`,
  `данные-из-transfermarkt.md`, `глоссарий.md`, `открытые-вопросы.md`.
- `tm-gf-import` стал git-репо: `README.md` + `AGENTS.md`, `PROMPT.md` помечен устаревшим.
- Ролевые AGENTS-секции (ссылка на договор): kit-generator, transfermarkt_scrapper,
  transfermarkt-api; созданы `AGENTS.md` для ratings-generator и tm-gf-face-generator.
- Вымышленный конвертер `build_gf_database.py` вычищен из доков скрейпера (роль исполняет
  `tm-gf-import`); зафиксировано, что прод-формат канона — `data/full/*`, а плоский `data/canon`
  реализован, но не в проде.

## [2026-09-08] fix | сборные: сортировка состава + дефолтная формация в импорте

Два бага, найденные сверкой вики с кодом:

1. **Сортировка состава сборной** (`src/data/teamdata.cpp`): флаг `national` никогда не
   выставлялся (в SELECT не было источника), а строка 249 ссылалась на несуществующую колонку
   `nationalformationorder` — состав сборной шёл в порядке вставки. Фикс: в SELECT добавлен
   `leagues.name as league_name`, `national` определяется по имени лиги «National Teams»
   (строка 74), сортировка — по существующей колонке `nationalteamformationorder` (строка 249).
   Проверено сборкой (MSVC, Release).
2. **Дефолтная формация в импорте** (`tm-gf-import`): `builders/teams.py` писал
   `formation_xml`/`tactics_xml` = NULL, из-за чего свежий импорт давал неиграбельную БД
   (матч зависал; раньше чинилось отдельным прогоном заливки, log.md 2026-09-05). Теперь
   `schema.py` содержит `DEFAULT_FORMATION` (4-2-3-1) и `DEFAULT_TACTICS` — байт-в-байт дефолты
   игры из `mainmenu.cpp:492-566`, `teams.py` пишет их для клубов и сборных. Дефолты сверены
   с shipped-БД (602/814 байт).

## [2026-09-08] fix | расстановка: роли сборных + кодировка порядка как слоты XI

Полевой тест выявил, что при игре за сборную нападающий вставал на фланг (Кейн у Англии — на
левом краю). Правка `teamdata.cpp` (см. выше) лишь обнажила две ошибки в ДАННЫХ:

1. **Роли сборных почти все `CM`**: `national_teams.json` отдаёт игрокам сборных только грубые
   категории (`Defender`/`Midfield`/`Attack`), а импорт их не знал и сваливал в `CM` (Англия:
   23 CM из 26). Фикс в импорте: роль игрока сборной берётся из его клубной записи по TM-id,
   иначе — грубый маппинг (`Defender`→CB, `Midfield`→CM, `Attack`→CF) — `mapping.py`
   (`NT_POSITION_MAP`), `builders/players.py`.
2. **Порядок кодировал рейтинг, а не расстановку**: игра ставит игрока с индексом `i` в слот
   `p(i+1)` формирования, поэтому `formationorder`/`nationalteamformationorder` должны быть
   слотами XI (GK→LB→CB→CB→RB→CM→CM→LM→AM→RM→CF), а не силой. В shipped-БД это был рейтинг по
   `base_stat` (Кейн 0.901 → индекс 1 → левый защитник). Новый `tm-gf-import/builders/lineup.py`
   выбирает XI по `base_stat` в пределах роли слота (точное совпадение, затем фолбэк по группе),
   бенч 11+.

Импорт и существующая БД приведены к правилу: `patch_lineups.py --db ... --clubs ... --nt ...`
(обновлено 3735 ролей сборных + пересчитаны порядки всех команд). Проверено: Англия — Кейн на
CF (индекс 10), состав корректный 4-2-3-1; Сити — Холанд на CF; Бавария — Кейн на CF.
Патч применён к обеим копиям БД (`data/databases/default` и `build/Release`). Бэкап — в
`%TEMP%\opencode\db_backup\`. Остаточные 254 клуба + 11 сборных с не-нападающим в CF — команды
без форвардов в составе (фолбэк группой).

Внимание: расстановки изменились для всех команд → при ручной проверке детерминизма
(`tools/determinism`) эталоны по затронутым командам нужно перебазировать.

## [2026-09-08] fix | точная позиция игрока сборной — со страницы состава, не профиль

Пользователь указал: роль из клубной записи — ненадёжный фикс (роль в сборной может отличаться
от клубной), докачивать профили игроков нельзя (TM банит такие запросы), а точная позиция есть
прямо на странице состава сборной. Проверено по живой странице: `/-/kader/verein/{id}` показывает
точные позиции во вложенной `table.inline-table` (вторая строка), а API брал только грубое
`title` у номера (Goalkeeper/Defender/Midfield/Attack).

Фикс:
- `transfermarkt-api`: XPath `FINE_POSITION` (`app/utils/xpath.py`, `NationalTeams.Players`),
  сервис `services/national_teams/players.py` возвращает `positionFine` (точное); грубое
  `position` оставлено для совместимости; схема — `position_fine`.
- `transfermarkt_scrapper`: `build_pilot_json.py` кладёт в NT-игроков `main_position =
  positionFine or клубная роль`; `fetch_nt_positions.py` (докачка профилей) удалён.
- `tm-gf-import`: роль NT-игрока = `POSITION_MAP[main_position]` → клубная роль → грубый маппинг.

Применено: поднят API (:8001), очищен кэш составов сборных (246 строк), перезапрошены все 246
составов (0 ошибок), пересобран `national_teams.json` (у Англии `main_position` 26/26),
`patch_lineups.py` по БД — состав Англии остался корректным (Кейн — CF). БД обновлена в обеих
копиях (`data/databases/default`, `build/Release`). Профили игроков не докачивались.

## [2026-09-08] fix | убран клубный и грубый фолбэк ролей сборных (main_position — 100%)

Проверка покрытия: `main_position` (точная позиция со страницы состава сборной) есть у всех
5439/5439 игроков сборных → клубный маппинг роли и грубые категории
(Defender/Midfield/Attack → CB/CM/CF) стали мёртвым кодом и удалены.

- `POSITION_MAP` (tm-gf-import): добавлены обобщённые значения страницы состава —
  `Midfielder`→CM (158), `Striker`→ST (88), `Defender`→CB (83); удалены `NT_POSITION_MAP` и
  `map_nt_position`.
- `builders/players.py`: роль NT-игрока = `map_position(main_position)`; `club_roles` и
  фолбэк на грубую позицию удалены (и из `build_club_players`, и из `import.py`).
- `patch_lineups.py` упрощён: только `--db --nt`, роль из `main_position`; игроки без
  `main_position` не трогаются.
- Поле `position` (грубое) в `national_teams.json` оставлено — его читают ratings-generator
  (OVR/возраст) и face-generator.

Применено: `patch_lineups.py` по БД — изменилось 978 ролей (т.е. у ~18% игроков роль в сборной
отличается от клубной, как и предполагалось). Состав Англии остался корректным (Кейн — CF).
БД обновлена в обеих копиях (`data/`, `build/Release`).

## [2026-09-09] fix | позиция сборных: единое поле `position` + рейтинг NT-only игроков

Переименование (по замечанию: `positionFine`/`main_position` — странные имена, в клубных записях
это просто `position`):

- `transfermarkt-api`: сервис NT-составов возвращает точную позицию в поле `position`
  (как в клубных записях), `positionFine`/`position_fine` удалены; грубая категория остаётся
  только как внутренний фолбэк парсера.
- `transfermarkt_scrapper`: `build_pilot_json.py` пишет NT-игрокам `position` = точная позиция
  (поле `main_position` убрано). Составы сборных перезапрошены (246), `national_teams.json`
  пересобран (у Англии `position` = Centre-Back/Left-Back/...).
- `tm-gf-import`: роль NT-игрока = `map_position(position)`; `patch_lineups.py` — по `position`.

Рейтинг NT-only игроков (по вопросу «а как же игроки, которых нет в клубных записях, напр. КНДР?»):

- Был пробел: `ratings-generator` обходит только клубные составы; игроки сборных вне клубных
  данных (1895, в т.ч. вся сборная КНДР) оставались **без рейтинга** → в БД `base_stat=0.6`
  и **пустой `profile_xml`**, а `PlayerData::GetStat` ассертит на отсутствующем стате
  (`playerdata.cpp:118-119`) — матч с такой сборной рисковал крашем.
- Фикс в `ratings-generator`: вычисление OVR вынесено в `compute_ovr` (клубный путь не изменился,
  проверено: 2/2000 расхождений ≈ шум снимков), добавлен проход по сборным — NT-only игроки
  считаются с нейтральной экономикой лиги (eco=1.0, `league=""` → нулевые группы калибровки).
- `ratings-generator/output.json` перегенерирован: 121927 игроков (было ~120k), NT-only 1895/1895
  оценены (КНДР: 29/29, OVR 55–64).
- `tm-gf-import/patch_nt_stats.py` (новый): пишет NT-игрокам `base_stat`/`profile_xml`/`weight`
  из новых рейтингов по имени внутри сборной. Применено: обновлено 5315 NT-игроков; пустых
  `profile_xml` у NT осталось 12 из 5362 (несостыковки имён). БД обновлена в обеих копиях
  (`data/`, `build/Release`); состав Англии по-прежнему корректен.

## [2026-09-09] session | закрытие: tm-id-матчинг — следующая задача

Сессия: договор пайплайна ([[пайплайн-данных]]), сверка вики всех репо с кодом, починка
расстановок (сборные по лиге + `nationalteamformationorder`, слоты XI в `builders/lineup.py`,
дефолтная формация в импорте), точная позиция игрока сборной со страницы состава (поле `position`
API), рейтинг NT-only игроков (ratings-generator, нейтральная лига), чистка 12 сирот старого среза
(10 удалено, 1 переименован Soulisack→Soulisak, 1 дубликат убран). Всё закоммичено и запушено
в 7 репо; создан `Open-Gameplay/tm-gf-import`.

Вывод сессии (эталон для следующей): **матчинг по имени ненадёжен** — фантом «Roberto Owono»
(нет в текущих данных), спеллинг «Soulisack/Soulisak» (тот же игрок, tm 1427661), риск коллизий.
Нужно везде опираться на TM-id. Следующая задача — Слой 2: колонки `tm_id` в БД и перевод всех
матчингов на id (handoff `docs/reports/2026-09-09-handoff-tm-id-matching.md`).

## [2026-09-09] session | Слой 2: схема БД v2 — матчинг данных на TM-id

Выполнен handoff `docs/reports/2026-09-09-handoff-tm-id-matching.md`: весь конвейер переведён на
матчинг по **TM-id**, имя — только отображение/логи.

- **Схема БД v2** (`tm-gf-import/schema.py`): колонки `tm_id` в players/teams/leagues/countries
  (NULL — только синтетика: «International», «National Teams»); rowid остаются ключами, которыми
  читает игра. `builders/*` пишут `tm_id` при вставке; `countries` в импорте клюются по TM-id, а не
  по имени; `patch_lineups.py`/`patch_nt_stats.py` матчат сборных/игроков по `tm_id`; дедуп дублей
  клуба внутри лиги — по TM-id (заменил прежний name-based дедуп, 4721→4406 клубов).
- **Сквозная проверка репо (субагенты по одному на репо)**: scrapper/api/kit — уже keyed по id
  (только осознанный name-based `kits/linked.py` и `/search/{name}` API не трогали); **ratings** —
  найдена и исправлена калибровка OVR по имени лиги (лиги-однофамильцы «Premier League»,
  «Bundesliga», «Ligue 1» получали чужой остаток) → переведена на TM-id лиги, `output.json`
  перегенерирован; **face** — `--countries` переведён на id страны, `load_players` несёт
  `_country_id`.
- **Пересборка БД** в staging (`import.py --db-only`, бэкап прежней), верификация (tm_id заполнен
  у всех строк; Англия — Кейн на CF слот 10; Экв. Гвинея — ровно 1 GK Jesús Owono tm 631693, нет
  Roberto; Лаос — Soulisak Souvankham tm 1427661; пустых `profile_xml` у игроков сборных — 0),
  подмена `data/databases/default` + `build/Release/databases/default`. Сборка (MSVC x86) проходит,
  матч headless (`determinism_runner`) работает.
- **Детерминизм**: данные сменились (свежий срез + калибровка лиг по id), раннер играет армянские
  клубы, а лига Армении «Premier League» раньше получала чужой остаток — эталон x86 перебазирован
  `372c4bbd…` → `60380de0…` (см. [[открытые-вопросы]]). linux/x64/macos-эталоны требуют пересъёмки
  на своих платформах.
- **НЕ делалось** (следующие милстоуны data v2): плоский canon, `PRAGMA user_version`,
  `manifest.json`/бандлы, пакеты лиг, LAN, карьера. `football_collection` не затронут.

## [2026-09-09] fix | Ярко-красные волосы у игроков с haircolor=red
Корень: `ResourceManager::Fetch` кэширует по **basename** (`get_file_name`,
`src/managers/resourcemanager.hpp:47`), без пути. Отладочный `media/objects/helpers/red.png`
(сплошной `(255,0,0)`, грузится при старте `GameContext`) перехватывал ключ `red.png`, и волосы
игроков с `haircolor=red` (302 игрока / 224 клуба / 31 сборная, в т.ч. 3 в Aruba: Lentink,
Vandepitte, Bennett) получали чистый красный вместо `hair/red.png`. Правки `hair/red.png` не
влияли — файл не читался. Лечение: хелпер переименован `helpers/red.png` → `helper_red.png`
(`red.ase` обновлён), `hair/red.png` приведён к натуральному медному (насыщенность 77 → 51).
Ловушка задокументирована в [[архитектура]].

## [2026-09-09] feat | Версионирование данных: user_version + manifest.json, чистота источника

Закрыт хвост data v2 «PRAGMA user_version + manifest.json» и устранена причина дедупа в импорте.

- **Версионирование** (`tm-gf-import`): `SCHEMA_VERSION = 2` в `schema.py`; импорт ставит
  `PRAGMA user_version` и пишет `manifest.json` рядом с БД (schema_version, data_version =
  дата среза `--snapshot-date`, snapshot_id = дата + git-хеши пяти инструментов, coverage,
  sha256 БД после WAL-checkpoint). Игра (`src/gamecontext.cpp`, `src/gamedefines.hpp`):
  константа `databaseSchemaVersion = 2`; при старте жёсткая сверка `user_version`
  (несовместимая БД → понятная ошибка, проверено на user_version=99) и лог `data_version`
  из манифеста (минимальный JSON-ридер, без зависимостей).
- **Источник без дублей** (`transfermarkt_scrapper`): `build_pilot_json.py` перезаписывает клуб
  по TM-id внутри лиги вместо аппенда (лиги Apertura/Clausura давали клуб 2–3×);
  `data/full/clubs.json` пересобран из resume-кэша офлайн: 4406 клубов / 121 473 игрока,
  совпадение с прежним файлом по id — 0 потеряно, 0 добавлено (и сборные: 246/5439 — 0/0).
  Дедуп из импорта (`builders/teams.py`, `players.py`) удалён — источник теперь чистый.
- **БД** пересобрана, подменена в `data/databases/default` + `build/Release`; контент бит-в-бит
  совпал с прежней (sha256 тот же), детерминизм не сдвинулся (`60380de0…`). Сборка проходит,
  `determinism_runner` логирует «Game data: schema v2, data version 2026-09-09».
- Побочно: восстановлены индексы `idx_players_team/national` в схеме импорта (были только в
  старой БД вручную; без них выбор команд ~1.5–2.5 с на лигу, см. [[база-данных]]); `manifest.json`
  и `-wal`/`-shm` добавлены в `.gitignore` данных.
- **НЕ делалось**: плоский canon, бандлы `GameplayFootball-data-<ver>.zip`, `data-versions.json`,
  пакеты лиг, LAN, карьера.

## [2026-09-09] session | handoff: плоский canon + бандлы — следующая задача

Написан handoff `docs/reports/2026-09-09-handoff-flat-canon-bundles.md` и помечен в
[[открытые-вопросы]] как активная задача: (1) canon становится прод-выходом скрейпера
(доделать colors сборных + logo/flag urls, прод-генерация из resume-кэша); (2) миграция четырёх
потребителей (ratings/kit/faces/tm-gf-import) на `data/canon/*` с неизменным выходом;
(3) бандлы `GameplayFootball-data-<ver>.zip` + реестр `data-versions.json` в репо игры.
Детерминизм: пересъёмка linux/x64/macos-эталонов — после стабилизации данных этим милстоуном.

## [2026-09-09] deploy | data v2: плоский canon стал прод-выходом, БД пересобрана и подменена
Плоский canon — единый контракт конвейера: скрейпер прод-генерует data/canon/ из resume-кэша
(uild_canon_from_cache.py), доделаны colors сборных, competitions.logo, 	ier (label),

ational_teams.flag (по стране, best-effort), irth_date/height NT-only. Возраст НЕ несётся
(TM-age ненадёжен, ~15% расхождений с birth_date): потребители считают его из birth_date на дату
сбора. Четыре потребителя (ratings --canon, kit --canon, faces --canon, tm-gf-import
--canon) мигрированы; вложенный data/full — legacy. Нормализация «один клуб на игрока»
(дубли главный+II схлопнуты): БД 125402 = 120032 клубных + 5370 NT; эталоны зелёные (Кейн слот
10 ST; Экв. Гвинея — 1 GK Owono tm 631693 без Roberto; Лаос 1427661; пустых profile_xml у
сборных 0). Флаги: скрейпер качает чёткие флаги стран по tm_id и национальностей по имени
(lags.py); игра грузит images_countries/<tm_id>.png; 	ools/fetch_flags.py удалён.
Бандл данных 	ools/release/package_data.py → dist/GameplayFootball-data-2026-09-09.zip
(sha256 3f6c260f...) + строка в data-versions.json (game v0.3.1). Детерминизм x86 перебазирован:
eference.txt = 52aa60a... (причина сдвига — возраст из birth_date и нормализация дублей на
армянских клубах матча раннера); linux/x64/macos — переснять на своих машинах.

## [2026-09-09] session | сессия: canon-миграция + бандлы + флаги
См. записи выше; закрыты пункты data v2 из [[открытые-вопросы]] (canon-контракт, миграция
потребителей, бандлы, реестр, флаги по tm-id). Осталось из милстоуна: пересъёмка linux/x64/macos
детерминизма, публикация бандла в релиз.

## [2026-09-09] fix | дубли «основной+II»: игрок остаётся во второй команде
В canon-нормализации дублей игроков (состав+«II»/молодёжка) клуб теперь выбирается в пользу
ВТОРОЙ команды (резерв-детекция по имени/тиру лиги в uild_canon._pick_club), а не первой.
Потери выбираемых команд: 9→4 (все оставшиеся — родственные молодёжки U19↔U21 одного клуба,
делящие ~20 игроков; при «один клуб на игрока» один из пары недобирает состав — принято).
Детерминизм x86 перебазирован повторно: eference.txt = 7134def2c0863d4978bb18742b1f173358e4bf66
(check exit 0). Бандл пересобран: sha256 840985221bdfdc62c720ca0453c5582ae627090eec5b7241636cec01f01a758a,
data-versions.json обновлён.

## [2026-09-09] fix | «International» дублировал сборные; лого «National Teams» — игровой ассет
Причина дубля: импорт создавал лигу «National Teams» с country_id = синтетическая страна
International (builders/leagues.py::build_nt_league), поэтому «International» появлялся в списке
стран и открывал ту же лигу сборных. Фикс: лига «National Teams» — country_id = NULL
(вики это и декларировала, код расходился). Картинка «National Teams», добавленная вручную,
оказалась не потеряна (нашлась в бэкапе подмены данных); перенесена в игровой ассет
data/media/textures/nationalteams.png (путь media/textures/nationalteams.png), импорт больше
не создаёт пустой плейсхолдер images_competitions/nationalteams.png. Игра пересобрана,
детерминизм не изменился (7134def2..., check exit 0).

## [2026-09-10] session | пересъёмка macOS-детерминизма после data v2
macOS arm64 эталон переснят под бандл данных 2026-09-09: сборка `build-mac-dtr` (Release,
AppleClang, CMake), `determinism_runner run` = cffeb1df6374339ea6693a1f6fd576d4628d37a3,
`check` exit 0 (воспроизводимо). Записан в `tools/determinism/reference-macos-arm64.txt`
(был 7b1c4983...). Найдена ловушка применения бандла: zip содержит СОДЕРЖИМОЕ
`databases/default` без префикса каталога (package_data.py кладёт relpath от data_dir), поэтому
распаковка в каталог сборки роняла картинки лиг/клубов/флаги и не находила manifest.json;
правильно — `unzip -o ... -d databases/default`. Зафиксировано в вики ([[пайплайн-данных]]).
Осталось из милстоуна: публикация бандла в релиз.

## [2026-09-10] deploy | v0.4.0: macOS arm64 с встроенной data v2
Опубликован GitHub Release `v0.4.0` (тег на вершине `squads-update` `88ac096`, `master` не
трогали — вариант B). Единственный ассет — `GameplayFootball-v0.4.0-macos-arm64.zip` (601 МБ):
data v2 встроена в .app (БД schema v2 193 МБ, 64 788 клубных картинок, бандл 2026-09-09,
манифест находится на старте). Пакет проверен: `otool -L` без `/opt/homebrew`/`@rpath`,
`codesign --verify --deep --strict` — valid. Перед упаковкой бандл применён в
`build-mac-dtr/databases/default`, `football.config` сброшен в пустой дефолт (иначе уезжали
локальные 0.6/0.4). Установлены `bash` 4 (нужен `package_macos.sh`) и `gh`. Windows/Linux
ассеты добавляются отдельно с другого ПК — собирать с применённым бандлом.

## [2026-09-10] session | Пересъёмка linux/x64-эталонов и публикация бандла данных
Windows-сторона милстоуна data v2 закрыта с этого ПК.

Бандл `dist/GameplayFootball-data-2026-09-09.zip` проверен (sha256
`84098522...` совпал с `data-versions.json`): распакован в чистый каталог, наложен на `media/`
из сборки, `determinism_runner check 7134def2...` — exit 0, самодостаточен для x86. После
проверки приложен к релизу `v0.4.0` (`gh release upload`) — теперь в релизе два ассета:
`GameplayFootball-data-2026-09-09.zip` (527 МБ) и `GameplayFootball-v0.4.0-macos-arm64.zip`.

Эталоны пересняты против data v2 на двух платформах:

- **Linux** = `26aedeb152fa18fe1c79a86043ef52fd4b1c31ee`. Окружение поднято заново: WSL2 не
  стартовал с «не включена виртуализация», хотя `VirtualMachinePlatform` уже была включена, а
  гипервизор присутствовал (VBS). Причина — выключенные `HypervisorPlatform` (WHPX) и
  `Microsoft-Windows-Subsystem-Linux`; после их включения через DISM и перезагрузки `wsl --status`
  перестал ругаться, Ubuntu установился из Store. Дистрибутив — **Ubuntu 26.04 LTS (resolute),
  gcc 15, SDL3 3.4.2, Boost 1.90**. Сборка в ext4 (`/root/gf-src`), БД — из `data/` рабочего
  дерева; `determinism_runner run` дал хэш, `check` воспроизвёл.
- **Windows x64** = `894671466e37c2fa4634d0c2c26cbb307ac29dca`. Локальная сборка `build-x64`
  (`-A x64`, vcpkg `x64-windows`; конфигурация ~672 с на пакеты boost).

Оба эталона записаны в `tools/determinism/reference-*.txt` вместе с macOS-эталоном (см. запись
выше); x86 (`7134def2...`), linux и x64 закоммичены с этого ПК (`fd8260a`), macOS — с Mac
(`88ac096`). Ветка `squads-update` запушена в origin. `gh` установлен и авторизован
(polite-cat-2001). Незакрытым остаётся выпуск **Windows/Linux ассетов** релиза `v0.4.0`.

## [2026-09-11] deploy | v0.4.0 доукомплектован: win32 x86/x64 и linux
Релиз `v0.4.0` собран целиком — теперь пять ассетов: macOS arm64 (601 МБ, см. запись выше),
`GameplayFootball-data-2026-09-09.zip` (527 МБ), `GameplayFootball-v0.4.0-win32-x86.zip`
(548 МБ), `-win32-x64.zip` (548 МБ), `-linux-x86_64.tar.gz` (515 МБ). Все self-contained:
Windows несёт DLL-замыкание + VC runtime, Linux — бинарник и данные (нужны системные
SDL3/OpenAL/Boost/sqlite3, Ubuntu 26.04+), macOS — dylib внутри .app.

Починена ловушка упаковки: `package_windows.ps1` брал данные из репозиторного `data\*`, куда
входят неиспользуемые `databases/default/faces` (4.1 ГБ, 67 205 фото) — архив раздувался бы до
~4 ГБ. Теперь, как macOS/Linux, скрипт берёт `databases/`, `media/`, `football.config` из
каталога сборки (там POST_BUILD `copy_data.cmake` уже выкинул `faces`). Windows-архивы проверены:
`faces` нет, `database.sqlite` schema v2 (193 МБ), `media/`, exe, 18 DLL и VC runtime на месте.
Вики [[релиз]] синхронизирована (Windows тоже берёт данные из сборки).

## [2026-09-11] decision | LAN-матч: host-authoritative, дизайн зафиксирован в спеке
Ветка `lan` отпочкована от `squads-update`. Для матча по локальной сети выбран
**host-authoritative тонкий клиент** (lockstep отклонён: кросс-платформенный
детерминизм не гарантирован — эталоны `tools/determinism` различаются по платформам).
Клиент строит `Match` из сериализуемого `MatchSetup`, не вызывает `Match::Process`,
проигрывает снапшоты рендер-состояния через Put-пайплайн. Лобби — **зеркальное**:
одно каноническое `LobbyState` на хосте, все видят одно и то же (в т.ч. живой
курсор выбора команды). Пауза равноправна (любой пир), хост меняет формы/погоду/
сложность AI, смена клуба в паузе заблокирована. Дисконнект и подключение во время
матча — пауза + выбор сторон (как при отключении геймпада); реконнект поддержан без
возврата владения стороной. Сетевой стек — `boost::asio` (TCP control + UDP
realtime). Полный дизайн, сообщения и таблицы состояний — неизменяемый снимок
`docs/specs/2026-09-11-lan-match-design.md`.
Уточнения по данным, fairness и лимитам зафиксированы в плане
`docs/plans/2026-09-11-lan-match.md`: данные команд/игроков — **ленивый стрим с
хоста** (своя БД клиенту не нужна; 2 команды + ~57 игроков ≈ 60 КБ, каталог 4652
команд ≈ 88 КБ; ассеты — общие из сборки), fairness — **все по самому медленному**
(пир задерживает ввод: хост `2U`, клиент `2U - u_i`), лимит 2 человека на команду
(макс 4 пира), зритель — штатная камера, keepalive 500 мс / таймаут 5 с, реконнект
как новый игрок, порт `27015` на TCP+UDP.

## [2026-09-11] feat | LAN: транспорт и handshake (Task 2)
Модуль `src/net` получил TCP control-канал на `boost::asio` и handshake. Формат кадра —
4 байта длины (LE) + `[тип|тело]`, асинхронные read/write и очередь записи. `ClientHello`
несёт protocol/build/dataVersion/dataHash/animationHash/имя; сервер сверяет и отвечает
`ServerHello{accepted, reason}` (причины: proto/build/data/anims mismatch и др.). Build-хеш —
git short-хеш из CMake; data version/hash — из `databases/default/manifest.json`; animation
hash — SHA-1 листинга `media/animations`. Добавлены `netbuffer`, `netmessages`, `netassets`,
`netserver`/`NetServerConnection`, `netclient`; `netlib` линкует `Boost::filesystem` и
`ws2_32` (Windows). Headless smoke-тест `tools/nettest` поднимает сервер и клиент на
localhost — `PASS`. Детерминизм Windows x86 (`7134def2...`) не сдвинулся. Вики: новая
страница [[сеть]] (+ [[константы]], [[архитектура]], index). Следующее — Task 3 (каталог и
сериализация данных).

## [2026-09-11] feat | LAN: каталог команд и сериализация данных матча (Task 3)
Данные команд/игроков теперь можно слать с хоста клиенту без БД на клиенте. В
`playerdata.hpp`/`teamdata.hpp` добавлены сериализуемые `PlayerDataRaw`/`TeamDataRaw`
(все поля БД + XML формации/тактик) и десериализующие конструкторы, переиспользующие
общий парсер (`PlayerData::Init`, `TeamData::InitFromRaw`) — БД-путь сохранён 1-в-1,
включая вызов `random(1, 4)` в том же месте, поэтому **эталон остался `7134def2...`**.
`src/net/netdata.cpp` (в `netlib`) сериализует только raw-структуры, не тянет реализацию
data-классов. `QueryTeamCatalog` (`src/data/teamcatalog.cpp`, `datalib`) — пейджинг/поиск
по `teams` для лобби. `tools/nettest` теперь проверяет и round-trip `TeamDataRaw` —
`PASS`. Вики [[сеть]] обновлена. Следующее — Task 4 (зеркальное лобби).

## [2026-09-11] feat | LAN: протокол зеркального лобби (Task 4a)
В `netmessages` добавлены `NetLobbyState`/`NetLobbyPlayer`/`NetLobbyAction`
(SetSide/SetReady/MoveCursor/CommitTeam) с (де)сериализацией. `NetServer` держит
каноническое состояние (`GetLobbyState`/`ApplyLobbyAction`), сам применяет действия
(с атрибуцией по `sessionId` соединения), переходит `Sides → Teams` по готовности всех,
назначает `chooser` сторон (хост / первый на противоположной), на join/leave сбрасывает
фазу и Ready, рассылает состояние. `NetClient` шлёт действия и хранит/сигналит
`sig_OnLobbyState`. Запись в сокет уходит через `boost::asio::post` на executor, так что
API потокобезопасен. `tools/nettest` теперь прогоняет и лобби-обмен (клиент выбирает
сторону, сервер и клиент видят её) — `PASS`; детерминизм `7134def2...` не сдвинулся.
Следующее — Task 4b: UI-страницы (host/join/IP+порт, зеркальный экран выбора).

## [2026-09-11] feat | LAN: UI меню сети (Task 4b)
Новые страницы `src/menu/network/`: `NetworkMenuPage` (Host/Join/Back), `NetworkHostPage`
(порт+имя → `NetServer` + каталог), `NetworkJoinPage` (IP+порт+имя → `NetClient`, опрос
состояния/ошибок), `NetworkLobbyPage` (рисует канонический `LobbyState`, ввод → `LobbyAction`:
фаза Sides — сторона/Ready, фаза Teams — курсор/подтверждение команды). Кнопка «Network» в
главном меню, страницы зарегистрированы в `PageFactory`. Сессия `NetServer`/`NetClient`
живёт в `MenuTask`. Каталог команд хост шлёт клиентам сообщением `Catalog` после handshake
(`NetServer::SetCatalog`/`NetClient::GetCatalog`), чтобы выбор команды работал на клиенте без
его БД. Попутно починен Windows-конфликт `winsock2.h`/`windows.h` (asio падал с «WinSock.h
has already been included»): `defines.hpp` включает `winsock2.h` до `windows.h`, а файлы с
прямым `windows.h` — тоже. Сборка ок, `nettest` `PASS`, детерминизм `7134def2...` не
сдвинулся. Вики [[сеть]] и [[архитектура]] обновлены. Следующее — Task 5 (снапшоты).

## [2026-09-11] feat | LAN: лобби — фиксы, фаза команд, устройства ввода
Довели лобби по фидбэку полевых запусков. Кнопки/поля сведены в `Gui2Grid` (иначе фокус
по ним не ходит); Back слева, Open lobby/Connect справа. Экран выбора сторон выглядит как
`ControllerSelectPage` (иконка устройства по стороне, имя, галочка Ready), порядок сторон
пространственный (Home↔Spectator↔Away) без цикла, геймпад читается напрямую
(`e_ButtonFunction_Left/Right`, A=Ready) как в `ControllerSelectPage`. Фаза команд —
две панели страна→турнир→команда как `TeamSelectPage` (переиспользованы
`AddCountries/AddLeagues/AddTeams`, включая «National Teams»), выбор зеркалится live,
под каждой панелью кнопка Ready. У игрока в `LobbyState` — последнее использованное
устройство (`device`), иконка меняется live; в фазе команд ввод ограничен этим
устройством; потеря устройства / выход игрока сбрасывают фазу в `Sides` (все в лобби).
Инстансы проверены вручную. Сборка ок, `nettest` `PASS`, детерминизм `7134def2...` не
сдвинулся. Следующее — Task 5 (снапшоты/старт матча).

## [2026-09-11] feat | LAN: старт матча и remote-презентация (Task 5)
Старт из лобби: когда обе стороны Ready и команды выбраны, хост рассылает
`NetMatchSetup{teamId[2]}` и переходит на `LoadingMatchPage`; клиент берёт те же
team id и строит `MatchData` из своей БД (handshake и так требует одинаковый
`data_hash`). После создания `Match` хост шлёт `AnimationTable` (имена
`Animation::GetName()` в порядке `AnimCollection`), клиент строит
`name→Animation*` (с учётом дублей имени у зеркальных/автоген-анимаций).
Новый `src/net/matchsnapshot.*` (компилируется в `gamelib`): захват на хосте
(заголовок время/счёт/фаза/inPlay, позы активных игроков из `animApplyBuffer`,
судьи, мяч) и применение на клиенте. `e_NetMessage_Snapshot` рассылается хостом
с частотой `net_snapshotRate_hz` (пока по TCP). Сущности адресуются
`team`/`slot`, а не глобальным `PlayerBase::id` (счётчик id процесс-глобален и
может разойтись). Remote-режим `Match` (`remotePresentation`): клиент не зовёт
`Match::Process`, `ApplyRemoteSnapshot` кладёт позы через
`HumanoidBase::SetRemotePose`/`Ball::SetRemoteState`, инкрементит итерации и
далее штатный `PreparePutBuffers/FetchPutBuffers/Put`; реплеи и неттинг на
клиенте выключены. Сборка ок, `nettest` `PASS`, детерминизм `7134def2...` не
сдвинулся. Вики [[сеть]] и [[константы]] обновлены. Ручной тест двух инстансов
не проводился (нужен GUI). Следующее — Task 6 (`NetHIDDevice`, ввод клиентов).

## [2026-09-11] session | LAN Task 5 — remote-презентация, ручной тест не проведён
Реализованы снапшоты и старт матча из лобби (см. выше). Headless-проверки
зелёные (`nettest`/`determinism`), но сценарий «клиент видит матч хоста» на двух
инстансах `gameplayfootball.exe` не запускался: в этой среде нет GUI. Хвост в
[[открытые-вопросы]] (проверка и UDP).

## [2026-09-11] fix | LAN: краш клиента — CalculateGeomOffsets без Process
Полевой тест Task 5: матч играется на хосте, но клиент падал почти сразу после
старта (`0xC0000005`). По map-файлу адрес падения — `MentalImage::GetBallPrediction`.
Причина: `HumanoidBase::PreparePutBuffers` каждый кадр звал виртуальный
`CalculateGeomOffsets()`, а `Humanoid::CalculateGeomOffsets` (в отличие от
базовой-заглушки) лезет в `currentMentalImage`, который выставляется только в
`Humanoid::Process` — на тонком клиенте его нет. Фикс: в remote-режиме
`CalculateGeomOffsets()` не вызывается. Попутно клиент получил стартовый zoom
камеры (вынесен в `UpdateIngameCameraStartEffect`, общий с `Process`).
Диагностика (crash-хендлер с RVA/стеком, `netdiag_*`, `/MAP`) помогла найти
причину и затем удалена.

## [2026-09-11] feat | LAN: сетевой ввод, пауза, камера, синхронизация реплеев (Task 6)
`src/net/nethiddevice.*` (`NetHIDDevice : IHIDevice`) — виртуальное устройство
хоста поверх TCP; создаётся на handshake по клиенту (`ownerId = sessionId`),
принимает `InputFrame`. Хост биндит своё устройство и сетевые к `Team::AddHumanGamer`
по `LobbyState` (локальный hotplug-сброс контроллеров в сетевом матче отключён);
клиент каждый тик сэмплит локальное устройство и шлёт `InputFrame`; 200 мс без
кадров → устройство отпускает кнопки. Владелец игрока несётся в снапшоте
(`ownerId`): подсветка/подписи локальные — свои цветом, designated соперника
серым. Пауза равноправна: `Match::Pause()` шлёт/рассылает `PauseRequest/PauseState`,
`pauseMenuRequested` открывает/закрывает in-game меню у всех (авто-реплеи флаг не
ставят). Камера считается хостом и передаётся в снапшоте (общий ракурс, слежение
за мячом). Гол-повторы: `goalScored`/`goalScoredTimer` в снапшоте, клиент пишет
свои кадры (`CaptureReplayFrame`), `ReplayStop` закрывает повтор у всех. Частота
снапшотов поднята 40→100 Гц (плавность клиента). Сборка ок, `nettest` `PASS`,
детерминизм `7134def2...` не сдвинулся. Вики [[сеть]]/[[константы]] и
[[открытые-вопросы]] обновлены.

## [2026-09-11] session | LAN Task 6 — ввод/пауза/камера/реплеи; полевой тест пройден
Полевые тесты двух инстансов подтвердили: матч идёт с управлением на обеих
сторонах, пауза-меню появляется и исчезает у всех, камера одинаковая (следит за
мячом), гол-повторы синхронны и синхронно пропускаются. Диагностика убрана.
Осталось (Task 7): UDP-канал, host input-delay/интерполяция (сейчас заметна
задержка ввода по TCP), дисконнект/реконнект/join во время матча, смена
сторон/команд в паузе. См. [[открытые-вопросы]].

## [2026-09-11] feat | LAN: ping/fairness, дисконнект/join, живая смена сторон (Task 7)
Ping/keepalive: у сервера (per-connection) и клиента — `steady_timer` на
`net_keepaliveInterval_ms`, сообщение `NetKeepalive{seq, echo}`, расчёт RTT,
таймаут `net_disconnectTimeout_ms` (сервер закрывает, клиент — «host timed out»).
Host input-delay: `src/net/delayedhiddevice.*` (`DelayedHIDDevice : IHIDevice`)
оборачивает локальное устройство хоста и отдаёт кадр возрастом `delayTicks`;
хост задерживает ввод на `maxRTT + B`, клиент — на `2U - u_i + B`, так что ввод
применяется в один момент; `maxRTT` едет в снапшоте, `B =
net_interpolationBuffer_ms`. Дисконнект/join во время матча: хост вычитывает
`ConsumeDisconnectedPlayer`/`ConsumeJoinedPlayer`, ставит паузу, лобби уходит в
режим выбора сторон (`NetLobbyState.sideSelect`), у всех открывается зеркальный
экран сторон; новичку `SendToPlayer` шлёт setup/anim/env/snapshot. Смена сторон
в паузе: `NetworkLobbyPage` в режиме `resumeOnClose` по готовности зовёт
`GameTask::RebindNetworkControllers` (host) → `SetupNetworkControllers` + unpause;
в пауза-меню добавлен пункт «side selection» (`RequestSideSelect` для клиента).
Сборка ок, `nettest` `PASS`, детерминизм `7134def2...` не сдвинулся. Вики
[[сеть]]/[[константы]] и [[открытые-вопросы]] обновлены.

## [2026-09-11] session | LAN Task 7 — fairness, дисконнект/join, смена сторон
Сделаны ping/RTT, host input-delay (`DelayedHIDDevice`), дисконнект/join во время
матча с паузой и зеркальным выбором сторон, живая смена сторон в паузе. Остался
хвост Task 7: UDP-канал и интерполяция снапшотов (`B` пока только в задержке
ввода); калибровка порога задержки и full-setup карьеры (Task 8). Полевой тест
Task 7 не проводился — только сборка/`nettest`/детерминизм.

## [2026-09-11] fix | LAN: немедленный pong — устранена ложная задержка ввода
Симптом: большая (~сотни мс) задержка между вводом и действием. Причина:
keepalive-пинг отвечался не сразу, а следующим срабатыванием 500-мс таймера, поэтому
RTT измерялся как «сеть + остаток до таймера» вплоть до ~500 мс, а `hostInputDelay =
maxRTT + B` раздувалась. Фикс: на входящий пинг (`seq != 0`) обе стороны шлют pong
(`seq = 0`, `echo = seq`) **сразу**; pong не вызывает встречного ответа. Проверено
диагностикой в `nettest`: `client_rtt=1 мс`, `server_max_rtt=0 мс`. Дополнительно
`net_interpolationBuffer_ms` снижен 20 → 0: B имеет смысл только вместе с
интерполяцией, поэтому теперь задержка = чистый RTT (на localhost ≈ 0 мс). Вики
[[сеть]] и [[константы]] обновлены.

## [2026-09-11] fix | LAN: краш хоста при дисконнекте клиента (use-after-free)
Симптом: при отключении клиента во время матча падал хост. Причина: io-поток в
`NetServer::RemoveConnection` уничтожал `NetServerConnection` вместе с его
`NetHIDDevice`, а `Team`/`HumanGamer` держат сырой `IHIDevice*` и читают его в
`Match::Process` (в т.ч. лочат `boost::mutex` освобождённого объекта). Фикс:
отключённое устройство «уходит в отставку» — `Clear()` (кнопки отпущены) и
хранение в `NetServer::retiredDevices` до `ClearRetiredDevices()`; хост в
`HandleNetworkRosterChanges` сразу зовёт `SetupNetworkControllers` (убирает
мёртвую привязку, AI берёт сторону) и затем освобождает retired. Сборка ок,
`nettest` `PASS`, детерминизм без изменений.

## [2026-09-11] fix | LAN: мигание окна выбора сторон при переподключении
Симптом: у переподключившегося клиента быстро мигало окно выбора сторон.
Причина: хост уже стоял на паузе и повторно `PauseState` не рассылал, поэтому
`Match` нового клиента не знал о паузе; клиентское resume-окно выходило по
`!Match::GetPause()`, `GamePage` тут же открывал его снова — цикл. Фикс: хост в
`HandleNetworkRosterChanges` шлёт новичку текущий `PauseState` (`SendToPlayer`);
клиентское resume-окно выходит по смене `LobbyState.sideSelect` (был true → стал
false), а не по паузе. Вики [[сеть]] обновлена.

## [2026-09-11] feat | LAN: хост-опции паузы (киты, погода, сложность AI)
`VisualOptionsPage` теперь хост-центрична: для сетевого клиента показывает только
пояснение, а хост/локалка получают киты, «Randomize sun position» и новый выбор
сложности AI. `Team` хранит текущий `kitNumber`; `Match::SetMatchDifficulty`
меняет `matchDifficulty` живьём (влияет только на AI-команды). `NetMatchEnvironment`
расширен номерами китов; `Match::GetMatchEnvironment`/`BroadcastMatchOptions`
рассылают солнце + киты, клиент применяет их при `ConsumeEnvironment` (в т.ч. при
join в матч). Сборка ок, `nettest` `PASS`, детерминизм `7134def2...` без изменений.
Вики [[сеть]] обновлена.

## [2026-09-11] feat | LAN: голосование за продолжение и личные экраны паузы
Выход из паузы больше не мгновенный: в пауза-меню кнопка «Continue (X/N)», пир
отмечает себя `e_NetLobbyAction_SetResumeReady` (`NetLobbyPlayer.resumeReady`), и
матч продолжается только когда отметились все (хост: `RecomputeResumeReady` →
`ConsumeAllResumeReady` → `Match::Pause(false)`). Голоса сбрасываются на входе/выходе
из паузы (`Match::Pause` → `NetServer::ResetResumeVotes`). Общим экраном остаётся
только выбор сторон (требует подтверждения всех); остальные экраны паузы —
личные, их навигация не зеркалится. Сборка ок, `nettest` `PASS`, детерминизм
`7134def2...` без изменений. Вики [[сеть]] обновлена.

## [2026-09-11] fix | LAN: выбор сторон не открывался у остальных машин
Симптом: при открытии «side selection» из паузы окно появлялось только у
инициатора. Причина: авто-открытие висело на `GamePage::Process`, но при активной
паузе сверху находится `IngamePage`, и `GamePage::Process` не вызывается. Фикс:
открытие перенесено в `GameTask::SyncNetworkSideSelectOverlay()` — вызывается
каждый тик на всех пирах по флагу `LobbyState.sideSelect`; блок из `GamePage`
убран. Сборка ок, `nettest` `PASS`, детерминизм без изменений. Вики [[сеть]]
обновлена.

## [2026-09-11] fix | LAN: краш при повторном open side selection (gui2 GoBack UAF)
Симптом: повторное открытие выбора сторон в рамках одного меню паузы роняло клиента.
Причина: `Gui2Page::GoBack()` пересоздавал предыдущую страницу через
`PageFactory::CreatePage(const Gui2PageData&)`, который **не** обновляет
`mostRecentlyCreatedPage`; после `delete this` указатель висел на освобождённой
странице, а `GameTask::OpenNetworkSideSelect()`/gamepad-блок дёргают
`GetMostRecentlyCreatedPage()` → use-after-free. Фикс: `GoBack` пересоздаёт через
`CreatePage(pageID, properties, data)` (обновляет `mostRecentlyCreatedPage`).
Плюс корректная отмена: клиент по Esc шлёт `RequestSideSelect` value = 0, хост снимает
`sideSelect` и резюмирует; локальный `GoBack` у клиента убран (иначе Sync мгновенно
переоткрывал окно). Сборка ок, `nettest` `PASS`, детерминизм без изменений.
Вики [[сеть]] обновлена.

## [2026-09-11] feat | LAN: личные camera/visual на клиенте, ограничения паузы
По просьбе: (1) «controller select» убран из пауза-меню сетевого матча —
устройство выбирается на экране выбора сторон, локальный `UpdateControllerSetup`
сломал бы сетевые привязки. (2) Клиент теперь может переопределять камеру
(`Match::SetRemoteCameraOverride`; `ApplyRemoteSnapshot` при override зовёт
`UpdateIngameCamera()` по снапшот-позициям вместо хостовой камеры) — в «camera
settings» слайдеры наконец действуют. (3) В «visual options» киты и погода
доступны всем (у клиента — локально, без рассылки; у хоста — с
`BroadcastMatchOptions`), а сложность AI только у хоста. (4) System settings →
gameplay у клиента заблокирован (кнопка неактивна), т.к. assist/agility влияют на
общую симуляцию; controller/graphics/audio остаются локальными. Сборка ок,
`nettest` `PASS`, детерминизм `7134def2...` без изменений. Вики [[сеть]] обновлена.

## [2026-09-11] fix | LAN: сетевая сессия не закрывалась при выходе в меню
Симптом: после сетевого матча в следующем **локальном** матче в паузе открывался
сетевой экран выбора сторон со старым составом («Host» + старый клиент). Причина:
`MenuTask::ProcessPhase` при `e_MenuAction_Menu` (forfeit/game over) не останавливал
`NetServer`/`NetClient`; указатели оставались, `networkMatch` был true, и сетевой
`NetworkLobbyPage` показывал устаревший `LobbyState`. Фикс: при переходе в меню
`netServer->Stop(); netServer.reset();` и `netClient->Disconnect(); netClient.reset();`.
Локальный матч снова показывает локальный выбор сторон (`ControllerSelectPage`).
Сборка ок, детерминизм без изменений.

## [2026-09-11] refactor | LAN: NetMatchSession + явное состояние паузы
Вынес сетевую логику матча из `GameTask` в `src/net/netmatchsession.{hpp,cpp}`
(в `gamelib`): input-delay, снапшоты, роster (дисконнект/join), пауза/голоса,
окружение, `SetupControllers`/`RebindControllers`. Ввёл `e_NetMatchPhaseState`
(`Playing`/`Paused`/`SideSelect`), **выводимый** из `Match::GetPause()` +
`LobbyState.sideSelect` вместо набора флагов. `GameTask::ProcessPhase` теперь:
`netSession.Process` → `PreparePutBuffers` (под мьютексом) →
`netSession.BroadcastSnapshot` → при `SideSelect` централизованно открыть оверлей.
Убраны `SetupNetworkControllers`/`SyncNetworkSideSelectOverlay`/
`HandleNetworkRosterChanges`, `hostInputDelay`/`clientInputQueue`/
`lastNetSnapshotTimeMs` переехали в сессию. GUI-навигация осталась в `GameTask`,
но решение — за состоянием. Сборка ок, `nettest` `PASS`, детерминизм
`7134def2...` без изменений. Вики [[сеть]] обновлена.

## [2026-09-11] fix | LAN: сдвиг пулдаунов в visual options у клиента
Симптом: у клиента в «visual options» киты и кнопка солнца уезжали вправо.
Причина: примечание для клиента лежало в `Gui2Grid` в колонке 0 и было шире
остальных подписей (40 против 20), а `Gui2Grid::UpdateLayout` считает ширину
колонки по самому широкому элементу — колонка 1 с пулдаунами/кнопкой сдвигалась.
Фикс: примечание вынесено из грида на `Gui2Frame` (абсолютная позиция), колонки
больше не меняются. Сборка ок, `nettest` `PASS`, детерминизм без изменений.

## [2026-09-11] fix | LAN: выход из SideSelection у клиента уводил в матч
Симптом: хост из SideSelection возвращался в пауза-меню, а клиент — сразу в матч
(пауза-меню пропадало). Причина: «выход» трактовался как «отмена + продолжить»:
host `Leave` и серверный `ConsumeSideSelectCancel` звали `RebindNetworkControllers`,
который снимает паузу; клиент, дождавшись `sideSelect=false` и `pause=false`,
через `GoBack` попадал в `GamePage`. Фикс: выход из выбора сторон **не** снимает
паузу — `GameTask::ApplyNetworkControllers` (`SetupControllers` без `Pause(false)`),
хост применяет стороны и возвращается в пауза-меню; возобновление — отдельным
голосованием Continue. Сборка ок, `nettest` `PASS`, детерминизм без изменений.
Вики [[сеть]] обновлена.

## [2026-09-11] fix | LAN: отмена SideSelection клиентом не закрывала экран у хоста
Симптом: клиент выходил из SideSelection, а хост оставался на экране. Причина:
проверка выхода по `LobbyState.sideSelect == false` была только в клиентской ветке
`NetworkLobbyPage::Process`; у хоста выход был только по all-ready или своему Esc,
поэтому серверная отмена (`sideSelectCancelPending`) закрывала экран лишь у
инициатора. Фикс: проверка `sawSideSelect && !state.sideSelect` вынесена на общий
уровень (хост и клиент); у клиента оставлен safety-net по `!Match::GetPause()`.
Сборка ок, `nettest` `PASS`, детерминизм без изменений. Вики [[сеть]] обновлена.
