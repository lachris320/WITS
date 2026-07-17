import QtQuick
import QtQuick.Layouts
import LOAMS

// Row-grid primitive (§11). columns: list of {key, title, weight?}; model:
// a QAbstractListModel whose roles match the column keys.
Rectangle {
    id: table
    property var columns: []
    property var model: null
    property bool selectable: false
    property string emptyStateText: qsTr("No records")
    // Opt-in row entrance (Phase 3 Task C), default OFF. tst_qml_components.qml
    // exercises this component assuming static geometry, and any future dense
    // static consumer (a Phase-4 database grid, an exports preview) must stay
    // visually unchanged by default — only a consumer that explicitly sets
    // this true (VisitLogsScreen) opts into the populate/add row fade-in.
    property bool animateRows: false

    // Introspection for tests + the empty state.
    readonly property int rowCount: list.count
    readonly property bool emptyVisible: list.count === 0

    color: Theme.card
    radius: Theme.radius.card
    border.width: 2
    border.color: Theme.border
    implicitWidth: 320
    implicitHeight: 200
    clip: true

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Header.
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 36
            color: Theme.tableHeaderBg
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.spacing.lg
                anchors.rightMargin: Theme.spacing.lg
                spacing: Theme.spacing.md
                Repeater {
                    model: table.columns
                    delegate: Text {
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.preferredWidth: modelData.weight !== undefined ? modelData.weight : 1
                        text: modelData.title !== undefined ? modelData.title : ""
                        color: Theme.mutedTextCaption
                        font.family: Theme.typography.sans
                        font.pixelSize: Theme.typography.eyebrow
                        elide: Text.ElideRight
                    }
                }
            }
        }

        // Body.
        ListView {
            id: list
            objectName: "rowsList"
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: table.model
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            // Row entrance (C2): populate + add Transitions (the KioskMain
            // idiom), NOT a per-delegate Component.onCompleted animation
            // (SearchScreen's approach) — populate re-fires on a genuine
            // model reset (VisitLogRowsModel::setRows() always does
            // beginResetModel/endResetModel, so every Students/Guests and
            // Today/Week toggle re-staggers the fresh row set), while
            // delegates created purely by scrolling do NOT replay it, so
            // paging through a long week view doesn't re-fade rows that
            // merely scroll into view. Gated by animateRows (opt-in default
            // false) AND Theme.motion.enabled (reduced motion: items appear
            // at their natural state, no transition — the correct pattern for
            // the Transition mechanism, unlike the duration-zeroing explicit
            // SequentialAnimations need). No opacity:0 default is set on the
            // delegate itself, so a row that never runs through an enabled
            // Transition renders at its natural opacity (1) immediately.
            populate: Transition {
                enabled: table.animateRows && Theme.motion.enabled
                // ViewTransition.index/.item only resolve to real per-item
                // values when read directly here, at the Transition's own
                // top level (confirmed empirically: even one level of
                // nesting — a plain property declared ON the child
                // SequentialAnimation, or a binding inside the nested
                // PauseAnimation/NumberAnimation itself — freezes at the
                // unset sentinel, index -1/item null, for every item). The
                // fix: read them here, then IMPERATIVELY assign (not bind)
                // the computed numbers onto the per-item PauseAnimation/
                // NumberAnimation instances by id. Each transitioning item
                // gets its own clone of this whole tree, so `idx`'s onChanged
                // handler and the ids it assigns into all belong to THAT
                // item's own clone — a plain assignment (unlike a live
                // binding) sticks for that clone regardless of what `idx`
                // does for later items in the same populate batch.
                property int idx: ViewTransition.index
                onIdxChanged: {
                    populatePause.duration = Math.max(0, Math.min(idx, Theme.motion.staggerCap)) * Theme.motion.rowStagger;
                    populateY.from = (ViewTransition.item ? ViewTransition.item.y : 0) + 10;
                }
                SequentialAnimation {
                    PauseAnimation { id: populatePause; duration: 0 }
                    ParallelAnimation {
                        NumberAnimation {
                            property: "opacity"; from: 0; to: 1
                            duration: Theme.motion.rowIn
                            easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.motion.easing
                        }
                        NumberAnimation {
                            id: populateY
                            property: "y"; from: 0
                            duration: Theme.motion.rowIn
                            easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.motion.easing
                        }
                    }
                }
            }
            add: Transition {
                enabled: table.animateRows && Theme.motion.enabled
                property int idx: ViewTransition.index
                onIdxChanged: {
                    addPause.duration = Math.max(0, Math.min(idx, Theme.motion.staggerCap)) * Theme.motion.rowStagger;
                    addY.from = (ViewTransition.item ? ViewTransition.item.y : 0) + 10;
                }
                SequentialAnimation {
                    PauseAnimation { id: addPause; duration: 0 }
                    ParallelAnimation {
                        NumberAnimation {
                            property: "opacity"; from: 0; to: 1
                            duration: Theme.motion.rowIn
                            easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.motion.easing
                        }
                        NumberAnimation {
                            id: addY
                            property: "y"; from: 0
                            duration: Theme.motion.rowIn
                            easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.motion.easing
                        }
                    }
                }
            }

            delegate: Rectangle {
                id: rowDelegate
                objectName: "tableRow_" + index
                required property var model
                required property int index
                width: ListView.view ? ListView.view.width : 0
                implicitHeight: 40
                color: hover.hovered ? Qt.alpha(Theme.brand.admin, 0.06)
                                     : (index % 2 === 0 ? Theme.card : Theme.rowHairline)
                // C3: row hover glide. Unconditional (not behind animateRows)
                // — imperceptible in tests, duration-gated by
                // Theme.motion.enabled like every other explicit animation.
                Behavior on color {
                    ColorAnimation { duration: Theme.motion.enabled ? Theme.motion.hoverLift : 0 }
                }

                HoverHandler { id: hover }
                // HoverHandler.hovered is not a QQuickItem and is invisible to
                // findChild() — expose it as a plain property for tests.
                readonly property bool isHovered: hover.hovered

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.spacing.lg
                    anchors.rightMargin: Theme.spacing.lg
                    spacing: Theme.spacing.md
                    Repeater {
                        model: table.columns
                        delegate: Text {
                            required property var modelData
                            Layout.fillWidth: true
                            Layout.preferredWidth: modelData.weight !== undefined ? modelData.weight : 1
                            text: {
                                var v = rowDelegate.model[modelData.key];
                                return v !== undefined && v !== null ? v : "";
                            }
                            color: Theme.text
                            font.family: Theme.typography.sans
                            font.pixelSize: Theme.typography.body
                            elide: Text.ElideRight
                        }
                    }
                }
            }
        }
    }

    // Empty state.
    Text {
        anchors.centerIn: parent
        visible: table.emptyVisible
        text: table.emptyStateText
        color: Theme.mutedText
        font.family: Theme.typography.sans
        font.pixelSize: Theme.typography.body
    }

    Accessible.role: Accessible.Table
}
