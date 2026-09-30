import QtQuick
import QtTest
import LOAMS

// AccessControlScreen QuickTests (Sub-plan 4). Stub VM + stub hub drive the
// screen offline; a null-VM/null-hub mount proves the defensive fallbacks; a
// default mount proves `hub` defaults to the real (disabled) AccessControl
// singleton that every QuickTest target's Setup installs.
// Geometry: ac 0..1100 x 0..760 | vmlessAc 1150..2250 x 0..760 |
//           singletonAc 1150..2250 x 800..1560.
Item {
    id: host
    width: 2300; height: 1600

    ListModel { id: acRows }

    QtObject {
        id: acVmStub
        property int entriesToday: 7
        property string lastEntryAt: "2026-09-30 08:15:00"
        property string updatedAt: "08:15:05"
        property bool emptyFeed: false
        property bool initialLoadFailed: false
        property bool loading: false
        property bool stale: false
        property string errorText: ""
        property bool authFailure: false
        property var entries: acRows
        property int refreshCount: 0
        function refresh() { refreshCount++ }
    }

    QtObject {
        id: hubStub
        property bool accessEnabled: false
        property bool enableLocked: false
        property int connectionState: 0
        property var lastContactAt: null
        property bool refuse: false
        property int setCalls: 0
        function setAccessEnabled(on) { setCalls++; if (!refuse) accessEnabled = on }
    }

    AccessControlScreen { id: ac; width: 1100; height: 760; vm: acVmStub; hub: hubStub }
    AccessControlScreen { id: vmlessAc; x: 1150; width: 1100; height: 760; hub: null }
    AccessControlScreen { id: singletonAc; x: 1150; y: 800; width: 1100; height: 760 }

    TestCase {
        name: "AccessControlScreen"
        when: windowShown

        function init() {
            acRows.clear();
            acRows.append({ name: "Test Student A", schoolId: "TEST-0001", course: "BS Test",
                            department: "Dept Test", createdAt: "2026-09-30 08:15:00",
                            reader: "1", card: "CARD0012", known: true });
            acVmStub.entriesToday = 7;
            acVmStub.lastEntryAt = "2026-09-30 08:15:00";
            acVmStub.updatedAt = "08:15:05";
            acVmStub.emptyFeed = false;
            acVmStub.initialLoadFailed = false;
            acVmStub.loading = false;
            acVmStub.stale = false;
            acVmStub.errorText = "";
            acVmStub.authFailure = false;
            acVmStub.refreshCount = 0;
            hubStub.accessEnabled = false;
            hubStub.enableLocked = false;
            hubStub.connectionState = 0;
            hubStub.lastContactAt = null;
            hubStub.refuse = false;
            hubStub.setCalls = 0;
            ac.now = new Date();
        }

        function test_tilesRenderVmCounts() {
            compare(findChild(ac, "entriesTodayTile").value, "7");
            compare(findChild(ac, "lastEntryTile").value, "2026-09-30 08:15:00");
        }
        function test_lastEntryShowsDashWhenNone() {
            acVmStub.lastEntryAt = "";
            compare(findChild(ac, "lastEntryTile").value, "—");
        }
        function test_tableBindsVmEntries() {
            var table = findChild(ac, "entriesTable");
            verify(table !== null);
            tryCompare(table, "rowCount", 1);
        }
        function test_updatedLabelAndStaleBadge() {
            var label = findChild(ac, "updatedLabel");
            var badge = findChild(ac, "staleBadge");
            compare(label.text, "Updated 08:15:05");
            compare(badge.visible, false);
            acVmStub.stale = true;
            compare(badge.visible, true);
            acVmStub.updatedAt = "";
            compare(label.text, "Not loaded yet");
        }
        function test_refreshButtonInvokesVm() {
            var btn = findChild(ac, "refreshButton");
            mouseClick(btn);
            compare(acVmStub.refreshCount, 1);
        }
        function test_refreshDisabledWhileLoading() {
            var btn = findChild(ac, "refreshButton");
            acVmStub.loading = true;
            compare(btn.enabled, false);
            compare(btn.text, "Refreshing…");
        }
        function test_emptyFeedShowsNoEntriesYet() {
            acRows.clear();
            acVmStub.emptyFeed = true;
            var table = findChild(ac, "entriesTable");
            tryCompare(table, "rowCount", 0);
            compare(findChild(table, "tableEmptyState").text, "No entries yet");
        }
        function test_failedInitialLoadIsDistinctFromEmpty() {
            acRows.clear();
            acVmStub.updatedAt = "";
            acVmStub.errorText = "Network error. Please try again.";
            acVmStub.initialLoadFailed = true;
            var table = findChild(ac, "entriesTable");
            compare(findChild(table, "tableEmptyState").text,
                    "Could not load the access feed. Use Refresh to retry.");
        }
        function test_authFailureShowsInlineErrorState() {
            acRows.clear();
            acVmStub.authFailure = true;
            compare(findChild(ac, "authError").visible, true);
            var table = findChild(ac, "entriesTable");
            compare(findChild(table, "tableEmptyState").text,
                    "Admin authentication failed — re-enter via admin login.");
        }
        // LToast's auto-dismiss sets message="" imperatively, so the screen
        // must raise it imperatively too (the DatabaseScreen idiom).
        function test_toastShowsSecondErrorAfterFirstDismissed() {
            var toast = findChild(ac, "accessToast");
            acVmStub.errorText = "First error";
            compare(toast.message, "First error");
            toast.message = "";                       // simulate auto-dismiss
            acVmStub.errorText = "";
            acVmStub.errorText = "Second error";
            compare(toast.message, "Second error");
        }
        function test_toggleReflectsHubIntent() {
            var toggle = findChild(ac, "monitorToggle");
            compare(toggle.checked, false);
            hubStub.accessEnabled = true;
            compare(toggle.checked, true);
        }
        function test_toggleClickCallsHub() {
            var toggle = findChild(ac, "monitorToggle");
            mouseClick(toggle);
            compare(hubStub.setCalls, 1);
            compare(hubStub.accessEnabled, true);
            compare(toggle.checked, true);
        }
        function test_refusedToggleSnapsBackAndKeepsBinding() {
            var toggle = findChild(ac, "monitorToggle");
            hubStub.refuse = true;
            mouseClick(toggle);                       // LCheckbox flips locally...
            compare(hubStub.setCalls, 1);
            compare(toggle.checked, false);           // ...binding re-asserted to hub intent
            hubStub.accessEnabled = true;             // and the binding is still live
            compare(toggle.checked, true);
        }
        function test_lockedDisablesToggleAndShowsNote() {
            var toggle = findChild(ac, "monitorToggle");
            hubStub.accessEnabled = true;
            hubStub.enableLocked = true;
            compare(toggle.enabled, false);
            compare(findChild(ac, "lockedNote").visible, true);
            mouseClick(toggle);
            compare(hubStub.setCalls, 0);
            compare(toggle.checked, true);
        }
        function test_helperTextExplainsScope() {
            verify(findChild(ac, "monitoringHelp").text.indexOf("does not disable the physical gate") >= 0);
            compare(findChild(ac, "lockedNote").visible, false);
        }
        function test_connectionStateLabels() {
            var tile = findChild(ac, "connectionTile");
            var labels = ["Disconnected", "Connecting", "Connected", "Degraded", "Error"];
            for (var i = 0; i < labels.length; i++) {
                hubStub.connectionState = i;
                compare(tile.value, labels[i]);
            }
        }
        function test_contactAgeThreeStates() {
            var tile = findChild(ac, "contactTile");
            hubStub.accessEnabled = false;
            compare(tile.value, "Monitoring off");
            hubStub.accessEnabled = true;
            hubStub.lastContactAt = null;
            compare(tile.value, "No contact yet");
            var t = new Date(2026, 8, 30, 8, 0, 0);
            hubStub.lastContactAt = t;
            ac.now = new Date(t.getTime() + 3000);
            compare(tile.value, "Last contact 3 s ago");
        }
        function test_contactAgeAdvancesWithoutNewEvents() {
            var tile = findChild(ac, "contactTile");
            hubStub.accessEnabled = true;
            var t = new Date(2026, 8, 30, 8, 0, 0);
            hubStub.lastContactAt = t;                // never changes below
            ac.now = new Date(t.getTime() + 5000);
            compare(tile.value, "Last contact 5 s ago");
            ac.now = new Date(t.getTime() + 125000);
            compare(tile.value, "Last contact 2 min ago");
        }
        function test_ageTimerRunsOnlyWhileMonitoring() {
            var timer = findChild(ac, "ageTimer");
            compare(timer.running, false);
            hubStub.accessEnabled = true;
            compare(timer.running, true);
        }
        function test_vmlessMountRendersFallbacks() {
            compare(findChild(vmlessAc, "entriesTodayTile").value, "0");
            compare(findChild(vmlessAc, "lastEntryTile").value, "—");
            compare(findChild(vmlessAc, "connectionTile").value, "Disconnected");
            compare(findChild(vmlessAc, "contactTile").value, "");
            compare(findChild(vmlessAc, "monitorToggle").enabled, false);
            compare(findChild(vmlessAc, "refreshButton").enabled, false);
            compare(findChild(vmlessAc, "updatedLabel").text, "Not loaded yet");
            compare(findChild(vmlessAc, "authError").visible, false);
            compare(findChild(vmlessAc, "staleBadge").visible, false);
        }
        function test_defaultHubIsTheAccessControlSingleton() {
            compare(singletonAc.hub, AccessControl);
            compare(findChild(singletonAc, "monitorToggle").checked, false);   // disabled harness hub
            compare(findChild(singletonAc, "contactTile").value, "Monitoring off");
        }
    }
}
