import QtQuick
import QtQuick.Controls
import LOAMS

// Flat single-select combo (§11 form primitive) — the reset-visits department
// picker. YAGNI: NOT the cascading Dept->Course->Year selector (that is 4a/4b).
// Model is a plain string list. Colors/radius from Theme only.
//
// Hosts Controls ComboBox as a child rather than subclassing it at the root:
// QQuickComboBox already declares a FINAL `currentValue` (Qt 5.14+, computed
// from currentIndex/valueRole), so a root `ComboBox { property string
// currentValue }` fails to compile ("Cannot override FINAL property"). The
// Item host sidesteps that collision while keeping currentValue/selectValue/
// selected exactly as this component's own writable, test-driven contract.
Item {
    id: root
    implicitWidth: combo.implicitWidth
    implicitHeight: combo.implicitHeight

    property alias model: combo.model
    property string placeholder: ""
    property string currentValue: ""
    property alias count: combo.count
    // `color` alias so the theme-token test reads the background fill directly.
    property alias color: bg.color
    signal selected(string value)

    // Test/click seam: same single path a delegate click and a test call take
    // (mirrors LSegmented.selectionChanged). Keeps currentValue authoritative.
    function selectValue(v) {
        root.currentValue = v;
        var i = combo.indexOfValue ? combo.indexOfValue(v) : combo.model.indexOf(v);
        if (i >= 0) combo.currentIndex = i;
        root.selected(v);
    }

    ComboBox {
        id: combo
        anchors.fill: parent
        currentIndex: -1

        font.family: Theme.typography.sans
        font.pixelSize: Theme.typography.body

        onActivated: function(index) {
            root.currentValue = String(model[index]);
            root.selected(root.currentValue);
        }

        background: Rectangle {
            id: bg
            implicitHeight: 40
            radius: Theme.radius.sm
            color: Theme.card
            border.width: 2
            border.color: combo.activeFocus ? Theme.brand.admin : Theme.border
        }
        contentItem: Text {
            leftPadding: Theme.spacing.md
            rightPadding: Theme.spacing.md
            verticalAlignment: Text.AlignVCenter
            text: root.currentValue.length > 0 ? root.currentValue : root.placeholder
            color: root.currentValue.length > 0 ? Theme.text : Theme.mutedTextCaption
            font: combo.font
            elide: Text.ElideRight
        }
    }
}
