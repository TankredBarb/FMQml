cmake_minimum_required(VERSION 3.21)

project(FM VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.7 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Concurrent)

qt_standard_project_setup(REQUIRES 6.7)
qt_policy(SET QTP0004 NEW)

qt_add_executable(fm
    src/main.cpp
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
    src/controllers/ThemeController.cpp
    src/controllers/ThemeController.h
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
        qml/components/Sidebar.qml
        qml/style/Theme.qml
)

target_link_libraries(fm
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Concurrent
)

if(WIN32)
    set_target_properties(fm PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
