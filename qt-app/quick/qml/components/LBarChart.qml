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

    // Motion (Phase 3 Task B, advisor §6): opt-in, default false. The bar
    // entrance binds to Repeater delegate lifecycle a screen structurally
    // cannot reach, so it lives here — but default-false keeps every
    // existing static consumer (tst_qml_components.qml, tst_barsmodel) and
    // any future dense consumer (e.g. a Phase-4 grid) visually unchanged:
    // the deliberate yScale:0 entrance start state only exists when
    // `animated: true` guarantees something will resolve it.
    property bool animated: false
    // Refresh double-grow guard: AdminScreen's Loader recreates the chart's
    // OWNER screen once per navigation, but the owning VM refresh()es
    // ~0.3-1s later and resets the model (BarsModel.setBars() always does a
    // full beginResetModel/endResetModel), destroying and recreating every
    // delegate a second time within the same visit. Only the first entrance
    // batch grows; this latches true once it starts, and later delegate
    // recreations snap straight to final geometry with no animation.
    property bool _entranceDone: false

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
                    id: bar
                    objectName: "hourBar_" + index
                    // Introspection for tests: HoverHandler is not a
                    // QQuickItem, so it is invisible to findChild()'s
                    // visual-tree search — expose its state directly instead
                    // of forcing tests to dig for the handler itself.
                    readonly property bool isHovered: barHover.hovered
                    Layout.fillWidth: true
                    // Value-tracking height binding — UNTOUCHED (must keep
                    // reacting to data changes; the entrance below animates
                    // a Scale transform instead of replacing this).
                    Layout.preferredHeight: chart.maxValue > 0
                        ? ((chart.height - 18) * (model.value / chart.maxValue)) : 0
                    // In-place height morphs (a genuine data change without a
                    // full model reset) glide instead of snapping. Behaviors
                    // don't fire on the initial construction-time assignment
                    // above, only on later changes, so this composes with
                    // the entrance grow without fighting it.
                    Behavior on Layout.preferredHeight {
                        NumberAnimation {
                            duration: Theme.motion.enabled ? Theme.motion.barGrow : 0
                            easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.motion.easing
                        }
                    }
                    radius: chart.barRadius
                    color: barHover.hovered ? Qt.lighter(chart.colorFor(index), 1.12) : chart.colorFor(index)
                    Behavior on color {
                        ColorAnimation { duration: Theme.motion.enabled ? Theme.motion.hoverLift : 0 }
                    }
                    Accessible.role: Accessible.StaticText
                    Accessible.name: (model.label !== undefined ? model.label : "") + " " + model.value

                    HoverHandler { id: barHover }

                    // Entrance grow (opt-in via chart.animated): scale up
                    // from the bottom via a Scale transform rather than
                    // animating Layout.preferredHeight directly, so the
                    // value-tracking binding above is never replaced.
                    // Starts at yScale 0 when animated — the house
                    // testability guarantee (SearchScreen's row entrance):
                    // a bar whose entrance never runs stays collapsed and
                    // fails loudly instead of silently passing at full
                    // height.
                    transform: Scale { id: barScale; origin.y: bar.height; yScale: chart.animated ? 0 : 1 }

                    SequentialAnimation {
                        id: barEntrance
                        objectName: "barEntrance"
                        // Teardown safety: index transiently goes to -1
                        // while a delegate is being torn down as part of a
                        // model reset; Math.max(0, ...) keeps that from
                        // feeding PauseAnimation a negative duration.
                        PauseAnimation {
                            duration: Math.max(0, Math.min(index, Theme.motion.staggerCap)) * Theme.motion.barStagger
                        }
                        NumberAnimation {
                            target: barScale; property: "yScale"; to: 1
                            duration: Theme.motion.enabled ? Theme.motion.barGrow : 0
                            easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.motion.easing
                        }
                    }
                    Component.onCompleted: {
                        if (chart.animated) {
                            if (!chart._entranceDone) {
                                barEntrance.start();
                                // Qt.callLater, not a timer: Repeater builds
                                // every delegate of THIS batch synchronously,
                                // so deferring the latch until after the
                                // current call stack unwinds means every bar
                                // in the batch still reads _entranceDone as
                                // false during its own onCompleted (else
                                // only bar 0 would ever grow — it would flip
                                // the latch before bar 1 is even
                                // constructed). A timer tuned to total
                                // duration would instead race the ~0.3-1s
                                // refresh and is flake bait offscreen.
                                Qt.callLater(function() { chart._entranceDone = true; });
                            } else {
                                // Later recreation (the post-refresh model
                                // reset): the entrance already played once
                                // this visit — snap to final geometry.
                                barScale.yScale = 1;
                            }
                        }
                    }

                    // Hover readout (owner-approved, brief B4): brighten
                    // alone is a false affordance without a way to read the
                    // actual count. Themed Rectangle+Text, not QtQuick
                    // Controls ToolTip (renders unstyled/off-brand).
                    Rectangle {
                        id: hoverReadout
                        objectName: "hourBarReadout_" + index
                        visible: barHover.hovered
                        z: 10
                        anchors.bottom: bar.top
                        anchors.bottomMargin: Theme.spacing.xs
                        anchors.horizontalCenter: bar.horizontalCenter
                        radius: Theme.radius.xs
                        color: Theme.card
                        border.width: 1
                        border.color: Theme.border
                        implicitWidth: hoverReadoutText.implicitWidth + Theme.spacing.sm * 2
                        implicitHeight: hoverReadoutText.implicitHeight + Theme.spacing.xs * 2
                        Text {
                            id: hoverReadoutText
                            objectName: "hourBarReadoutText_" + index
                            anchors.centerIn: parent
                            text: qsTr("%1 visits").arg(model.value)
                            color: Theme.text
                            font.family: Theme.typography.sans
                            font.pixelSize: Theme.typography.eyebrow
                        }
                    }
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
                        objectName: "deptBarFill_" + index
                        height: parent.height
                        width: chart.maxValue > 0 ? (parent.width * (model.value / chart.maxValue)) : 0
                        // Glide to a new width on every value change (brief
                        // B3) — a transition, not a keyframe entrance; this
                        // is the one place the reference already models
                        // refresh correctly. Fill ONLY, never the track.
                        Behavior on width {
                            NumberAnimation {
                                duration: Theme.motion.enabled ? Theme.motion.deptBarFill : 0
                                easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.motion.easing
                            }
                        }
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
