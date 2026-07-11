import QtQuick
import QtQuick.Layouts
import LOAMS

// N-way segmented toggle (§11) — e.g. the Today/This-Week visit-log control.
Rectangle {
    id: seg
    property var options: []          // list of {value, label}
    property var currentValue: null
    signal selectionChanged(var value)

    color: Theme.card
    radius: Theme.radius.sm2
    border.width: 2
    border.color: Theme.border
    implicitHeight: 34
    implicitWidth: 200

    RowLayout {
        anchors.fill: parent
        anchors.margins: 2
        spacing: 2
        Repeater {
            model: seg.options
            delegate: Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: Theme.radius.sm2
                color: modelData.value === seg.currentValue ? Theme.brand.admin
                                                            : "transparent"
                Text {
                    anchors.centerIn: parent
                    text: modelData.label
                    color: modelData.value === seg.currentValue ? Theme.brand.onPrimary
                                                               : Theme.text
                    font.family: Theme.typography.sans
                    font.pixelSize: Theme.typography.control
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        seg.currentValue = modelData.value;
                        seg.selectionChanged(modelData.value);
                    }
                }
                Accessible.role: Accessible.PageTab
            }
        }
    }

    Accessible.role: Accessible.PageTabList
}
