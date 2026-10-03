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

## [2026-09-11] test | nettest: сценарные тесты протокола LAN
`tools/nettest` расширен из handshake-smoke в набор сценарных проверок (своя
`CHECK`/`WaitFor`-обвязка, `PASS (N checks)`/`FAIL`): handshake, поток лобби
(side/ready → Sides→Teams), открытие/отмена выбора сторон
(`RequestSideSelect` value 1/0 → `sideSelect` + `ConsumeSideSelectCancel`),
resume-голосование (`SetResumeReady` → `ConsumeAllResumeReady`, `ResetResumeVotes`),
дисконнект (`ConsumeDisconnectedPlayer`), RTT/keepalive и round-trip новых
сообщений (`NetLobbyState` с sideSelect/resumeReady/device, `NetMatchEnvironment`
с китами, `NetKeepalive`). 43 проверки, все зелёные. Это база для безопасных
дальнейших правок. Вики [[сеть]] обновлена.

## [2026-09-11] refactor | LAN: гигиена потоков и версия протокола
Быстрая гигиена после тестов. (1) Убрана гонка `NetClient::lobbyState` (и
`serverHello`/`catalog`): io-поток писал, игровой/меню читал ссылку. Теперь
`stateMutex`, геттеры возвращают копии; `playerId` — `std::atomic`. (2) Удалены
неиспользуемые сигналы `NetClient/NetServer::sig_OnHandshake`/`sig_OnLobbyState`
— они вызывались из io-потока (мина при подключении слота); во всём коде слотов
не было, только старый nettest. (3) `net_protocolVersion` 1 → 2: формат уже
менялся (sideSelect/resumeReady, киты, maxRtt, keepalive), константа теперь
отражает ревизию. Сборка ок, `nettest` `PASS (43 checks)`, детерминизм
`7134def2...` без изменений. Вики [[сеть]] обновлена.

## [2026-09-11] test | lanmatchtest: headless интеграционный тест host-стороны
Новый `tools/lanmatchtest` (в CMake, линкует `${LIBRARIES}` как `determinism`):
реальный `Match` + `NetServer` + «сырой» `NetClient`, прогон через
`NetMatchSession`. Сценарий: старт матча + привязка контроллеров, запрос паузы,
resume-голосование (одна сторона не резюмит, обе — резюмит), открытие/отмена
выбора сторон (возврат в паузу, пауза сохраняется), дисконнект (ростер-пауза +
выбор сторон, клиент уходит из лобби), отмена выбора сторон хостом и резюм после
дисконнекта. `PASS (21 checks)`. Два `Match` в одном процессе не поднимаются
(глобальные `PlayerBase::id`/контроллеры/`MenuTask`), поэтому клиентская
remote-презентация не инстанцируется — её протокол покрыт `nettest`. `nettest`
`PASS (43 checks)`, детерминизм `7134def2...` без изменений. Вики [[сеть]]
обновлена.

## [2026-09-11] refactor | LAN: GUI-развязка GameTask (сетевые оверлеи)
Открытие зеркального выбора сторон убрано из `GameTask` в меню-слой:
`MenuTask::UpdateNetworkOverlay()` каждый тик читает
`GameTask::GetNetSession()->GetState()` и при `SideSelect` открывает
`NetworkLobbyPage` (через top-page `CreatePage`). `GameTask::OpenNetworkSideSelect`
удалён, `ProcessPhase` больше не создаёт страниц для сети; gamepad-missing блок
(локальный `ControllerSelectPage`) оставлен — перенос в меню поменял бы тайминг
относительно `RefreshGamepads` (`MenuTask::Process` идёт до `GameTask::Process`).
Сборка ок, `nettest` `PASS (43)`, `lanmatchtest` `PASS (21)`, детерминизм
`7134def2...` без изменений. Вики [[сеть]] обновлена.

## [2026-09-11] fix | LAN: у клиента не показывалась подпись автора гола
Симптом: у клиента после гола нет подписи «кто забил». Причина: подпись создаёт
`Match::SpamMessage` в `Match::Process`, а thin-клиент `Process` не вызывает;
в снапшоте текста не было. Фикс: `Match` запоминает последнее `SpamMessage`
(текст/длительность/счётчик), снапшот несёт `message`/`messageTime_ms`/
`messageCounter`; `ApplyRemoteSnapshot` при новом счётчике показывает подпись
через `SpamMessage`. Закрывает и гол-автора, и сообщения рефери. `lanmatchtest`
дополнен round-trip снапшота (`PASS (26 checks)`). `nettest` `PASS (43)`,
детерминизм `7134def2...` без изменений. Вики [[сеть]] обновлена.

## [2026-09-11] refactor | LAN: SideSelectPage + бэкенды, GUI-развязка GameTask
Локальный `ControllerSelectPage` и сетевая Sides-фаза `NetworkLobbyPage` сведены в
один `SideSelectPage` (`src/menu/sideselect.*`) поверх `SideSelectBackend`:
`LocalSideSelectBackend` (устройства из `GetControllers()`, `SetControllerSetup`) и
`NetworkSideSelectBackend` (`NetLobbyState`/`LobbyAction`), плюс сценарий матча (все
Ready → хост `SetSideSelectMode(false)` + `RebindNetworkControllers`; выход → назад в
пауза-меню без возобновления). `NetworkLobbyPage` стал только фазой Teams (при
сбросе лобби в Sides возвращается на `SideSelectPage`). `ControllerSelectPage`
удалён, `e_PageID_ControllerSelect` → `e_PageID_SideSelect`; `NetworkHostPage`/
`NetworkJoinPage`/`MainMenuPage`/`InGamePage`/`MenuTask` переведены на новый экран.
Gamepad-missing оверлей перенесён из `GameTask::ProcessPhase` в
`MenuTask::UpdateGamepadMissingOverlay` — `GameTask` больше не трогает окно/фабрику
страниц (тайминг: меню видит список контроллеров на кадр раньше, проверка и так раз
в 1 с). Смешанный режим (локальные девайсы хоста + удалённые пиры) не делал — это
бонус цели, вне текущего UI. Сборка ок, `nettest` `PASS (43)`, `lanmatchtest`
`PASS (26)`, детерминизм `7134def2...` без изменений. Вики [[сеть]],
[[архитектура]], [[глоссарий]], [[открытые-вопросы]] обновлены.

## [2026-09-11] fix | LAN: join во время матча открывал prematch-выбор сторон
Симптом: клиент после реконнекта/join в идущий матч попадал на экран выбора
сторон как при создании матча, а не в матч на паузе. Причина: `NetworkJoinPage`
теперь ведёт на общий `SideSelectPage`, а вычитку `MatchSetup` (переход в
`LoadingMatch`) оставили только в teams-фазе `NetworkLobbyPage` — новичок её не
достигал и застревал в prematch-экране. Фикс: `NetworkSideSelectBackend::
PollTransition` (клиентская, не-resume ветка) вычитывает `MatchSetup`, ставит team
id и возвращает `e_PageID_LoadingMatch`; дальше меню-слой открывает матчевый
`SideSelectPage` поверх паузы. Сборка ок, `nettest` `PASS (43)`, `lanmatchtest`
`PASS (26)`, детерминизм `7134def2...` без изменений. Вики [[сеть]] обновлена.

## [2026-09-11] fix | LAN: геймпад не выбирался (device терялся между фазами)
Симптом: в сетевом матче управление всегда оставалось клавиатурным. Причина: при
развязке UI потерялись два поведения `NetworkLobbyPage`: (1) в фазе **Teams**
больше не отправлялся `e_NetLobbyAction_SetDevice` по вводу — устройство,
выбранное на экране сторон, не обновлялось, если игрок брал геймпад уже в выборе
команды; (2) иконка устройства на экране сторон перестраивалась только при
изменении числа участников, а не при смене устройства. Фикс: в `NetworkLobbyPage`
(teams) снова шлём `SetDevice` по клавиатурному/джойстик-событию и перезапускаем
`ConfigureTeamsInput()` при смене устройства из `LobbyState`; в `SideSelectPage`
иконка обновляется при смене `device` (`RefreshDeviceIcons`). Сборка ок,
`nettest` `PASS (43)`, `lanmatchtest` `PASS (26)`, детерминизм `7134def2...` без
изменений. Вики [[сеть]] обновлена.

## [2026-09-11] feat | LAN: UDP realtime-канал и интерполяция снапшотов
Point 1 из плана. Realtime (ввод клиента и снапшоты) переехал на **UDP** на том же
порту; TCP остаётся control-каналом (handshake/лобби/setup/пауза). Формат
дейтаграммы — `src/net/netudp.hpp` (`magic|type|sessionId|seq`). Хост учит
`sender_endpoint` из `Hello`/`Input` и шлёт снапшоты туда; пока endpoint не
известен — TCP-фолбэк. Клиент биндит UDP на эфемерный порт, шлёт `Hello` (и
повторяет, пока не получит первый UDP-снапшот), ввод дублирует по TCP до
подтверждения, чтобы не потерять его при блокировке UDP. `NetClient` копит
снапшоты с меткой времени `steady_clock` (`DrainSnapshots`); `NetMatchSession`
рендерит на `now - net_interpolationBuffer_ms` (B=120) и блендит два
снапшота-скобки через `BlendSnapshots` (дискретное — из нового, позиции/углы/
`frameNum`/мяч/камера — линейно, `frameNum` только внутри одной анимации).
`Match::ApplyRemoteSnapshot(const Snapshot&)` — перегрузка для готового снапшота;
`net_snapshotRate_hz` 100 → 50, `net_protocolVersion` 2 → 3. Тесты: `nettest` +
UDP round-trip (endpoint/снапшот/ввод) → `PASS (47)`; `lanmatchtest` +
`BlendSnapshots` → `PASS (31)`; детерминизм `7134def2...` без изменений. Вики
[[сеть]], [[константы]], [[открытые-вопросы]] обновлены; полевой тест на реальной
сети ещё предстоит.

## [2026-09-11] fix | LAN: дёрганые анимации у клиента (зацикливание frameNum)
Симптом: у тонкого клиента анимации движения дёргались. Причина: `frameNum`
анимации зацикливается (сбрасывается в 0 в конце цикла), а `BlendSnapshots`
интерполировал его линейно — на каждом витке кадр проходил от конца к началу
**назад**, давая рывок. Фикс: инкрементальная интерполяция `frameNum` только когда
новый кадр не меньше старого (в одной анимации); при зацикливании — snap на новый
кадр (циклы авторские и стыкуются бесшовно). Сборка ок, `nettest` `PASS (47)`,
`lanmatchtest` `PASS (31)`, детерминизм `7134def2...` без изменений. Вики [[сеть]]
обновлена. Если остаточная дрожь будет заметна — следующий шаг: playout-часы по
хостовому `actualTime_ms` вместо меток прихода пакетов.

## [2026-09-11] fix | LAN: сглаживание анимаций у клиента (smooth=true)
Симптом: анимации движения у тонкого клиента оставались дёргаными после фикса
зацикливания кадров. Причина: `HumanoidBase::SetRemotePose` ставил
`animApplyBuffer.smooth = false`, из-за чего `Animation::Apply` пропускал весь блок
SMOOTHING (ограничение скорости изменения поворотов конечностей, slerp к
предыдущей позе) — в отличие от хоста, где `smooth=true`. Клиент щёлкал по «сырым»
кейфреймам. Фикс: `smooth = true` (smoothFactor 1.0) в `SetRemotePose`; хост тоже
гладкий, поэтому клиент теперь выглядит так же. Правка не трогает локальную
симуляцию (детерминизм `7134def2...` без изменений). Сборка ок, `nettest`
`PASS (47)`, `lanmatchtest` `PASS (31)`. Вики [[сеть]] обновлена.

## [2026-09-11] fix | LAN: клиент «хуже, чем до UDP» — частота и B
Симптом: после playout-часов плавность лучше, но всё ещё «хуже, чем до UDP».
Причина: до UDP снапшоты шли на 100 Гц (= игровой тик) и клиент держал последний —
шаги совпадали с хостом; теперь 50 Гц + B=120 давали крупные шаги интерполяции и
двойную задержку. Фикс: `net_snapshotRate_hz` 50 → 100 (шаг 10 мс, как у хоста),
`net_interpolationBuffer_ms` 120 → 50, плюс `frameNum` через точку зацикливания
интерполируется по модулю длины анимации (`GetRemoteAnimTable`/`GetFrameCount`)
вместо snap — пропал рывок раз в цикл. Локальной симуляции не касается,
детерминизм `7134def2...` без изменений. Сборка ок, `nettest` `PASS (47)`,
`lanmatchtest` `PASS (31)`. Вики [[сеть]], [[константы]] обновлены.

## [2026-09-11] fix | LAN: playout-часы клиента (плавность как у хоста)
Симптом: после фиксов ввод стал отзывчивым, но плавность клиента всё ещё
отличалась от хоста. Причина: интерполяция использовала метки **прихода** пакетов
(`steady_clock` в io-потоке), поэтому джиттер сети напрямую превращался в
колебания скорости проигрывания. Фикс: снапшот несёт хостовый `actualTime_ms`
(пишется в первые байты payload, клиент читает его при приёме); `NetMatchSession`
держит playout-часы `renderHostTime`, которые идут ровным локальным временем и
мягко подтягиваются к `новейший_хостовый_время - B` (вперёд не пускают, при
отставании >100 мс догоняют). Скобка ищется по хостовому времени. Зависящий от
джиттера таймлайн убран. Локальной симуляции не касается, детерминизм
`7134def2...` без изменений. Сборка ок, `nettest` `PASS (47)`, `lanmatchtest`
`PASS (31)`. Вики [[сеть]] обновлена.

## [2026-09-11] fix | LAN: точность анимаций и переходов у клиента
Три правки по анимациям. (1) Снапшот теперь несёт `smooth`/`smoothFactor` из
`animApplyBuffer`; `SetRemotePose` применяет их вместо жёстких `true/1.0` — переходы
между анимациями блендятся как у хоста (напр. `0.6` на смене). (2) На тонком
клиенте `buf_LowDetailMode` (half-FPS дальних от мяча) считался по мёртвому
`spatialState.position`, из-за чего в half-FPS попадал не тот набор игроков; теперь
берётся снапшот-поза `animApplyBuffer.position`, а локально управляемый игрок (по
`ownerId == localPeerId`) исключён, как у хоста. (3) `BlendSnapshots` больше не
блендит позицию, если между снапшотами сменился `noPos` (иначе разовый сдвиг).
`net_protocolVersion` 3 → 4. Локальной симуляции не касается, детерминизм
`7134def2...` без изменений. Сборка ок, `nettest` `PASS (47)`, `lanmatchtest`
`PASS (34)` (+3 проверки: round-trip blend-state, noPos-snap). Вики [[сеть]],
[[константы]] обновлены.

## [2026-09-11] fix | LAN: интерполяция выключена (B=0, hold-last поверх UDP)
Симптом: после всех подстроек (100 Гц, B=50, playout-часы, smooth) плавность
клиента всё ещё субъективно не как до UDP. Решение (по согласованию): оставить
UDP-канал (нет head-of-line blocking) и вернуть прежнее поведение —
`net_interpolationBuffer_ms` 50 → **0**. При `B=0` playout-часы каждый тик
сходятся к новейшему снапшоту, т.е. клиент снова делает **hold-last** (шаг 10 мс,
как игровой тик), а интерполяция/`BlendSnapshots`/модульная прокрутка кадров
остаются в коде и включаются поднятием `B`. Хост в этом режиме задерживает ввод
ровно на `maxRTT`. Локальной симуляции не касается, детерминизм `7134def2...` без
изменений. Сборка ок, `nettest` `PASS (47)`, `lanmatchtest` `PASS (31)`. Вики
[[сеть]], [[константы]], [[открытые-вопросы]] обновлены.

## [2026-09-11] fix | LAN: возвращена интерполяция снапшотов (B=40)
По просьбе вернуть сглаживание движения: `net_interpolationBuffer_ms` 0 → 40.
Сглаживание конечностей (`SetRemotePose`: `smooth=true`) не откатывалось — при
`B=0` было выключено только сглаживание *между снапшотами*. `B=40` (≈4 снапшота
при 100 Гц) возвращает интерполяцию, не трогая клиентский input-delay (там `B` не
добавляется; хост несёт `maxRTT + B`). Локальной симуляции не касается,
детерминизм `7134def2...` без изменений. Сборка ок, `nettest` `PASS (47)`,
`lanmatchtest` `PASS (31)`. Вики [[сеть]], [[константы]], [[открытые-вопросы]]
обновлены.

## [2026-09-11] fix | LAN: ввод клиента стал менее отзывчивым (B в input-delay)
Симптом: после включения интерполяции ввод на клиенте просел по отзывчивости.
Причина: `B` добавлялся и к клиентской задержке ввода
(`maxRTT + B - u_i`), хотя клиент уже рендерит на `B` позади — его реакция и так
на `B` позже; `B` платился дважды. Фикс: клиент задерживает ввод на `2U - u_i`
**без** `B`, хост по-прежнему на `maxRTT + B`, поэтому реакции на один видимый
момент попадают в симуляцию одновременно, а собственный ввод клиента больше не
съедает лишние 120 мс. Локальной симуляции не касается, детерминизм
`7134def2...` без изменений. Сборка ок, `nettest` `PASS (47)`, `lanmatchtest`
`PASS (31)`. Вики [[сеть]] обновлена.

## [2026-09-11] fix | UI: не редактировались поля ввода (Gui2EditLine)
Симптом: в сетевых экранах не менялись Address/Port/Name. Причина (давняя, не
связана с сетью): `Gui2EditLine::ProcessKeyboardEvent` перебирал
`event->GetKeysymRepeated()`, но набор `keysymRepeated` нигде не заполняется —
`Gui2Task` пишет только `keyRepeated` (а `keysym*` — нереализованная задумка).
Цикл всегда был пуст → ни одна клавиша не обрабатывалась. Фикс: добавлен геттер
`KeyboardEvent::GetRepeatedKeys()` (возвращает `keyRepeated`), `Gui2EditLine`
использует его. Вскрылся второй, давний баг: ASCII-проверка `(key & 0xFF80) == 0`
не отсекала скан-кодные коды SDL (бит `SDLK_SCANCODE_MASK` = 0x40000000 выше маски),
поэтому стрелки вставляли младший байт — `SDLK_UP 0x40000052` → `'R'`,
`DOWN …51` → `'Q'`, `RIGHT …4F` → `'O'`, `LEFT …50` → `'P'`. Порт спасал
`SetAllowedChars("0123456789")`. Фикс: принимать только печатный ASCII
(`key >= 0x20 && key <= 0x7E`). Плюс на страницах Host/Join добавлена валидация
при подтверждении: адрес — IPv4 или hostname (без regex, маленькие
`IsValidIPv4`/`IsValidHostname` в `network.cpp`), имя — непустое; порт уже
проверялся. Локальной симуляции не касается, детерминизм `7134def2...` без
изменений. Сборка ок, `nettest` `PASS (47)`, `lanmatchtest` `PASS (34)`.

## [2026-09-11] fix | выбор команды: LAN UX выровнен с локальным
Три вещи. (1) В сетевой фазе Teams Enter на команде теперь переводит фокус на
Ready (как `TeamSelectPage`), а Enter на стране мимо турнира идёт сразу в команду,
если выбран «National Teams». (2) Сфокусированный Ready у сетевого матча рисуется
цветом обычного фокуса (`Bright2`), как в локальном; добавлен флаг
`Gui2Button::SetToggledColorWhileFocused` (по умолчанию true), у сетевых Ready —
false, поэтому «готовность» видна красным только когда фокус ушёл. (3) И в
локальном `TeamSelectPage`, и в сетевом лобби обе стороны по умолчанию стоят на
пункте «National Teams» (раньше — первая страна), фокус на нём; в сетевом
`ApplyTeamState` научен дефолту national (`cid 0`) и `FirstCountryIndex` удалён.
(4) Esc/B в сетевой фазе Teams теперь идёт по шагам назад, как локально: Ready →
команда → лига (для national пропускается) → страна, и только на стране —
`Leave()` (закрыть лобби); обработка перенесена в `ProcessWindowingEvent`, поэтому
работает и Esc, и Back на геймпаде. (5) Нажатие Ready «гасит» подсветку кнопки
(`Gui2Button::SetUncolorWhenToggled`, вместо прежнего `SetToggledColorWhileFocused`),
а Esc/B на Ready снимает готовность в лобби (нельзя начать матч) и возвращает фокус
на команду. Локальной симуляции не касается, детерминизм `7134def2...` без
изменений. Сборка ок, `nettest` `PASS (47)`, `lanmatchtest` `PASS (34)`. Вики
[[сеть]] обновлена.

## [2026-09-11] feat | LAN: экран опций матча перед стартом
После выбора команд добавлен экран «Match options» (как в локальном матче):
сложность AI и длительность. Новая фаза лобби `e_NetLobbyPhase_Options` — хост
переводит её, когда обе команды выбраны и Ready (раньше хост сразу стартовал).
Значения хост шлёт `e_NetLobbyAction_SetMatchOptions` (`value` = 0 difficulty /
1 duration, `value2` = значение×1000; сервер применяет только от `isHost`) и они
зеркалятся в `NetLobbyState.matchDifficulty/matchDuration`; клиент видит слайдеры
read-only. Новая `NetworkMatchOptionsPage` (`e_PageID_NetworkMatchOptions`), хост
на старт кладёт значения в конфиг (их читает `Match`), рассылает `MatchSetup` и
идёт в `LoadingMatch`, клиент ждёт `MatchSetup`. Экраны не тупик: Esc/B у **любого**
пира шлёт `e_NetLobbyAction_BackToTeams`, сервер возвращает фазу в `Teams` и
сбрасывает `teamReady[0..1]` (выбор команд можно переиграть). `NetworkLobbyPage`
стал только фазой Teams (старт матча убран), `net_protocolVersion` 4 → 5. `nettest`
+13 проверок (round-trip options, Sides→Teams→Options, применение хостом, запрет для
не-хоста, возврат к командам хостом и не-хостом) → `PASS (60)`. Локальной
симуляции не касается, детерминизм `7134def2...` без изменений. `lanmatchtest`
`PASS (34)`. Вики [[сеть]], [[константы]] обновлены.

## [2026-09-14] feat | Замены в матче + сетевой синк; экран плана игры
Довели план игры до рабочего состояния и включили замены. **UI:** `GamePlanPage`
переписан в PES-подобный вид (карточка: фото → позиция+рейтинг → имя; лавка
горизонтально «позиция | имя | рейтинг», при большом составе — прокрутка; клампинг
карточек внутрь поля, зона вратаря резервируется отдельно; нижняя панель повторяет
карточку). В матче план строится по **активным** игрокам `Team::GetActivePlayers`
с ролями из `runtimeFormation`, лавка — по неактивным. **Замены:** подтверждение
«полевой ↔ запасной» → `Match::QueueSubstitution` (повтор — `CancelSubstitution`);
применяются на ближайшем возобновлении (`referee.cpp` → `ApplyPendingSubstitutions`),
игрок наследует роль ушедшего. Всплывашка «X off, Y on» в верхнем углу. **Сеть:** клиент шлёт
`e_NetLobbyAction_RequestSubstitution/CancelSubstitution`
(сторона=teamID, слоты), хост в `NetMatchSession::ProcessHost` проверяет владение
стороной, резолвит слоты и ставит в очередь; применённые замены (по слотам) целиком
идут в `Snapshot.substitutions`+`substitutionCounter`, клиент выполняет те же
`Team::Substitute` (активация модели + роль). Добавлены `Team::GetPlayerSlot`,
слоты в `Substitution`/`SubstitutionNotice`, `Match::CancelSubstitution`;
`net_protocolVersion` 5 → 6. Локальной симуляции не касается, детерминизм
`7134def2...` без изменений. Сборка ок, `nettest` `PASS (60)`, `lanmatchtest`
`PASS (34)`. Вики [[матч]], [[сеть]], [[константы]] обновлены; полевой тест замен
(UDP, отказ/отмена) — в [[открытые-вопросы]].

## [2026-09-14] fix | Замены: зависание при замене вратаря, мгновенный отклик, циклы
Три правки по следам полевого теста. (1) **Зависание**: `TeamAIController::CalculateDynamicRoles`
искал вратаря циклом `while (iter != end)` без `iter++`, если вратарь не первый в списке;
после замены вратаря он уезжал в конец активных — вечный цикл, затем краш. Добавлен `iter++`
(латентный баг, не связанный с сетью). (2) **Мгновенный отклик**: `GamePlanPage` теперь
пересобирает карточки с учётом очереди — при постановке замены уходящий сразу уезжает на
лавку, приходящий встаёт на его позицию/роль; пересборка отложена в `Process()`, чтобы не
удалять кнопку в её же обработчике. (3) **Циклические замены**: и `Match`, и страница
нормализуют цепочки (`X→Z`,`Z→W` ⇒ `X→W`; `X↔Z` ⇒ ноль), в `Match::QueueSubstitution`
ослаблена валидация (out может быть уже поставленным приходящим). Проверено headless:
GK-замена больше не виснет, цепочка схлопывается в одну запись. `nettest` `PASS (60)`,
`lanmatchtest` `PASS (34)`, детерминизм `7134def2...` без изменений. Вики [[матч]], [[сеть]]
обновлены.

## [2026-09-14] fix | Замена: краш (гонка активации) и запрет возврата игрока
Краш при выходе замененного игрока разобран по стеку: включили PDB (`/Zi`+`/DEBUG`) и
перехват `SetUnhandledExceptionFilter` с символьным стеком в `crash.txt` (dbghelp).
Стек: `Player::Put2D` → `GetGeomPosition()`, `humanoid == nullptr` (offset 0x14) у
активного игрока. Причина — **гонка активации**: `Player::Activate` ставил
`isActive = true` до `humanoid = new Humanoid(...)`, а воркер графики
(`GameTask::PutPhase` → `Match::Put` → `Team::Put2D`) идёт параллельно `Match::Process`
и в этом окне разыменовывал ещё не созданную модель; на старте матча рендер ещё не
крутится, поэтому проявлялось только после замены. Фикс: в `Activate` флаг ставится
последним, в `Deactivate` — снимается первым; `Put2D`/`Hide2D` защищены проверками на
пустые `humanoid`/подписи. Побочно: `GraphicsTask::ProcessPhase` теперь тоже держит
`getPhaseMutex` (обход списков объектов не должен идти параллельно структурным правкам).
Также добавлен запрет возврата: `Team::leftPitch` (+`HasLeftPitch`/`MarkPlayerLeftPitch`),
блокирует `Team::Substitute` и `Match::QueueSubstitution`, `Player::SendOff` помечает
удалённых; в плане игры ушедшие серые с `(out)` и не выбираются. Детерминизм
`7134def2...`, `nettest` `PASS (60)`, `lanmatchtest` `PASS (34)`. Вики [[матч]] обновлена.

## [2026-09-14] feat | План игры: две команды на одном экране (локальная игра вдвоём)
В локальном матче вдвоём экран плана переписан под две панели: у каждой команды своя
схема и лавка **справа** от неё; курсор и «взятый» игрок — свои у каждой панели. Ввод
читается напрямую с устройства-владельца (паттерн `SideSelectPage`:
`ProcessKeyboardEvent`/`ProcessJoystickEvent`, `event->GetButton(joyID,…)` +
`gamepad->GetButtonValue(…)`), `ProcessWindowingEvent` в двухпанельном режиме глушится,
фокус GUI2 держит сама страница. У ИИ-команды лавки/управления нет. Выход — голосование
(Back/B: первый раз «готов», повторно — снять; уходим когда готовы все). Однопанельный
режим (1 игрок / против ИИ / пре-матч / сеть) сохранён. Детерминизм не затронут;
`nettest`/`lanmatchtest` зелёные. Вики [[матч]] обновлена.

## [2026-09-16] fix | Сеть: план игры правит команду локального пира, а не всегда team 0
`GamePlanPage` в сетевом матче брал `teamID` из `IngamePage` (при паузе он жёстко 0,
`gamepage.cpp`), а для одиночной панели — из `MenuTask::GetControllerSetup`, который
зеркальный экран сторон не заполняет (`NetworkSideSelectBackend` не зовёт
`SetControllerSetup`; это делает только `LocalSideSelectBackend`). В итоге гостевая
сторона (и хост на выезде) редактировала команду 0, а хост молча отбрасывал такие
запросы в `NetMatchSession::ProcessHost` (несовпадение владельца стороны). Добавлен
`MenuTask::GetLocalNetworkTeamID()` — сторона локального пира из авторитетного
`LobbyState` (хост — `NetServer`, клиент — `NetClient` + `GetPlayerId`); в сети
`GamePlanPage::SetupPanels` строит единственную панель по ней, соперник read-only.
Меню-слой, детерминизм не затронут; сборка ок, `lanmatchtest` `PASS (34)`.
Вики [[матч]], [[сеть]] обновлены.

## [2026-09-16] fix | Сеть: план игры — доведена правка стороны (ветка «одна панель» затирала выбор)
Предыдущий фикс `GamePlanPage::SetupPanels` был неполным: ниже по коду ветка «одна
панель» делала `teams.clear()` и подставляла `requestedTeamID` (= 0), затирая
команду, вычисленную из лобби. В полевом тесте оба пира правили команду хозяев,
а у гостя не было своей лавки. Теперь `requestedTeamID` подставляется только при
пустом списке (`teams.empty()`), а единственная команда локального пира
сохраняется. Меню-слой, детерминизм не затронут; сборка ок, `lanmatchtest`/`nettest`
зелёные. Вики без изменений (описываемое поведение теперь соответствует коду).

## [2026-09-16] feat | Сеть: пре-матч хаб как в одиночке, голосования и синк составов
Фаза `Options` в LAN теперь открывает `PreMatchPage` (тот же хаб, что в одиночной
игре: Kit / Stadium / Kick-off / Game plan / Options / Camera / System) вместо
редуцированной `NetworkMatchOptionsPage`. Хост-авторитетно: Options-слайдеры
меняет только хост (`SetMatchOptions`, зеркалится), Camera/System — локально.
**Game plan** — экран общий (зеркальный), открывает только хост
(`e_NetLobbyAction_SetGamePlanOpen` → `LobbyState.gamePlanOpen`; меню-слой
`MenuTask::UpdateNetworkPlanOverlay` поднимает `GamePlanPage` у всех). Каждый пир
правит только свою команду; клиентские правки — `e_NetLobbyAction_PlanSwap`
(side + db id), хост в `MenuTask::ProcessNetworkPlanEdits` проверяет владение,
применяет к своему `MatchData` и рассылает авторитетный `e_NetMessage_PlanSwap`;
`GetPlanRevision()` заставляет страницу перестроиться. **Голосования** peer-equal
на три действия (`e_NetHubVote`: back-to-teams, close-plan, start-match):
`e_NetLobbyAction_HubVote` + `LobbyState.hubVote`/`NetLobbyPlayer.hubVote`,
хост исполняет через `NetServer::ConsumeHubVoteResult`. Протокол 6 → 7. Меню-слой,
детерминизм не затронут; сборка ок, `nettest` `PASS (60)`, `lanmatchtest` `PASS (34)`.
Вики [[сеть]], [[матч]], [[константы]] обновлены; полевой тест хаба — в
[[открытые-вопросы]].

## [2026-09-16] fix | Хаб: выход из плана, зеркальная раскладка гостя, отмена голосов
Три правки по следам полевого теста. (1) **Выход из плана не работал**: оверлей
плана открывался через `topPage->CreatePage`, который удаляет страницу-хаб, поэтому
её `Process` больше не потреблял результат голосования. Голоса перенесены в
`MenuTask::ProcessNetworkHubVotes` (идёт всегда): close-plan → `SetGamePlanOpen(false)`,
back → `BackToTeams`, start → `ConsumeHubStartRequested()` (забирает `PreMatchPage`).
(2) **Раскладка гостя**: в сетевом одиночном плане своя команда теперь стоит по
стороне матча — home слева, away справа, лавка справа от неё, соперник с другой
стороны (`GamePlanPage::SetupPanels`, геометрия d0/d1 + `oppX/oppW`). (3) **Отмена
голосов и счётчики**: повторное нажатие Start/Esc отзывает свой голос
(`HubVote` value 0); `PreMatchPage` показывает tally в `statusCaption` (start/back),
`GamePlanPage` — в `exitStatus` (close plan), кнопка превращается в «Cancel start».
Сборка ок, `nettest` `PASS (60)`, `lanmatchtest` `PASS (34)`. Вики [[сеть]], [[матч]]
обновлены.

## [2026-09-16] fix | Хаб: слоёный Esc (снять start → выйти → отменить выход)
Esc в сетевом хабе теперь разбирается по порядку, а не сразу предлагает выход:
если пир уже подтвердил старт — снимает эту готовность; иначе предлагает выход из
хаба (`BackToTeams`); повторный Esc на своём предложении выхода отзывает его.
Правка в `PreMatchPage::ProcessWindowingEvent` (сеть). Сборка ок.

## [2026-09-16] fix | Хаб: отмена голоса не зависела от зеркала, tally при части голосов
Отмена «Confirm start» не работала: страницы определяли свой голос через
зеркальный `LobbyState`, а при одном проголосовавшем глобальный `hubVote` = None,
поэтому Esc уходил в «выход из хаба». Теперь страницы помнят свой голос сами
(`PreMatchPage::localVote`, `GamePlanPage::localCloseVote`) и переключают его:
Esc в хабе сперва снимает start, затем предлагает выход, затем отменяет выход;
Start-кнопка превращается в «Cancel start». Статус показывает tally по каждому
действию («start 1/2») уже при части голосов. Неиспользуемый `MenuTask::GetLocalHubVote`
удалён. Сборка ок, `nettest` `PASS (60)`, `lanmatchtest` `PASS (34)`.

## [2026-09-16] fix | Хаб: стрелка вверх возвращает фокус на вкладки
С кнопки Start нельзя было уйти на полосу вкладок: сетки контента принимают
windowing-событие направления и не пускают его наверх (у них нет выбираемых строк
выше), поэтому `PreMatchPage::ProcessWindowingEvent` до события «вверх» не доходил.
Добавлен `PreMatchPage::ProcessKeyboardEvent`: повтор `SDLK_UP` из контента
возвращает фокус на активную вкладку (работает и по клавиатуре, и через
windowing). Сборка ок.

## [2026-09-16] fix | Сеть: краш клиента при замене (поза на неактивного игрока)
Клиент падал в `HumanoidBase::SetRemotePose` (`crash.txt`, 0xC0000005) при
`ApplySnapshot → ApplySnapshotPose`: `Match::ApplyRemoteSnapshot` применял позы
снапшота **до** релея замен, а в снапшоте пришедший игрок уже активен у хоста —
у клиента его `humanoid` ещё `null`. Фикс: релей `Team::Substitute` теперь идёт
первым (модель создана до поз), плюс защитные проверки — `PlayerBase::SetRemotePose`
не разыменовывает пустой `humanoid`, `ApplySnapshot` пропускает неактивных игроков.
Детерминизм не затронут, сборка ок, `nettest` `PASS (60)`, `lanmatchtest` `PASS (34)`.
Вики [[сеть]] обновлена.

## [2026-09-16] fix | Замена выходила в старой форме после смены кита в матче
`Team::ActivatePlayer` (вызывается при активации и при замене) строил путь кита из
очереди матча — `GetMenuTask()->GetTeamKitNum(GetID())`, — которая не меняется по
ходу игры, тогда как `Team::SetKitNumber` обновляет рантайм-поле `kitNumber`. После
смены кита в матче замена выходила в старой форме. Теперь `ActivatePlayer` берёт
`kitNumber` (с тем же добиванием до двух знаков, что и `SetKitNumber`). На старте
матча поведение прежнее (снимок очереди = kitNumber), визуальное изменение,
детерминизм не затронут. Сборка ок, `nettest`/`lanmatchtest` зелёные. Вики [[матч]].

## [2026-09-17] feat | Хаб: план игры открывается локально, без голосования за выход
По требованию UX: экран плана больше не зеркалится на всех. Каждый пир открывает
его у себя («Open game plan» доступна и клиенту), Esc закрывает локально, без
голосования; синхронизация правок состава осталась как была (`PlanSwap` через хоста,
`GetPlanRevision` → перестройка у обоих). Убраны `LobbyState.gamePlanOpen`,
`e_NetLobbyAction_SetGamePlanOpen`, `e_NetHubVote_CloseGamePlan`,
`MenuTask::UpdateNetworkPlanOverlay` и close-plan-ветки; `GamePlanPage` в сети
теперь только read-only-соперник + отправка свапов и refresh. Протокол 7 → 8.
Сборка ок, `nettest` `PASS (60)`, `lanmatchtest` `PASS (34)`. Вики [[сеть]],
[[открытые-вопросы]] обновлены.

## [2026-09-18] feat | Локальный выбор команд: гость с первой ступени, два игрока параллельно
Две UX-правки в локальном быстром матче (`TeamSelectPage`). (1) Гостевая панель начинала
выбор сразу с команды: `FocusCompetitionSelect2` прыгал на `teamSelect2` из-за дефолта
«National Teams». Теперь обе панели стартуют на первой ступени (страна), как домашняя.
(2) Два игрока за одним ПК теперь выбирают команды **параллельно**: если на экране выбора
сторон обе стороны назначены разным устройствам, страница показывает обе панели сразу и
ведёт их независимыми курсорами, читая сырые `KeyboardEvent`/`JoystickEvent` (глобальный
фокус GUI не используется). Один человек против CPU — прежний пошаговый поток: панель
гостей открывается после Ready хозяина и управляется тем же устройством. В ядре GUI
добавлены `Gui2IconSelector::MoveSelection`/`SetHighlighted` и `Gui2Button::SetHighlighted`
(подсветка без фокуса). Удалён ставший ненужным `SetActiveController`. LAN-фаза Teams не
затронута (там фокус уже на стране, а параллельность естественная — у каждого свой экран).
Сборка ок; полевой тест на реальных двух устройствах не делался. Вики [[архитектура]],
[[открытые-вопросы]] обновлены.

## [2026-09-18] feat | Уровни выбора команд переключаются вверх/вниз (локально и LAN)
По UX-замечанию: не нужно подтверждать каждый уровень (страна → лига → клуб) отдельно.
Up/Down (крестовина/стик/стрелки) переключают уровень, Left/Right — выбор внутри уровня,
а команды фиксируются кнопкой Ready (текущее показанное); Enter/A уровень больше не
подтверждает. Локальный `TeamSelectPage`: `ActivateSide` теперь только подтверждает Ready,
добавлен `MoveSideRow` (национальные пропускают пустую лигу). LAN `NetworkLobbyPage`: убраны
`sig_OnClick`-переходы фокуса, уровни двигает `Gui2Grid` по Up/Down, пустая лига для
национальных не-selectable и пропускается; подсказки обновлены. Сборка ок, `determinism`
совпал, `nettest` `PASS (60)`, `lanmatchtest` `PASS (34)`. Вики [[архитектура]], [[сеть]]
обновлены.

## [2026-09-18] fix | Крестовина геймпада не двигала выбор команд локально
Локальный `TeamSelectPage` опрашивал направление через `HIDGamepad::GetButtonValue(
e_ButtonFunction_Up/Down/Left/Right)`, а по умолчанию эти функции замаплены на левый
**стик** (`controllerMapping` = оси), не на крестовину — поэтому D-pad не двигал ни уровни,
ни выбор. Теперь в `Process()` направление опрашивается каждый кадр как сумма стика и
`SDL_GAMEPAD_BUTTON_DPAD_*` из `UserEventManager` (как в `guitask.cpp`); заодно удержание
крестовины повторяется (раньше — только одиночные нажатия). LAN-фаза Teams крестовину видит
и без этого: там направление собирает `guitask` через активный joystick. Сборка ок,
`determinism` совпал, `nettest` `PASS (60)`, `lanmatchtest` `PASS (34)`.

## [2026-09-18] fix | Enter/A снова переводит на следующую секцию выбора команд
Уточнение к предыдущей правке: Enter/A на стране/лиге/клубе — это переход к следующей
секции (страна → лига → клуб → Ready; для национальных лига пропускается), а Up/Down —
свободное переключение секций. То есть стрелки дополняют ввод, а не заменяют его.
В локальном `TeamSelectPage` возвращён advance в `ActivateSide` (при сохранённом
`MoveSideRow`), в LAN `NetworkLobbyPage` возвращены `sig_OnClick`-переходы фокуса.
Сборка ок.

## [2026-09-18] fix | Возврат из хаба сохраняет выбранные команды (локально)
Раньше Esc из локального пре-матч хаба (`PreMatchPage::GoBack`) пересоздавал
`TeamSelectPage`, который сбрасывался на «National Teams» — выбор терялся. Теперь
`GoOptionsMenu` ставит флаг `restoreSelections` в своих `pageData.properties` (общий с
записью в `pagePath`, поэтому он переживает GoBack), а конструктор страницы по флагу
восстанавливает карусели: `MenuTask` уже хранит id команд (`SetTeamIDs`/`GetTeamID`),
страница резолвит по БД `team → league → country` (для сборных — спец-пункт «national»)
и выставляет страну/лигу/команду каждой стороны. Свежий заход с экрана выбора сторон
флага не имеет и по-прежнему стартует с дефолта. LAN не трогал: там выбор живёт в
серверном `LobbyState` и уже восстанавливается при возврате в фазу Teams (BackToTeams
не сбрасывает `countryId/leagueId/teamId`). Сборка ок, `determinism` совпал, `nettest`
`PASS (60)`, `lanmatchtest` `PASS (34)`.

## [2026-09-18] feat | Усталость игрока в плане игры
На экране плана (`GamePlanPage`) у каждой записи справа от рейтинга показывается условие
игрока: `GetFatigueFactorInv()` × 100 %, цвет по шкале голубой (свежий) → зелёный → жёлтый →
оранжевый → красный (выжат), пороги вынесены в `FatigueColor` (`src/menu/gameplan.cpp`,
[[константы]]). Пишется для всех, включая лавку; у запасных и в пре-матче тика нет, поэтому
там всегда 100 %. Значение берётся из живого `Team` панели (`GamePlanPage::EntryFatigue`).
Чтобы тонкий клиент не показывал всех свежими, `fatigue` добавлена в `SnapshotPlayer`
(захват/запись/чтение/применение/бленд, `PlayerBase::SetFatigueFactorInv`), протокол поднят
до v9 ([[сеть]]). Сборка ок, `lanmatchtest` `PASS (34)`.

## [2026-09-18] feat | Лавка шире и локальные голосования вдвоём
В плане игры в локальной игре вдвоём панели лавки расширены (ширина 15 → 18 %, поле 30 → 29 %),
чтобы имена влезали рядом с рейтингом и условием (`src/menu/gameplan.cpp`, `d0_*`/`d1_*`).
Старт матча из пре-матч хаба и снятие паузы теперь голосуются **по сторонам** и в локальной
игре вдвоём: каждая сторона подтверждает своим устройством (клавиатура Enter, геймпад A),
кнопка показывает счётчик; GUI-активация Start/Continue в этом режиме — заглушка, голоса
собираются с устройств по `controllerID` (`MenuTask::GetLocalSideDevices`/`HasTwoLocalPlayers`,
`PreMatchPage::ToggleLocalStartVote`, `IngamePage::ToggleLocalResumeVote`). Так же, как в LAN,
но вместо пиров — стороны на одном компе. Сборка ок, `determinism` не трогали, `nettest`
`PASS (60)`, `lanmatchtest` `PASS (34)`.

## [2026-09-18] decision | Заполнение меню на весь экран попробовали и откатили
Пытались убрать леттербоксинг 5:4 и заполнить окно (`aspectRatio = contextW/contextH` в
`Gui2WindowManager`), чтобы план игры занял и «свободное» поле по бокам. Опровергнуто на
глазах: арт меню нарисован под кадр 5:4, и растягивание на 16:9 неравномерно тянет фоны,
картинки геймпада/клавиатуры и попутно мылит их (`SDL_ScaleSurface` апскейл). Свободное поле
существует именно из-за вёрстки в 5:4 — использовать его без перерисовки экранов нельзя.
Откат в `windowmanager.cpp` и `iconselector.cpp` (компенсация квадрата там не нужна при
5:4). Осталась починка навигации плана игры **крестовиной**:
`GamePlanPage::ProcessJoystickEvent` читает `SDL_GAMEPAD_BUTTON_DPAD_*` напрямую, т.к.
функции движения замаплены на стик (как `TeamSelectPage`). Сборка ок, `nettest` `PASS (60)`,
`lanmatchtest` `PASS (34)`.

## [2026-09-18] fix | Back в паузе = Continue, повторное нажатие снимает готовность
В паузе матча Back (Esc/клавиатура, B/геймпад) теперь равнозначен кнопке Continue:
тот же голос за возобновление, повторное нажатие его снимает. В сети это уже делал
`IngamePage::VoteResume` (toggle `resumeReady`); для локальной игры вдвоём Back добавлен
в сырые обработчики `ProcessKeyboardEvent`/`ProcessJoystickEvent` (по устройству стороны),
а windowing-Escape в локальном режиме глушится, чтобы не уйти `GoBack`. Сборка ок.

## [2026-09-18] fix | Back после Start match снимает готовность и возвращает на Kick-off
В пре-матч хабе Back (Esc/клавиатура, B/геймпад) после поставленного голоса за старт
теперь снимает именно этот голос и переводит фокус на вкладку Kick-off
(`PreMatchPage::FocusKickOffTab`), а не предлагает выход из хаба. В сети слоёный Esc
(`localVote`) при снятии Start поднимает вкладку Kick-off; в локальной игре вдвоём — сырые
обработчики по устройству стороны (`CancelLocalStartVote`), а следующий за ними
windowing-Escape глушится флагом `suppressLocalEscape` (одноразовый, сбрасывается в
`Process`). Повторное нажатие Back после снятия — обычный слоёный выход. Сборка ок.

## [2026-09-18] fix | Нейтральная подпись Start match при игре вдвоём
После голоса первого игрока кнопка старта подписывалась «Cancel start», но второй игрок
видит тот же общий экран и тем же нажатием **запускает** матч — подпись вводила в
заблуждение. В локальной игре вдвоём подпись нейтральна: «Start match (N/2)»
(`PreMatchPage::UpdateLocalStatus`); повторное нажатие снимает готовность только того
устройства, что нажало. В сети подпись «Cancel start» остаётся — экран у каждого свой.
Сборка ок.

## [2026-09-18] fix | После замены соперника план игры показывал старого игрока
На хосте (и на клиенте) чужая команда в однопанельном плане рисуется read-only через
`GamePlanPage::BuildOpponent` из `TeamData` — стартовый XI, поэтому после замены команды
клиента на хосте ушедший игрок оставался на панели. Теперь в матче `BuildOpponent` берёт
**живой** `Team` (`GetAllPlayers` + `IsActive` + `GetFormationEntry` из `runtimeFormation`),
как `BuildPanel`; без матча (пре-матч) источник остаётся `TeamData`. Сборка ок,
`lanmatchtest` `PASS (34)`, `nettest` `PASS (60)`.

## [2026-09-23] decision | «Назад» — только Escape/B, Backspace убран из игры
Правило проекта (записано в AGENTS.md, раздел Conventions): «назад» в UI — это **Escape**
(клавиатура) и **B** (геймпад); **Backspace не используется нигде** — ни как «назад», ни как
отладочная клавиша. Убраны два места: `GamePlanPage::ProcessKeyboardEvent` (был `ESCAPE ||
BACKSPACE`) и отладочный телепорт мяча в `Ball::Process` (был `SDLK_BACKSPACE`, перенесён на
`SDLK_F9`). Причина: в плане игры Backspace уводил/не уводил не туда, а с редактора состава
нужно возвращаться на панель разделов именно Escape/B.

## [2026-09-23] feat | По-сторонние панели разделов в плане игры
У `GamePlanPage` (`src/menu/gameplan.cpp`, `src/menu/gameplan.hpp`) появились нижние панели
разделов **Tactics / Positions / Roles** (`e_GamePlanSection`) — **своя у каждой действующей
стороны**, под её полем в её половине экрана; состояние раздела (`activeSection`, `barCursor`,
`barFocused`) переехало в `PlanPanel`. Вход нейтральный на каждую сторону (стоит на своей панели):
**Left/Right** выбирает раздел, **Enter** открывает, **Down** с поля (когда ниже игрока нет) или
**Back** возвращает на панель, **Back** с панели — голос «выйти» (в локальном 2P навигация идёт
через per-device `HandlePanelInput`; в однопанельном — через GUI-фокус). Сторона без локального
управления (ИИ или удалённый пир) вместо панели показывает статичный **READY**. Шапка `GAME PLAN`
убрана, на её месте — баннер **READY TO LEAVE N/M** во время голосования. Работает только
**Positions**; **Tactics** и **Roles** (исполнители стандартов) — заглушки. Сборка `Release` x86
(Win32) ок. Детали — [[открытые-вопросы]].

## [2026-09-23] feat | Тактические схемы в плане игры (локально и по сети)
Раздел **Tactics** (`GamePlanPage`, `src/menu/gameplan.cpp`) больше не заглушка: пока он открыт,
колонка лавки превращается в список схем; листание **предпросматривает** подсвеченную схему на
поле, **Enter/A фиксирует**, **Back/Esc/B** возвращает к панели и откатывает неподтверждённый
предпросмотр. Схемы захардкожены в новом `src/menu/tacticschemes.cpp` (4-4-2, 4-3-3, 4-2-3-1,
3-5-2, 5-3-2, 4-5-1); позиция роли адаптируется той же формулой, что и формации команд
(`databasePosition * 0.6 + GetDefaultRolePosition(role) * 0.4` — объявление функции вынесено в
`teamdata.hpp`). Расстановка — оптимальное назначение венгерским методом
(`src/misc/hungarian.c`): вратарь закреплён, остальные минимизируют сумму «дистанция + штраф за
чужую линию» (защита/полузащита/атака). В пре-матче схема пишется в `TeamData`
(`ApplySchemeToTeamData`), в матче — в рантайм-формацию живых игроков
(`Team::SetRuntimeFormationEntry`, `ApplySchemeToTeam`), поэтому ИИ подхватывает расстановку. В
сети правка host-authoritative: `e_NetLobbyAction_PlanScheme` → `MenuTask::ApplyPlanScheme` →
`e_NetMessage_PlanScheme` (протокол v10, см. [[сеть]]). Сборка `Release` x86 (Win32) ок,
`nettest` `PASS (60)`, `lanmatchtest` `PASS (34)`, детерминизм `7134def2…` совпал. Полевой тест
схем (локально и по сети) не делался — [[открытые-вопросы]].

## [2026-09-23] feat | Роли (капитан и исполнители стандартов) в плане игры
Раздел **Roles** (`GamePlanPage`, `src/menu/gameplan.cpp`) реализован. Пока он открыт, **полоса
между полем и панелью разделов** становится листаемым списком ролей (капитан, штрафной дальний,
штрафной ближний, пенальтист, левый/правый угловой — порядок `planRoleOrder`, имена
`GetTeamRoleName`). Справа в строке — текущий исполнитель: назначенный слот или, если роль не
задана, лучший подходящий игрок. **Enter/A** открывает выбор: список скрывается, в той же полосе
показывается карточка наводимого игрока, стрелки водят курсор **только по полю**, **Enter/A**
фиксирует и возвращает к списку, **Back/Esc/B** отменяет выбор (двухуровневый Back: выбор →
список → панель).
Модель ролей переведена на **слот состава** (`MatchData::SetRolePlayer`/`GetRolePlayer`, -1 =
не задан): слот валиден и в пре-матче (`TeamData`), и в матче (`Team`), и не зависит от
процесс-глобального `PlayerBase::id`. `Team::GetRoleSlot` возвращает назначенный слот, если
игрок ещё на поле, иначе **лучшего подходящего активного** (`Team::SuggestRoleSlot` +
`TeamRoleSuitability` в `src/data/teamdata.{hpp,cpp}`; вратарь исключён; детерминированный
бленд статов — общий для ИИ и пре-матч-подсказки). ИИ (`TeamAIController::PrepareSetPiece`)
теперь реально ставит лучших на пенальти и угловые; для штрафных исключение: **со своей
половины — ближайший** (вынос), **с чужой — назначенный ближний/дальний** (порог
`freeKickNearDistance` без изменений). В сети правка host-authoritative:
`e_NetLobbyAction_PlanRole` (`value` = `e_TeamRole`, `value2` = слот) →
`MenuTask::ApplyPlanRole` → `e_NetMessage_PlanRole`; передаётся слот, а не player id
(протокол **v11**, см. [[сеть]]). Сборка `Release` x86 (Win32) ок, `nettest` `PASS (60)`,
`lanmatchtest` `PASS (34)`, детерминизм `7134def2…` совпал. Полевой тест раздела Roles
(локально, 2P, по сети) не делался — [[открытые-вопросы]].

## [2026-09-24] fix | План игры: кнопки списка ролей делили картинку с карточками поля

При листании списка ролей в плане игры (локальный 1P) оранжевая плашка появлялась «на игроках
на поле» и двигалась вместе с курсором списка. Причина - коллизия имён виджетов GUI2:
`Gui2WindowManager::CreateImage2D` берёт поверхность через
`ResourceManager::Fetch(name, load=false, useExisting=true)`, то есть по имени виджета
возвращает уже существующий ресурс. Кнопки списка ролей и подписи роли на карточках
поля/лавки назывались одинаково (`gameplan_role_<teamID>_<i>`), поэтому оранжевая
подсветка выбранной строки заливала карточку того же слота. Диагностика (`fmin=210`,
`fcard/hcard/tcard` пусты) отсекла версию с фокусом/fade карточки и указала на чужое
изображение. Кнопки списка переименованы в `gameplan_rolerow_*`. Ловушка записана в
[[архитектура]] (инварианты). Сборка Release x86 (Win32) ок, проверено полем.

## [2026-09-24] feat | Роли в снапшоте сети (v12): реконсиляция для позднего клиента

Роли стандартов (`MatchData::rolePlayers`) добавлены в `Snapshot`
(`roles[2][e_TeamRole_SIZE]`, -1 = не задан): `CaptureSnapshot` берёт их из `MatchData`,
`WriteSnapshot`/`ReadSnapshot` сериализуют, `ApplySnapshot` применяет к `MatchData`
клиента. Раньше роли жили только в live-релее `e_NetMessage_PlanRole`, поэтому клиент,
подключившийся или переподключившийся посреди матча, не видел назначения, сделанные до его
входа (план показывал авто-подбор `SuggestRoleSlot`). Протокол v11 → v12. `nettest`
PASS (60), `lanmatchtest` PASS (34), детерминизм `7134def2…` совпал (снапшот не влияет на
симуляцию). Обновлены [[константы]] (было 9) и [[сеть]].

## [2026-09-24] feat | Фото игроков в плане игры: портретные вырезки

План игры показывает реальные портреты: ассеты — вырезки Transfermarkt (фон удалён, палитра
128, 160×208, ~10 КБ) из `data/images/faces_cutout/<tm_id>.png`. Генератор
`tm-gf-face-generator/export_cutouts.py --matting mediapipe` делает их из
`data/images/faces/<tm_id>.jpg` (плейсхолдеры TM дают пустую маску → файла нет → в игре
плейсхолдер); импорт (`builders/files.py::copy_faces`) кладёт PNG в
`databases/default/faces/`. В `src/menu/gameplan.cpp` все фото-боксы стали портретными
(`photoAspect = 208/160`): карточки поля, read-only соперник, нижняя инфо-полоса и пикер
ролей; `PlayerFacePath` читает `.png`. `tools/copy_data.cmake` копирует `faces/` в сборку
только для Release (флаг `COPY_FACES`), dev-сборка исключает. Собрано Release x86 (Win32);
визуально полем не проверялось (см. [[открытые-вопросы]]: mediapipe-дырки, вес бандла
~680 МБ). Обновлены [[данные-из-transfermarkt]], [[матч]], [[константы]].

## [2026-09-24] fix | Палитровые PNG в плане игры: индексы читались как RGB

Фото на карточках появлялись, но текстура была мусором (диагональный «снег»). Причина:
вырезки — палитровые PNG (`PLTE`+`tRNS`), SDL_image 3.4.4 оставляет их как
`SDL_PIXELFORMAT_INDEX8` (`IMG_libpng.c`: в RGB/RGBA конвертируются только не-палитровые),
а `GetGLPixelFormatFromSurface` (`opengl_renderer3d.cpp`) на 1 байт/пиксель отдаёт `GL_RGB`
— палитровые индексы грузятся как цвет. Фикс: `Surface::SetData`
(`src/scene/resources/surface.cpp`) конвертирует индексированные поверхности в RGBA32
(`SDL_ConvertSurface`) на входе, до GL-загрузки; путь общий для GUI и 3D-текстур. Заодно из
`data/databases/default/faces/` удалены мёртвые raw `.jpg` (~4 ГБ, игра читает `.png`).
Обновлена [[данные-из-transfermarkt]].

## [2026-09-24] fix | Кэш копирования фото: сборка снова быстрая

После подключения фото каждый Release-билд копировал ~63k PNG (~680 МБ) через
`file(COPY)`, из-за чего сборка шла минутами. `tools/copy_data.cmake` теперь кэширует
копирование: stamp-файл `<build>/.faces_copied` хранит время исходного каталога
`data/databases/default/faces`, и фото копируются только если его нет или исходник
изменился (ре-импорт добавляет/удаляет файлы и обновляет mtime). Не-face данные по-прежнему
копируются каждый билд. Проверено: повторная Release-сборка ~46 c против ~5 мин, сообщение
`copy_data: copying player photos` не появляется. Обновлена [[данные-из-transfermarkt]].

## [2026-09-24] feat | Выделение карточки обводкой и скрытие панели разделов

План игры: выделение карточки игрока (курсор/held) — теперь **белая обводка по контуру
фото**, а не оранжевый квадрат. Кнопка карточки оставлена для кликов, но её рамка
отключена (`Gui2Button::SetDrawFrame(false)`); для каждой карточки заранее готовится вторая
копия фото с контуром (`Gui2Image::SetDrawOutline(true, 4)`, радиус 4, чтобы портрет мало
сжимался из-за паддинга canvas) и показывается вместо обычной — переключение только
видимость, без перезагрузки. `SetDrawOutline` получил необязательный радиус.
Панель разделов (Tactics / Positions / Roles) скрывается, пока открыт какой-либо раздел, и
возвращается при выходе на панель (`RefreshSectionBar` → видимость по `barFocused`).
Собрано Release x86 (Win32), 0 ошибок. Обновлены [[матч]] и [[константы]].

## [2026-09-24] fix | Фото не менялось при пре-матчевом обмене игроков

В пре-матче `PerformAction` меняет игроков через `TeamData::SwitchPlayers` и обновляет
экран на месте (`Refresh()`, без пересборки), а `RefreshPanel` перерисовывал только
имя/позицию, не портрет — карточка оставалась с фото прежнего игрока (в матче замены
идут через `Rebuild`, поэтому там фото обновлялось). Фикс: `RefreshPanel` для карточек
поля догружает портрет по `PlayerFacePath(player)` (обычный и контурный) — `LoadImage`
пропускает тот же путь, так что лишней работы нет. Обновлена [[матч]].

## [2026-09-24] fix | Локальный 2P: стороны путались местами в плане игры

При локальной игре вдвоём панели плана игры раскладывались по порядку итерации
`GetControllerSetup()`, а не по выбранной стороне. Порядок зависит от `controllerID`, а
клавиатура не обязательно нулевая — при наборе «геймпад слева, клавиатура справа» панели
выходили зеркально (правая команда на левой половине). Фикс: `SetupPanels` сортирует
`SideSelection` по `side` (−1 слева, +1 справа) перед раскладкой. Обновлена [[матч]].

## [2026-09-24] deploy | Лица игроков — отдельный релизный ассет; инструкция сборки в README

`package_data.py` теперь собирает два бандла: каталог (БД + `images_*` + `template_kit.png`,
минус `faces/`) и `GameplayFootball-faces-<ver>.zip`; реестр `data-versions.json` получил
`faces_sha256`. Добавлен режим `--faces-only` — дособрать лица к уже опубликованному каталогу,
не пересобирая его: zip каталога **не** байт-воспроизводим (локальная пересборка дала
`a7d4fa39…` против опубликованного `84098522…`), поэтому sha каталога не трогаем. Бандл лиц
(`GameplayFootball-faces-2026-09-09.zip`, 668 МБ, sha `e46f741f…`) прикреплён к релизу v0.4.0.
В README игры добавлен раздел «Game data»: скачать оба архива и распаковать внутрь
`databases/default`. Решение: данные не коммитить в git и не выкладывать на внешний хостинг —
GitHub Releases + реестр в git (см. [[релиз]], [[пайплайн-данных]]).

## [2026-09-26] feat | Пре-матч хаб: 3D-капитаны и локальный выбор формы

В пре-матч хабе по бокам контента появились две 3D-модели капитанов в выбранной
форме и спокойной idle-позе (`PreMatchCaptainPreview`): приватная копия
`fullbody.object` с уникальным ресурсом, бейк позы портирован из `HumanoidBase`
(кадр `movement/idle/000_idlelevel1.anim`), рост нормализован. Рисует их камера
меню-сцены (отдельный camera-view не заводится): модели — `ScreenAnchor` в
`MenuScene`, трансформ пересчитывается вместе с камерой под `getPhaseMutex` (иначе
капитаны дрожали при панораме), и помечены `no_cull`, иначе консервативные
плоскости отсечения камеры их выкидывали. Вкладка Kit стала реальным выбором
комплекта (пулдауны `_kit_01..06`, `MenuTask::SetTeamKitNum` → `Team::InitPlayers`).
Формы — **чисто локальная настройка** каждого пира, по сети не синхронизируются:
из протокола убраны поля kit (`NetLobbyState`/`NetMatchSetup`/`NetMatchEnvironment`),
в `NetMatchEnvironment` остаётся только солнце/погода (v14). По ходу найдены и
починены два бага: залипающий `Gui2Grid::hasSelectables` (вечный цикл
`ProcessWindowingEvent` при direction-навигации, если представление стало
non-selectable после `AddView`; пересчитывается в `UpdateLayout`) и отсутствие
`getPhaseMutex` у `Team::SetKitNumber` (гонка с обходом сцены). `nettest` PASS (59),
`lanmatchtest` PASS (34), детерминизм `7134def2…` без изменений. Обновлены [[матч]],
[[сеть]], [[константы]], [[открытые-вопросы]].

## [2026-09-27] fix | Подписи регионов кит-шаблона проступали на формах

На формах всех команд читались серые надписи `shirt front` / `shorts front` / `right sock`.
Причина — не код игры: `data/databases/default/template_kit.png` несёт зелёные подписи
регионов, а `kit-generator` красит кит как `цвет_региона × (G/база)`, поэтому тёмно-зелёный
текст подписей попадал в затенение и печатался на каждом сгенерированном PNG. Киты master
нарисованы вручную и подписей не имели — они появились вместе со сгенерированным датасетом.

Фикс корня: `kit-generator/kits/regions.py` выравнивает яркость подписей к базе региона
перед затенением — шаблон остаётся с аннотациями для людей, киты чистые. Существующие PNG
(по 27 750 в `data/`, `build/Release/`, `build-x64/Release/`) очищены in-place
(`pixel / brightness`); 12 файлов в `build/Release`, обрезанных прерванным прогоном до нуля,
восстановлены из `data/`. Обновлены [[данные-из-transfermarkt]], `kit-generator`
AGENTS.md/NOTES.md.

## [2026-09-27] fix | Кит-шаблон: вязаная сетка убрана, киты плоские

Поверх убранных подписей на формах читались серые клетки — это запечённая в киты
«вязаная» сетка шаблона (`template_kit.png`, зелёный канал как затенение); на шортах/гетрах
добавлялся мелкий рельеф bump-карты `kit_UVWnormal.png`. По решению владельца — плоские цвета.

`kits/regions.py` теперь игнорирует зелёный канал и красит регионы сплошным цветом
(`brightness = 1` везде, кроме швов/фона); bump-карта оставлена (даёт тканевые складки).
Существующие PNG (по 27 750 в `data/`, `build/Release/`, `build-x64/Release/`) выровнены
in-place (`pixel / brightness` для всех пикселей, кроме подписей; сами подписи не трогаются —
идемпотентно). Обновлены [[данные-из-transfermarkt]], `kit-generator` AGENTS.md/NOTES.md.

## [2026-09-27] decision | Маппинг ввода новых фич: curl/chip, раздача вратаря, одна раскладка

Закрыт тикет карты wayfinder [Маппинг ввода](https://github.com/polite-cat-2001/GameplayFootball/issues/4).
Убраны FIFA/PES-пресеты — остаётся одна PES-like раскладка. curl = `R2`/`C`+Shot, chip =
`L1`/`Q`+Shot (оба зажаты → обычный удар, тег `e_ShotType`; траектории — #8); раздача вратаря
на `ShortPass`/`Shot`/`HighPass`/`LongPass` (тап = раскат низом ближнему, hold = бросок верхом
дальнему, стик задаёт направление, порог 0.2 с); attacking-run + стоп-на-месте + super-cancel
переезжают на `L2`/`Special`, knock-on — на двойной тап Sprint. Полное решение — в issue #4,
индекс решений — в карте #1. Обновлён [[глоссарий]].

## [2026-09-27] decision | Прицеливание стандартов: heading-arc, виды подач, закрутка стиком

Закрыт тикет карты wayfinder [Прицеливание штрафной/угловой/от ворот](https://github.com/polite-cat-2001/GameplayFootball/issues/6). Модель прицеливания: стик-X крутит world-вектор направления подачи (штрафной — полный оборот, угловой и от ворот — сектор вперёд; база — центр ворот / 11-метровая отметка / вперёд по полю), прицел не сбрасывается. Вид подачи выбирается кнопкой, сила — удержанием: штрафной — ShortPass/LongPass/HighPass/Shot, угловой и от ворот — ShortPass/HighPass. Удар штрафного — сила+высота от заряда (две оси), прочие подачи — высота стиком-Y. Закрутка копится боковым стиком от нажатия до касания на любую подачу; ассист-магнетизм 18% только у удара, случайный scatter не переносим; черпачок на стандартах не используется. Поведение ИИ-тайкера вынесено в новый тикет #16 (заблокирован #8). Полное решение — в issue #6, индекс решений — в карте #1. Обновлён [[глоссарий]].

## [2026-09-27] decision | Игра за вратаря: отдельный слой, автовынос за 6 с, хэндофф

Закрыт тикет карты wayfinder [Вратарь: руки, бег, распределение](https://github.com/polite-cat-2001/GameplayFootball/issues/7). Механика — отдельный слой `src/onthepitch/keeper/` (Hands/Outfield/Returning) поверх `Match::ballRetainer`; новых `e_FunctionType_*` нет — вброс переиспользует аутовый throw-путь (`SelectRetainAnim` + `.../special/*_throw.anim`). Управление включается форсом на ловле и на бэкпасе (в руки нельзя, приём ногами); в Hands жёсткий кламп штрафной, «в ноги» → полевой без клэмпа, потеря мяча → бегом домой и управление полевому. Правило 6 секунд → автовынос в центр, управление ближайшему к приземлению; после ручной раздачи управление сразу адресату. Прицел — стик + автоподбор партнёра + заряд, без ретика; камера и HUD обычные. Сеть — как с полевыми, снапшот не меняем. Gameplay-механика → нужны новые эталоны `tools/determinism` (в тумане карты). Полное решение — в issue #7, индекс решений — в карте #1. Обновлён [[глоссарий]].

## [2026-09-28] decision | Удары curl/chip: баллистика от заряда + сильный низовой удар

Закрыт тикет карты wayfinder [Curl/chip через SetRotation и Magnus](https://github.com/polite-cat-2001/GameplayFootball/issues/8).
Модель удара перенесена из open_football `ShotSystem`/`action_executor`: обычный удар и curl — баллистический
пуск в точку на линии ворот, высота прицела растёт с зарядом (`AimYMin 0.8 → goalHeight+OverLift 3.5`, потолок
подъёма `6 м/с`); короткий тап (заряд < `0.35`) — настильный удар низом с фиксированной скоростью не ниже
`60 м/с` (аналог `SHOT_GROUND_POWER`, усилен с 40); время заряда **удара** `0.5 с`, у пасов `1 с`; curl = доворот
прицела `0.1 рад` + боковое вращение `zRot 90`, chip = отдельная дуга `8 м/с`; `shot direction assist` дефолт
`0.74`. Новых анимаций не нужно — и curl, и chip используют существующий клип `shot`. Двойной тап низом
отклонён: в GF удар летит на отпускании, окно «до касания» ~80 мс, флаг не успевал ставиться; модель эталона
(низом = короткий тап) принята. Gameplay-изменения меняют хеш `tools/determinism` → нужны новые эталоны
(остаётся в тумане карты). Прототип — ветка `prototype/curl-chip` (worktree `GameplayFootball-curl-chip`),
дебаг-клавиша `F2` убирает полевых соперников. Полное решение — в issue #8, индекс — в карте #1.

## [2026-09-28] decision | Оборона пенальти: вратарь остаётся ИИ, нырок всегда

Закрыт тикет карты wayfinder [Оборона пенальти: вратарь — человек и ИИ](https://github.com/polite-cat-2001/GameplayFootball/issues/13).
Управление: вратарь остаётся под ИИ, `controlled_player` не меняем; намерение — сайд-канал (стик
защищающегося человека в момент касания / случайная зона ИИ). Человеческое намерение берётся из уже
передаваемого направления устройства (`NetInputFrame.direction`) — протокол и снапшот не меняются
(v15 с `e_SetPiece` — это #2/#3, не нырок). Один стик: до удара X ведёт вратаря вдоль линии (кламп
до штанги, 3.0 м/с), в момент касания X+Y выбирают зону; мёртвая зона → CENTER. Пять зон; CENTER —
центральная реакция по высоте, не боковой прыжок. ИИ-зона — симметричный взвешенный `random`
(низ 25%, верх 12%, центр 26%) из детерминированного `random()`. Нырок выполняется всегда: при
угаданной зоне цель заменяется реальным перехватом летящего мяча, иначе прыжок идёт в представительную
точку зоны (в пустоту). Реализация — подстановка синтетического предсказания мяча в путь `deflect`:
штатное корневое движение клипа + `actionSmuggle`, новой физики нет. Искусственных «неберущихся»
порогов нет — ловля/отбой/гол решает геометрия и существующая сложность. Gameplay-изменение →
эталоны `tools/determinism` перегенерировать. Решение записано в issue #13, строка — в карте #1.
Связано: [[матч]].

## [2026-09-29] decision | Стенка при штрафном: 9.15 м к ближней штанг, авто-прыжок анимацией

Закрыт тикет карты wayfinder [Стенка при штрафном у своих ворот](https://github.com/polite-cat-2001/GameplayFootball/issues/14).
Направление (вариант b) подтверждено: стенка остаётся под ИИ, человеческого управления нет. Число игроков —
правило open_football: 0 дальше 40 м, 4 ближе 25 м, линейно между, минимум 2 (`FK_WALL_FAR/NEAR_DIST`,
`MIN/MAX_PLAYERS`). Линия — от мяча к ближней штанге ровным рядом с шагом 0.62 м (прежний веер на линии
к центру ворот убираем); ближе 9.15 м к воротам ряд встаёт на линию ворот. В стенку — N ближайших полевых
без вратаря и без игрока-человека. Прыжок — только анимацией: в GF игрок плоский в симуляции, подъём тела
задаёт z-канал кости `player`, своей физики прыжка нет; трёхмерное столкновение по позным костям
(`CheckBallCollisions`) делает поднятое тело реальным перехватом. Триггер высото-независимый (как в
референсе): одна команда на всю стенку, порог = половина `freeKickWallJumpTime` (0.25 с), повторно не
прыгать. Переиспользуем готовый клип (~0.3 м), отдельной константы высоты нет — `FK_WALL_JUMP_HEIGHT` из
формулировки тикета снята. Константы: `freeKickWallDistance=9.15`, `freeKickWallFarDist=40`,
`freeKickWallNearDist=25`, `freeKickWallMinPlayers=2`, `freeKickWallMaxPlayers=4`, `freeKickWallSpacing=0.62`,
`freeKickWallJumpTime=0.5` (`src/gamedefines.hpp`). Gameplay-изменение → эталоны `tools/determinism`
перегенерировать (уже в тумане карты). Решение записано в issue #14, строка — в карте #1. Термины «Стенка»
и «Прыжок стенки» — в [[глоссарий]]. Связано: [[матч]].

## [2026-09-29] decision | Стандарт: designated = бьющий, автопереключение владеет выбором

Закрыт тикет карты wayfinder [Автопереключение и designatedPossessionPlayer на стандартах](https://github.com/polite-cat-2001/GameplayFootball/issues/15).
Решено: тэйкера управляемым ставит **автопереключение** (`Team::UpdateSwitch`), а не презентация
(костыль `DebugForcePenalty` прототипа #9 не возвращаем); на стандарте designated отыгрывающей
команды = бьющий, поэтому все потребители (HUD, камера, `HumanController`) читают его как раньше.
Хост форсит designated при входе в стандарт; блок `match.cpp:1272-1288` (расчёт по времени до мяча)
пропускается при `IsInSetPiece()` — иначе затирает выбор автопереключения; порядок
`UpdateSwitch` -> `Process` -> `UpdatePossessionStats` не менялся (на `UpdateSwitch` держится очередь
`switchPriority`). Вратаря разрешено выбирать **только на стандарте** (когда он designated/бьющий);
в открытой игре запрет остаётся. `Switch` на стандарте подавлен (занят chip-комбо). При двух
локальных игроках бьющий достаётся первому в очереди `switchPriority`. На тонком клиенте designated
берётся из снапшотного тэйкера (team+slot, v15), не по близости. Код — расширение автопереключения,
не модуль `setpiece/`. Gameplay -> перегенерация эталонов `tools/determinism`.

Следствие: здесь же решили и закрыли отдельным тикетом [Смена бьющего на стандарте через меню (Share/Tab)](https://github.com/polite-cat-2001/GameplayFootball/issues/18) —
меню игроков команды (включая вратаря) меняет фактического бьющего текущего стандарта, только для
открывшего его бьющего-человека, управление остаётся за ним; кнопка — существующая
`e_ButtonFunction_Select` (геймпад Share, клавиатура Tab вместо F1); навигация стрелки/WASD,
Enter — применить, Escape — закрыть; вне стандарта и во время заряда/полёта не открывается; локально
у пира, намерение хосту по существующему input-каналу. Термины «Designated-игрок», «Бьющий»,
«Меню смены бьющего» — в [[глоссарий]]. Связано: [[матч]].

## [2026-09-29] decision | ИИ-тайкер: общий расчёт удара и стандарты

Закрыт тикет карты wayfinder [ИИ-тайкер: использование общего расчёта удара и стандартов](https://github.com/polite-cat-2001/GameplayFootball/issues/16).
Область — только **пенальти** и **штрафной**; аут, кикофф, угловой, от ворот остаются как сейчас (ИИ-поведение
углового — out of scope карты). Архитектура: выбор подачи — **чистые планировщики** в
`src/onthepitch/setpiece/`, `ElizaController` только собирает `PlayerCommand`; ИИ идёт напрямую, минуя
презентацию (#3). Chip на стандартах не применяется (#6). Штрафной: прямой удар, если точка в радиусе
(~≤30 м до ворот) и угол на створ в секторе ~±50°; иначе подача. Тип удара — 50% curl / 50% прямой силовой;
прицел — 90% угол, закрытый стенкой (обходят закруткой), 10% вратарский; высота — 95% верхний угол, 5%
низом под стенкой; curl фиксирован в сторону выбранного угла, сила — по дистанции. Подача: навес в штрафную
по умолчанию, низом — когда рядом открытый партнёр; адресат — партнёр с лучшей оценкой «открытость +
выдвинутость вперёд» вместо нынешних случайных точек и `AI_GetClosestPlayer` (`elizacontroller.cpp:176-183`).
Пенальти: прицел — угол (случайная сторона) или центр, сила случайная, чаще высокая; curl/chip нет. Числа —
в `src/gamedefines.hpp`. Gameplay → перегенерация эталонов `tools/determinism`. Решение записано в issue #16,
строка — в карте #1. Связано: [[матч]].

## [2026-09-29] decision | Разбег бьющего перед стандартом: движение-разбег и придержанный удар

Закрыт тикет карты wayfinder [Разбег бьющего перед исполнением стандартов](https://github.com/polite-cat-2001/GameplayFootball/issues/17).
Прототип в движке (ветка `prototype/setpiece-runup`), скоуп — пенальти; проверено в игре: разбег
виден, удар по мячу происходит, коридор за бьющим чист. Механика (GF-native): бьющий-человек
стартует за мячом (`_default_SetPiece_RunupDist = 2.2 м`, латераль 0.4 м под опорную ногу), разбег
ведёт обычная система движения (спринт-команда к мячу), а удар **придержан** до добегания
(`_default_SetPiece_KickReach = 1.1 м`), затем срабатывает штатный удар и клип доводит ногу до
касания. Аудит клипов: `.anim` — текстовый, смещение мяча на кадре касания задаёт дистанцию
(`shot\idle\020` ≈ 0.99 м, `walk\090_2step` ≈ 2.16 м, `sprint\090_2step` ≈ 2.27 м), поэтому отдельный
root-motion-клип (как `penalty_kick_*` в референсе) не нужен. У человека заряд совмещён с разбегом
(удержание кнопки) — отклонение от референса (там разбег на коммите), проверено и принято. Коридор
пенальти чистится в `Referee::PrepareSetPiece` (полуширина 1.5 м, 6 м за мячом) — обобщение лана
штрафного из #12. Разбег — в контроллере, не в презентации; `setpiece/`-модуль не нужен. Gameplay →
перегенерация эталонов `tools/determinism`. Не подтверждено прототипом (в реализацию): обобщение на
штрафной/угловой/от ворот и на ИИ-бьющего. Термин «Разбег бьющего» — в [[глоссарий]]. Решение записано
в issue #17, строка — в карте #1.

## [2026-09-29] session | Wayfinder #17: прототип разбега бьющего

Сессия wayfinder по карте #1: взят и закрыт тикет #17 (разбег бьющего). Работа велась в новом
ворктри `GameplayFootball-setpiece-runup` (ветка `prototype/setpiece-runup` от `task/setpiece-debug-keys`),
код прототипа — отдельным коммитом в этой ветке. При запуске ворктри пришлось скопировать
gitignored `databases/default/database.sqlite` из соседнего ворктри. Незакрытый хвост — прототип
пенальти-only: обобщение разбега на штрафной/угловой/от ворот и на ИИ-бьющего уезжает в спеку #11.

## [2026-09-29] decision | Уточнение #17: разбег только у пенальти и штрафного

Уточнение к записи о разбеге бьющего ([#17](https://github.com/polite-cat-2001/GameplayFootball/issues/17)):
разбег нужен только **пенальти и штрафному**; у **углового и удара от ворот разбега нет** — бьющий
остаётся вплотную к мячу, как сейчас. Формулировка предыдущей записи («обобщение на
штрафной/угловой/от ворот») отменяется в части углового и от ворот. В реализацию уезжает разбег
штрафного и ИИ-бьющего (механизм тот же). Карта #1 и термин в [[глоссарий]] обновлены.

## [2026-09-29] session | Wayfinder #11: спека-хендофф, карта закрыта

Финальная сессия wayfinder по карте [Порт фич из open_football](https://github.com/polite-cat-2001/GameplayFootball/issues/1):
взят и закрыт тикет #11 «Собрать спеку-хендофф по итогам решений». Собран датированный неизменяемый
снимок `docs/specs/2026-09-29-setpiece-handoff.md` — свод 15 решений карты в единый маршрут
реализации: сетевая классификация A/B/C и протокол v15 (`e_SetPiece` + фактический тэйкер); шов
презентации стандартов и роли `{None, Kicker, Keeper}`; камеры стандартов и прицел подач; общий
чистый `src/onthepitch/setpiece/` и модель удара curl/chip через баллистику `GetShotVector`; прицел
пенальти; единая PES-like раскладка ввода и раздача вратаря; управление бьющим (автопереключение,
designated, подавление Switch, меню смены Share/Tab); слой `keeper/` (Hands/Outfield/Returning,
правило 6 с) и оборона пенальти; стенка штрафного, разбег бьющего, ИИ-тайкер; порт `NetSim`;
полный список новых констант и 9 фаз реализации. Раздел «Not yet specified» карты очищен (все
вопросы до старта кодинга решены), перегенерация эталонов `tools/determinism` отнесена к
реализации (спека §16). Все 16 дочерних тикетов закрыты, карта #1 закрыта — дестинация достигнута.
Дальше — отдельное усилие по реализации.

## [2026-09-29] session | Распил спеки стандартов на тикеты реализации (#19–#40)

Скиллом `to-tickets` спека `docs/specs/2026-09-29-setpiece-handoff.md` разрезана на 22
tracer-bullet тикета в трекере GitHub: **#19–#40**, по девяти фазам §15, с нативными
GitHub blocking-зависимостями. 21 тикет помечен `ready-for-agent`; #40 (перегенерация
эталонов `tools/determinism` на всех платформах + правка вики) — `ready-for-human` и
заблокирован всеми gameplay-тикетами. Карта wayfinder #1 не трогалась.

Ключевая находка при нарезке: часть фич уже **пред-реализована прототипами** в отдельных
ветках/worktree, не влитых в рабочую линию (`prototype/curl-chip`, `prototype/penalty-aim`,
`prototype/setpiece-runup`, `task/setpiece-debug-keys`, `net-wobble`; `net-classification` и
`of_port` пусты). Прототипы стоят на предке `daea9bf` (до гейм-плана и designated-ролей),
расходятся со спекой по значениям констант и архитектуре (инлайн вместо чистого
`src/onthepitch/setpiece/`). Девять тикетов (#21, #23, #24, #25, #27, #34, #35, #37, #39)
переписаны на месте: добавлена секция «Готовый прототип → ветка» и скоуп переформулирован
как «портировать и согласовать со спекой»; blocking-рёбра не менялись.

Эталоны детерминизма не трогались — все gameplay-сдвиги сходятся в #40, чтобы
регенерировать их один раз. Frontier (без открытых блокеров): #19, #20, #21, #22, #34, #37.

## [2026-09-29] session | #21: e_ShotType и curl/chip-модификаторы удара

Тикет #21 (часть распила #19-#40) закрыт на ветке `of_port` (была пуста; ff-мержнута на
`gameplan-substitutions` d14e90e). Из прототипа `prototype/curl-chip` портирован только
вводный слой, согласованный со спекой §5 (без инлайн-архитектуры и без баллистики).

- `enum e_ShotType {Normal, Curl, Chip}` и поле `TouchInfo::shotType` в `src/gamedefines.hpp`;
  модификатор сэмплируется в `HumanController` в момент **нажатия** `Shot` (`_SampleShotType`)
  и доезжает в ударную команду, в т.ч. для одно-касания/queued удара.
- curl = `Dribble`(R2/C) + Shot, chip = `Switch`(L1/Q) + Shot; оба модификатора одновременно
  (или ни одного) → `Normal`. Новых `e_ButtonFunction` и правок `defaultKeyIDs` нет.
- При заряде варианта подавлены knock-on и super-cancel (`IsChargingShotVariant`); при заряде
  chip `Switch` не переключает игрока (`IsChargingChip` в гейте `Team::Process`).
- **Не переносилось**: баллистика curl/chip (Магнус, дуга, заряд 0.5 с, автодирекшн,
  settings-ползунки, debug F2) — это #23/#24.
- Эталоны `tools/determinism` не трогались: ввод хеш не двигает, `shotType` пока никем не
  читается. Сборка Release x86 зелёная. Вики: секция «Варианты удара (curl/chip)» в `docs/wiki/матч.md`.

## [2026-09-29] session | #22: переезд биндов на Special и двойной тап Sprint

Тикет #22 (часть распила #19–#40, фаза 1) закрыт на ветке `of_port`. В
`HumanController` четыре механики переехали с комбо `Dribble`+`Sprint`:

- **attacking-run** (`Process`, с мячом) и **стоп-на-месте** (`_GetHidInput`, designated =
  управляемый) — на `Special` (`L2` / `Z`).
- **super-cancel** (`RequestCommand`, `!hasBestPossession`) — на `Special`.
- **knock-on** — на **двойной тап `Sprint`** (`R1` / `E`): новая инфраструктура двойного тапа в
  `HumanController` (`lastSprintTapTime_ms` / `knockOnArmed` / `knockOnReleaseGraceUntil_ms`) и
  константа `sprintDoubleTapWindow_ms = 300` в `src/gamedefines.hpp`. Второй тап в окне
  вооружает `e_PlayerCommandModifier_KnockOn`; флаг держится, пока `Sprint` удержан, и ещё окно
  после второго тапа, если кнопку отпустили раньше. Одиночный тап — обычный спринт.
- Комбо `Dribble`+`Sprint` удалено полностью; `Dribble` остаётся ведением и модификатором curl,
  `Switch` — переключением игрока и модификатором chip.
- Согласованность с #21: `IsChargingShotVariant` теперь гейтит новые триггеры — knock-on
  (двойной тап) и super-cancel (`Special`); подавление edge-переключения при заряде chip в
  `Team::Process` не тронуто.
- **Не переносилось**: баллистика/curl/chip (#23/#24), раскладки/sideselect (#20),
  протокол/снапшот (#19).

Проверка: Release x86 собран зелёным; `determinism_runner check
7134def2c0863d4978bb18742b1f173358e4bf66` — без изменений (харнесс `HumanController` не
запускает). Вики: «Ручной режим и бинды» в `docs/wiki/матч.md`, `sprintDoubleTapWindow_ms` в
`docs/wiki/константы.md`, полевой хвост — в `docs/wiki/открытые-вопросы.md`. Вручную в открытой
игре бинды ещё не проверялись.

## [2026-09-29] fix | #22: knock-on стал одноразовым

Полевой тест вскрыл дефект семантики из записи выше: `knockOnArmed` держался, пока удержан
`Sprint`, поэтому после двойного тапа **каждое** следующее касание мяча снова получало
`e_PlayerCommandModifier_KnockOn` — игрок продолжал пробрасывать мяч вместо обычного спринта.
Исправлено: knock-on теперь одноразовый. Модификатор сходит с первым touch-командованием
(`_BallControlCommand`/`_TrapCommand` вооружены — флаг снимается), а заряд ограничен окном
`sprintDoubleTapWindow_ms` и не продлевается удержанием кнопки. `knockOnReleaseGraceUntil_ms`
заменён на `knockOnExpireTime_ms`. Обновлены `docs/wiki/матч.md` и `docs/wiki/константы.md`.

## [2026-09-29] fix | #22: knock-on доживает до касания

Второй полевой отзыв: при беге knock-on почти не срабатывал, приходилось спамить `Sprint`.
Причина — предыдущая правка сбрасывала заряд на первом же touch-**командовании** и держала
лимит в 300 мс; при беге командование уходит раньше, чем анимация доходит до кадра касания,
так что модификатор сгорал без проброса. Исправлено: заряд больше не сбрасывается по факту
командования — он снимается по **фактическому касанию** мяча этим игроком (Process сравнивает
`Player::GetLastTouchTime_ms()` с зафиксированным до касания значением), с общим потолком в
новой константе `sprintKnockOnWindow_ms = 800` мс. Одиночный тап по-прежнему не вооружает;
удержанный после проброса `Sprint` — обычный спринт. Смотреть по-прежнему глазами.

## [2026-09-29] fix | #22: knock-on на фиксированном окне

Третий полевой отзыв: проброс всё равно срабатывал не всегда. Причина — disarm по факту
касания через `GetLastTouchTime_ms` сгорал на касаниях, которые mod не получали (движение с
controlled collision, отклонённый requeue), а окно детекта 300 мс узкое для реального
дабл-тапа. Упрощено: knock-on больше не «потребляется» касанием — двойной тап включает
модификатор на фиксированное окно `sprintKnockOnWindow_ms = 600` мс, в течение которого
`_BallControlCommand`/`_TrapCommand` несут `KnockOn` и он срабатывает на кадре касания; окно
длиннее одной ballcontrol-анимации (24 кадра ≈ 240 мс, touch на кадре 12), поэтому до touch
успевает дойти. `sprintDoubleTapWindow_ms` расширен до 400 мс. Убраны `knockOnBaselineSet` /
`knockOnTouchBaseline_ms`. Удержание `Sprint` заряд не продлевает. Остаточный риск, за которым
следят глазами: два проброса, если в окно попадают два ballcontrol-касания.

## [2026-09-29] session | #20: одна PES-like раскладка геймпада

Тикет #20 (часть распила #19–#40, фаза 1) закрыт на ветке `of_port`. Удалены PES/FIFA-пресеты
геймпада — раскладка одна, PES-like.

- `src/gamedefines.hpp`: убраны `enum e_ControllerLayout` и `defaultControllerLayout`;
  `defaultControllerSimpleMode` оставлен (это отдельная фича — геймпады без стиков).
- `src/hid/gamepad.{hpp,cpp}`: FIFA-пресет удалён, остался один `GetDefaultFunctionMapping`;
  убраны `GetLayout`/`SetLayout` и чтение/запись конфиг-ключа `input_gamepad_<id>_layout`
  (ключ мёртв); флаг `..._simple` по-прежнему читается.
- `src/menu/sideselect.{hpp,cpp}`: убраны поле `layout`, `ToggleLayout`, подпись «LAYOUT: …»
  и циклический переключатель раскладки LB/RB.
- `src/menu/settings.cpp`: подписи функций ввода приведены к единому виду на обеих страницах —
  `switch`, `special`, `sprint`, `dribble` (на клавиатуре было `switch player` / `slow dribble`,
  на геймпаде — `switch` / `slow dribble`).

Детерминизм не затронут (ввод/UI, симуляция не менялась). Сборка Release x86 зелёная. Вики:
[[архитектура]], [[глоссарий]], [[константы]].

## [2026-09-29] session | #19: снапшот v15 — тип стандарта и фактический бьющий

Тикет #19 (распил #19–#40, фаза 1) закрыт на ветке `of_port` — единственное расширение
снапшота из эффорта стандартов.

- `Match`: `bool inSetPiece` заменён на `e_SetPiece setPieceType`; `IsInSetPiece()` выводится
  (`type != None`), `StartSetPiece(type)` / `StopSetPiece()`, `GetSetPieceType()`.
  `ProcessState` сериализует тип — хеш `tools/determinism` поплыл (`7134def2…` → `3b9fa5f2…`),
  эталоны перегенерируются один раз в #40.
- `Referee::Process` передаёт в `StartSetPiece` `buffer.desiredSetPiece`.
- `Snapshot` (v15): вместо `bool inSetPiece` — `setPieceType` + `setPieceTakerTeam`/`Slot`
  (team+slot, `-1` = не назначен). `CaptureSnapshot` берёт тип из `Match`, тэйкера — из
  `RefereeBuffer.taker` через `Team::GetPlayerSlot`. `inSetPiece` на проводе нет; клиент
  выводит его из типа в `ApplyRemoteSnapshot`.
- План-роли (`Snapshot.roles`, v12) не тронуты и тэйкером не являются; потребитель
  идентичности — презентация #26.
- `net_protocolVersion` 14 → 15 (политика прежняя: несовпадение версии отклоняется в handshake);
  `nettypes.hpp`, `lanmatchtest` (+7 проверок round-trip).

Вики: [[сеть]] (v15, идентичность стандарта), [[матч]] (`setPieceType`), [[константы]].
Сборка Release x86 зелёная; `lanmatchtest` PASS (42), `nettest` PASS (59).

## [2026-09-29] session | #23: баллистика удара и curl/chip (общий расчёт)

Тикет #23 (распил #19–#40, фаза 2) закрыт на ветке `of_port`: расчёт удара вынесен в чистый
модуль и приведён к спеке §2/§7.

- Новый `src/onthepitch/setpiece/setpiecelogic.{hpp,cpp}` — чистые функции без `Match`/`Player`
  (headless-пригодны): `IsGroundShot`, `CalculateAimHeight`, `CalculateAimLateral`,
  `CalculateShotVelocity` и планировщик `PlanShot`. Отдаёт направление/мощность/высоту/тип.
- `Humanoid::GetShotVector`: вместо фиксированной высоты `0.05` — баллистический пуск в точку
  на линии ворот (латераль клампится у створа), высота растёт с зарядом
  (`_default_Shot_AimYMin` → `goalHeight + _default_Shot_OverLift`), вертикаль из
  `vz = Δz/t + ½g·t` с потолком `_default_Shot_MaxLift`; короткий тап — низом не ниже
  `_default_Shot_GroundPower`. Curl — доворот `_default_Shot_Curl_AimOut` + вращение
  `_default_Shot_Curl_ZRot` (низом вертикаль 0, вращение остаётся); chip — дуга
  `_default_Shot_Chip_Loft` × `_default_Shot_Chip_HorizFactor`.
- `TouchInfo` получил `aimHeight`/`useAimHeight`; `HumanController` и `ElizaController`
  (пенальти) зовут `PlanShot` — один путь для человека и ИИ. `_default_Shot_AutoDirection`
  0.2 → 0.74. Заряд удара — `KICK_CHARGE_MAX_TIME` 0.5 с (пасы 1 с), автовыстрел на полном.
- Константы добавлены в `src/gamedefines.hpp`; `setpiecelogic.cpp` подключён в `gamelib`.

Детерминизм: `tools/determinism` за 5000 шагов не совершает ни одного удара (`GetShotVector`
не вызывается), поэтому хеш не сдвинулся (`3b9fa5f2…`, как после #19) — эталоны не трогали,
перегенерация за #40. Ручная проверка удара (низом/верхом, короткий тап, curl/chip) остаётся.

Вики: [[матч]] (расчёт траектории), [[константы]] (секция удара, +дефолт ассиста),
[[открытые-вопросы]] (хвост не покрывает удар). Сборка Release x86 зелёная.

## [2026-09-29] decision | Модель chip: фиксированный угол + заряд (по мотивам FIFA/PES)

Изначальный chip из прототипа (фикс `Chip_Loft` 8 м/с + горизонталь ×0.55) при близкой
дистанции всегда перелетал; правка на «вертикаль × заряд» тоже не подошла (на слабом заряде
почти не подбрасывал). По исследованию (EA FC 26 chip guide, futfc.gg, outsidergaming, PES
Mastery) в FIFA/PES chip — это модификатор (L1/LB + удар) для 1-в-1 против выбежавшего
вратаря, где **всё решает заряд**: короткий чип — лёгкий тап, дальний — 2–3 деления, полный
заряд перебивает выше ворот; PES: сила определяет высоту.

Принято: **фиксированный угол вылета** `_default_Shot_Chip_Angle` 30°, а заряд масштабирует
**общую** скорость (`_default_Shot_Chip_SpeedFactor` 0.45 от силы удара). Дуга есть всегда
(даже тап подбрасывает), дальность/высота растут с зарядом, перезаряд = перелёт. Константы
`Chip_Loft`/`Chip_HorizFactor` заменены на `Chip_Angle`/`Chip_SpeedFactor` (отступление от
§14 спеки осознанное, по решению владельца). Chip больше не считается низовым ударом.
Числа угла/фактора — на плейтест. Вики: [[матч]], [[константы]].

## [2026-09-29] feat | Дебаг-хоткей F2: убрать полевых соперников за поле

Для ручного тестирования удара добавлен `F2` (в духе §11 спеки): `Match` опрашивает клавишу в
`Process`, тумблер `debugRemoveOpponents` зовёт `Match::DebugParkOpponents`. Паркует **полевых
игроков команды без живого человека** за боковой линией (x = 60), вратарь остаётся в воротах;
`F2` обратно — возвращает по формации. Пока тумблер включён, позиция переставляется каждый тик,
чтобы ИИ не увёл игроков обратно. Команду с человеком и вратаря не трогает. Вики: [[матч]]
(раздел «Отладочные хоткеи»). Сборка Release x86 зелёная, детерминизм не затронут (клавиша в
headless не нажимается).

## [2026-09-29] decision | Обычный удар бьёт выше/перелетает, curl режет силу

По исследованию FIFA/PES (PES Mastery, u7buy/esportsguides/mmoexp FC 26 finesse, Outsidergaming,
fifplay) обычный удар там — «сила определяет высоту», и перезаряд **перебивает выше ворот**;
finesse/controlled — «обмен силы на точность и закрутку». У нас же `_default_Shot_MaxLift` 6
давал потолок апекса ~1.84 м (ниже перекладины 2.5 м), т.е. верхний угол/перелёт были
недостижимы, а curl не торговал силой.

Принято (отступление от §14 спеки по решению владельца):
- `_default_Shot_OverLift` 3.5 → **1.0** (полный заряд целится в 3.5 м — чуть выше перекладины,
  а не в 6 м), `_default_Shot_MaxLift` 6 → **12** — теперь заряд даёт верхний угол и перелёт,
  слабый/средний заряд ложится в створ.
- Новый `_default_Shot_Curl_SpeedFactor` **0.85**: curl/finesse горизонтально тише обычного удара.
Числа — на плейтест. Вики: [[матч]], [[константы]]. Сборка Release x86 зелёная, детерминизм
`3b9fa5f2…` не двигался (фикстура ударов не делает).

## [2026-09-29] session | #24: curl как вход TouchInfo + ползунки

Тикет #24 (фаза 2, распил стандартов) закрыт на ветке `of_port`. Механики curl/chip в основном
уже вошли в #23; здесь доделан шов «curl — вход, а не вывод».

- `TouchInfo` получил поле `curl` (знак out-направления, 0 = нет). `PlanShot` теперь принимает
  `(from, side, desiredDirection, charge, shotType)` и выводит знак закрутки из **прицела**
  (`CalculateAimLateral`), а не из случайного `bodyTouchAngle`; `ApplyShotPlan` кладёт `curl`.
- `GetShotVector`: ветка curl берёт знак из `touchInfo.curl`, спин/доворот — из конфига
  (`gameplay_shot_curlspin`/`gameplay_shot_curlaimout`). Оба контроллера зовут `PlanShot` с
  позицией мяча и стороной.
- Добавлены `_default_Shot_Curl_ZRotMax` 300 и `_default_Shot_Curl_AimOutMax` 0.3, ползунки
  «shot curl - spin / aim offset» на странице геймплея с числовым показом (порт
  `Gui2Slider::SetValueDisplay`), запись в конфиг в `Exit`.
- Chip считать закрытым (реализован ранее, модель фикс. угол + заряд).
- Случайный `zRot` в ударном пути **не трогали** — решим отдельно.

Вики: [[матч]] (Curl), [[константы]] (+2 константы ползунков). Сборка Release x86 зелёная,
`nettest`/`lanmatchtest` PASS, детерминизм `3b9fa5f2…` без изменений.

## [2026-09-29] decision | Случайный zRot в ударном пути оставляем

Пункт AC тикета #24 «случайный `zRot` больше не используется в ударном пути» **сознательно не
выполняем**: у обычного удара остаётся `zRot = amount * -420 + random(-20,20) * 0.7`
(детерминированная кривая от корпуса + небольшая случайная подкрутка). Причина — живая
вариативность обычных ударов; полностью плановый спин (обнуление либо перенос в `TouchInfo`)
сделает их стерильнее, а делать это логичнее в #28, где закрутку задаёт ввод. #24 закрыт с этим
сознательным исключением.

## [2026-09-30] feat | #25: 2D-прицел пенальти и ветка useAimTarget

Тикет #25 (фаза 3 распила стандартов, спека `docs/specs/2026-09-29-setpiece-handoff.md` §7)
закрыт на ветке `of_port`. Портирован прототип `prototype/penalty-aim` (`5704e48`) и согласован
с §7/§14; камера и дебаг-клавиша из прототипа в скоуп не входили (тикеты #27 и #39).

- `TouchInfo` получил `useAimTarget`/`aimLateral`/`aimSpeed` (`aimHeight`/`useAimHeight`
  переиспользованы под высоту точки); 8 констант `_default_Pen_*` в `src/gamedefines.hpp`.
- Чистые функции в `setpiecelogic`: `DefaultPenaltyAim`, `UpdatePenaltyAim` (стик, клампы,
  возврат к центру), `PlanPenaltyShot` (выборка из диска разброса + скорость по заряду).
- `HumanController`: состояние ретикла, `_UpdatePenaltyAim` (заморозка на нажатии `Shot`, сброс
  маркера после стандарта), ветка пенальти в ударном пути вместо `AI_GetShotDirection`.
- `GetShotVector`: при `useAimTarget` берёт латераль/высоту/скорость из `TouchInfo`, считает
  вертикаль без потолка `_default_Shot_MaxLift`, обнуляет worst-case и пропускает боковую кривую;
  жёлтый дебаг-маркер ставится на линии ворот.

Вики: [[матч]] (раздел «Прицел пенальти»), [[константы]] (+8 строк, дата сверки), [[открытые-вопросы]].
Сборка Release x86 зелёная, `nettest` PASS (59), `lanmatchtest` PASS (42), детерминизм
`3b9fa5f2…` без изменений (прицел — презентация, хеш-фикстура ударов не делает). Полевой тест
пенальти — в открытых вопросах (нужен заслуженный пенальти до #39).

## [2026-09-30] feat | Дебаг-активация стандартов P/F/Y (+Alt)

Поверх #25 добавлена дебаг-активация стандартов для ручной проверки (порт решений #12/#39,
спека §11): клавиши `P`/`F`/`Y` форсируют пенальти/штрафной/угловой в атакуемые ворота для
команды человека, `Alt`-варианты — в свои (стандарт отыгрывает соперник-ИИ, человек защищает).

- `Referee::DebugForceSetPiece(e_SetPiece, teamID, restartPos)` выставляет буфер судьи и
  запускает штатный `PrepareSetPiece` + `StartPlay`/`StartSetPiece`; бьющего выбирает и передаёт
  человеку обычное автопереключение (`Team::UpdateSwitch`), форсированного бьющего нет.
- `Match::Process` опрашивает `SDLK_P/F/Y`, `Alt` — через `SDL_GetModState()`; только из чистого
  игрового состояния (`IsInPlay() && !IsInSetPiece()`).
- Лана штрафного и расчистка коридора в скоуп не вошли (тикет #34). `F2` уже был.

Вики: [[матч]] («Отладочные хоткеи»), [[открытые-вопросы]]. Сборка Release x86 зелёная,
`nettest` PASS (59), `lanmatchtest` PASS (42), детерминизм `3b9fa5f2…` без изменений (клавиши в
headless не нажимаются). Полевой тест — в открытых вопросах.

## [2026-09-30] fix | Жёлтый маркер прицела не сбрасывался после пенальти

Полевой баг #25: после пенальти жёлтый маркер прицела иногда оставался висеть на воротах.
Причина — `HumanController` существует по одному на игрока-человека и переиспользуется при смене
управляемого игрока (`HumanGamer::SetSelectedPlayerID` → `SetExternalController`). При смене
зовётся `HumanController::Reset()`, который сбрасывал `penaltyAimActive` **без** скрытия маркера;
а `Match::Process` вызывает `Team::UpdateSwitch` (смену управления) до `Team::Process`, поэтому в
тик окончания стандарта маркер оставался (`_UpdatePenaltyAim` уже видел `penaltyAimActive == false`
и не прятал).

Фикс: `Reset()` прячет маркер (`SetYellowDebugPilon(0,0,-100)`), если ретикл был активен; новые
поля ретикла получили инициализаторы на месте объявления, чтобы `Reset()` из конструктора не читал
неинициализированный флаг. Сборка Release x86 зелёная, `nettest`/`lanmatchtest` PASS, детерминизм
`3b9fa5f2…` без изменений.

## [2026-09-30] session | Полевая проверка прицела пенальти (#25)

Ручная проверка в игре (форс пенальти клавишей `P`): ретикл двигается стиком с инверсией
горизонтали, клампится у штамбы/перекладины, без ввода возвращается к центру; на нажатии `Shot`
маркер замирает, удар идёт в точку с разбросом по заряду; после розыгрыша маркер гаснет (включая
случай смены управляемого игрока — баг выше). Владелец: «вроде норм». Пункт открытых вопросов по
прицелу закрыт; тикет #25 закрывается. Не проверено полем: `F`/`Y` и `Alt`-варианты дебаг-активации
(остаются в открытых вопросах).

## [2026-09-30] feat | #26: презентация стандартов — носитель и роли

Тикет #26 (фаза 4 спеки стандартов, §2) закрыт на ветке `of_port`. Введён единый объект
презентации `SetPiecePresentation` (`src/onthepitch/setpiece/setpiecepresentation.{hpp,cpp}`),
владеемый `Match` (`GetSetPiecePresentation()`): своего автомата и жизненного цикла нет —
фаза выводится из `RefereeBuffer.active` + `IsInSetPiece()` + таймеров, конец по-прежнему
`buffer.taker->TouchAnim()` в `Referee::Process`.

- Идентичность: на хосте `SetPiecePresentation::Process()` (зовётся из `Match::Process` после
  `referee->Process()`) берёт тип/тэйкера из `RefereeBuffer`; на тонком клиенте
  `SetRemoteIdentity()` из `ApplyRemoteSnapshot` разрешает тэйкера из снапшота v15 (team+slot).
  Внутри — `switch (e_SetPiece)` только для набора ролей, класса-на-стандарт нет.
- Роли `e_SetPieceRole { None, Kicker, Keeper }`: `Kicker` — локально управляемый игрок ==
  тэйкер; `Keeper` — только на пенальти, вратарь защищающейся команды. Роль считается на
  каждого локального игрока; локальность — `GetControllingPeerId` (хост) /
  `GetRemoteOwnerId` (клиент). `Wall` не заводится.
- `GetHudState(Player*)` отдаёт `{role, type, aimPoint, chargeRatio}`; отрисовка отдельная и
  идёт только при `role != None`. Состояние ретикла пенальти переехало из `HumanController` в
  носитель (плюс отрисовка жёлтого маркера); `HumanController` только скармливает сырой стик
  (`UpdatePenaltyAim`) и читает прицел при ударе; `GetChargeRatio()` вынесен для HUD.

Презентация — хеш не меняет: `tools/determinism` `3b9fa5f2` без изменений; `nettest` PASS (59),
`lanmatchtest` PASS (42). Обновлены [[матч]], [[сеть]], [[глоссарий]].

## [2026-09-30] session | полевая проверка презентации стандартов (#26)

Владелец прогнал базовый сценарий #26 в собранной игре: пенальти за человека (`P`) — жёлтый
маркер ретикла виден, стик двигает, фиксация на `Shot`; вердикт «вроде норм». Не проверено
(осталось в [[открытые-вопросы]]): роли на всех типах стандартов и на двух локальных игроках
(`Kicker`/`Keeper`, смена управления), отсутствие застревания/пропадания маркера у чужого игрока.

## [2026-09-30] feat | камеры стандартов (#27)

Тикет #27 (фаза 4, §4). Аппликатор Match::SetSetPieceCamera(eye, lookAt, fov, nearCap, farCap)
повторяет паттерн фол-камеры: гасит авто-камеру и раскладывает look-at в cameraNodeOrientation
(yaw) / cameraOrientation (pitch) / cameraNodePosition; FollowCamera не трогается.

- Камеру получают только пенальти, штрафной, угловой и от ворот
  (SetPiecePresentation::HasSetPieceCamera); аут и кикофф — обычная камера. Позы — новые
  _default_SetPiece_Cam* в gamedefines.hpp (пенальти 9/4/1.2, штрафной 8/3.5/1.4, угловой
  8/4/2.0, от ворот 8/3.5/1.4, AHEAD 4, FOV 45, near/far 1/220, hold 1.5 с); spot —
  buffer.restartPos (z=0), заморожен на активацию.
- Активация в prepareTime (SetPiecePresentation::UpdateCamera зовётся из Match::Process
  после фол-камеры, поэтому film-referee отдаёт управление в этой точке); IsGoalScored() снимает
  камеру — scorer-cam приоритетен.
- Релиз: HumanController в момент коммита действия (отпускание кнопки/автовыстрел) зовёт
  NotifyKickerCommitted(isShot). Пенальти держит 1.5 с после отпускания; штрафной-удар — 1.5 с,
  пас/навес — сразу; угловой и от ворот — сразу. Пока держим, поза заморожена.
- Keeper на пенальти получает ту же камеру (отдельного ракурса нет). На тонком клиенте камера
  строится локально в ApplyRemoteSnapshot (тип из снапшота, spot — поза мяча).
- Направление подачи пока базовое (от спота в центр ворот): поворот с прицелом — #28.

## [2026-09-30] session | камеры стандартов (#27)

Реализован #27 на ветке `of_port`. Собрано Release x86, `nettest` PASS (59),
`lanmatchtest` PASS (42), детерминизм `3b9fa5f2...` без изменений (камера — презентация,
`ProcessState` её не сериализует). Полевой проверки нет: осталось в [[открытые-вопросы]] —
ощущение поз/клип-плоскостей и заморозки, своевременность релиза, `Keeper`-роль на пенальти,
регрессия на ауте/кикоффе. См. [[матч]], [[константы]].

## [2026-09-30] fix | пенальти за защищающегося: камера автоматом (#27)

Уточнение по #27: на пенальти в сторону защищающегося человека (`Alt+P`) камера стандарта должна
вставать автоматически за бьющим, без ручного выбора вратаря. Раньше `LocalRoleOwnsCamera` давала
камеру защите только когда локальный человек управляет вратарём (`Keeper`-роль, §2.2), поэтому
`Alt+P` показывал обычную камеру.

- Защита владеет камерой, если на защищающейся команде есть любой локальный человек;
  `Keeper`-роль/HUD не трогали — вратарское управление на пенальти остаётся отдельной задачей.
  Поза прежняя — за бьющим (`camSide`/`camSpot` от бьющего), релиз: 1.5 с после касания мяча.
- Правка presentation-only: хеш `3b9fa5f2` без изменений, `nettest` PASS (59),
  `lanmatchtest` PASS (42).

## [2026-09-30] fix | камера стандарта мигала после розыгрыша (#27)

Полевой отчёт: после удара от ворот камеру «эпилептит», у штрафного/углового/пенальти — слабее.
Причина: коммит действия (`HumanController` → `NotifyKickerCommitted`) срабатывает каждый тик, пока
кнопка отпущена, а `actionMode` ещё не сброшен анимацией. Камера успевала отдать управление
(релиз на отпускании), на следующем тике снова активироваться (стандарт ещё жив — мяч не коснулись)
и снова релизнуться: полярность скакала каждый тик. Для пенальти/удара штрафного скачка не было —
там `camFrozen` ставился один раз и держал 1.5 с.

- Добавлена защёлка `camSuppressed`: отпущенная камера не активируется, пока стандарт не закончится
  (сбрасывается, когда тип перестаёт быть камерным). Убирает мигание на угловом/от ворот/пасе
  штрафного; на пенальти/ударе поведение прежнее.
- Презентация: хеш `3b9fa5f2` без изменений, `nettest` PASS (59), `lanmatchtest` PASS (42).

## [2026-09-30] fix | прицел пенальти: горизонталь зависит от стороны (#25)

Полевой отчёт: после смены сторон (2-й тайм) на пенальти стик вправо вёл маркер влево. Инверсия
`UpdatePenaltyAim` была зашита под одну ориентацию (`lateral -= stickLateral`), а камера пенальти
смотрит из спота в атакуемые ворота (`forward = -side`). Правый вектор экрана = `forward × up =
(0, side, 0)`, поэтому «вправо» — это `+y` при `side == 1` и `-y` при `side == -1`.

- `setpiecelogic::UpdatePenaltyAim` принимает `side` бьющего; `SetPiecePresentation` берёт его из
  `taker->GetTeam()->GetSide()`. В 1-м тайме (для стороны пользователя) знак совпал со старым
  поведением, во 2-м — исправлен.
- Хеш `3b9fa5f2` без изменений (в headless-фикстуре человеческого ввода в пенальти нет),
  `nettest` PASS (59), `lanmatchtest` PASS (42).

## [2026-09-30] fix | дебаг-клавиши стандартов: команда локального пира (#39)

Полевой отчёт в сетевом матче: `Alt+F` отдавал стандарт не той стороне, ИИ бил сам. `Match::Process`
определял «команду человека» как первую команду с любым человеко-управляемым игроком
(`IsHumanControlled`). В сети люди есть в обеих командах, поэтому всегда выбиралась команда 0 —
для хоста за команду 1 `F`/`Alt+F` выбирали чужую сторону.

- Заменено на `GetControllingPeerId(player) == GetLocalPeerId()`: берётся команда локального пира
  (на хосте — 0, у клиента свои игроки имеют ненулевой ownerId). Клавиши по-прежнему host-only
  (`Match::Process` на клиенте early-return).
- ИИ-удар по стандарту у чужой/своей команды лечится отдельно: управление на бьющего форсит
  designated — тикет #29 (не реализован).
- Дебаг-хук: хеш `3b9fa5f2` без изменений, `nettest` PASS (59), `lanmatchtest` PASS (42).

## [2026-09-30] fix | камера стандарта на клиенте строится по команде бьющего (#27)

Полевой отчёт: пенальти-камера на клиенте смотрела не из-за спины бьющего, как на хосте, а
оставалась обычной (за мячом). Причина: `LocalRoleOwnsCamera` проверяла именно игрока-бьющего
через `IsLocalPlayer` -> `GetRemoteOwnerId()`, а в снапшоте owner id выставлен только у **выбранного**
игрока. Если бьющий ещё не выбран управляемым (передача управления — #29), его owner id = -1, клиент
не признаёт себя владельцем роли и копирует хост-камеру. На хосте бьющий обычно уже выбран, поэтому
там камера работала.

- Владение камерой теперь считается по **команде** бьющего (`TeamHasLocalHuman`): любой локальный
  человек на бьющей команде — владелец. Защита на пенальти — так же по команде.
- Хеш `3b9fa5f2` без изменений, `nettest` PASS (59), `lanmatchtest` PASS (42).

## [2026-09-30] fix | клиентская камера стандарта: спот перечитывается до коммита (#27)

Полевой отчёт: на клиенте после форса пенальти (P на хосте) камера замирала у мяча из момента
нажатия, а не вставала за бьющим. `CameraSpot()` на клиенте берёт позицию мяча из снапшота, а
снапшот клиенту приходит **интерполированным** (`BlendSnapshots`): в тик, когда тип стандарта уже
`Penalty` (из нового снапшота), позиция мяча ещё доезжает от старой к точке пенальти. Спот
захватывался в этот тик и замораживался -> камера у старого мяча.

- Пока удар не закоммичен, `camSpot`/`camForward` перечитываются каждый тик (на хосте
  `restartPos` постоянен, поведение то же). После коммита/в fallback — заморозка как было.
- До этого тем же изменением (#27) владение камерой стало считаться по команде бьющего
  (`TeamHasLocalHuman`), иначе до #29 клиент вообще не строил камеру.
- Хеш `3b9fa5f2` без изменений, `nettest` PASS (59), `lanmatchtest` PASS (42).

## [2026-09-30] fix | не-владелец не наследует set-piece-камеру (#27)

Полевой отчёт: на штрафном/угловом/от ворот, которые бьёт хост, клиент из противоположной команды
получал set-piece-камеру хоста вместо обычной. По спеке §4 не-владельцы получают host-камеру, а
камера хоста в этот момент — set-piece-камера, отсюда «у обеих команд 3-е лицо» на F.

- `ApplyRemoteSnapshot`: если идёт камерный стандарт и локальная set-piece-камера не активна
  (значит, пир не владелец), клиент считает **свою обычную** камеру (`UpdateIngameCamera`), а не
  копирует хост-камеру. Владелец по-прежнему строит set-piece-камеру локально; на пенальти
  защищающийся — владелец, поэтому у него камера за бьющим. Осознанное отклонение от §4.
- Хеш `3b9fa5f2` без изменений, `nettest` PASS (59), `lanmatchtest` PASS (42).

## [2026-09-30] fix | клиент-не-владелец держит обычную камеру и во время удержания после удара (#27)

Полевой отчёт: до удара хоста клиент-не-владелец уже показывал обычную камеру (предыдущий фикс), но
после удара ещё какое-то время смотрел мячу вслед, как хост. Причина: после коммита хост держит
set-piece-камеру `_default_SetPiece_CamHold_ms` (1.5 с, смотрит полёт), при этом тип стандарта в
снапшоте уже `None` (мяч коснулись) — клиент переставал считать свою камеру и копировал хост-камеру
(set-piece, замороженную).

- `SetPiecePresentation::UseLocalNormalCamera()`: на тонком клиенте true, пока идёт камерный
  стандарт, и ещё `_default_SetPiece_CamHold_ms` после ухода типа в `None`. `ApplyRemoteSnapshot`
  по этому флагу считает свою обычную камеру. Владелец по-прежнему строит set-piece-камеру
  (у него авто-камера выключена, блок не выполняется).
- Хеш `3b9fa5f2` без изменений, `nettest` PASS (59), `lanmatchtest` PASS (42).

## [2026-09-30] fix | set-piece-камера стала display-only; в снапшот всегда идёт обычная (#27)

Полевой отчёт: у клиента-не-владельца на штрафном через ~1.5 с после удара был скачок «обычная
камера на другую обычную». Причина: во время стандарта клиент считал **свою** обычную камеру, а
после удержания хоста возвращался на **хост-камеру** — две разные обычные камеры (разный designated
и история сглаживания) давали скачок. Предыдущий грейс `UseLocalNormalCamera` лишь откладывал момент.

- `Match::SetSetPieceCamera` больше **не выключает авто-камеру**: поза складывается в отдельные
  поля `setPieceCamera*` + флаг `setPieceCameraActive`, а `PreparePutBuffers` подставляет их в
  `buf_camera*` для локального показа. Живая камера продолжает считаться (`UpdateIngameCamera`) и
  уезжает в снапшот — не-владелец всегда получает обычную хост-камеру, без скачка на релизе.
- Убраны клиентские спец-ветки (`UseLocalNormalCamera`, `IsCameraTypeSetPiece`): клиент снова
  просто копирует хост-камеру, локальный override включается только у владельца.
- Владелец по-прежнему видит set-piece-камеру (через display-поля); вход/выход сглаживает
  `TemporalSmoother` в `PreparePutBuffers`.
- Хеш `3b9fa5f2` без изменений, `nettest` PASS (59), `lanmatchtest` PASS (42).

## [2026-09-30] fix | маркер прицела пенальти на тонком клиенте (#25/#27)

Полевой отчёт: на клиенте нет жёлтого маркера прицела на пенальти. Причина: маркер рисует
`SetPiecePresentation::UpdatePenaltyAim`, а её зовёт `HumanController::_UpdatePenaltyAim`; на
тонком клиенте `Match::Process` early-return, `Team::Process`/`HumanController` не тикают, поэтому
ретикл вообще не обновлялся.

- `NetMatchSession::ProcessClient` отдаёт презентации локальное HID-устройство
  (`SetPiecePresentation::SetLocalHIDDevice`); `UpdateRemotePenaltyAim` (зовётся из `UpdateCamera`,
  т.е. каждый тик из `ApplyRemoteSnapshot`) читает стик и эдж `Shot` (локальный флаг
  `remoteShotHeld`) и двигает ретикл. `chargeRatio` на клиенте не важен (полоса силы не рисуется).
- `DrawPenaltyReticle` теперь по команде бьющего (`TeamHasLocalHuman`), как и камера: на клиенте
  конкретный тэйкер может читаться как ИИ до автопереключения (#29).
- Хеш `3b9fa5f2` без изменений, `nettest` PASS (59), `lanmatchtest` PASS (42).

## [2026-09-30] fix | удар пенальти бьющего-клиента летел в центр (#25)

Полевой отчёт: на клиенте наводишь точку и бьёшь в угол, а мяч идёт по центру. Причина:
`HumanController::_IsPenaltyTaker` брал роль через `GetRole`/`IsLocalPlayer`, а на хосте бьющий —
удалённый игрок клиента (owner != локал), поэтому `_IsPenaltyTaker` = false. Значит и прицел не
обновлялся, и удар считался веткой авто-аима `AI_GetShotDirection` (в центр).

- `_IsPenaltyTaker` теперь сравнивает тэйкера напрямую: `GetTaker() == CastPlayer()`. Для
  `HumanController` этого достаточно — он существует только у человеческого игрока (на хосте у
  удалённого бьющего это контроллер на `NetHIDDevice`). Прицел снова читается и хост двигает
  `penaltyAim` по стику клиента.
- Хеш `3b9fa5f2` без изменений, `nettest` PASS (59), `lanmatchtest` PASS (42).

## [2026-09-30] feat | хост форсит designated = бьющий на стандарте (#29)

Полевой отчёт: на клиенте наводишь точку пенальти и бьёшь в угол, а мяч идёт в центр. Гейт
`_IsPenaltyTaker` к этому отношения не имел (исправлен ранее, но «не помогло»): на хосте бьющий
команды клиента не был выбран управляемым, поэтому его контроллер — ИИ (`ElizaController`), а тот
бьёт авто-аимом `AI_GetShotDirection` в центр. Корень — #29 (designated = бьющий).

- `Match::Process`: при `IsInSetPiece()` и известном `RefereeBuffer.taker` ставим
  `designatedPossessionPlayer = taker` и `designatedTeamPossessionPlayer` бьющей команды = taker,
  а блок подсчёта designated по времени до мяча пропускаем (спек §3). `Team::UpdateSwitch` далее
  выбирает бьющего, и он получает человеческое управление (в т.ч. удалённый клиент) — прицел
  наконец применяется, ИИ больше не бьёт за человека.
- Остальное по #29 не трогали: вратарь-бьющий (от ворот) по-прежнему не выбирается (#31),
  меню смены бьющего (#30), клиентский designated из снапшота.
- **Gameplay-правка: хеш `tools/determinism` сдвинулся** `3b9fa5f2…` → `5a477078…` (`nettest` 59,
  `lanmatchtest` 42 зелёные). Эталоны — за #40.

## [2026-09-30] fix | прицел бьющего-клиента применяется на хосте (#25)

Причина «мяч в центр»: `SetPiecePresentation::Process()` на хосте сбрасывал `penaltyAim` каждый
тик — условие `GetRole(taker) != Kicker` ложно для удалённого бьющего (GetRole локальный). Плюс
ранее добавленные `_IsPenaltyTaker` (сравнение с `GetTaker()`) и host-форс designated = бьющий
(#29) — без них бьющий был под ИИ и не получал управление.

- Сброс прицела теперь только при `!IsPenalty()`. Отрисовка маркера гейтится отдельно
  (`DrawPenaltyReticle` — по команде). Временные логи `PenaltyDebug` убраны после полевой проверки
  («вроде норм»).
- Хеш `5a477078` (сдвинут host-форсом designated, #29), `nettest` 59, `lanmatchtest` 42 зелёные;
  эталоны — за #40.

## [2026-09-30] feat | #29: клиентский designated из снапшота + подавление Switch на стандарте

Доделан остаток #29 после host-форса designated:

- `Match::ApplyRemoteSnapshot`: при `e_SetPiece != None` со снапшотным тэйкером (team+slot v15)
  `designatedPossessionPlayer` и designated своей команды ставятся в этого тэйкера, а не по
  близости (`match.cpp`). HUD и камера клиента теперь смотрят на фактического бьющего.
- `Team::Process`: ручное переключение по `Switch` загейчено `!IsInSetPiece()` — на стандарте
  кнопка занята chip-комбо и не ворует бьющего.
- Вратаря-бьющего (от ворот) не трогали — гейт `designatedTeamPossessionPlayer != GetGoalie()`
  остаётся, это «игра за вратаря» (#31). Закрываем #29 с этой оговоркой.
- Хеш `5a477078` без изменений (правки клиентские/человеческие, headless-фикстура их не трогает);
  `nettest` 59, `lanmatchtest` 42. Эталоны — за #40.

## [2026-09-30] fix | дебаг-форс стандарта с обычным окном подготовки (#39)

Полевой отчёт: при форсе штрафного/пенальти игроки «по инерции продолжали бежать». Симуляция
скорость обнуляет (`ResetPosition`), но `DebugForceSetPiece` стартовал мгновенно
(`prepareTime = startTime = now`), поэтому расстановка, возврат ИИ к позициям и клиентская
интерполяция накладывались — выглядело как скольжение.

- `Referee::DebugForceSetPiece` теперь как настоящий стандарт: `StopPlay()`, `prepareTime =
  stopTime + 2000`, `startTime = prepareTime + 2000`; расстановку и свисток делает штатный
  `Referee::Process`. Игроки успевают встать до живого мяча, поведение 1:1 с матчем (вратарь-бьющий
  и прочее — как в игре).
- Хеш `5a477078` без изменений (дебаг-хук, headless его не зовёт), `nettest` 59, `lanmatchtest` 42.

## [2026-10-01] session | прицеливание подач со стандартов (#28)

Реализована фаза 4 спеки стандартов (`docs/specs/2026-09-29-setpiece-handoff.md` §6): штрафной,
угловой и удар от ворот наводятся стиком.

- Чистая модель в `src/onthepitch/setpiece/setpiecelogic`: `SetPieceAim` (угол от стартового
  направления), `SetPieceBaseHeading` (штрафной — центр ворот, угловой — 11-метровая, от ворот —
  вперёд), `RotateSetPieceAim` (1.6 рад/с, дуги: полный оборот / 1.4 / 1.2), `PlanSetPieceKick`
  (виды подач, высота стик-Y, две оси заряда удара штрафного 36→46 м/с и 4°→22°, `goal_magnet`
  0.18 только у удара).
- Состояние прицела/закрутки — в `SetPiecePresentation` (как ретикл пенальти); `HumanController`
  кормит сырые оси стика, гейтит виды подач (угловой/от ворот — только пасы), глушит чип и на
  отпускании собирает команду; закрутка копится от нажатия до касания. `GetShotVector` и
  touch-ветка паса используют высоту и curl из `TouchInfo`; камера стандарта следует за прицелом.
- Хеш `5a477078` без изменений (headless-раннер ударов не совершает), `nettest` 59,
  `lanmatchtest` 42. Полевой проверки прицеливания не было — см. [[открытые-вопросы]].

## [2026-10-01] feat | #30: смена бьющего через меню стандарта

Тикет #30 (фаза 5 спеки стандартов, §3) закрыт на ветке `of_port`. Бьющий-человек открывает
меню смены бьющего (`SetPieceTakerPage`) существующей `e_ButtonFunction_Select` (геймпад Share,
клавиатура — `Tab` вместо `F1` в `defaultKeyIDs`) и выбирает активного игрока команды, включая
вратаря.

- `Match`: `SetSetPieceTaker` (host-authoritative, зовётся из `Process` для локального меню и из
  `NetMatchSession::ProcessHost` для клиента) меняет `RefereeBuffer.taker`, переставляет designated
  и отдаёт нового игрока тому же `HumanGamer` (`Team::SelectPlayerForTakerChange`).
  `TeamAIController::SetPieceTaker` выносит позиционирование бьющего из `PrepareSetPiece`
  (то же смещение, для аута передаёт retain).
- Открытие: на хосте `HumanController::Process` (только своё устройство:
  `GetOwnerId() == GetLocalPeerId()`), на тонком клиенте детект `Select` в
  `NetMatchSession::ProcessClient` через `SetPiecePresentation::IsLocalTaker()` (клиент не тикает
  `HumanController`). Пока меню открыто, `HumanController` не читает ввод бьющего, а клиент шлёт
  нейтральный `NetInputFrame` (иначе навигация управляла бы игроком на хосте). Меню не открывается
  вне стандарта и во время заряда/полёта.
- Сеть: `NetInputFrame` получил `takerSlot`, протокол v15 → **v16**; клиент шлёт слот, пока идёт
  стандарт (потеря датаграммы выбор не сбрасывает), хост достаёт его из `NetHIDDevice` и проверяет
  владельца по лобби.
- Навигация меню — стрелки/WASD, Enter — применить, Escape — закрыть; авто-закрытие по концу
  стандарта.

Вики: [[матч]] (раздел «Выбор игрока и designated на стандарте»), [[сеть]] (v16), [[глоссарий]]
(термин уже был). Хеш `5a477078` не изменился (человеческий ввод в headless не нажимается),
`nettest` 60 (добавлена проверка `takerSlot` в UDP-канале), `lanmatchtest` 42. Полевой проверки
(хост+клиент, два локальных игрока) не было.

## [2026-10-01] feat | #30: авто-закрытие меню бьющего и подсказка над полем

Доработка #30 по замечаниям владельца:

- Нажатие любой кнопки розыгрыша (пас/навес/удар — `SetPiecePresentation::IsKickButtonPressed`)
  закрывает меню, не выполняя удар: пока оно открыто, ввод бьющего заморожен. На клавиатуре эти
  кнопки — `W`/`A`/`S`/`D`, поэтому клавиатурная навигация меню теперь только стрелками (WASD из
  `ProcessKeyboardEvent` убраны); геймпад — стик/крестовина.
- Пока идёт стандарт, локальный человек — бьющий, меню не открыто и кнопка розыгрыша не нажата,
  справа по центру висит текстовая подсказка (`Match::UpdateSetPieceTakerHint`, отдельный
  `Gui2Caption` в стиле `messageCaption`): `Tab: change taker` / `Share: change taker`. Устройство
  даёт `SetPiecePresentation::LocalKickerDevice()` (`Team::GetHumanGamerControllingPlayer`).
- Presentation-only: хеш `5a477078` не сдвинулся; `nettest` 60, `lanmatchtest` 42. Обновлены
  [[матч]] и [[открытые-вопросы]].

## [2026-10-01] fix | #30: правки меню бьющего по полевому отчёту

Четыре замечания владельца после проверки в игре:

1. Подсказка прижата к правому краю (`SetPosition(100 - w, 50)`) и текст мельче (высота caption
   ~2.6 %; размер глифов в `Gui2Caption` масштабируется по высоте вью).
2. Аналоговый стик пролистывал пачку игроков за короткий наклон: дебаунс `MoveSelection` 220 мс
   (`lastMoveTime_ms`); крестовина и раньше давала один эдж.
3. Второй локальный игрок со своего устройства мог листать/подтверждать чужое меню: пока
   `SetPieceTakerPage` открыт, GUI ограничен устройством бьющего — клавиатура
   `Enable/DisableKeyboard`, `SetActiveJoystickID` (новый `IHIDevice::GetGamepadID`,
   делегируется `DelayedHIDDevice`), с восстановлением в `Exit()`.
4. **Критичное:** A (подтвердить) и B (назад) — те же пас/навес, поэтому после закрытия меню
   удержанная кнопка тут же разыгрывала мяч. Добавлен латч ввода
   `Match::IsSetPieceTakerMenuInputBlocked()`/`setPieceTakerMenuSwallow`: `HumanController` (и
   нейтральный ввод клиента) заморожены, пока устройство бьющего не отпустит кнопку
   (`UpdateSetPieceTakerMenuRelease` зовётся в `Match::Process`/`ProcessClient`); сбрасывается в
   `StopSetPiece`.

Хеш `5a477078` не сдвинулся; `nettest` 60, `lanmatchtest` 42. Вики: [[матч]].

## [2026-10-01] fix | #30: старый бьющий уходит в строй, а не копится за мячом

Полевой отчёт: при смене исполнителя стандарта старые бьющие оставались стоять за мячом —
толпа. Причина: `Match::SetSetPieceTaker` двигал только нового бьющего, а старый оставался на
позиции из `PrepareSetPiece` (во время стандарта `TeamAIController::Process` не тикает, сам уйти
он не мог).

- `TeamAIController::PrepareSetPiece` получил параметр `forcedTaker`: с ним пропускается выбор
  бьющего по ролям/близости, и вся расстановка бьющей команды пересчитывается заново — новый
  бьющий встаёт за мяч, старый возвращается в свой слот. `SetSetPieceTaker` теперь зовёт
  `PrepareSetPiece(type, teamID, newTaker)`. Отдельный `SetPieceTaker` удалён.
- Хеш `5a477078` не сдвинулся (меню в headless не вызывается); `nettest` 60, `lanmatchtest` 42.
  Вики: [[матч]].

## [2026-10-01] fix | #30: вратарь убран из списка смены бьющего (это #31)

Полевой отчёт: при выборе вратаря бьющим на пенальти игра крашится. Разбор через headless-харнесс
(`nettest`/`lanmatchtest`): весь путь `Match::SetSetPieceTaker`/`PrepareSetPiece(forcedTaker)` и
полный цикл страницы меню (создание → Process → выбор вратаря → GoBack) отрабатывают без краша на
всех типах стандартов, с `Put` и в открытой игре. Значит, краш — в незавершённом пути **управления
вратарём человеком (#31)**, который штатный `Team::UpdateSwitch` специально не пускает
(`designatedTeamPossessionPlayer != GetGoalie()`). Тикет #30 заблокирован только #29, а не #31,
поэтому вратаря-бьющего из #30 выносим за скобки.

- `SetPieceTakerPage` не создаёт кнопку вратаря, если он не текущий бьющий (удар от ворот / аут,
  где он и так бьющий — no-op).
- `Match::SetSetPieceTaker` хост-гардом отбрасывает смену на `team->GetGoalie()` (защита от
  клиента, который всё равно пришлёт слот).
- Хеш `5a477078` не сдвинулся; `nettest` 60, `lanmatchtest` 42. Вики: [[матч]].

## [2026-10-01] docs | #30: краш вратаря-бьющего в открытых вопросах

Полевой отчёт «выбор вратаря бьющим на пенальти крашит» воспроизвести не удалось. Диагностический
билд (пошаговый лог в `taker_debug.log` по `Match::SetSetPieceTaker` → `PrepareSetPiece` →
`SelectPlayerForTakerChange`, вратарь возвращён в список меню, хост-гард снят) прошёл выбор
вратаря, удар и открытую игру на форс-стандартах `P`/`F`/`Y`; headless-харнесс (`nettest` 60,
`lanmatchtest` 42) тоже чист. Временный код убран, состояние — как в `15d17fd`.

Отмечено в [[открытые-вопросы]] на случай рецидива: управление вратарём человеком — незавершённый
#31, штатный `Team::UpdateSwitch` его не пускает, поэтому вратарь умышленно исключён из списка
бьющего и отбрасывается хост-гардом; включать его следует вместе с #31.

## [2026-10-01] feat | #30: вратарь снова может исполнять стандарты

По решению владельца (краш не воспроизводится, риск принят) сняты введённые в `15d17fd`
перестраховочные ограничения:

- `SetPieceTakerPage` снова показывает вратаря в списке бьющего наравне со всеми.
- `Match::SetSetPieceTaker` больше не отбрасывает смену на `team->GetGoalie()`.

Полноценного управления вратарём человеком по-прежнему нет — это #31; после стандарта человек
будет играть вратарём вне ворот без вратарской логики. Хеш `5a477078` не сдвинулся; `nettest` 60,
`lanmatchtest` 42. Вики: [[матч]], [[открытые-вопросы]].

## [2026-10-01] session | #30 закрыт

Тикет #30 «Смена бьющего через меню (Share/Tab)» закрыт на ветке `of_port`. Итог: меню на
Share/`Tab`, host-authoritative смена фактического бьющего (клиент — `NetInputFrame.takerSlot`,
протокол v16, v15→v16), авто-закрытие по кнопке розыгрыша с латчем ввода (A/B — те же пас/навес),
подсказка справа по центру, ввод только с устройства бьющего, дебаунс стика, пересчёт расстановки
при смене (старый бьющий уходит в строй), вратарь снова выбирается бьющим (управление вратарём по
ходу игры `Q`/`L1` не даётся — так и задумано). Сборка зелёная; `nettest` 60, `lanmatchtest` 42;
хеш `5a477078` без изменений. Полевой краш при выборе вратаря не воспроизведён — записан в
[[открытые-вопросы]] вместе с #31. См. [[матч]], [[сеть]].

## [2026-10-02] feat | #31: слой вратаря и включение управления

Введён GF-native слой вратаря (`src/onthepitch/keeper/keeperlogic.{hpp,cpp}`): перечисление
`e_KeeperState { None, Hands, Outfield, Returning }` и чистый расчёт (`ClampToBox`,
`SelectDistributionTarget`, `PredictLandingPoint`). Состояние host-sim, по команде, владеет
`Match` рядом с `ballRetainer`; владение по-прежнему только через `SetBallRetainer` (ловля —
существующий `e_FunctionType_Deflect` + `onlyDeflectAnimsThatPickupBall`, приклейка мяча к кости
не трогается).

- **Управление ровно в двух случаях:** ловля (`ballRetainer` стал вратарём → `Hands`) и бэкпас
  (вратарь designated и последний намеренный кик — партнёра → `Outfield`, выбор заранее). Снятие
  вратарского пропуска в `Team::UpdateSwitch` — только под `IsKeeperControllable`; вне этих
  случаев вратарь под ИИ.
- **`Hands`:** жёсткий кламп штрафной (`keeperBoxDepth 16.4`, `keeperBoxHalfWidth 20.05`) в
  `Humanoid::ClampKeeperToBox` (сдвигается origin анимации); `Switch` заперт (`Team::Process`
  требует `GetKeeperState != Hands`). Потеря мяча → `Returning`, управление уходит полевому.
- Константы — в [[константы]]; состояние добавлено в `Match::ProcessState`, поэтому хеш Windows
  x86 сменился `7134def2…` → `b1e8e432…` (перегенерация эталонов — #40). `nettest` 60,
  `lanmatchtest` 42 — зелёные. См. [[матч]], [[открытые-вопросы]].

## [2026-10-02] session | #31 закрыт

Тикет #31 «Вратарь: слой keeper и включение управления» закрыт на ветке `of_port`. Итог: каталог
`src/onthepitch/keeper/`, стейт-машина и чистый `keeperlogic`, кламп в `Hands`, выбор вратаря на
ловле и бэкпасе, запрет `Switch` в `Hands`; раздача и правило 6 секунд — #32, нырок — #33.
Сборка Release зелёная; `determinism_runner run` даёт `b1e8e432…` (эталоны — за #40).
Вручную поле не проверялось (ловит/бегает/бэкпас), тонкий клиент не покрыт — записано в
[[открытые-вопросы]].

## [2026-10-02] fix | #31: Outfield по владению, убран IsKeeperBackpass

Разбором установлено, что выбор вратаря на пас назад существовал и до #31: на кадре касания паса
`Humanoid::Process` зовёт `Team::SelectPlayer(адресат)` (`humanoid.cpp:515`), а в `Team::SelectPlayer`
вратарского исключения нет — управление переходило на вратаря, и он играл как полевой. Отдельная
эвристика `Match::IsKeeperBackpass` (designated + намеренный кик партнёра + своя половина) была
вторым, менее точным способом того же самого и удалена вместе со своим переходом.

`Outfield` теперь ставится по владению: вратарь с мячом у ног (`HasPossession()` при
`ballRetainer != goalie`) → `Outfield`, и `UpdateSwitch` выбирает его при `Hands || Outfield`. Пас
назад по-прежнему выбирает вратаря старым пасовым путём. Ловля (`Hands`), кламп штрафной и запрет
`Switch` в `Hands` — без изменений. Хеш не сдвинулся (`b1e8e432…`); `nettest` 60, `lanmatchtest`
42. См. [[матч]].

## [2026-10-02] feat | #32: раздача вратаря, правило 6 секунд, хэндофф

Реализован #32 на ветке `of_port` поверх слоя keeper из #31. Четыре действия раздачи из `Hands`,
ключуются от семантических кнопок в `HumanController::_KeeperDistributionCommand`:
- **рука** (`ShortPass`) — тап (< `keeperHandThrowCharge_ms`) = раскат низом ближнему, удержание =
  бросок верхом дальнему; цель — `keeperlogic::SelectDistributionTarget` (скоринг «направление ×
  полоса дальности», полоса от `keeperHandRollDist`/`keeperHandThrowDist`), вброс идёт обычной
  `ShortPass`-командой и существующим throw-клипом (`pass/*/special`, `incoming_retain_state`);
- **нога-в-центр** (`Shot`) — мгновенный фиксированный вынос (`Match::KeeperClearCenter`);
- **нога-по-стику** (`HighPass`) — направление стика, заряд = дальность (`Match::KeeperClearDirected`,
  хэндофф адресату);
- **«в ноги»** (`LongPass`) — `Match::KeeperDropToFeet`: мяч к ногам, `Hands → Outfield`, ударной
  команды нет.

Правило 6 с: `Match::keeperHandsStart_ms` стартует на ловле, по истечении хост форсит вынос в центр
и `SelectPlayer` ближайшему к приземлению. Ножные раздачи выполнены прямым запуском мяча через
`Match` (у `Shot` нет retain-анимации), поэтому они без клипа удара; вброс руками — с throw-клипом.
Потеря мяча → `Returning`, управление уходит полевому. Константы — в [[константы]]; добавление
`keeperHandsStart_ms` в `Match::ProcessState` + новая физика сдвинули хеш Windows x86
`b1e8e432…` → `4305b934…` (эталоны — #40). Сборка Release зелёная; вручную поле не проверялось
(раздача, таймер, потеря мяча) — записано в [[открытые-вопросы]].

## [2026-10-02] fix | #32: ножные раздачи — дроп-кик с клипом удара

Уточнение к #32: вместо прямого запуска мяча для «нога-в-центр»/«нога-по-стику» сделан дроп-кик.
`Match::KeeperPrepareDropKick` снимает retain и кладёт мяч к ногам (`keeperDropFeetOffset`/
`keeperDropFeetHeight`), после чего `HumanController::_KeeperDistributionCommand` шлёт обычную
`Shot`/`HighPass`-команду — играет существующий клип удара/навеса. `Shot` (ножной вынос в центр)
теперь мгновенно строится по нажатию, `HighPass` — по отпусканию с зарядом; хэндофф — адресату.
`Match::KeeperClearDirected` и множители `keeperClearSpeed*Factor` удалены как ненужные;
`KeeperClearCenter` (прямой запуск) остаётся только для автовыноса по правилу 6 с. Хеш не
сдвинулся (`4305b934…`, раздача в захвате детерминизма не исполняется). Полевой проверки
дроп-кика (контакт мяча, дуга, зависание клипа) нет — записано в [[открытые-вопросы]].
