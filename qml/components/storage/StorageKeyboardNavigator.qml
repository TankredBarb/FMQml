import QtQuick

QtObject {
    id: navigator

    required property var storageRoot
    required property var flickable
    required property var layout
    required property var driveGrid
    required property var quickAccessGrid

function handleKey(event) {
    if (navigator.storageRoot.Window.window && navigator.storageRoot.Window.window.anyOverlayOpen) {
        event.accepted = true
        return
    }

    let drives = navigator.storageRoot.getDriveIndexes()
    let portable = navigator.storageRoot.getPortableIndexes()
    let folders = navigator.storageRoot.getFolderIndexes()

    if (drives.length === 0 && portable.length === 0 && folders.length === 0) return

    let isDriveSelected = (navigator.storageRoot.currentDriveIndex >= 0)
    let isPortableSelected = (navigator.storageRoot.currentPortableIndex >= 0)
    let isFolderSelected = (navigator.storageRoot.currentFolderIndex >= 0)
    let m = workspaceController.placesModel

    function previewRow(row) {
        if (m.data(m.index(row, 0), navigator.storageRoot.isDriveRole)) {
            navigator.storageRoot.previewDrive(row)
        } else {
            quickLookController.preview(m.data(m.index(row, 0), navigator.storageRoot.pathRole))
        }
    }

    function selectDrive(row) {
        navigator.storageRoot.currentDriveIndex = row
        navigator.storageRoot.currentPortableIndex = -1
        navigator.storageRoot.currentFolderIndex = -1
        previewRow(row)
    }

    function selectPortable(row) {
        navigator.storageRoot.currentDriveIndex = -1
        navigator.storageRoot.currentPortableIndex = row
        navigator.storageRoot.currentFolderIndex = -1
        previewRow(row)
    }

    function selectFolder(row) {
        navigator.storageRoot.currentDriveIndex = -1
        navigator.storageRoot.currentPortableIndex = -1
        navigator.storageRoot.currentFolderIndex = row
        previewRow(row)
    }

    // Initial selection if none
    if (!isDriveSelected && !isPortableSelected && !isFolderSelected) {
        if (event.key === Qt.Key_Up || event.key === Qt.Key_Down || event.key === Qt.Key_Left || event.key === Qt.Key_Right) {
            if (drives.length > 0) {
                selectDrive(drives[0])
            } else if (portable.length > 0) {
                selectPortable(portable[0])
            } else if (folders.length > 0) {
                selectFolder(folders[0])
            }
            event.accepted = true
            return
        }
    }

    if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
        if (isDriveSelected) {
            let path = m.data(m.index(navigator.storageRoot.currentDriveIndex, 0), Qt.UserRole + 2)
            if (path) navigator.storageRoot.controller.openPath(path)
        } else if (isPortableSelected) {
            let portablePath = m.data(m.index(navigator.storageRoot.currentPortableIndex, 0), Qt.UserRole + 2)
            if (portablePath) navigator.storageRoot.controller.openPath(portablePath)
        } else if (isFolderSelected) {
            let folderPath = m.data(m.index(navigator.storageRoot.currentFolderIndex, 0), Qt.UserRole + 2)
            if (folderPath) navigator.storageRoot.controller.openPath(folderPath)
        }
        event.accepted = true
        return
    }

    if (event.key === Qt.Key_Right) {
        if (isDriveSelected) {
            let idx = drives.indexOf(navigator.storageRoot.currentDriveIndex)
            if (idx >= 0 && idx < drives.length - 1) {
                selectDrive(drives[idx + 1])
            }
        } else if (isPortableSelected) {
            let idx = portable.indexOf(navigator.storageRoot.currentPortableIndex)
            if (idx >= 0 && idx < portable.length - 1) {
                selectPortable(portable[idx + 1])
            }
        } else if (isFolderSelected) {
            let idx = folders.indexOf(navigator.storageRoot.currentFolderIndex)
            if (idx >= 0 && idx < folders.length - 1) {
                selectFolder(folders[idx + 1])
            }
        }
        event.accepted = true
    } else if (event.key === Qt.Key_Left) {
        if (isDriveSelected) {
            let idx = drives.indexOf(navigator.storageRoot.currentDriveIndex)
            if (idx > 0) {
                selectDrive(drives[idx - 1])
            }
        } else if (isPortableSelected) {
            let idx = portable.indexOf(navigator.storageRoot.currentPortableIndex)
            if (idx > 0) {
                selectPortable(portable[idx - 1])
            }
        } else if (isFolderSelected) {
            let idx = folders.indexOf(navigator.storageRoot.currentFolderIndex)
            if (idx > 0) {
                selectFolder(folders[idx - 1])
            }
        }
        event.accepted = true
    } else if (event.key === Qt.Key_Down) {
        if (isDriveSelected) {
            let idx = drives.indexOf(navigator.storageRoot.currentDriveIndex)
            let cols = navigator.driveGrid.driveColumns
            if (idx >= 0 && idx + cols < drives.length) {
                selectDrive(drives[idx + cols])
            } else if (portable.length > 0) {
                selectPortable(portable[0])
            } else if (folders.length > 0) {
                selectFolder(folders[0])
            }
        } else if (isPortableSelected) {
            let idx = portable.indexOf(navigator.storageRoot.currentPortableIndex)
            let cols = navigator.driveGrid.portableColumns
            if (idx >= 0 && idx + cols < portable.length) {
                selectPortable(portable[idx + cols])
            } else if (folders.length > 0) {
                selectFolder(folders[0])
            }
        } else if (isFolderSelected) {
            let idx = folders.indexOf(navigator.storageRoot.currentFolderIndex)
            let cols = navigator.quickAccessGrid.columns
            if (idx >= 0 && idx + cols < folders.length) {
                selectFolder(folders[idx + cols])
            }
        }
        event.accepted = true
    } else if (event.key === Qt.Key_Up) {
        if (isDriveSelected) {
            let idx = drives.indexOf(navigator.storageRoot.currentDriveIndex)
            let cols = navigator.driveGrid.driveColumns
            if (idx - cols >= 0) {
                selectDrive(drives[idx - cols])
            }
        } else if (isPortableSelected) {
            let idx = portable.indexOf(navigator.storageRoot.currentPortableIndex)
            let cols = navigator.driveGrid.portableColumns
            if (idx - cols >= 0) {
                selectPortable(portable[idx - cols])
            } else if (drives.length > 0) {
                selectDrive(drives[drives.length - 1])
            }
        } else if (isFolderSelected) {
            let idx = folders.indexOf(navigator.storageRoot.currentFolderIndex)
            let cols = navigator.quickAccessGrid.columns
            if (idx - cols >= 0) {
                selectFolder(folders[idx - cols])
            } else if (portable.length > 0) {
                selectPortable(portable[portable.length - 1])
            } else if (drives.length > 0) {
                selectDrive(drives[drives.length - 1])
            }
        }
        event.accepted = true
    }
}

function ensureVisible(item) {
    if (!item) return
    var itemY = item.mapToItem(navigator.layout, 0, 0).y
    var itemHeight = item.height
    
    var viewportHeight = navigator.flickable.height
    var currentScrollY = navigator.flickable.contentY
    
    if (itemY < currentScrollY) {
        navigator.flickable.contentY = Math.max(0, itemY - 10)
    } else if (itemY + itemHeight > currentScrollY + viewportHeight) {
        navigator.flickable.contentY = Math.min(navigator.flickable.contentHeight - viewportHeight, itemY + itemHeight - viewportHeight + 10)
    }
}

function currentDriveIndexChanged() {
    navigator.storageRoot.currentDrivePath = navigator.storageRoot.currentDriveIndex >= 0
        ? navigator.storageRoot.modelValue(navigator.storageRoot.currentDriveIndex, navigator.storageRoot.pathRole, "")
        : ""
    if (navigator.storageRoot.currentDriveIndex >= 0) {
        Qt.callLater(() => {
            var item = navigator.driveGrid.driveItemAt(navigator.storageRoot.driveIndexes.indexOf(navigator.storageRoot.currentDriveIndex))
            if (item) navigator.ensureVisible(item)
        })
    }
}

function currentPortableIndexChanged() {
    navigator.storageRoot.currentPortablePath = navigator.storageRoot.currentPortableIndex >= 0
        ? navigator.storageRoot.modelValue(navigator.storageRoot.currentPortableIndex, navigator.storageRoot.pathRole, "")
        : ""
    if (navigator.storageRoot.currentPortableIndex >= 0) {
        Qt.callLater(() => {
            var item = navigator.driveGrid.portableItemAt(navigator.storageRoot.portableIndexes.indexOf(navigator.storageRoot.currentPortableIndex))
            if (item) navigator.ensureVisible(item)
        })
    }
}

function currentFolderIndexChanged() {
    navigator.storageRoot.currentFolderPath = navigator.storageRoot.currentFolderIndex >= 0
        ? navigator.storageRoot.modelValue(navigator.storageRoot.currentFolderIndex, navigator.storageRoot.pathRole, "")
        : ""
    if (navigator.storageRoot.currentFolderIndex >= 0) {
        Qt.callLater(() => {
            var item = navigator.quickAccessGrid.itemAt(navigator.storageRoot.folderIndexes.indexOf(navigator.storageRoot.currentFolderIndex))
            if (item) navigator.ensureVisible(item)
        })
    }
}
}
