import QtQuick
import QtQuick.Layouts
import LOAMS

Rectangle {
    id: screen
    property var appWindow          // the ApplicationWindow, for RFID install
    color: Theme.appBackground

    KioskViewModel {
        id: kioskVm
        onAdminRequested: Navigator.showAdmin()
        onGuestRequested: guestDialog.show()
        // Assign severity + message together from the live VM source. NOT a
        // declarative `severity: kioskVm.statusSeverity` binding on the toast:
        // the guest Connections below set severity imperatively, which would
        // permanently destroy such a binding and leave later kiosk-login
        // toasts showing a stale guest severity. Every raise-site sets both.
        onStatusChanged: if (statusMessage.length > 0) {
            statusToast.severity = kioskVm.statusSeverity;
            statusToast.message = statusMessage;
        }
    }
    GuestViewModel { id: guestVm }

    // Install the RFID filter on the real window once it exists.
    Component.onCompleted: if (screen.appWindow) kioskVm.installRfid(screen.appWindow)

    // Responsive split: side-by-side on wide screens, stacked when narrow.
    GridLayout {
        anchors.fill: parent
        columns: screen.width < 980 ? 1 : 2
        columnSpacing: 0; rowSpacing: 0
        BrandPanel {
            id: brand
            vm: kioskVm
            Layout.fillHeight: true
            Layout.preferredWidth: screen.width < 980 ? screen.width : 390
            Layout.fillWidth: screen.width < 980
        }
        KioskMain {
            vm: kioskVm
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
    }

    // Transient status toast (bottom-center). severity is NOT bound here —
    // it is set imperatively alongside message at every raise-site (kiosk
    // onStatusChanged + the two guest handlers below), so no raise-site's
    // assignment can strand a stale severity from a destroyed binding.
    LToast {
        id: statusToast
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.spacing.xxxl
    }

    GuestDialog {
        id: guestDialog
        vm: guestVm
        anchors.centerIn: parent
        visible: false
    }
    Connections {
        target: guestVm
        function onGuestSucceeded(message) { statusToast.severity = "Success"; statusToast.message = message }
        function onGuestFailed(message) { statusToast.severity = "Error"; statusToast.message = message }
    }
}
