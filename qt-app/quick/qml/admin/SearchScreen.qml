import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LOAMS

// Search (spec §6.2). Name/ID/course search, a department filter that
// re-scopes the course chips, active-filter pills, and live (debounced)
// type-to-search. Results render as ONE bordered container of hairline rows
// (not per-student cards) — a result-count header on top, then rows of
// avatar + identity + the frozen "Total Visits: N" label (never a bare
// number — spec §6.2 owner decision, since the backend has no per-month
// count).
Rectangle {
    id: screen
    property var vm
    property string selectedCourse: ""
    property string selectedDepartment: ""
    // Debounce interval for live type-to-search; a screen property (not a
    // hardcoded Timer literal) so tests can shrink it instead of waiting out
    // the real 300ms on every keystroke-driven test.
    property int debounceMs: 300
    // Distinguishes "no search run yet" from "search ran, zero rows" so the
    // empty state doesn't flash on first navigation to this page before the
    // user has typed or picked anything.
    property bool hasSearched: false

    // Introspection for tests: derived from the live model's count, not a
    // test-only mirror, so it cannot silently pass if vm/results is broken.
    readonly property int resultCount: vm && vm.results ? vm.results.count : 0
    readonly property bool isLoading: vm ? vm.loading : false
    // Ternary, not `vm && ...` bound directly — see errorBlock.visible below
    // for why that shape is unsafe on a bool-flavored property.
    readonly property bool isEmpty: (screen.hasSearched && !screen.isLoading)
        ? (vm ? (vm.errorText.length === 0 && screen.resultCount === 0) : false)
        : false

    // Active filter pills, derived from the two selections — never stored
    // state of its own, so it can't drift from what's actually applied.
    readonly property var activePills: {
        var p = [];
        if (screen.selectedDepartment !== "")
            p.push({ key: "department", label: qsTr("Department: %1").arg(screen.selectedDepartment) });
        if (screen.selectedCourse !== "")
            p.push({ key: "course", label: qsTr("Course: %1").arg(screen.selectedCourse) });
        return p;
    }

    color: Theme.appBackground

    function runSearch() {
        screen.hasSearched = true;
        if (vm) vm.search(queryField.text, screen.selectedCourse);
    }

    function selectDepartment(department) {
        screen.selectedDepartment = (screen.selectedDepartment === department) ? "" : department;
        if (vm) vm.setDepartment(screen.selectedDepartment);
        screen.runSearch();
    }

    function selectCourse(course) {
        screen.selectedCourse = (screen.selectedCourse === course) ? "" : course;
        screen.runSearch();
    }

    function clearDepartment() {
        screen.selectedDepartment = "";
        if (vm) vm.setDepartment("");
        screen.runSearch();
    }

    function clearCourse() {
        screen.selectedCourse = "";
        screen.runSearch();
    }

    function clearAllFilters() {
        screen.selectedDepartment = "";
        screen.selectedCourse = "";
        if (vm) vm.setDepartment("");
        screen.runSearch();
    }

    // Course-reset semantics (spec §6.2 / SearchViewModel::setDepartment doc):
    // SearchViewModel does not track "the selected course" itself — this
    // screen owns `selectedCourse` and is responsible for reconciling it
    // whenever `courses` is re-scoped out from under it (a department change,
    // or refresh()). Without this, a stale course chip could stay visually
    // "active" and keep being sent to search() even though it no longer
    // exists under the new department.
    Connections {
        target: vm
        function onCoursesChanged() {
            if (!vm) return;
            if (screen.selectedCourse !== "" && vm.courses.indexOf(screen.selectedCourse) === -1) {
                screen.selectedCourse = "";
                screen.runSearch();
            }
        }
    }

    Timer {
        id: debounceTimer
        objectName: "debounceTimer"
        interval: screen.debounceMs
        onTriggered: screen.runSearch()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacing.xxl
        spacing: Theme.spacing.lg

        // --- Filter card: query, department, course, active pills ---
        Rectangle {
            Layout.fillWidth: true
            color: Theme.card
            radius: Theme.radius.card
            border.width: 2
            border.color: Theme.border
            implicitHeight: filterColumn.implicitHeight + Theme.spacing.xl * 2

            ColumnLayout {
                id: filterColumn
                anchors.fill: parent
                anchors.margins: Theme.spacing.xl
                spacing: Theme.spacing.md

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacing.md
                    TextField {
                        id: queryField
                        objectName: "queryField"
                        Layout.fillWidth: true
                        placeholderText: qsTr("Search by name, ID, or course")
                        font.family: Theme.typography.sans
                        font.pixelSize: Theme.typography.body
                        color: Theme.text
                        background: Rectangle {
                            radius: Theme.radius.sm
                            color: Theme.card
                            border.width: 1
                            border.color: Theme.border
                        }
                        onTextChanged: debounceTimer.restart()
                        onAccepted: { debounceTimer.stop(); screen.runSearch(); }
                    }
                    LButton {
                        objectName: "searchButton"
                        text: qsTr("Search")
                        onClicked: { debounceTimer.stop(); screen.runSearch(); }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacing.xs
                    Text {
                        text: qsTr("Department")
                        color: Theme.mutedTextCaption
                        font.family: Theme.typography.sans
                        font.pixelSize: Theme.typography.eyebrow
                        font.weight: Font.ExtraBold
                    }
                    Flow {
                        Layout.fillWidth: true
                        spacing: Theme.spacing.sm
                        Repeater {
                            model: vm ? vm.departments : []
                            delegate: Rectangle {
                                id: deptChip
                                required property var modelData
                                objectName: "deptChip_" + modelData
                                radius: Theme.radius.pill
                                implicitHeight: 28
                                implicitWidth: deptChipText.implicitWidth + Theme.spacing.xl
                                readonly property bool active: screen.selectedDepartment === modelData
                                color: deptChip.active ? Theme.brand.admin : Theme.card
                                border.width: 2
                                border.color: deptChip.active ? Theme.brand.admin : Theme.border
                                Text {
                                    id: deptChipText
                                    anchors.centerIn: parent
                                    text: deptChip.modelData
                                    color: deptChip.active ? Theme.brand.onPrimary : Theme.text
                                    font.family: Theme.typography.sans
                                    font.pixelSize: Theme.typography.control
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: screen.selectDepartment(deptChip.modelData)
                                }
                            }
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacing.xs
                    Text {
                        text: qsTr("Course")
                        color: Theme.mutedTextCaption
                        font.family: Theme.typography.sans
                        font.pixelSize: Theme.typography.eyebrow
                        font.weight: Font.ExtraBold
                    }
                    Flow {
                        Layout.fillWidth: true
                        spacing: Theme.spacing.sm
                        Repeater {
                            model: vm ? vm.courses : []
                            delegate: Rectangle {
                                id: chip
                                required property var modelData
                                objectName: "chip_" + modelData
                                radius: Theme.radius.pill
                                implicitHeight: 28
                                implicitWidth: chipText.implicitWidth + Theme.spacing.xl
                                readonly property bool active: screen.selectedCourse === modelData
                                color: chip.active ? Theme.brand.admin : Theme.card
                                border.width: 2
                                border.color: chip.active ? Theme.brand.admin : Theme.border
                                Text {
                                    id: chipText
                                    anchors.centerIn: parent
                                    text: chip.modelData
                                    color: chip.active ? Theme.brand.onPrimary : Theme.text
                                    font.family: Theme.typography.sans
                                    font.pixelSize: Theme.typography.control
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: screen.selectCourse(chip.modelData)
                                }
                            }
                        }
                    }
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: Theme.spacing.sm
                    visible: screen.activePills.length > 0
                    Repeater {
                        model: screen.activePills
                        delegate: Rectangle {
                            id: pill
                            required property var modelData
                            objectName: "pill_" + modelData.key
                            radius: Theme.radius.pill
                            implicitHeight: 26
                            implicitWidth: pillRow.implicitWidth + Theme.spacing.lg
                            color: Qt.alpha(Theme.brand.admin, 0.10)
                            border.width: 1
                            border.color: Theme.brand.admin
                            // Row (a positioner), not RowLayout: RowLayout's
                            // implicitWidth is not reliably available as a
                            // free-floating child outside an actual Layout,
                            // which left every pill/button at implicitWidth 0
                            // — Flow then stacked them all at the same (0,0)
                            // position, so clicks landed on whichever was
                            // topmost in paint order instead of the item
                            // findChild() had actually located.
                            Row {
                                id: pillRow
                                anchors.centerIn: parent
                                spacing: Theme.spacing.xs
                                Text {
                                    text: pill.modelData.label
                                    color: Theme.brand.admin
                                    font.family: Theme.typography.sans
                                    font.pixelSize: Theme.typography.eyebrow
                                    font.weight: Font.DemiBold
                                }
                                Text {
                                    text: "×"
                                    color: Theme.brand.admin
                                    font.family: Theme.typography.sans
                                    font.pixelSize: Theme.typography.control
                                    font.weight: Font.ExtraBold
                                }
                            }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    if (pill.modelData.key === "department") screen.clearDepartment();
                                    else screen.clearCourse();
                                }
                            }
                        }
                    }
                    LButton {
                        objectName: "clearAllButton"
                        visible: screen.activePills.length > 0
                        variant: "Ghost"
                        compact: true
                        text: qsTr("Clear all")
                        onClicked: screen.clearAllFilters()
                    }
                }
            }
        }

        // --- Results: one bordered container, count header + hairline rows ---
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Theme.card
            radius: Theme.radius.card
            border.width: 2
            border.color: Theme.border
            clip: true

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: resultCountHeader.implicitHeight + Theme.spacing.lg * 2
                    color: "transparent"
                    Text {
                        id: resultCountHeader
                        objectName: "resultCountHeader"
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: Theme.spacing.xl2
                        text: qsTr("%1 results").arg(screen.resultCount)
                        color: Theme.mutedTextCaption
                        font.family: Theme.typography.sans
                        font.pixelSize: Theme.typography.control
                        font.weight: Font.ExtraBold
                    }
                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: 1
                        color: Theme.rowHairline
                    }
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    ListView {
                        id: resultsList
                        objectName: "resultsList"
                        anchors.fill: parent
                        clip: true
                        visible: !screen.isLoading && !screen.isEmpty
                        model: vm ? vm.results : null
                        delegate: Rectangle {
                            id: rowDelegate
                            required property var model
                            required property int index
                            objectName: "resultRow_" + model.schoolId
                            width: ListView.view ? ListView.view.width : 0
                            implicitHeight: 64
                            // Starts invisible/offset; entranceAnim below is
                            // the ONLY thing that brings it to its resting
                            // state, so a row that is never animated stays
                            // invisible rather than silently "just working".
                            opacity: 0
                            color: rowHover.hovered ? Qt.alpha(Theme.brand.admin, 0.06) : "transparent"
                            Behavior on color { ColorAnimation { duration: Theme.motion.hoverLift } }
                            transform: Translate { id: rowTranslate; y: 10 }

                            HoverHandler { id: rowHover }

                            Rectangle {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                height: 1
                                color: Theme.rowHairline
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: Theme.spacing.xl2
                                anchors.rightMargin: Theme.spacing.xl2
                                spacing: Theme.spacing.md

                                // Circle avatar. No student photo data exists
                                // yet (spec owner decision, Phase 3:
                                // search_students.php returns no photo
                                // column) — initials-only, same chip pattern
                                // as the kiosk login feed.
                                Rectangle {
                                    Layout.preferredWidth: 40
                                    Layout.preferredHeight: 40
                                    Layout.alignment: Qt.AlignVCenter
                                    radius: width / 2
                                    color: Theme.brand.adminSoft
                                    Text {
                                        objectName: "avatarInitials"
                                        anchors.centerIn: parent
                                        text: model.initials
                                        color: Theme.brand.admin
                                        font.family: Theme.typography.sans
                                        font.pixelSize: Theme.typography.control
                                        font.weight: Font.ExtraBold
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Layout.alignment: Qt.AlignVCenter
                                    spacing: Theme.spacing.xs
                                    Text {
                                        Layout.fillWidth: true
                                        text: model.name
                                        color: Theme.text
                                        elide: Text.ElideRight
                                        font.family: Theme.typography.sans
                                        font.pixelSize: Theme.typography.cardTitle
                                        font.weight: Font.ExtraBold
                                    }
                                    // ID + course + department. Year level is
                                    // deliberately NOT shown here: on real
                                    // deployments year_level holds a section
                                    // name for the vast majority of students
                                    // and a numeric year for the rest — mixed
                                    // semantics that would mislead if rendered
                                    // as if it were always a year (same
                                    // reasoning that keeps it out of the
                                    // filters).
                                    Text {
                                        Layout.fillWidth: true
                                        text: qsTr("%1 · %2 · %3").arg(model.schoolId).arg(model.course).arg(model.department)
                                        color: Theme.mutedText
                                        elide: Text.ElideRight
                                        font.family: Theme.typography.sans
                                        font.pixelSize: Theme.typography.control
                                    }
                                }

                                // "Total Visits: N" — frozen label (spec
                                // §6.2). Kept as ONE string (not split into a
                                // caption line + a bare number) so it can
                                // never regress into looking like an
                                // unlabeled figure.
                                Text {
                                    objectName: "visitsLabel"
                                    Layout.alignment: Qt.AlignVCenter
                                    horizontalAlignment: Text.AlignRight
                                    text: qsTr("Total Visits: %1").arg(model.visits)
                                    color: Theme.text
                                    font.family: Theme.typography.sans
                                    font.pixelSize: Theme.typography.body
                                    font.weight: Font.ExtraBold
                                }
                            }

                            // Staggered entrance: fade + rise, per-row delay
                            // capped at 10 rows so a large result set doesn't
                            // take seconds to finish appearing. Clamped to 0
                            // at the low end too: a delegate's `index`
                            // transiently becomes -1 while it's being torn
                            // down as part of a model reset (e.g. a fresh
                            // search clearing the previous rows), and this
                            // binding re-evaluates during that teardown —
                            // without the max(0, ...) that would feed
                            // PauseAnimation a negative duration.
                            SequentialAnimation {
                                id: entranceAnim
                                PauseAnimation { duration: Math.max(0, Math.min(rowDelegate.index, 10)) * 25 }
                                ParallelAnimation {
                                    NumberAnimation {
                                        target: rowDelegate; property: "opacity"; to: 1
                                        duration: Theme.motion.rowIn; easing.type: Easing.OutCubic
                                    }
                                    NumberAnimation {
                                        target: rowTranslate; property: "y"; to: 0
                                        duration: Theme.motion.rowIn; easing.type: Easing.OutCubic
                                    }
                                }
                            }
                            Component.onCompleted: entranceAnim.start()
                        }
                    }

                    // Skeleton rows while loading — placeholder bars that
                    // preserve the row layout, instead of a centered spinner
                    // that jumps the content once results land.
                    Column {
                        id: skeletonColumn
                        objectName: "skeletonColumn"
                        anchors.fill: parent
                        visible: screen.isLoading
                        Repeater {
                            model: 5
                            delegate: Rectangle {
                                width: skeletonColumn.width
                                implicitHeight: 64
                                color: "transparent"
                                Rectangle {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    height: 1
                                    color: Theme.rowHairline
                                }
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: Theme.spacing.xl2
                                    anchors.rightMargin: Theme.spacing.xl2
                                    spacing: Theme.spacing.md
                                    Rectangle {
                                        Layout.preferredWidth: 40; Layout.preferredHeight: 40
                                        radius: 20
                                        color: Qt.alpha(Theme.mutedTextCaption, 0.25)
                                    }
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: Theme.spacing.xs
                                        Rectangle { Layout.preferredWidth: 160; Layout.preferredHeight: 14; radius: 4; color: Qt.alpha(Theme.mutedTextCaption, 0.25) }
                                        Rectangle { Layout.preferredWidth: 220; Layout.preferredHeight: 10; radius: 4; color: Qt.alpha(Theme.mutedTextCaption, 0.18) }
                                    }
                                    Rectangle { Layout.preferredWidth: 70; Layout.preferredHeight: 14; radius: 4; color: Qt.alpha(Theme.mutedTextCaption, 0.25) }
                                }
                            }
                        }
                    }

                    // Empty state — distinct from the error state (errorBlock
                    // below), shown only once a search has actually run and
                    // came back with zero rows.
                    ColumnLayout {
                        id: emptyState
                        objectName: "emptyState"
                        anchors.centerIn: parent
                        visible: screen.isEmpty
                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: qsTr("No students found")
                            color: Theme.mutedText
                            font.family: Theme.typography.sans
                            font.pixelSize: Theme.typography.body
                        }
                    }
                }
            }
        }
    }

    // Error + retry.
    ColumnLayout {
        id: errorBlock
        objectName: "errorBlock"
        anchors.centerIn: parent
        // Ternary, not `vm && ...`: with no vm the latter evaluates to
        // undefined, and a binding yielding undefined leaves a bool property
        // at its default — which for `visible` is true, flashing an empty
        // error block with a Retry button over the screen.
        visible: vm ? vm.errorText.length > 0 : false
        spacing: Theme.spacing.md
        Text { text: vm ? vm.errorText : ""; color: Theme.error
               font.family: Theme.typography.sans; font.pixelSize: Theme.typography.body }
        LButton { objectName: "retryButton"; text: qsTr("Retry"); onClicked: screen.runSearch() }
    }
}
