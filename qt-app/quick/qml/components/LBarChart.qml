import QtQuick
import QtQuick.Layouts
import LOAMS

// Custom bar visualization (§11) — NOT QtCharts (Risk 2). Consumes a BarsModel
// (roles label/value). Colors come from Theme (Global Constraints): all bars
// use barColor, except the bar at highlightIndex which uses highlightColor.
Item {
    id: chart
    property string orientation: "Vertical"     // Vertical | Horizontal
    property var model: null                     // BarsModel / ListModel {label,value}
    property real maxValue: 100
    property int barRadius: Theme.radius.xs
    property color barColor: Theme.brand.admin
    property color highlightColor: Theme.secondary
    property int highlightIndex: -1

    readonly property int barCount: repeaterV.model ? repeaterV.count
                                    : (repeaterH.model ? repeaterH.count : 0)

    implicitWidth: 320
    implicitHeight: 160

    function colorFor(i) { return i === chart.highlightIndex ? chart.highlightColor : chart.barColor }

    // --- Vertical (hourly): bottom-aligned columns + a label under each bar. ---
    RowLayout {
        id: vertical
        anchors.fill: parent
        visible: chart.orientation === "Vertical"
        spacing: Theme.spacing.sm
        Repeater {
            id: repeaterV
            model: chart.orientation === "Vertical" ? chart.model : null
            delegate: ColumnLayout {
                required property var model
                required property int index
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: Theme.spacing.xs
                Item { Layout.fillHeight: true }   // spacer so the bar sits at the bottom
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: chart.maxValue > 0
                        ? ((chart.height - 18) * (model.value / chart.maxValue)) : 0
                    radius: chart.barRadius
                    color: chart.colorFor(index)
                    Accessible.role: Accessible.StaticText
                    Accessible.name: (model.label !== undefined ? model.label : "") + " " + model.value
                }
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: model.label !== undefined ? model.label : ""
                    color: Theme.mutedTextCaption
                    font.family: Theme.typography.sans
                    font.pixelSize: Theme.typography.eyebrow
                }
            }
        }
    }

    // --- Horizontal (departments): label + width-proportional track. ---
    ColumnLayout {
        id: horizontal
        anchors.fill: parent
        visible: chart.orientation === "Horizontal"
        spacing: Theme.spacing.sm
        Repeater {
            id: repeaterH
            model: chart.orientation === "Horizontal" ? chart.model : null
            delegate: RowLayout {
                required property var model
                required property int index
                Layout.fillWidth: true
                spacing: Theme.spacing.md
                Text {
                    Layout.preferredWidth: 80
                    text: model.label !== undefined ? model.label : ""
                    color: Theme.text
                    font.family: Theme.typography.sans
                    font.pixelSize: Theme.typography.control
                    elide: Text.ElideRight
                }
                Rectangle {   // track
                    Layout.fillWidth: true
                    implicitHeight: 14
                    radius: chart.barRadius
                    color: Qt.alpha(Theme.brand.admin, 0.10)
                    Rectangle {   // fill
                        height: parent.height
                        width: chart.maxValue > 0 ? (parent.width * (model.value / chart.maxValue)) : 0
                        radius: chart.barRadius
                        color: chart.colorFor(index)
                        Accessible.role: Accessible.StaticText
                        Accessible.name: (model.label !== undefined ? model.label : "") + " " + model.value
                    }
                }
            }
        }
    }

    Accessible.role: Accessible.Pane
}
