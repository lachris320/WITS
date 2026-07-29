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
        onStatusChanged: {
            if (statusMessage.length > 0) {
                statusToast.severity = kioskVm.statusSeverity;
                statusToast.message = statusMessage;
            } else {
                statusToast.message = "";   // clear a lingering error on success
            }
        }
    }
    GuestViewModel { id: guestVm }

    // Pick up school/name, school/address, school/libraryHours and
    // school/logoPath written by the admin Settings screen. The kiosk cannot
    // see SettingsViewModel (it lives inside AdminScreen, which is torn down
    // before the kiosk is shown), so the hook is the surface change itself —
    // returning to the kiosk is exactly the moment a stale name or logo would
    // be looked at. Named function so QuickTests can drive the same path.
    //
    // Both call sites are needed and neither is redundant:
    //  * Component.onCompleted covers today's AppShell, whose Loader swaps
    //    sourceComponent and so DESTROYS and rebuilds this screen (and its VM)
    //    on every surface change. The VM constructor re-reads QSettings but
    //    does not sync() it first; reload() does, which is what actually makes
    //    another QSettings object's writes visible.
    //  * The Navigator handler covers a retained screen — if AppShell ever
    //    keeps the kiosk alive behind admin (a StackView, a visible toggle),
    //    onCompleted fires once at startup and the screen would otherwise
    //    never refresh again.
    // reload() is signal-quiet when nothing moved, so the overlap is free.
    function reloadSchoolInfo() { kioskVm.reload() }
    Connections {
        target: Navigator
        function onCurrentSurfaceChanged() {
            if (Navigator.currentSurface === Navigator.Kiosk)
                screen.reloadSchoolInfo();
        }
    }

    // Install the RFID filter on the real window once it exists.
    Component.onCompleted: {
        screen.reloadSchoolInfo();
        if (screen.appWindow)
            kioskVm.installRfid(screen.appWindow);
    }

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
