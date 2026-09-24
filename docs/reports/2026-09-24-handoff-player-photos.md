# Handoff: фото игроков в плане игры (портретные вырезки)

Дата: 2026-09-24. Репозиторий: `GameplayFootball`, ветка `gameplan-substitutions`.
Смежные репо: `tm-gf-face-generator`, `tm-gf-import`, `transfermarkt_scrapper`.

Это одноразовый пакет для продолжения в свежей сессии. После выполнения — удалить/не
обновлять (устаревает по природе).

## Цель

Подключить реальные фото игроков в экран плана игры (карточки поля, инфо-полоса,
пикер ролей). Ассеты — портретные вырезки из фото Transfermarkt (фон удалён, палитра,
160×208). Источник: уже скачанные TM-портреты.

## Что уже сделано (не переделывать)

- **Фаза 1 (карточки)** — закоммичено `ae996b9`, запушено. Карточки поля: фото задаёт
  ширину, имя/позиция той же ширины с ellipsis, тонкая полоса усталости, выделение —
  цветная рамка (`Gui2Button::SetFrameOnly`). Расталкивания нет (искажало схемы).
- **Фаза 2 (проводка tm_id)** — закоммичено `de7c5b4`, запушено:
  - `PlayerDataRaw.tmId` (`src/data/playerdata.hpp`), читается из `players.tm_id`
    (`src/data/playerdata.cpp`).
  - Сериализация в сетевом setup (`src/net/netdata.cpp`), протокол **v13**
    (`src/net/nettypes.hpp`).
  - В `src/menu/gameplan.cpp` есть `PlayerFacePath(PlayerData*)`: путь
    `databases/default/faces/<tm_id>.jpg`, иначе `media/menu/player_placeholder.png`.
    **Сейчас расширение `.jpg` — поменять на `.png`.**
  - `Gui2Image::LoadImage` пропускает повторную загрузку того же пути.
- **Экспортёр вырезок** — `tm-gf-face-generator/export_cutouts.py` (новый файл,
  не закоммичен). Бэкенды: `--matting mediapipe` (быстро) и `--matting rembg`
  (`--rembg-model u2net|isnet-general-use`, качественнее, но медленно). Проверен на
  100 фото; листы сравнения — `C:\Users\Egor\Desktop\faces_compare\`.

## Решения

- Разрешение ассетов: **160×208** (родное соотношение 300:390 ≈ 10:13), **палитра 128
  цветов** (FASTOCTREE, альфа сохраняется). ~10 КБ/фото → ~680 МБ на 67k.
- Бэкенд на этом этапе: **mediapipe** (≈24 мин на 67k, 8 потоков). Качество: на
  студийных портретах ок; на уличных/сложных фото бывают дырки (футболка/шея) — если
  после полевого теста не устроит, переключить на `rembg u2net` (~15 ч) одной опцией.
- Вырезка = маска (бинарная, апскейл bilinear) → bbox человека + margin 4% → вписать в
  160×208 с сохранением пропорций (прозрачные поля по бокам, если кроп уже). Плейсхолдеры
  TM (белый силуэт) дают пустую маску → файл не пишется → в игре плейсхолдер.

## Шаги

### 1. Полный прогон вырезок (mediapipe)

```bat
cd C:\Users\Egor\Desktop\projects\tm-gf-face-generator
.venv\Scripts\python.exe export_cutouts.py ^
  --faces C:\Users\Egor\Desktop\projects\transfermarkt_scrapper\data\images\faces ^
  --out   C:\Users\Egor\Desktop\projects\transfermarkt_scrapper\data\images\faces_cutout ^
  --width 160 --height 208 --colors 128 --margin 0.04 --matting mediapipe ^
  --threads 8 --resume
```

- В `faces_cutout` уже ~19.6k файлов от остановленного прогона — `--resume` их пропустит.
- Долгий процесс: запускать в фоне, лог в файл, не блокировать диалог.

### 2. Импорт: класть вырезки в игровой каталог

`tm-gf-import/builders/files.py::copy_faces` сейчас читает `images_dir/faces/<id>.jpg`.
Поменять на каталог вырезок и `.png`: копировать `<images_dir>/faces_cutout/<id>.png` →
`out_dir/faces/<id>.png`. `import.py` передаёт `--images` (путь к `data/images`).

### 3. Игра: портретные боксы фото

Ассеты портретные (160:208), значит все фото-боксы в `src/menu/gameplan.cpp` сделать
портретными (`photoAspect = 208.0f/160.0f = 1.3`):

- `PlayerFacePath` — расширение `.png`.
- `PitchCardHeight` / `pitchCardContentH` — высота фото `cardW * photoAspect`.
- `placeCard` (ветка `onPitch`): размер фото `cardW × cardW*photoAspect`, сдвиг по `y`.
- `ApplyPitchCardGeometry`: `roleY = cy + cardW*photoAspect + cardGap`.
- `BuildOpponent`: то же для read-only панели.
- Инфо-полоса (`infoPhotoA/B`, `UpdateInfoDetail`): фото `infoPhotoW × infoPhotoH`,
  `centerX = photoX + infoPhotoW*0.5`, позиции бейджа/имени под фото.
- Пикер ролей (`rolePickerPhoto`, `RefreshRoles`): фото `rolePickerPhotoW × rolePickerPhotoH`,
  бейдж/имя ниже.
- Готовые константы уже были набросаны (`photoAspect`, `infoPhotoW/H`, `rolePickerPhotoW/H`),
  но правка откачена — сделать заново и аккуратно.

### 4. Копирование faces в сборку

`tools/copy_data.cmake` сейчас исключает `faces/` (raw 4 ГБ). Теперь это ~680 МБ мелких
PNG; либо убрать исключение, либо копировать только для релизной сборки (67k файлов —
копирование небыстрое; в dev-сборке можно оставить исключение и тестировать, положив
несколько PNG вручную).

### 5. Проверка

- Собрать: `cmake --build . --parallel --config Release` в `build/`.
- Запустить, открыть план игры: на карточках/в инфо-полосе/пикере — портреты, где есть
  файл, иначе плейсхолдер; ник обрезки и раскладка не сломаны.
- В сетевом матче проверить, что фото есть и у клиента (tmId едет в setup, протокол v13).

### 6. Вики

- `docs/wiki/данные-из-transfermarkt.md` — раздел про фото: заменить «игра не использует
  faces» на актуальное (вырезки 160×208 палитра, импорт кладёт `faces/<id>.png`).
- `docs/wiki/матч.md` — карточка: фото портретное, ключ `tm_id`.
- `docs/wiki/константы.md` — размер фото/аспект.
- `docs/wiki/открытые-вопросы.md` — что не проверено полем (mediapipe-дырки, вес бандла).
- `log.md` — запись о вехе (фото подключены).

## Известные проблемы

- **mediapipe рвёт низ на нестудийных фото** (пример: `1000703.jpg` — уличный кадр).
  Лечится `--matting rembg --rembg-model u2net` (медленно). Решить после полевого теста.
- `rembg` и `onnxruntime` уже стоят в venv генератора; модели u2net/isnet скачаны.
- Вес бандла ~680 МБ (палитра). При необходимости — меньше разрешение (96×125 ~310 МБ)
  или WebP (потребует фичу в `vcpkg.json`).
