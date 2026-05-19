cmake_minimum_required(VERSION 3.21)

project(FM VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.7 REQUIRED COMPONENTS Core Gui Widgets Qml Quick QuickControls2 Concurrent Svg QuickEffects)

qt_standard_project_setup(REQUIRES 6.7)
qt_policy(SET QTP0004 NEW)

find_package(Qt6 COMPONENTS Pdf QUIET)

qt_add_executable(fm
    src/main.cpp
    src/models/DirectoryModel.cpp
    src/models/DirectoryModel.h
    src/models/TreeModel.cpp
    src/models/TreeModel.h
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
    src/core/FileProvider.h
    src/core/LocalFileProvider.cpp
    src/core/LocalFileProvider.h
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
    src/core/MetadataExtractor.cpp
    src/core/MetadataExtractor.h
    src/core/WinThumbnailExtractor.cpp
    src/core/WinThumbnailExtractor.h
)

set_source_files_properties(qml/style/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

target_include_directories(fm PRIVATE src/core src/models src/controllers)

set(MY_QML_FILES
    qml/App.qml
    qml/Splash.qml
    qml/components/ConflictDialog.qml
    qml/components/DeleteConfirmDialog.qml
    qml/components/FileDelegate.qml
    qml/components/FilePanel.qml
    qml/components/FileWorkspace.qml
    qml/components/HelpDialog.qml
    qml/components/MainToolbar.qml
    qml/components/OperationsDrawer.qml
    qml/components/PathBar.qml
    qml/components/PropertiesDialog.qml
    qml/components/PreviewPane.qml
    qml/components/QuickLook.qml
    qml/components/ThemedContextMenu.qml
    qml/components/ThemedMenuItem.qml
    qml/components/ThemedMenuSeparator.qml
    qml/components/Sidebar.qml
    qml/style/Theme.qml
)

if (Qt6Pdf_FOUND OR Qt6_Pdf_FOUND)
    list(APPEND MY_QML_FILES qml/components/PdfPreviewer.qml)
endif()

qt_add_qml_module(fm
    URI FM
    VERSION 1.0
    SOURCES
        src/core/OperationQueue.h
    QML_FILES
        ${MY_QML_FILES}
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
        qml/assets/icons/folder.svg
        qml/assets/icons/file-manager.svg
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
        qml/assets/icons/select-all.svg
        qml/assets/icons/settings.svg
        qml/assets/icons/sun.svg
        qml/assets/icons/terminal.svg
        qml/assets/icons/video.svg
        qml/assets/lucide-toolbar/arrow-left.svg
        qml/assets/lucide-toolbar/arrow-right.svg
        qml/assets/lucide-toolbar/arrow-up.svg
        qml/assets/lucide-toolbar/columns-2.svg
        qml/assets/lucide-toolbar/copy.svg
        qml/assets/lucide-toolbar/eye.svg
        qml/assets/lucide-toolbar/eye-off.svg
        qml/assets/lucide-toolbar/folder-plus.svg
        qml/assets/lucide-toolbar/info.svg
        qml/assets/lucide-toolbar/layout-grid.svg
        qml/assets/lucide-toolbar/layout-list.svg
        qml/assets/lucide-toolbar/list.svg
        qml/assets/lucide-toolbar/moon.svg
        qml/assets/lucide-toolbar/move.svg
        qml/assets/lucide-toolbar/panel-right.svg
        qml/assets/lucide-toolbar/refresh-cw.svg
        qml/assets/lucide-toolbar/search.svg
        qml/assets/lucide-toolbar/sun.svg
)

target_link_libraries(fm
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::QuickEffects
        Qt6::Concurrent
        Qt6::Svg
)

if (Qt6Pdf_FOUND OR Qt6_Pdf_FOUND)
    message(STATUS "Qt6 PDF module found and enabled!")
    target_link_libraries(fm PRIVATE Qt6::Pdf)
    target_compile_definitions(fm PRIVATE HAS_QT_PDF)
else()
    message(WARNING "Qt6 PDF module NOT found! PDF previews will fall back to Windows Shell icons.")
endif()

find_package(TagLib CONFIG QUIET)
if (TagLib_FOUND)
    message(STATUS "TagLib found via CONFIG!")
    target_link_libraries(fm PRIVATE TagLib::TagLib)
    target_compile_definitions(fm PRIVATE HAS_TAGLIB)
else()
    find_package(TagLib QUIET)
    if (TagLib_FOUND)
        message(STATUS "TagLib found via MODULE!")
        target_link_libraries(fm PRIVATE ${TagLib_LIBRARIES})
        target_include_directories(fm PRIVATE ${TagLib_INCLUDES} ${TagLib_INCLUDE_DIRS})
        target_compile_definitions(fm PRIVATE HAS_TAGLIB)
    else()
        message(WARNING "TagLib NOT found! Audio metadata extraction will be disabled.")
    endif()
endif()


if(WIN32)
    set_target_properties(fm PROPERTIES WIN32_EXECUTABLE TRUE)
    target_link_libraries(fm PRIVATE Shell32 Comctl32)

    # Automatically copy dependency DLLs (tag.dll, zd.dll/z.dll) from vcpkg to output dir on Windows
    set(VCPKG_ROOT "C:/Users/tankr/vcpkg")
    if(EXISTS "${VCPKG_ROOT}")
        add_custom_command(TARGET fm POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${VCPKG_ROOT}/installed/x64-windows/$<IF:$<CONFIG:Debug>,debug/bin/zd.dll,bin/z.dll>"
                "${VCPKG_ROOT}/installed/x64-windows/$<IF:$<CONFIG:Debug>,debug/bin/tag.dll,bin/tag.dll>"
                $<TARGET_FILE_DIR:fm>
            COMMENT "Copying TagLib and ZLib DLLs from vcpkg to output folder..."
        )
    endif()
endif()
