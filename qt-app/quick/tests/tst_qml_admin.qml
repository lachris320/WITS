import QtQuick
import QtTest
import LOAMS

Item {
    id: host
    width: 1100; height: 760

    // --- Dashboard stub VM ---
    ListModel { id: hourlyStub
        property real maxValue: 41
        ListElement { label: "8"; value: 12 }
        ListElement { label: "10"; value: 41 }
    }
    ListModel { id: deptStub
        property real maxValue: 210
        ListElement { label: "CE"; value: 210 }
        ListElement { label: "IT"; value: 180 }
    }
    QtObject {
        id: dashStub
        property int statToday: 128
        property int statWeek: 812
        property int statStudents: 3450
        property string peakHourLabel: "10 AM"
        property int peakHourIndex: 1
        property var hourlyModel: hourlyStub
        property var departmentModel: deptStub
        property bool loading: false
        property string errorText: ""
        function refresh() {}
    }
    DashboardScreen { id: dash; anchors.fill: parent; vm: dashStub }

    TestCase {
        name: "DashboardScreen"
        when: windowShown
        function test_showsPeakHourLabel() {
            compare(dash.peakShown, "10 AM");
        }
        function test_loadsWithStubVm() {
            verify(dash !== null);
        }
    }
}
