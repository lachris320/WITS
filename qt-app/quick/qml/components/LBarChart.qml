import QtQuick
import QtQuick.Layouts
import LOAMS

// Custom bar visualization (§11) — NOT QtCharts (Risk 2). Per-bar color comes
// from the model item; the component supplies only structural tokens.
Item {
    id: chart
    property string orientation: "Vertical"   // Vertical | Horizontal
    property var model: []                     // list of {label, value, color}
    property real maxValue: 100
    property int barRadius: Theme.radius.xs

    implicitWidth: 320
    implicitHeight: 160

    RowLayout {
        anchors.fill: parent
        spacing: Theme.spacing.sm
        Repeater {
            model: chart.model
            delegate: Rectangle {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignBottom
                Layout.preferredHeight: chart.maxValue > 0
                        ? (chart.height * (modelData.value / chart.maxValue)) : 0
                radius: chart.barRadius
                color: modelData.color !== undefined ? modelData.color : Theme.brand.admin
                Accessible.role: Accessible.StaticText
                Accessible.name: (modelData.label !== undefined ? modelData.label : "")
                                 + " " + modelData.value
            }
        }
    }

    Accessible.role: Accessible.Pane
}
