# PRD: Fast QML File Manager

## 1. Идея

Небольшой, быстрый и плавный файловый менеджер для Windows и Linux, вдохновленный удобством KDE Dolphin, но построенный вокруг современного QML-интерфейса, C++/Qt backend-а и аккуратной поддержки тем.

На первом этапе приложение фокусируется только на локальной файловой системе: просмотр каталогов, копирование, перенос, удаление, переименование и split screen для удобной работы с двумя папками.

## 2. Цели продукта

- Быстрый старт приложения и мгновенная навигация по каталогам.
- Плавный современный QML UI с аккуратными анимациями.
- Удобная двухпанельная работа через split screen.
- Кроссплатформенность: Windows и Linux.
- Локальные файловые операции: copy, move, rename, delete, create folder.
- Очередь операций с прогрессом, отменой и обработкой конфликтов.
- Архитектура, готовая к темам и будущим расширениям.

## 3. Не цели первой версии

- Сетевые протоколы: SMB, FTP, SFTP, WebDAV.
- Архивы как виртуальные папки.
- Плагины.
- Индексация всего диска.
- Облачные провайдеры.
- Полноценный терминал внутри приложения.
- Админские операции с повышением прав.

## 4. Целевая платформа

- Qt 6.7+.
- C++20.
- QML/Qt Quick Controls 2.
- CMake.
- Windows 10/11.
- Современные Linux-дистрибутивы с Wayland/X11.

## 5. UX-концепция

Основной экран состоит из одной или двух файловых панелей.

В однопанельном режиме:

- Верхняя панель: навигация назад/вперед, путь, поиск по текущей папке, кнопка split.
- Основная область: список или grid файлов.
- Нижняя/плавающая область: активные файловые операции.

В split screen:

- Левая и правая панели имеют независимые текущие пути.
- Активная панель подсвечена.
- Copy/Move могут использовать вторую панель как целевую папку по умолчанию.
- Split можно включать/выключать без потери состояния панелей.

Интерфейс должен быть спокойным и рабочим: плотная сетка, хорошая читаемость, минимум декоративного шума, плавные переходы только там, где они помогают ориентации.

## 6. Основные сценарии

### 6.1 Навигация

Пользователь открывает приложение, видит домашнюю папку или последний открытый путь, переходит по каталогам двойным кликом, через breadcrumb/path bar или кнопками назад/вперед.

### 6.2 Split screen copy

Пользователь включает split screen, открывает источник слева и цель справа, выбирает файлы, нажимает copy или перетаскивает в другую панель. Приложение показывает операцию в очереди и прогресс.

### 6.3 Move

Пользователь выбирает файлы и переносит их в другую папку. Если перенос внутри одного filesystem volume, операция должна использовать быстрый rename/move. Если между разными volume, выполняется copy + delete source после успешного копирования.

### 6.4 Конфликты имен

Если целевой файл уже существует, показывается диалог:

- Replace.
- Skip.
- Keep both.
- Apply to all.

### 6.5 Отмена операции

Пользователь может отменить копирование/перенос. Для partially copied файла поведение должно быть предсказуемым: временный файл удаляется, исходник не трогается до полного успешного завершения move.

## 7. Архитектура

Приложение делится на три слоя:

- UI Layer: QML-компоненты, визуальные состояния, анимации, темы.
- Presentation Layer: C++ QObject/ViewModel классы, которые отдают данные в QML.
- Core Layer: файловая модель, операции, очереди, platform abstraction.

QML не должен напрямую выполнять тяжелые операции с файловой системой. UI только отправляет команды в C++ и подписывается на состояния.

## 8. Компоненты

### 8.1 Core

`FileItem`

- path.
- name.
- extension.
- size.
- type: file, directory, symlink, drive.
- modified time.
- permissions.
- icon key.
- hidden flag.

`DirectoryScanner`

- Асинхронно читает содержимое каталога.
- Работает вне UI thread.
- Возвращает батчи, чтобы UI мог начать отображение до полного завершения.
- Поддерживает cancel при смене папки.

`FileOperation`

- Базовый класс операции.
- Поля: id, type, sources, destination, state, progress, speed, currentFile, error.
- Состояния: queued, running, paused, waitingForConflict, completed, failed, canceled.

`CopyOperation`

- Копирование файлов и директорий.
- Подсчет общего размера может идти лениво.
- Копирование через временный файл в destination.

`MoveOperation`

- Сначала пытается быстрый move/rename.
- При невозможности использует copy + delete.

`OperationQueue`

- Очередь операций.
- По умолчанию 1 активная операция для HDD-friendly поведения.
- В будущем можно добавить параллельные операции.
- Отдает прогресс в QML.

`ConflictResolver`

- Описывает конфликт.
- Принимает решение пользователя.
- Поддерживает apply to all.

`PlatformFileSystem`

- Абстракция поверх различий Windows/Linux.
- Drives на Windows.
- Mount points/home/trash особенности на Linux.
- Нормализация путей.
- Проверка case sensitivity.

### 8.2 Presentation

`DirectoryModel : QAbstractListModel`

Роли:

- name.
- path.
- displayPath.
- size.
- sizeText.
- type.
- iconName.
- modifiedText.
- isDirectory.
- isHidden.
- isSelected.

Обязанности:

- Хранит текущий путь панели.
- Запускает `DirectoryScanner`.
- Сортирует и фильтрует элементы.
- Поддерживает selection model.

`FilePanelController : QObject`

- openPath(path).
- goBack().
- goForward().
- goUp().
- refresh().
- selectedItems().
- setViewMode(list/grid/details).
- commands: copyTo, moveTo, rename, delete.

`WorkspaceController : QObject`

- Управляет одной или двумя панелями.
- Хранит activePanel.
- Включает/выключает split screen.
- Отдает target panel для copy/move.

`OperationsModel : QAbstractListModel`

- Список активных и завершенных операций.
- Прогресс, скорость, ошибки.
- Команды cancel/retry.

`ThemeController : QObject`

- currentTheme.
- systemTheme detection.
- accent color.
- density: comfortable/compact.
- QML получает palette tokens.

### 8.3 UI/QML

`App.qml`

- Корневое окно.
- Глобальные shortcuts.
- Layout основного workspace.

`MainToolbar.qml`

- Back/Forward/Up.
- Path bar.
- Search field.
- Split toggle.
- View mode button.

`FileWorkspace.qml`

- Контейнер одной или двух панелей.
- Split animation.
- Active panel focus handling.

`FilePanel.qml`

- Одна файловая панель.
- Header с текущим путем.
- View area.
- Empty/loading/error states.

`FileListView.qml`

- Details/list mode.
- Виртуализированный ListView/TableView.
- Делегаты фиксированной высоты.

`FileGridView.qml`

- Grid mode.
- Фиксированные размеры ячеек.

`FileDelegate.qml`

- Иконка, имя, метаданные.
- Selection/focus/hover states.
- Inline rename.

`PathBar.qml`

- Breadcrumb или editable path.
- Быстрый переход по сегментам пути.

`OperationsDrawer.qml`

- Компактный список файловых операций.
- Прогресс, скорость, cancel.

`ConflictDialog.qml`

- Replace/Skip/Keep both.
- Apply to all.

`Theme.qml`

- Design tokens: colors, spacing, radius, typography, motion durations.

## 9. Структура проекта

```text
FM/
  CMakeLists.txt
  README.md
  PRD.md
  src/
    main.cpp
    app/
      Application.cpp
      Application.h
    core/
      FileItem.h
      DirectoryScanner.cpp
      DirectoryScanner.h
      FileOperation.cpp
      FileOperation.h
      CopyOperation.cpp
      CopyOperation.h
      MoveOperation.cpp
      MoveOperation.h
      OperationQueue.cpp
      OperationQueue.h
      ConflictResolver.cpp
      ConflictResolver.h
      PlatformFileSystem.cpp
      PlatformFileSystem.h
    models/
      DirectoryModel.cpp
      DirectoryModel.h
      OperationsModel.cpp
      OperationsModel.h
    controllers/
      FilePanelController.cpp
      FilePanelController.h
      WorkspaceController.cpp
      WorkspaceController.h
      ThemeController.cpp
      ThemeController.h
  qml/
    App.qml
    components/
      MainToolbar.qml
      FileWorkspace.qml
      FilePanel.qml
      FileListView.qml
      FileGridView.qml
      FileDelegate.qml
      PathBar.qml
      OperationsDrawer.qml
      ConflictDialog.qml
    style/
      Theme.qml
      LightTheme.qml
      DarkTheme.qml
  resources/
    icons/
  tests/
    core/
    models/
```

## 10. Performance principles

- UI thread не читает директории и не копирует файлы.
- Данные каталога приходят батчами.
- Для больших директорий используется виртуализация QML view.
- Иконки кешируются по типу файла и пути.
- Thumbnails не нужны в первой версии.
- Сортировка должна работать в C++ модели, а не в QML.
- Операции копирования используют буфер фиксированного размера и периодический progress throttling, чтобы не спамить UI сигналами.
- При смене каталога старый scan отменяется.
- Обновления модели должны быть батчевыми, без полного reset при каждом изменении.

## 11. Motion design

Анимации должны быть короткими и функциональными:

- Split open/close: 160-220 ms.
- Hover/selection: 80-120 ms.
- Dialog/drawer enter: 120-180 ms.
- Path transition: без тяжелых page transitions.
- Drag target highlight: мгновенно, с мягким fade.

Нельзя делать анимации, которые задерживают файловые операции или навигацию.

## 12. Темы

Первая версия должна иметь:

- Light theme.
- Dark theme.
- System theme mode.

Theme tokens:

- `bg`.
- `surface`.
- `surfaceHover`.
- `surfaceActive`.
- `textPrimary`.
- `textSecondary`.
- `border`.
- `accent`.
- `danger`.
- `warning`.
- `success`.
- `radius`.
- `spacing`.
- `rowHeight`.
- `motionFast`.
- `motionNormal`.

QML-компоненты не должны хардкодить цвета, кроме редких служебных случаев.

## 13. Минимальный MVP

MVP считается готовым, если есть:

- Запуск приложения на Windows и Linux.
- Одна файловая панель.
- Навигация по локальным каталогам.
- List/details view.
- Выбор файлов.
- Copy в выбранную папку.
- Move в выбранную папку.
- Progress UI.
- Conflict dialog.
- Split screen с независимыми путями.
- Copy/move из одной панели в другую.
- Light/dark theme.

## 14. План разработки

### Milestone 1: Skeleton

- CMake + Qt Quick приложение.
- Регистрация C++ моделей в QML.
- Главное окно и базовая тема.
- Пустой `FilePanel`.

### Milestone 2: Directory browsing

- `DirectoryScanner`.
- `DirectoryModel`.
- Навигация open/up/back/forward.
- List view с сортировкой.

### Milestone 3: Selection and commands

- Selection model.
- Rename.
- Create folder.
- Delete в обычное удаление или platform trash, если реализовано.

### Milestone 4: Operations queue

- `OperationQueue`.
- `CopyOperation`.
- `MoveOperation`.
- Progress drawer.
- Cancel.

### Milestone 5: Conflicts

- Conflict detection.
- Conflict dialog.
- Replace/Skip/Keep both.
- Apply to all.

### Milestone 6: Split screen

- `WorkspaceController`.
- Две независимые панели.
- Active panel.
- Copy/move to opposite panel.
- Drag and drop between panels.

### Milestone 7: Polish

- Темы.
- Анимации.
- Keyboard shortcuts.
- Context menu.
- Performance tuning на больших директориях.

## 15. Keyboard shortcuts

- `Alt+Left`: back.
- `Alt+Right`: forward.
- `Alt+Up`: parent folder.
- `F2`: rename.
- `F5`: refresh.
- `Ctrl+C`: copy selection metadata/source list.
- `Ctrl+X`: mark selection for move.
- `Ctrl+V`: paste into current panel.
- `Delete`: delete.
- `Ctrl+L`: focus path bar.
- `F3`: toggle split screen.
- `Tab`: switch active panel.

## 16. Риски

- QML view может тормозить на огромных директориях, если делегаты будут слишком тяжелыми.
- Кроссплатформенные отличия путей и rename semantics требуют отдельного слоя.
- Без аккуратной отмены операций можно получить частично скопированные файлы.
- File watching может быть сложным на разных платформах; для MVP можно начать с manual refresh.
- Thumbnails лучше отложить, иначе они быстро усложнят performance profile.

## 17. Рекомендуемые технические решения

- `QAbstractListModel` для файловых списков.
- `QThreadPool` или dedicated worker thread для scanning/copying.
- `std::filesystem` для базовых операций, но с Qt wrappers там, где лучше нужна интеграция с QString/QUrl.
- `QSaveFile` полезен для атомарной записи небольших файлов, но для больших copy лучше собственный потоковый copy во временный файл.
- `QFileSystemWatcher` добавить после стабильной навигации.
- `QSettings` для сохранения темы, split state, последних путей и размеров колонок.

## 18. Критерии качества

- Открытие домашней папки быстрее 300 ms на обычном SSD после cold start UI.
- Навигация по папке с 10 000 файлов не блокирует интерфейс.
- Копирование большого файла не фризит UI.
- Отмена copy не оставляет финальный файл с ожидаемым именем в поврежденном состоянии.
- Split screen не сбрасывает текущие пути.
- Тема меняется без перезапуска.


