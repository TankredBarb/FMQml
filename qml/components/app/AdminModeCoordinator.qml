import QtQuick

QtObject {
    id: coordinator

    required property var appRoot
    required property var adminController
    required property var safetyDialog

    function available() {
        return coordinator.adminController !== null && coordinator.adminController !== undefined
    }

    function relaunchAsAdmin() {
        if (!available()) return false
        coordinator.appRoot.saveWorkspaceStateNow(true)
        return coordinator.adminController.relaunchAsAdmin()
    }

    function unlockAdminMode() {
        if (!available()) return false
        if (coordinator.adminController.shouldShowAdminSafetyWarning) {
            coordinator.safetyDialog.open()
            return false
        }
        const unlocked = coordinator.adminController.unlockAdminMode()
        if (!unlocked && coordinator.adminController.adminModeUnavailableReason) {
            coordinator.appRoot.showTransientInfo(coordinator.adminController.adminModeUnavailableReason)
        }
        return unlocked
    }

    function adminModeActive() {
        return available() && coordinator.adminController.adminModeActive
    }

    function confirmSafetyAndUnlock() {
        if (!available()) return false
        coordinator.adminController.acknowledgeAdminSafetyWarning()
        coordinator.safetyDialog.close()
        return unlockAdminMode()
    }

    function lockAdminMode() {
        if (!available()) return
        coordinator.adminController.lockAdminMode()
        coordinator.appRoot.showTransientInfo("Administrator mode locked")
    }

    function showStatus() {
        if (!available()) return
        let message = "Administrator mode: " + coordinator.adminController.adminModeStateName
        if (coordinator.adminController.adminModeRemainingSeconds > 0) {
            message += " (" + coordinator.adminController.adminModeRemainingSeconds + "s remaining)"
        } else if (coordinator.adminController.adminModeUnavailableReason) {
            message += " - " + coordinator.adminController.adminModeUnavailableReason
        }
        coordinator.appRoot.showTransientInfo(message)
    }
}
