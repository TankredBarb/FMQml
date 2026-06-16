cmake_minimum_required(VERSION 3.21)

project(FM VERSION 0.1.0 LANGUAGES CXX)

include(CTest)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY
)

add_custom_target(uninstall
    COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

if(WIN32)
    add_compile_definitions(NOMINMAX)
endif()

set(CMAKE_UNITY_BUILD ON)

set(CMAKE_UNITY_BUILD_BATCH_SIZE 8)

find_package(Qt6 6.7 REQUIRED COMPONENTS Core Gui Widgets Qml Quick QuickControls2 Concurrent Network NetworkAuth Svg QuickEffects QuickDialogs2)

qt_standard_project_setup(REQUIRES 6.7)
qt_policy(SET QTP0004 NEW)

if(UNIX AND NOT APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBSECRET REQUIRED IMPORTED_TARGET libsecret-1)
endif()

find_package(Qt6 COMPONENTS Pdf QUIET)

option(FM_ENABLE_MULTIMEDIA_PREVIEW "Enable experimental QtMultimedia audio playback controls in QuickLook" OFF)
# Uncomment this line to force-enable the experimental audio playback controls.
set(FM_ENABLE_MULTIMEDIA_PREVIEW ON CACHE BOOL "Enable experimental QtMultimedia audio playback controls in QuickLook" FORCE)
set(FM_MULTIMEDIA_PREVIEW_AVAILABLE OFF)
if(FM_ENABLE_MULTIMEDIA_PREVIEW)
    find_package(Qt6 COMPONENTS Multimedia REQUIRED)
    if(Qt6Multimedia_FOUND OR Qt6_Multimedia_FOUND)
        get_target_property(FM_QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
        execute_process(
            COMMAND "${FM_QMAKE_EXECUTABLE}" -query QT_INSTALL_QML
            OUTPUT_VARIABLE FM_QT_QML_IMPORT_PATH
            OUTPUT_STRIP_TRAILING_WHITESPACE
            COMMAND_ERROR_IS_FATAL ANY
        )
        execute_process(
            COMMAND "${FM_QMAKE_EXECUTABLE}" -query QT_INSTALL_PLUGINS
            OUTPUT_VARIABLE FM_QT_PLUGIN_PATH
            OUTPUT_STRIP_TRAILING_WHITESPACE
            COMMAND_ERROR_IS_FATAL ANY
        )
        set(FM_QT_MULTIMEDIA_QML_DIR "${FM_QT_QML_IMPORT_PATH}/QtMultimedia")
        set(FM_QT_MULTIMEDIA_PLUGIN_DIR "${FM_QT_PLUGIN_PATH}/multimedia")
        file(GLOB FM_QT_MULTIMEDIA_BACKENDS
            "${FM_QT_MULTIMEDIA_PLUGIN_DIR}/*mediaplugin*.dll"
            "${FM_QT_MULTIMEDIA_PLUGIN_DIR}/*mediaplugin*.so"
            "${FM_QT_MULTIMEDIA_PLUGIN_DIR}/*mediaplugin*.dylib"
        )
        if(EXISTS "${FM_QT_MULTIMEDIA_QML_DIR}/qmldir" AND FM_QT_MULTIMEDIA_BACKENDS)
            set(FM_MULTIMEDIA_PREVIEW_AVAILABLE ON)
        elseif(NOT EXISTS "${FM_QT_MULTIMEDIA_QML_DIR}/qmldir")
            message(FATAL_ERROR "Qt6 Multimedia C++ module found, but QML import QtMultimedia was not found at ${FM_QT_MULTIMEDIA_QML_DIR}.")
        else()
            message(FATAL_ERROR "Qt6 Multimedia C++/QML modules found, but no QMediaPlayer backend plugin was found at ${FM_QT_MULTIMEDIA_PLUGIN_DIR}.")
        endif()
    endif()
endif()

qt_add_executable(fm
    src/main.cpp
    src/app/AppServices.cpp
    src/app/AppServices.h
    src/app/MainWindowSetup.cpp
    src/app/MainWindowSetup.h
    src/app/QmlEngineBootstrap.cpp
    src/app/QmlEngineBootstrap.h
    src/app/SplashController.cpp
    src/app/SplashController.h
    src/platform/PlatformIntegration.cpp
    src/platform/PlatformIntegration.h
    src/models/DirectoryModel.cpp
    src/models/DirectoryModel.h
    src/models/DiskUsageModel.cpp
    src/models/DiskUsageModel.h
    src/models/FileSearchModel.cpp
    src/models/FileSearchModel.h
    src/models/FavoritesModel.cpp
    src/models/FavoritesModel.h
    src/models/TreeModel.cpp
    src/models/TreeModel.h
    src/models/PlacesModel.cpp
    src/models/PlacesModel.h
    src/controllers/FilePanelController.cpp
    src/controllers/FilePanelController.h
    src/controllers/FileSearchController.cpp
    src/controllers/FileSearchController.h
    src/controllers/PluginActionController.cpp
    src/controllers/PluginActionController.h
    src/controllers/DiskUsageController.cpp
    src/controllers/DiskUsageController.h
    src/controllers/FavoritesController.cpp
    src/controllers/FavoritesController.h
    src/controllers/AdminController.cpp
    src/controllers/AdminController.h
    src/controllers/AppSettingsController.cpp
    src/controllers/AppSettingsController.h
    src/controllers/SystemTrayController.cpp
    src/controllers/SystemTrayController.h
    src/controllers/WorkspaceController.cpp
    src/controllers/WorkspaceController.h
    src/core/OperationQueue.cpp
    src/core/OperationQueue.h
    src/core/HistoryManager.cpp
    src/core/HistoryManager.h
    src/core/FileProvider.h
    src/core/FileError.cpp
    src/core/FileError.h
    src/core/FileAccessResolver.cpp
    src/core/FileAccessResolver.h
    src/core/FavoritesStore.cpp
    src/core/FavoritesStore.h
    src/core/DirectoryChangeWatcher.cpp
    src/core/DirectoryChangeWatcher.h
    src/core/DiskUsageScanner.cpp
    src/core/DiskUsageScanner.h
    src/core/FileSearchScanner.cpp
    src/core/FileSearchScanner.h
    src/core/QtDirectoryChangeWatcher.cpp
    src/core/QtDirectoryChangeWatcher.h
    src/core/WinDirectoryChangeWatcher.cpp
    src/core/WinDirectoryChangeWatcher.h
    src/core/LocalFileProvider.cpp
    src/core/LocalFileProvider.h
    src/core/ArchiveSupport.cpp
    src/core/ArchiveSupport.h
    src/core/ArchiveFileProvider.cpp
    src/core/ArchiveFileProvider.h
    src/core/IsoSupport.cpp
    src/core/IsoSupport.h
    src/core/IsoMountManager.cpp
    src/core/IsoMountManager.h
    src/core/FileProviderFactory.cpp
    src/core/FileProviderFactory.h
    src/core/FileActionPlugin.h
    src/core/FileProviderPlugin.h
    src/core/PlacesProviderPlugin.h
    src/core/FileProviderPluginRegistry.cpp
    src/core/FileProviderPluginRegistry.h
    src/core/IconProvider.cpp
    src/core/IconProvider.h
    src/core/SvgRecolorProvider.cpp
    src/core/SvgRecolorProvider.h
    src/core/FileTypeIconResolver.cpp
    src/core/FileTypeIconResolver.h
    src/core/TerminalLauncher.cpp
    src/core/TerminalLauncher.h
    src/core/WallpaperSetter.cpp
    src/core/WallpaperSetter.h
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
    src/core/ChecksumCalculator.cpp
    src/core/ChecksumCalculator.h
    src/core/BatchRenameEngine.cpp
    src/core/BatchRenameEngine.h
    src/core/DriveUtils.cpp
    src/core/DriveUtils.h
    src/core/VolumeMonitor.cpp
    src/core/VolumeMonitor.h
    src/core/SystemInfoProvider.cpp
    src/core/SystemInfoProvider.h
)

target_precompile_headers(fm PRIVATE
    <QObject>
    <QString>
    <QVariant>
    <QVector>
    <QList>
    <QMap>
    <QSharedPointer>
    <QDebug>
    <memory>
    <vector>
    <string>
    <filesystem>
)

set_source_files_properties(qml/style/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
set_source_files_properties(src/core/WinThumbnailExtractor.cpp PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)

target_include_directories(fm PRIVATE src src/core src/models src/controllers)

# Provider plugins are loaded dynamically and link against FileProvider symbols from the host executable.
set_target_properties(fm PROPERTIES ENABLE_EXPORTS TRUE)

if(WIN32)
    set_target_properties(fm PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
endif()

set(MY_QML_FILES
    qml/App.qml
    qml/Splash.qml
    qml/components/ConflictDialog.qml
    qml/components/DeleteConfirmDialog.qml
    qml/components/DiskUsageDialog.qml
    qml/components/ChecksumDialog.qml
    qml/components/BatchRenameDialog.qml
    qml/components/ArchivePasswordDialog.qml
    qml/components/DebugInformationDialog.qml
    qml/components/CommandPalette.qml
    qml/components/ColumnPickerMenu.qml
    qml/components/FileBriefAdaptiveDelegate.qml
    qml/components/FileDelegate.qml
    qml/components/FileBriefDelegate.qml
    qml/components/FileBriefResizeDelegate.qml
    qml/components/FileGridResizeDelegate.qml
    qml/components/FilePanel.qml
    qml/components/FavoritesView.qml
    qml/components/FilePanelHeader.qml
    qml/components/FileTableAdaptiveDelegate.qml
    qml/components/FileTableDelegate.qml
    qml/components/FileTableResizeDelegate.qml
    qml/components/FileWorkspace.qml
    qml/components/IconButton.qml
    qml/components/HelpDialog.qml
    qml/components/IsoMountDialog.qml
    qml/components/NestedArchiveDialog.qml
    qml/components/KeyBadge.qml
    qml/components/MainToolbar.qml
    qml/components/OperationsDrawer.qml
    qml/components/PathBar.qml
    qml/components/PremiumTextField.qml
    qml/components/PropertiesDialog.qml
    qml/components/FileSearchDialog.qml
    qml/components/SettingsDialog.qml
    qml/components/DriveContextMenu.qml
    qml/components/DrivePropertiesHero.qml
    qml/components/PreviewPane.qml
    qml/components/QuickLook.qml
    qml/components/ThemeEditorDialog.qml
    qml/components/ThemeEditorPreviewCard.qml
    qml/components/ThemeSchemeCard.qml
    qml/components/ThemeSelectorMenu.qml
    qml/components/ThemedContextMenu.qml
    qml/components/ThemedMenuItem.qml
    qml/components/ThemedMenuSeparator.qml
    qml/components/dialogs/DialogActionButton.qml
    qml/components/dialogs/DialogFooter.qml
    qml/components/dialogs/DialogHeader.qml
    qml/components/dialogs/DialogListRow.qml
    qml/components/dialogs/DialogSection.qml
    qml/components/dialogs/DialogShell.qml
    qml/components/common/EmptyState.qml
    qml/components/common/IconTile.qml
    qml/components/common/InlineBadge.qml
    qml/components/common/LinearProgress.qml
    qml/components/common/RecolorSvgIcon.qml
    qml/components/common/SurfaceCard.qml
    qml/components/app/CommandRegistry.qml
    qml/components/app/AppShortcuts.qml
    qml/components/app/InputCoordinator.qml
    qml/components/app/WorkspaceOverlays.qml
    qml/components/app/PreviewCoordinator.qml
    qml/components/toolbar/NavigationControls.qml
    qml/components/toolbar/ToolbarActions.qml
    qml/components/toolbar/ToolbarPathEditor.qml
    qml/components/toolbar/ToolbarSearch.qml
    qml/components/toolbar/ToolbarSegment.qml
    qml/components/toolbar/ViewControls.qml
    qml/components/preview/PreviewHeader.qml
    qml/components/preview/PreviewRenderer.qml
    qml/components/preview/ImagePreview.qml
    qml/components/preview/AudioPreview.qml
    qml/components/preview/ArchivePreview.qml
    qml/components/preview/ArchiveLimitedPreview.qml
    qml/components/preview/BookPreview.qml
    qml/components/preview/ExecutablePreview.qml
    qml/components/preview/FontPreview.qml
    qml/components/preview/FolderPreview.qml
    qml/components/preview/ZoomableImagePreview.qml
    qml/components/preview/MediaPreview.qml
    qml/components/preview/VideoPreview.qml
    qml/components/preview/UnsupportedPreview.qml
    qml/components/preview/PdfPreviewFallback.qml
    qml/components/preview/PreviewMetaStrip.qml
    qml/components/preview/TextPreview.qml
    qml/components/preview/PreviewFactsPanel.qml
    qml/components/preview/PreviewPropertiesList.qml
    qml/components/filepanel/FilePanelStatusBar.qml
    qml/components/filepanel/FilePanelFooter.qml
    qml/components/filepanel/FilePanelDetailsPolicy.qml
    qml/components/filepanel/FilePanelErrorBanner.qml
    qml/components/filepanel/FilePanelIconPolicy.qml
    qml/components/filepanel/FilePanelInlineRenamePolicy.qml
    qml/components/filepanel/FilePanelKeyboardPolicy.qml
    qml/components/filepanel/FilePanelLoadingPolicy.qml
    qml/components/filepanel/FilePanelActionPolicy.qml
    qml/components/filepanel/FilePanelMenuPolicy.qml
    qml/components/filepanel/FilePanelNavigationPolicy.qml
    qml/components/filepanel/FilePanelPathPolicy.qml
    qml/components/filepanel/FilePanelRubberBandPolicy.qml
    qml/components/filepanel/FilePanelSelectionPolicy.qml
    qml/components/filepanel/FilePanelScrollRestorePolicy.qml
    qml/components/filepanel/FilePanelScrollState.qml
    qml/components/filepanel/FilePanelStatusMessagePolicy.qml
    qml/components/filepanel/FilePanelViewAnchorPolicy.qml
    qml/components/filepanel/FilePanelViewRegistry.qml
    qml/components/filepanel/FileViewsReusePolicy.qml
    qml/components/filepanel/FilePanelContextMenu.qml
    qml/components/filepanel/FilePanelEmptyMenu.qml
    qml/components/filepanel/FilePanelDropOverlay.qml
    qml/components/filepanel/FilePanelShell.qml
    qml/components/filepanel/FileIconCell.qml
    qml/components/filepanel/FileNameEditor.qml
    qml/components/filepanel/FileItemStateLayer.qml
    qml/components/filepanel/SelectionToggleBadge.qml
    qml/components/filepanel/FileAttributeBadges.qml
    qml/components/filepanel/FileMetaText.qml
    qml/components/filepanel/FilePanelSelectionActions.qml
    qml/components/filepanel/FilePanelSelectionBadge.qml
    qml/components/filepanel/FilePanelFilterPopover.qml
    qml/components/filepanel/FilePanelViewMenu.qml
    qml/components/filepanel/PluginActionResultDialog.qml
    qml/components/PluginManagerDialog.qml
    qml/components/Sidebar.qml
    qml/components/StorageView.qml
    qml/style/Theme.qml
)

if (Qt6Pdf_FOUND OR Qt6_Pdf_FOUND)
    list(APPEND MY_QML_FILES qml/components/PdfPreviewer.qml)
endif()

if (FM_MULTIMEDIA_PREVIEW_AVAILABLE)
    list(APPEND MY_QML_FILES qml/components/preview/AudioPlaybackControls.qml)
endif()

qt_add_qml_module(fm
    URI FM
    VERSION 1.0
    SOURCES
        src/core/OperationQueue.h
        src/controllers/AppSettingsController.h
        src/controllers/FilePanelController.h
        src/controllers/FileSearchController.h
        src/controllers/PluginActionController.h
        src/controllers/DiskUsageController.h
        src/controllers/FavoritesController.h
        src/controllers/SystemTrayController.h
        src/models/FavoritesModel.h
        src/controllers/WorkspaceController.h
        src/controllers/ThemeController.h
        src/controllers/QuickLookController.h
        src/controllers/PropertiesController.h
        src/core/ChecksumCalculator.h
        src/core/IsoMountManager.h
    QML_FILES
        ${MY_QML_FILES}
    RESOURCES
        qml/assets/icons/arrow-left.svg
        qml/assets/icons/arrow-right.svg
        qml/assets/icons/arrow-up.svg
        qml/assets/icons/archive.svg
        qml/assets/icons/columns-2.svg
        qml/assets/icons/copy.svg
        qml/assets/icons/clipboard-copy.svg
        qml/assets/icons/delete.svg
        qml/assets/icons/computer.svg
        qml/assets/icons/cut.svg
        qml/assets/icons/desktop.svg
        qml/assets/icons/document.svg
        qml/assets/icons/download.svg
        qml/assets/icons/duplicate.svg
        qml/assets/icons/eject.svg
        qml/assets/icons/exit.svg
        qml/assets/icons/eye.svg
        qml/assets/icons/eye-off.svg
        qml/assets/icons/file-plus.svg
        qml/assets/icons/folder-open.svg
        qml/assets/icons/folder-plus.svg
        qml/assets/icons/folder.svg
        qml/assets/icons/ftp.svg
        qml/assets/icons/file-manager.svg
        qml/assets/icons/app_icon.ico
        qml/assets/icons/app_icon.svg
        qml/assets/icons/app_icon.png
        qml/assets/icons/grid.svg
        qml/assets/icons/hard-drive.svg
        qml/assets/icons/home.svg
        qml/assets/icons/image.svg
        qml/assets/icons/info.svg
        qml/assets/icons/invert-selection.svg
        qml/assets/icons/list.svg
        qml/assets/icons/moon.svg
        qml/assets/icons/move.svg
        qml/assets/icons/plugin.svg
        qml/assets/icons/music.svg
        qml/assets/icons/open.svg
        qml/assets/icons/paste.svg
        qml/assets/icons/refresh.svg
        qml/assets/icons/rename.svg
        qml/assets/icons/reveal.svg
        qml/assets/icons/search.svg
        qml/assets/icons/select-all.svg
        qml/assets/icons/settings.svg
        qml/assets/icons/star.svg
        qml/assets/icons/star-off.svg
        qml/assets/icons/sun.svg
        qml/assets/icons/tag.svg
        qml/assets/icons/terminal.svg
        qml/assets/icons/text-file.svg
        qml/assets/icons/video.svg
        qml/assets/icons/arrow-down.svg
        qml/assets/icons/artist.svg
        qml/assets/icons/attributes.svg
        qml/assets/icons/album.svg
        qml/assets/icons/bitrate.svg
        qml/assets/icons/calendar-clock.svg
        qml/assets/icons/calendar-plus.svg
        qml/assets/icons/checksum.svg
        qml/assets/icons/disk-usage.svg
        qml/assets/icons/duration.svg
        qml/assets/icons/extension.svg
        qml/assets/icons/size.svg
        qml/assets/icons/sort-ascending.svg
        qml/assets/icons/sort-date-created.svg
        qml/assets/icons/sort-date-modified.svg
        qml/assets/icons/sort-default.svg
        qml/assets/icons/sort-descending.svg
        qml/assets/icons/sort-extension.svg
        qml/assets/icons/sort-folders-first.svg
        qml/assets/icons/sort-name.svg
        qml/assets/icons/sort-size.svg
        qml/assets/icons/sort-type.svg
        qml/assets/icons/sort-mixed.svg
        qml/assets/icons/text-nowrap.svg
        qml/assets/icons/text-wrap.svg
        qml/assets/icons/zebra.svg
        qml/assets/filetypes/archive.svg
        qml/assets/filetypes/code.svg
        qml/assets/filetypes/document.svg
        qml/assets/filetypes/executable.svg
        qml/assets/filetypes/fb2.svg
        qml/assets/filetypes/font.svg
        qml/assets/filetypes/folder.svg
        qml/assets/filetypes/image.svg
        qml/assets/filetypes/music.svg
        qml/assets/filetypes/pdf.svg
        qml/assets/filetypes/presentation.svg
        qml/assets/filetypes/spreadsheet.svg
        qml/assets/filetypes/shortcut.svg
        qml/assets/filetypes/text.svg
        qml/assets/filetypes/video.svg
        qml/assets/filetypes-next/archive.svg
        qml/assets/filetypes-next/code.svg
        qml/assets/filetypes-next/document.svg
        qml/assets/filetypes-next/executable.svg
        qml/assets/filetypes-next/fb2.svg
        qml/assets/filetypes-next/folder.svg
        qml/assets/filetypes-next/font.svg
        qml/assets/filetypes-next/gdrive-mydrive.svg
        qml/assets/filetypes-next/gdrive-shared.svg
        qml/assets/filetypes-next/gdrive-shortcut.svg
        qml/assets/filetypes-next/gdrive-file-shortcut.svg
        qml/assets/filetypes-next/gdrive-trash.svg
        qml/assets/filetypes-next/gdrive.svg
        qml/assets/filetypes-next/image.svg
        qml/assets/filetypes-next/music.svg
        qml/assets/filetypes-next/pdf.svg
        qml/assets/filetypes-next/presentation.svg
        qml/assets/filetypes-next/shortcut.svg
        qml/assets/filetypes-next/spreadsheet.svg
        qml/assets/filetypes-next/text.svg
        qml/assets/filetypes-next/video.svg
        qml/assets/toolbar-next/arrow-left.svg
        qml/assets/toolbar-next/arrow-right.svg
        qml/assets/toolbar-next/arrow-up.svg
        qml/assets/toolbar-next/columns-2.svg
        qml/assets/toolbar-next/copy.svg
        qml/assets/toolbar-next/copy-to-panel.svg
        qml/assets/toolbar-next/eye.svg
        qml/assets/toolbar-next/eye-off.svg
        qml/assets/toolbar-next/folder-plus.svg
        qml/assets/toolbar-next/funnel.svg
        qml/assets/toolbar-next/info.svg
        qml/assets/toolbar-next/layout-grid.svg
        qml/assets/toolbar-next/layout-list.svg
        qml/assets/toolbar-next/list.svg
        qml/assets/toolbar-next/moon.svg
        qml/assets/toolbar-next/move.svg
        qml/assets/toolbar-next/move-to-panel.svg
        qml/assets/toolbar-next/pause.svg
        qml/assets/toolbar-next/panel-open.svg
        qml/assets/toolbar-next/panel-right.svg
        qml/assets/toolbar-next/play.svg
        qml/assets/toolbar-next/refresh-cw.svg
        qml/assets/toolbar-next/search.svg
        qml/assets/toolbar-next/sun.svg
        qml/assets/toolbar-next/volume-2.svg
        qml/assets/toolbar-next/volume-x.svg
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
        Qt6::QuickDialogs2
        Qt6::Concurrent
        Qt6::Network
        Qt6::Svg
)

find_package(unofficial-bit7z CONFIG QUIET)
if(NOT unofficial-bit7z_FOUND)
    find_package(bit7z CONFIG QUIET)
endif()

set(FM_BIT7Z_TARGET "")
foreach(candidate unofficial::bit7z::bit7z64 bit7z::bit7z bit7z64 bit7z)
    if(TARGET ${candidate})
        set(FM_BIT7Z_TARGET ${candidate})
        break()
    endif()
endforeach()

if(FM_BIT7Z_TARGET STREQUAL "")
    message(FATAL_ERROR "bit7z is required for archive support. Install unofficial-bit7z/bit7z and add its prefix to CMAKE_PREFIX_PATH or set unofficial-bit7z_DIR/bit7z_DIR.")
endif()

message(STATUS "bit7z found via target ${FM_BIT7Z_TARGET}; archive support is required.")
target_link_libraries(fm PRIVATE ${FM_BIT7Z_TARGET})
target_compile_definitions(fm PRIVATE HAS_UNOFFICIAL_BIT7Z)

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
    target_sources(fm PRIVATE
        fm.rc
        src/platform/WindowsTaskbarProgress.cpp
        src/platform/WindowsTaskbarProgress.h
    )
    set_source_files_properties(fm.rc PROPERTIES
        OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/qml/assets/icons/app_icon.ico"
    )
    target_link_libraries(fm PRIVATE Shell32 Comctl32 VirtDisk Ole32 Uuid Advapi32 Authz Dwmapi Cfgmgr32 Setupapi)

    # Automatically copy dependency DLLs (tag.dll, zd.dll/z.dll) from vcpkg to output dir on Windows.
    set(FM_VCPKG_ROOT "")
    if(DEFINED VCPKG_ROOT)
        set(FM_VCPKG_ROOT "${VCPKG_ROOT}")
    elseif(DEFINED ENV{VCPKG_ROOT})
        file(TO_CMAKE_PATH "$ENV{VCPKG_ROOT}" FM_VCPKG_ROOT)
    elseif(CMAKE_TOOLCHAIN_FILE MATCHES "[/\\\\]scripts[/\\\\]buildsystems[/\\\\]vcpkg\\.cmake$")
        get_filename_component(FM_VCPKG_BUILDSYSTEM_DIR "${CMAKE_TOOLCHAIN_FILE}" DIRECTORY)
        get_filename_component(FM_VCPKG_ROOT "${FM_VCPKG_BUILDSYSTEM_DIR}/../.." ABSOLUTE)
    endif()
    set(FM_VCPKG_TRIPLET "${VCPKG_TARGET_TRIPLET}")
    if(FM_VCPKG_TRIPLET STREQUAL "")
        set(FM_VCPKG_TRIPLET "x64-windows")
    endif()
    set(FM_VCPKG_INSTALLED_DIR "${FM_VCPKG_ROOT}/installed/${FM_VCPKG_TRIPLET}")
    if(FM_VCPKG_ROOT AND EXISTS "${FM_VCPKG_INSTALLED_DIR}")
        add_custom_command(TARGET fm POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${FM_VCPKG_INSTALLED_DIR}/$<IF:$<CONFIG:Debug>,debug/bin/zd.dll,bin/z.dll>"
                "${FM_VCPKG_INSTALLED_DIR}/$<IF:$<CONFIG:Debug>,debug/bin/tag.dll,bin/tag.dll>"
                $<TARGET_FILE_DIR:fm>
            COMMENT "Copying TagLib and ZLib DLLs from vcpkg to output folder..."
        )
    endif()

    add_executable(access_audit EXCLUDE_FROM_ALL
        tools/access-audit/main.cpp
        src/core/FileAccessResolver.cpp
        src/core/FileAccessResolver.h
    )
    target_compile_definitions(access_audit PRIVATE FM_ACCESS_RESOLVER_LOCAL_ONLY NOMINMAX WIN32_LEAN_AND_MEAN)
    target_include_directories(access_audit PRIVATE src src/core)
    target_link_libraries(access_audit PRIVATE Qt6::Core Advapi32 Authz)
endif()

if(UNIX AND NOT APPLE)
    target_sources(fm PRIVATE
        src/core/LinuxFileEnumerator.cpp
        src/core/LinuxFileEnumerator.h
        src/core/LinuxDirectoryChangeWatcher.cpp
        src/core/LinuxDirectoryChangeWatcher.h
    )

    install(TARGETS fm
        RUNTIME DESTINATION bin
    )
    install(FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/fm.desktop"
        DESTINATION share/applications
    )
    install(FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/qml/assets/icons/app_icon.png"
        DESTINATION share/icons/hicolor/256x256/apps
        RENAME fm.png
    )
endif()

if (FM_MULTIMEDIA_PREVIEW_AVAILABLE)
    message(STATUS "Qt6 Multimedia and QML import found; experimental audio playback controls are enabled!")
    target_link_libraries(fm PRIVATE Qt6::Multimedia)
    target_compile_definitions(fm PRIVATE
        HAS_QT_MULTIMEDIA
        FM_QT_QML_IMPORT_PATH="${FM_QT_QML_IMPORT_PATH}"
        FM_QT_PLUGIN_PATH="${FM_QT_PLUGIN_PATH}"
    )
else()
    message(STATUS "Experimental audio playback controls are disabled.")
endif()

set(FM_PROVIDER_PLUGIN_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/plugins/providers")
set(FM_PROVIDER_PLUGIN_IMPORT_DIR "${CMAKE_CURRENT_BINARY_DIR}/plugins/provider-imports")

qt_add_plugin(fm_mock_provider
    CLASS_NAME MockFileProviderPlugin
    src/plugins/mock/MockFileProviderPlugin.cpp
    src/plugins/mock/MockFileProviderPlugin.h
)

target_include_directories(fm_mock_provider PRIVATE src src/core)
target_link_libraries(fm_mock_provider PRIVATE Qt6::Core fm)
set_target_properties(fm_mock_provider PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${FM_PROVIDER_PLUGIN_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${FM_PROVIDER_PLUGIN_OUTPUT_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY "${FM_PROVIDER_PLUGIN_IMPORT_DIR}"
)

foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(fm_mock_provider PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_${config} "${FM_PROVIDER_PLUGIN_OUTPUT_DIR}"
        LIBRARY_OUTPUT_DIRECTORY_${config} "${FM_PROVIDER_PLUGIN_OUTPUT_DIR}"
        ARCHIVE_OUTPUT_DIRECTORY_${config} "${FM_PROVIDER_PLUGIN_IMPORT_DIR}"
    )
endforeach()

qt_add_plugin(fm_ftp_provider
    CLASS_NAME FtpFileProviderPlugin
    src/plugins/ftp/FtpFileProviderPlugin.cpp
    src/plugins/ftp/FtpFileProviderPlugin.h
)

target_include_directories(fm_ftp_provider PRIVATE src src/core)
target_link_libraries(fm_ftp_provider PRIVATE Qt6::Core Qt6::Concurrent Qt6::Network fm)
set_target_properties(fm_ftp_provider PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${FM_PROVIDER_PLUGIN_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${FM_PROVIDER_PLUGIN_OUTPUT_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY "${FM_PROVIDER_PLUGIN_IMPORT_DIR}"
)

foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(fm_ftp_provider PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_${config} "${FM_PROVIDER_PLUGIN_OUTPUT_DIR}"
        LIBRARY_OUTPUT_DIRECTORY_${config} "${FM_PROVIDER_PLUGIN_OUTPUT_DIR}"
        ARCHIVE_OUTPUT_DIRECTORY_${config} "${FM_PROVIDER_PLUGIN_IMPORT_DIR}"
    )
endforeach()

qt_add_plugin(fm_gdrive_provider
    CLASS_NAME GDriveFileProviderPlugin
    src/plugins/gdrive/GDriveAuth.cpp
    src/plugins/gdrive/GDriveAuth.h
    src/plugins/gdrive/GDriveCache.cpp
    src/plugins/gdrive/GDriveCache.h
    src/plugins/gdrive/GDriveFileProviderPlugin.cpp
    src/plugins/gdrive/GDriveFileProviderPlugin.h
    src/plugins/gdrive/GDrivePath.cpp
    src/plugins/gdrive/GDrivePath.h
    src/plugins/gdrive/GDriveTypes.h
)

target_include_directories(fm_gdrive_provider PRIVATE src src/core)
target_link_libraries(fm_gdrive_provider PRIVATE Qt6::Core Qt6::Gui Qt6::Network Qt6::NetworkAuth fm)
if(UNIX AND NOT APPLE)
    target_link_libraries(fm_gdrive_provider PRIVATE PkgConfig::LIBSECRET)
    target_compile_definitions(fm_gdrive_provider PRIVATE HAS_LIBSECRET)
endif()
set_target_properties(fm_gdrive_provider PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${FM_PROVIDER_PLUGIN_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${FM_PROVIDER_PLUGIN_OUTPUT_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY "${FM_PROVIDER_PLUGIN_IMPORT_DIR}"
)

foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(fm_gdrive_provider PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_${config} "${FM_PROVIDER_PLUGIN_OUTPUT_DIR}"
        LIBRARY_OUTPUT_DIRECTORY_${config} "${FM_PROVIDER_PLUGIN_OUTPUT_DIR}"
        ARCHIVE_OUTPUT_DIRECTORY_${config} "${FM_PROVIDER_PLUGIN_IMPORT_DIR}"
    )
endforeach()

if(WIN32)
    target_link_libraries(fm_gdrive_provider PRIVATE Advapi32)
    add_custom_command(TARGET fm_gdrive_provider POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:Qt6::NetworkAuth>"
            "$<TARGET_FILE_DIR:fm>"
        COMMENT "Copying Qt NetworkAuth runtime for Google Drive provider..."
    )
endif()

if(WIN32)
    qt_add_plugin(fm_portable_device_provider
        CLASS_NAME PortableDeviceFileProviderPlugin
        src/plugins/portable_device/PortableDeviceFileProviderPlugin.cpp
        src/plugins/portable_device/PortableDeviceFileProviderPlugin.h
    )

    target_include_directories(fm_portable_device_provider PRIVATE src src/core)
    target_link_libraries(fm_portable_device_provider PRIVATE Qt6::Core Qt6::Concurrent fm)
    set_target_properties(fm_portable_device_provider PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${FM_PROVIDER_PLUGIN_OUTPUT_DIR}"
        LIBRARY_OUTPUT_DIRECTORY "${FM_PROVIDER_PLUGIN_OUTPUT_DIR}"
        ARCHIVE_OUTPUT_DIRECTORY "${FM_PROVIDER_PLUGIN_IMPORT_DIR}"
    )

    foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
        set_target_properties(fm_portable_device_provider PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY_${config} "${FM_PROVIDER_PLUGIN_OUTPUT_DIR}"
            LIBRARY_OUTPUT_DIRECTORY_${config} "${FM_PROVIDER_PLUGIN_OUTPUT_DIR}"
            ARCHIVE_OUTPUT_DIRECTORY_${config} "${FM_PROVIDER_PLUGIN_IMPORT_DIR}"
        )
    endforeach()

    target_link_libraries(fm_portable_device_provider PRIVATE Ole32 PortableDeviceGuids)
endif()

if(BUILD_TESTING AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(inotify_directory_change_watcher_test
        tests/watchers/InotifyDirectoryChangeWatcherTest.cpp
        src/core/DirectoryChangeWatcher.cpp
        src/core/DirectoryChangeWatcher.h
        src/core/LinuxDirectoryChangeWatcher.cpp
        src/core/LinuxDirectoryChangeWatcher.h
    )
    target_include_directories(inotify_directory_change_watcher_test PRIVATE src/core)
    target_link_libraries(inotify_directory_change_watcher_test PRIVATE Qt6::Core)
    add_test(NAME inotify_directory_change_watcher_test COMMAND inotify_directory_change_watcher_test)

    add_executable(linux_root_traversal_test
        tests/scanners/LinuxRootTraversalTest.cpp
        src/core/LinuxFileEnumerator.cpp
        src/core/LinuxFileEnumerator.h
        src/core/DriveUtils.cpp
        src/core/DriveUtils.h
    )
    target_include_directories(linux_root_traversal_test PRIVATE src src/core)
    target_link_libraries(linux_root_traversal_test PRIVATE Qt6::Core)
    add_test(NAME linux_root_traversal_test COMMAND linux_root_traversal_test)

    add_executable(linux_file_access_resolver_test
        tests/core/LinuxFileAccessResolverTest.cpp
        src/core/FileAccessResolver.cpp
        src/core/FileAccessResolver.h
    )
    target_compile_definitions(linux_file_access_resolver_test PRIVATE FM_ACCESS_RESOLVER_LOCAL_ONLY)
    target_include_directories(linux_file_access_resolver_test PRIVATE src/core)
    target_link_libraries(linux_file_access_resolver_test PRIVATE Qt6::Core)
    add_test(NAME linux_file_access_resolver_test COMMAND linux_file_access_resolver_test)
endif()
