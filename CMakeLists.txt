cmake_minimum_required(VERSION 3.21)

project(FM VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.7 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Concurrent Svg QuickEffects)

qt_standard_project_setup(REQUIRES 6.7)
qt_policy(SET QTP0004 NEW)

qt_add_executable(fm
    src/main.cpp
    src/SplashWindow.h
    src/SplashWindow.cpp
    src/models/DirectoryModel.cpp
    src/models/DirectoryModel.h
    src/models/PlacesModel.cpp
    src/models/PlacesModel.h
    src/controllers/FilePanelController.cpp
    src/controllers/FilePanelController.h
    src/controllers/WorkspaceController.cpp
    src/controllers/WorkspaceController.h
    src/core/OperationQueue.cpp
    src/core/OperationQueue.h
    src/core/HistoryManager.cpp
    src/core/HistoryManager.h
    src/core/DirectoryScanner.cpp
    src/core/DirectoryScanner.h
    src/core/IconProvider.cpp
    src/core/IconProvider.h
    src/core/ThumbnailProvider.cpp
    src/core/ThumbnailProvider.h
    src/core/FolderSizeCalculator.cpp
    src/core/FolderSizeCalculator.h
    src/controllers/ThemeController.cpp
    src/controllers/ThemeController.h
    src/controllers/QuickLookController.cpp
    src/controllers/QuickLookController.h
    src/controllers/PropertiesController.cpp
    src/controllers/PropertiesController.h
)

set_source_files_properties(qml/style/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(fm
    URI FM
    VERSION 1.0
    QML_FILES
        qml/App.qml
        qml/components/ConflictDialog.qml
        qml/components/FileDelegate.qml
        qml/components/FilePanel.qml
        qml/components/FileWorkspace.qml
        qml/components/MainToolbar.qml
        qml/components/OperationsDrawer.qml
        qml/components/PathBar.qml
        qml/components/PropertiesDialog.qml
        qml/components/QuickLook.qml
        qml/components/ThemedContextMenu.qml
        qml/components/ThemedMenuItem.qml
        qml/components/ThemedMenuSeparator.qml
        qml/components/Sidebar.qml
        qml/style/Theme.qml
    RESOURCES
        qml/assets/icons/arrow-left.svg
        qml/assets/icons/arrow-right.svg
        qml/assets/icons/arrow-up.svg
        qml/assets/icons/columns-2.svg
        qml/assets/icons/copy.svg
        qml/assets/icons/delete.svg
        qml/assets/icons/desktop.svg
        qml/assets/icons/document.svg
        qml/assets/icons/download.svg
        qml/assets/icons/eye.svg
        qml/assets/icons/eye-off.svg
        qml/assets/icons/folder-plus.svg
        qml/assets/icons/grid.svg
        qml/assets/icons/hard-drive.svg
        qml/assets/icons/home.svg
        qml/assets/icons/image.svg
        qml/assets/icons/info.svg
        qml/assets/icons/list.svg
        qml/assets/icons/moon.svg
        qml/assets/icons/move.svg
        qml/assets/icons/music.svg
        qml/assets/icons/paste.svg
        qml/assets/icons/refresh.svg
        qml/assets/icons/rename.svg
        qml/assets/icons/reveal.svg
        qml/assets/icons/search.svg
        qml/assets/icons/settings.svg
        qml/assets/icons/sun.svg
        qml/assets/icons/terminal.svg
        qml/assets/icons/video.svg
)

target_link_libraries(fm
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::QuickEffects
        Qt6::Concurrent
        Qt6::Svg
)

if(WIN32)
    set_target_properties(fm PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
