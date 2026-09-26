# Handoff: 3D-капитаны в пре-матч хабе + фиксы выбора комплектов формы

**Дата:** 2026-09-25
**Статус:** откатано (рабочее дерево чистое на `51ad848`), задача не выполнена.
**Тип:** handoff-пакет для новой сессии (одноразовый, устаревает по природе — см. `AGENTS.md`).

## Цель

В пре-матч хаб (`PreMatchPage`, `src/menu/startmatch/prematch.cpp`) добавить по бокам списка
составов **две 3D-модели капитанов** (по одной на команду) в выбранной форме, стоящих в
**спокойной нейтральной позе**. Плюс переделать вкладку **Kit** в реальный выбор комплекта
формы, при смене которого модели перекрашиваются.

До этого была сделана вкладка Kit с выбором комплекта и LAN-синхронизация — они тоже
откачены, но их стоит повторить (см. «Что было сделано» — код проверенный, компилировался и
проходил headless-тесты).

## Что реально сделано и проверено в откаченной попытке

Всё это компилировалось (Release x86/Win32), `nettest` PASS (61), `lanmatchtest` PASS (34),
детерминизм `7134def2c0863d4978bb18742b1f173358e4bf66` не сдвигался — то есть **логика
корректна**, проблема только в рендере 3D-превью.

### Вкладка Kit (работает)
- `PreMatchPage::BuildKitTab`, `availableKits[2]`, `Gui2Pulldown *kitPulldown[2]`.
- Перебор комплектов `01..06`: файл существует? (`<club>_kit_NN.png`, где
  `<club>` = `TeamData::GetKitUrl()`), иначе не предлагать; если ни одного — 01/02.
- Выбор пишется в `MenuTask::team1KitNum/team2KitNum` (добавлен `SetTeamKitNum`), это тот
  сток, что читает `Team::InitPlayers` при старте матча.
- Имя entry пулдауна **уникальное** вида `prematch_kit_t<team>_<kit>` (иначе
  `Gui2WindowManager::CreateImage2D` делит `Surface` по имени виджета — см. инварианты в
  [[архитектура]]); парсить номер кита из суффикса после последнего `_`.
- **Баг навигации (пофикшен):** `Gui2Pulldown` при раскрытии делает `grid->SetMaxVisibleRows(5)`
  и `isOverlay = true`. `PreMatchPage::ProcessKeyboardEvent` перехватывал `SDLK_UP` и уводил
  фокус на полосу вкладок. Надо **не перехватывать** `SDLK_UP`, если фокус внутри открытого
  overlay (пройти по `GetParent()` и проверить `IsOverlay()`) — либо вовсе убрать перехват и
  дать контент-гриду обработать Up сам. Второй уровень (список комплектов, выбор между home и
  away kit) сначала тоже был сломан, потом починен этим же условием.
- Также `Properties::GetBool` принимает **только литерал `"true"`** (`"1"` → false).

### LAN-синхронизация комплекта (работает)
- `NetLobbyState.homeKit/awayKit` (сеттеры-дефолты 1/2), сериализация в
  `WriteLobbyState`/`ReadLobbyState`.
- `e_NetLobbyAction_SetMatchOptions`: `value` = 0/1 — difficulty/duration (float×1000),
  `value` = 2/3 — home/away kit (raw номер); обрабатывается в `NetServer::ApplyLobbyAction`
  (только host) с clamp 1..6.
- `NetMatchSetup.kit[2]`, сериализация; хост кладёт номера в setup и в `SetTeamKitNum`,
  клиент читает их из setup.
- `PreMatchPage::SendKitOption`/`ApplyKit`/`SyncKitFromNetwork`; в сети пулдауны
  интерактивны только у хоста.
- Протокол поднять **v13 → v14** (`src/net/nettypes.hpp`, комментарий) и обновить
  `docs/wiki/константы.md`.
- `tools/nettest/main.cpp`: добавить проверки homeKit/awayKit в round-trip `NetLobbyState`.

### 3D-модель капитана (НЕ работает — главная проблема)
Класс `PreMatchCaptainPreview` (новый файл `src/menu/startmatch/prematchcaptain.{hpp,cpp}`,
добавить в `CMakeLists.txt` в `menulib` после `prematch.cpp`).

Что было и что выяснено:
- Модель — **копия** `fullbody.object` (`loader.LoadObject` → `Node(*template, postfix, scene3D)`),
  чтобы не менять общий кэш меша матча. Плюс **уникальное имя** ресурса на инстанс
  (`static int previewSerial`), иначе `FetchCopy` кэширует прошлую копию с чужим kit-идентификатором.
- Кит/кожа/волосы: подмена `meshes[i].material.diffuseTexture` по ident (`kit_template.png`,
  `skin.jpg`), как в `HumanoidBase`. Для волос —
  `FetchCopy(hairstyles/<style>.ase, уникальное_имя)`.
- **Нейтральная поза**: `BakeStraightPose` — порт
  `HumanoidBase::PrepareFullbodyModel`/`UpdateFullbodyModel` без `Player`/`Match`. Загружает
  `player.object` (риг), `base.anim.util` (поза, в которой меш авторится) и целевой кадр
  `movement/idle/000_idlelevel1.anim`; веса берёт из `GetVertexColors(colorCoords)` по **сырым**
  координатам вершины (`jointID = floor(color*0.1)`, `weight = (color-jointID*10)/9`); смешивает
  `target = targetPos + targetRot * inverse(baseRot) * (v - basePos)`; масштабирует по росту.
  Потом `OnUpdateGeometryData()`.
- **Нормализация размера**: после bake домножить все вершины так, чтобы высота была
  `previewHeight = 1.75` (тогда рост игрока не влияет).
- Модель создаётся под `GetGraphicsSystem()->getPhaseMutex` (как `Match::ApplyPendingSubstitutions`).

### Проблема: модели не видны на экране
Что установлено **точно** (диагностикой через `log.txt`, НЕ догадки):
1. `GetObjects<Camera>`/`PokeObjects` находят превью-камеры, `RenderView` для `view 1`/`view 2`
   **вызывается**, `geo` = 5 (после отключения culling) — то есть геометрия в вид попадает.
2. Но на экране капитанов нет. Оба прошлых «рабочих» скриншота, где были видны две модели
   разного размера, были на **фоне панорамы меню** — то есть рисовала их, скорее всего,
   **основная камера меню (`view 0`)**, а не превью-виды; сами превью-виды, судя по всему,
   выводят пустоту или не выводятся на экран вообще.
3. Отдельный `camera-view` **заливает свой прямоугольник** фоном из `postprocess.frag`
   (`fogColor ≈ (0.85,0.85,0.9)`) — это давало белый прямоугольник. Лечилось флагом
   `transparent_background` (в `postprocess.frag` alpha 0 для «неба» + альфа-блендинг в
   `Renderer3DMessage_RenderView`).
4. `Renderer3DMessage_RenderView` в начале делал `ClearBuffer(..., clearColor=true)` — `glClear`
   игнорирует viewport и стирает **весь** back-буфер, убивая ранее отрисованные виды. Надо
   чистить **только depth**.
5. `GraphicsTaskCommand_EnqueueView` собирает геометрию по bounding-плоскостям камеры; для
   превью-камер это отсекало модель (`geo 0`). Обходилось свойством `no_cull`/орто — тогда
   `geo 5`.
6. Ортографический вид (`CreateOrthoMatrix`) добавлен как флаг `View::orthographic` +
   `orthoHalfHeight`, но модели в нём **тоже не появились**.

**Что осталось невыясненным (главный вопрос новой сессии):** почему при `RenderView` с
`geo 5` и рабочей матрице модель не видна. Возможные направления:
- порядок вывода: превью-виды рисуются **до** меню-вида и/или меню-вид перекрывает полосы
  (проверить порядок `EnqueueView`/`RenderView` и не стирает ли `view 0` их прямоугольники);
- `backend`-буфер/`glClear` всё ещё что-то затирает;
- альфа-блендинг + depth-маска (`SetDepthMask`) скрывают модель;
- модель за пределами near/far орто/перспективы (лог `camPos` и `cameraMatrix`).

**Рекомендация:** НЕ пытаться угадывать по скриншотам. Первый шаг новой сессии — включить
per-view дамп (номер вида, rect, `geo`, projection/view-матрицы, AABB модели в world) и
**снять окно игры скриптом** (`PIL.ImageGrab` + `EnumWindows` по title `Gameplay Football`),
сравнивая пиксели в полосах. Также проверить, что `view 1/2` вообще блитятся на экран:
временно залить весь их прямоугольник сплошным цветом через `glScissor`+`glClear` **внутри
viewport** (обычный `glClear` не годится — игнорирует viewport).

## Альтернатива, если 3D не заводится за разумное время

2D-вариант (пользователь спросил, что это значит; выбор не сделан):
портрет `databases/default/faces/<tm_id>.png` (как в плане игры, `PlayerFacePath`) +
картинка комплекта `<club>_kit_NN.png` рядом. Рисуется обычным `Gui2Image`, одинаковый размер
гарантирован, без камер/рендера. См. [[матч]] (фото игроков в плане игры).

## Полезные факты про рендер (собраны по ходу)

- Меню-сцена: `MenuScene` (`src/menu/menuscene.{hpp,cpp}`) — камера `(0,0,1)`, FOV 90,
  `containerNode` в начале координат, фон — один меш `media/objects/menu/background01.ase`
  (плоский, z=0). У `MenuScene` нет геттера камеры — при необходимости добавить.
- `GraphicsTask::GetPhase` собирает **все** камеры сцены (`GetObjects<Camera>`) и делает
  `EnqueueView`; `ProcessPhase` → `RenderCamera` → `scene->PokeObjects(Camera)` → `OnPoke` →
  `RenderView`. `GetPhase`/`ProcessPhase` берут `getPhaseMutex`.
- `View` (`interface_renderer3d.hpp`): `x,y,width,height` считаются от `context_width/height`
  (1280×720) в `CreateView`. `SetViewport` — по пикселям.
- `OpenGLRenderer3D::ClearBuffer` игнорирует viewport (это root-cause ловушки с затиранием).
- `Geometry::GetAABB`/`GeometryData::GetAABB` **кэшируют** AABB; после правки вершин надо
  инвалидировать (`AddTriangleMesh` ставит `aabb.dirty`; прямого сеттера для vertex-правки нет).
- `Node::PokeObjects`/`GetObjects<T>` обходят дерево одинаково; `PokeObjects` уважает
  `IsEnabled()`.
- `Object::Attach(interpreter, thisPtr)` — `thisPtr` идёт в `Observer::subjectPtr` (protected).

## Ловушки, на которые я наступил (не повторять)

- **Счётчики в логах** (`if (dbg++ < N)`) многократно вводили в заблуждение: первые N записей —
  меню до создания капитанов. Логировать **без счётчика** или фильтровать по имени.
- `Properties::GetBool` — только `"true"`.
- Имена GUI-виджетов = ключ ресурса `Surface`: дубликаты делят картинку (см. [[архитектура]]).
- `EnqueueView` отсекает по bounding камеры — для «студийных» превью нужно `no_cull`.
- Первый camera-view стирает весь буфер (color clear) — чистить только depth.

## Готовые команды (для новой сессии)

```bat
:: сборка (каталог build/ уже настроен: VS2022, Win32, vcpkg x86-windows)
cmake --build . --parallel --config Release            :: из build/
:: проверки (из build/Release, рядом лежит data/)
.\nettest.exe
.\lanmatchtest.exe
.\determinism_runner.exe check 7134def2c0863d4978bb18742b1f173358e4bf66
```

Скриншот окна игры (PowerShell+Python, окно с заголовком `Gameplay Football`):
`PIL.ImageGrab.grab(bbox=GetWindowRect(...))` после `EnumWindows` с фильтром по title.

## Затронутые файлы (для повторной реализации)

| Файл | Что |
|---|---|
| `src/menu/startmatch/prematchcaptain.{hpp,cpp}` | новый класс превью (создать) |
| `src/menu/startmatch/prematch.{hpp,cpp}` | вкладка Kit, пулдауны, превью, навигация |
| `src/menu/menutask.hpp` | `SetTeamKitNum` |
| `src/menu/menuscene.{hpp,cpp}` | (опц.) геттер камеры, если ставить превью в меню-вид |
| `src/net/netmessages.{hpp,cpp}`, `netserver.cpp`, `nettypes.hpp` | kit в лобби/setup, протокол v14 |
| `src/systems/graphics/rendering/*` | флаги вида (transparent/ortho/no_cull), только depth clear |
| `src/systems/graphics/graphics_task.cpp`, `objects/graphics_camera.*` | прокидывание свойств |
| `data/media/shaders/postprocess.frag` | alpha 0 для «неба» прозрачного вида |
| `CMakeLists.txt`, `tools/nettest/main.cpp` | сборка + тесты |

## Обновление документации (после успеха)

`docs/wiki/матч.md` (капитаны/Kit), `docs/wiki/сеть.md` (kit в хабе, v14),
`docs/wiki/константы.md` (net_protocolVersion), `docs/wiki/архитектура.md` (прозрачные виды,
только-depth clear), `docs/wiki/открытые-вопросы.md`, запись в `log.md`.
