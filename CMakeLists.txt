cmake_minimum_required(VERSION 3.21)

project(FM VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

if(WIN32)
    add_compile_definitions(NOMINMAX)
endif()

set(CMAKE_UNITY_BUILD ON)

set(CMAKE_UNITY_BUILD_BATCH_SIZE 8)

find_package(Qt6 6.7 REQUIRED COMPONENTS Core Gui Widgets Qml Quick QuickControls2 Concurrent Svg QuickEffects QuickDialogs2)

qt_standard_project_setup(REQUIRES 6.7)
qt_policy(SET QTP0004 NEW)

find_package(Qt6 COMPONENTS Pdf QUIET)

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
    src/models/FavoritesModel.cpp
    src/models/FavoritesModel.h
    src/models/TreeModel.cpp
    src/models/TreeModel.h
    src/models/PlacesModel.cpp
    src/models/PlacesModel.h
    src/controllers/FilePanelController.cpp
    src/controllers/FilePanelController.h
    src/controllers/FavoritesController.cpp
    src/controllers/FavoritesController.h
    src/controllers/AdminController.cpp
    src/controllers/AdminController.h
    src/controllers/AppSettingsController.cpp
    src/controllers/AppSettingsController.h
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
    src/core/IconProvider.cpp
    src/core/IconProvider.h
    src/core/FileTypeIconResolver.cpp
    src/core/FileTypeIconResolver.h
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

target_include_directories(fm PRIVATE src src/core src/models src/controllers)

set(MY_QML_FILES
    qml/App.qml
    qml/Splash.qml
    qml/components/ConflictDialog.qml
    qml/components/DeleteConfirmDialog.qml
    qml/components/ChecksumDialog.qml
    qml/components/BatchRenameDialog.qml
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
    qml/components/KeyBadge.qml
    qml/components/MainToolbar.qml
    qml/components/OperationsDrawer.qml
    qml/components/PathBar.qml
    qml/components/PremiumTextField.qml
    qml/components/PropertiesDialog.qml
    qml/components/SettingsDialog.qml
    qml/components/DriveContextMenu.qml
    qml/components/DrivePropertiesHero.qml
    qml/components/PreviewPane.qml
    qml/components/QuickLook.qml
    qml/components/ThemeEditorDialog.qml
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
    qml/components/common/SurfaceCard.qml
    qml/components/app/CommandRegistry.qml
    qml/components/app/AppShortcuts.qml
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
    qml/components/preview/ZoomableImagePreview.qml
    qml/components/preview/MediaPreview.qml
    qml/components/preview/VideoPreview.qml
    qml/components/preview/PdfPreviewFallback.qml
    qml/components/preview/TextPreview.qml
    qml/components/preview/PreviewPropertiesList.qml
    qml/components/filepanel/FilePanelStatusBar.qml
    qml/components/filepanel/FilePanelFooter.qml
    qml/components/filepanel/FilePanelErrorBanner.qml
    qml/components/filepanel/FilePanelContextMenu.qml
    qml/components/filepanel/FilePanelEmptyMenu.qml
    qml/components/filepanel/FilePanelDropOverlay.qml
    qml/components/filepanel/FilePanelShell.qml
    qml/components/filepanel/FileIconCell.qml
    qml/components/filepanel/FileNameEditor.qml
    qml/components/filepanel/FileItemStateLayer.qml
    qml/components/filepanel/FileAttributeBadges.qml
    qml/components/filepanel/FileMetaText.qml
    qml/components/filepanel/FilePanelSelectionActions.qml
    qml/components/filepanel/FilePanelSelectionBadge.qml
    qml/components/filepanel/FilePanelFilterPopover.qml
    qml/components/filepanel/FilePanelViewMenu.qml
    qml/components/Sidebar.qml
    qml/components/StorageView.qml
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
        src/controllers/AppSettingsController.h
        src/controllers/FilePanelController.h
        src/controllers/FavoritesController.h
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
        qml/assets/icons/delete.svg
        qml/assets/icons/computer.svg
        qml/assets/icons/desktop.svg
        qml/assets/icons/document.svg
        qml/assets/icons/download.svg
        qml/assets/icons/eye.svg
        qml/assets/icons/eye-off.svg
        qml/assets/icons/folder-plus.svg
        qml/assets/icons/folder.svg
        qml/assets/icons/file-manager.svg
        qml/assets/icons/app_icon.svg
        qml/assets/icons/app_icon.png
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
        qml/assets/icons/star.svg
        qml/assets/icons/sun.svg
        qml/assets/icons/terminal.svg
        qml/assets/icons/video.svg
        qml/assets/filetypes/archive.svg
        qml/assets/filetypes/code.svg
        qml/assets/filetypes/document.svg
        qml/assets/filetypes/executable.svg
        qml/assets/filetypes/font.svg
        qml/assets/filetypes/folder.svg
        qml/assets/filetypes/image.svg
        qml/assets/filetypes/music.svg
        qml/assets/filetypes/pdf.svg
        qml/assets/filetypes/presentation.svg
        qml/assets/filetypes/spreadsheet.svg
        qml/assets/filetypes/video.svg
        qml/assets/lucide-toolbar/arrow-left.svg
        qml/assets/lucide-toolbar/arrow-right.svg
        qml/assets/lucide-toolbar/arrow-up.svg
        qml/assets/lucide-toolbar/columns-2.svg
        qml/assets/lucide-toolbar/copy.svg
        qml/assets/lucide-toolbar/copy-to-panel.svg
        qml/assets/lucide-toolbar/eye.svg
        qml/assets/lucide-toolbar/eye-off.svg
        qml/assets/lucide-toolbar/folder-plus.svg
        qml/assets/lucide-toolbar/funnel.svg
        qml/assets/lucide-toolbar/info.svg
        qml/assets/lucide-toolbar/layout-grid.svg
        qml/assets/lucide-toolbar/layout-list.svg
        qml/assets/lucide-toolbar/list.svg
        qml/assets/lucide-toolbar/moon.svg
        qml/assets/lucide-toolbar/move.svg
        qml/assets/lucide-toolbar/move-to-panel.svg
        qml/assets/lucide-toolbar/panel-open.svg
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
        Qt6::QuickDialogs2
        Qt6::Concurrent
        Qt6::Svg
)

find_package(unofficial-bit7z CONFIG QUIET)
if (unofficial-bit7z_FOUND OR TARGET unofficial::bit7z::bit7z64)
    message(STATUS "unofficial-bit7z found!")
    target_link_libraries(fm PRIVATE unofficial::bit7z::bit7z64)
    target_compile_definitions(fm PRIVATE HAS_UNOFFICIAL_BIT7Z)
else()
    message(WARNING "unofficial-bit7z NOT found! Archive support will fall back to local-only behavior.")
endif()

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
    target_link_libraries(fm PRIVATE Shell32 Comctl32 VirtDisk Ole32 Uuid Advapi32)

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
