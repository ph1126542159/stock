import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

ApplicationWindow {
    id: window
    width: 1680
    height: 960
    minimumWidth: 1440
    minimumHeight: 760
    flags: Qt.Window | Qt.FramelessWindowHint
    visible: true
    title: localizationController.tr("app.title")
    color: theme.page

    Component.onCompleted: shellController.attachShellWindow(window)

    QtObject {
        id: theme
        readonly property color page: "#0b1118"
        readonly property color chrome: "#0d141d"
        readonly property color ink: "#f7fbff"
        readonly property color text: "#e8eef8"
        readonly property color muted: "#8fa0b5"
        readonly property color faint: "#64748b"
        readonly property color line: "#223044"
        readonly property color softLine: "#182434"
        readonly property color nav: "#08101c"
        readonly property color nav2: "#111c2b"
        readonly property color navText: "#eef6ff"
        readonly property color navMuted: "#91a4bd"
        readonly property color accent: "#4ea1ff"
        readonly property color accentSoft: "#132b45"
        readonly property color green: "#19c58a"
        readonly property color danger: "#dc3e42"
        readonly property color terminal: "#07111f"
    }

    component WindowButton: Button {
        id: control
        property color normalColor: "transparent"
        property color hoverColor: "#162235"
        property color pressColor: "#1e3048"
        property color textColor: theme.text
        implicitWidth: 42
        implicitHeight: 32
        flat: true
        padding: 0
        font.family: "Segoe UI"
        font.pixelSize: 15
        background: Rectangle {
            radius: 7
            color: control.down ? control.pressColor : (control.hovered ? control.hoverColor : control.normalColor)
        }
        contentItem: Text {
            text: control.text
            color: control.textColor
            font: control.font
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    component NavButton: Rectangle {
        id: navButton
        required property string itemId
        required property string label
        required property string desc
        required property bool selected
        required property bool enabledState
        signal clicked()

        width: parent ? parent.width : 228
        height: 54
        radius: 8
        color: selected ? theme.accentSoft : (mouse.containsMouse ? theme.nav2 : "transparent")
        border.color: selected ? "#2d75c8" : "transparent"
        opacity: enabledState ? 1.0 : 0.62

        MouseArea {
            id: mouse
            anchors.fill: parent
            hoverEnabled: true
            enabled: navButton.enabledState
            cursorShape: navButton.enabledState ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked: navButton.clicked()
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            spacing: 11

            Rectangle {
                Layout.preferredWidth: 3
                Layout.preferredHeight: 28
                radius: 2
                color: navButton.selected ? theme.accent : "transparent"
            }

            Rectangle {
                Layout.preferredWidth: 9
                Layout.preferredHeight: 9
                radius: 5
                color: navButton.selected ? theme.accent : (navButton.enabledState ? theme.green : theme.navMuted)
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Text {
                    Layout.fillWidth: true
                    text: navButton.label
                    color: navButton.selected ? "#ffffff" : theme.navText
                    font.family: "Segoe UI"
                    font.pixelSize: 15
                    font.bold: navButton.selected
                    elide: Text.ElideRight
                }
                Text {
                    Layout.fillWidth: true
                    text: navButton.desc
                    color: navButton.selected ? "#b9d7ff" : theme.navMuted
                    font.family: "Segoe UI"
                    font.pixelSize: 11
                    elide: Text.ElideRight
                    visible: navButton.desc.length > 0
                }
            }
        }
    }

    component LogRow: Rectangle {
        id: logRow
        required property string label
        required property bool selected
        signal clicked()
        signal pointerEntered()
        signal pointerExited()

        width: parent ? parent.width : 204
        height: 32
        radius: 7
        color: selected ? theme.accentSoft : (mouse.containsMouse ? "#17263b" : "transparent")

        MouseArea {
            id: mouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: logRow.clicked()
            onEntered: logRow.pointerEntered()
            onExited: logRow.pointerExited()
        }

        Text {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 8
            verticalAlignment: Text.AlignVCenter
            text: logRow.label
            color: logRow.selected ? "#ffffff" : theme.navText
            font.family: "Segoe UI"
            font.pixelSize: 12
            elide: Text.ElideRight
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            id: titleBar
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            visible: true
            color: theme.chrome
            border.color: theme.softLine
            z: 100

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                onPressed: window.startSystemMove()
                onDoubleClicked: window.visibility === Window.Maximized ? window.showNormal() : window.showMaximized()
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 18
                anchors.rightMargin: 8
                spacing: 12

                Rectangle {
                    Layout.preferredWidth: 24
                    Layout.preferredHeight: 24
                    radius: 7
                    color: theme.accent
                    Text {
                        anchors.centerIn: parent
                        text: "S"
                        color: "#ffffff"
                        font.family: "Segoe UI"
                        font.pixelSize: 14
                        font.bold: true
                    }
                }

                Text {
                    text: localizationController.tr("app.title")
                    color: theme.text
                    font.family: "Segoe UI"
                    font.pixelSize: 15
                    font.bold: true
                }

                Rectangle { Layout.preferredWidth: 1; Layout.preferredHeight: 18; color: theme.softLine }

                Text {
                    Layout.fillWidth: true
                    text: shellController.logConsoleActive
                        ? localizationController.tr("log.prefix") + shellController.activeLogProcessName
                        : shellController.activeProcessTitle
                    color: theme.muted
                    font.family: "Segoe UI"
                    font.pixelSize: 13
                    elide: Text.ElideRight
                }

                WindowButton {
                    id: languageToggle
                    text: localizationController.toggleLabel
                    implicitWidth: 36
                    onClicked: localizationController.toggle()
                }

                WindowButton {
                    text: "-"
                    onClicked: window.showMinimized()
                }
                WindowButton {
                    text: window.visibility === Window.Maximized ? "❐" : "□"
                    onClicked: window.visibility === Window.Maximized ? window.showNormal() : window.showMaximized()
                }
                WindowButton {
                    text: "×"
                    hoverColor: "#fde8e8"
                    pressColor: "#f8caca"
                    textColor: hovered ? theme.danger : theme.text
                    onClicked: shellController.requestManagedShutdown()
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Rectangle {
                Layout.preferredWidth: 248
                Layout.fillHeight: true
                color: theme.nav

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 16

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        Text {
                            Layout.fillWidth: true
                            text: localizationController.tr("nav.header")
                            color: theme.navText
                            font.family: "Segoe UI"
                            font.pixelSize: 24
                            font.bold: true
                            elide: Text.ElideRight
                        }
                        Text {
                            Layout.fillWidth: true
                            text: localizationController.tr("nav.subheader")
                            color: theme.navMuted
                            font.family: "Segoe UI"
                            font.pixelSize: 12
                            elide: Text.ElideRight
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 54
                        radius: 8
                        color: "#0f1b2d"
                        border.color: "#203757"
                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 10
                            Rectangle {
                                Layout.preferredWidth: 8
                                Layout.preferredHeight: 8
                                radius: 4
                                color: theme.green
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                Text {
                                    Layout.fillWidth: true
                                    text: shellController.statusLine
                                    color: theme.navText
                                    font.family: "Segoe UI"
                                    font.pixelSize: 12
                                    font.bold: true
                                    elide: Text.ElideRight
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: shellController.feedState
                                    color: theme.navMuted
                                    font.family: "Segoe UI"
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }

                    Flickable {
                        id: navFlick
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        contentWidth: width
                        contentHeight: navColumn.implicitHeight
                        boundsBehavior: Flickable.StopAtBounds

                        Column {
                            id: navColumn
                            width: navFlick.width
                            spacing: 6

                            Repeater {
                                model: shellController.processMenuModel
                                delegate: NavButton {
                                    required property int index
                                    required property string menuId
                                    required property string title
                                    required property string description
                                    required property bool active
                                    required property bool running
                                    itemId: menuId
                                    label: title
                                    desc: description
                                    selected: active && !shellController.logConsoleActive
                                    enabledState: menuId !== "log-viewer"
                                    visible: menuId !== "log-viewer"
                                    height: visible ? 54 : 0
                                    onClicked: shellController.activateProcessMenu(index)
                                }
                            }

                            Rectangle {
                                id: logBlock
                                property bool hoverHeld: false
                                property bool expanded: hoverHeld
                                function keepOpen() {
                                    hoverHeld = true
                                    logCollapseTimer.stop()
                                }
                                function scheduleClose() {
                                    logCollapseTimer.restart()
                                }
                                width: parent.width
                                height: 44 + (expanded ? logItems.implicitHeight + 8 : 0)
                                radius: 8
                                color: shellController.logConsoleActive ? theme.accentSoft : (hoverHeld ? theme.nav2 : "transparent")
                                clip: true

                                Timer {
                                    id: logCollapseTimer
                                    interval: 650
                                    repeat: false
                                    onTriggered: logBlock.hoverHeld = false
                                }

                                MouseArea {
                                    id: logHover
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    acceptedButtons: Qt.NoButton
                                    onEntered: logBlock.keepOpen()
                                    onExited: logBlock.scheduleClose()
                                }

                                Column {
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    spacing: 6

                                    RowLayout {
                                        width: parent.width
                                        height: 28
                                        spacing: 10
                                        Rectangle {
                                            Layout.preferredWidth: 3
                                            Layout.preferredHeight: 22
                                            radius: 2
                                            color: shellController.logConsoleActive ? theme.accent : "transparent"
                                        }
                                        Text {
                                            Layout.fillWidth: true
                                            text: localizationController.tr("log.label")
                                            color: shellController.logConsoleActive ? "#ffffff" : theme.navText
                                            font.family: "Segoe UI"
                                            font.pixelSize: 15
                                            font.bold: shellController.logConsoleActive
                                            elide: Text.ElideRight
                                        }
                                        Text {
                                            text: logBlock.expanded ? "⌃" : "⌄"
                                            color: shellController.logConsoleActive ? theme.muted : theme.navMuted
                                            font.family: "Segoe UI"
                                            font.pixelSize: 14
                                        }
                                    }

                                    Column {
                                        id: logItems
                                        width: parent.width
                                        visible: logBlock.expanded
                                        spacing: 4
                                        Repeater {
                                            model: shellController.logProcessMenuModel
                                            delegate: LogRow {
                                                required property int index
                                                required property string title
                                                required property bool active
                                                label: title
                                                selected: active && shellController.logConsoleActive
                                                onPointerEntered: logBlock.keepOpen()
                                                onPointerExited: logBlock.scheduleClose()
                                                onClicked: shellController.activateLogProcess(index)
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: theme.page

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 12

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 48
                        spacing: 12

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3
                            Text {
                                Layout.fillWidth: true
                                text: shellController.logConsoleActive ? localizationController.tr("log.title.live") : shellController.activeProcessTitle
                                color: theme.text
                                font.family: "Segoe UI"
                                font.pixelSize: 25
                                font.bold: true
                                elide: Text.ElideRight
                            }
                            Text {
                                Layout.fillWidth: true
                                text: shellController.logConsoleActive
                                    ? localizationController.tr("log.subtitle.live")
                                    : shellController.activeProcessDescription
                                color: theme.muted
                                font.family: "Segoe UI"
                                font.pixelSize: 13
                                elide: Text.ElideRight
                            }
                        }

                        Rectangle {
                            Layout.preferredWidth: 126
                            Layout.preferredHeight: 34
                            radius: 8
                            color: theme.accentSoft
                            border.color: "#285d99"
                            Text {
                                anchors.centerIn: parent
                                text: shellController.logConsoleActive ? localizationController.tr("log.mode") : shellController.activeProcessStatus
                                color: theme.accent
                                font.family: "Segoe UI"
                                font.pixelSize: 13
                                font.bold: true
                                elide: Text.ElideRight
                            }
                        }
                    }

                    Rectangle {
                        id: contentFrame
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: 10
                        color: "#0f1723"
                        border.color: theme.line
                        clip: true

                        Item {
                            id: nativeHostSurface
                            anchors.fill: parent
                            visible: !shellController.logConsoleActive && shellController.activeProcessReady

                            function syncNativeArea() {
                                // Coordinates relative to the shell window's content item,
                                // which matches the parent HWND's client area.
                                var root = window.contentItem
                                var point = contentFrame.mapToItem(root, 0, 0)
                                shellController.setHostedWindowArea(
                                    Math.round(point.x),
                                    Math.round(point.y),
                                    Math.round(contentFrame.width),
                                    Math.round(contentFrame.height))
                            }

                            Component.onCompleted: syncNativeArea()
                            onWidthChanged: syncNativeArea()
                            onHeightChanged: syncNativeArea()
                            onXChanged: syncNativeArea()
                            onYChanged: syncNativeArea()

                            Timer {
                                interval: 250
                                running: nativeHostSurface.visible
                                repeat: true
                                onTriggered: nativeHostSurface.syncNativeArea()
                            }
                        }

                        Connections {
                            target: shellController
                            function onActiveProcessWindowChanged() {}
                            function onLogConsoleChanged() {}
                        }

                        Rectangle {
                            anchors.fill: parent
                            visible: shellController.logConsoleActive
                            color: theme.terminal

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 18
                                spacing: 12

                                Text {
                                    Layout.fillWidth: true
                                    text: shellController.activeLogProcessName.length > 0
                                        ? shellController.activeLogProcessName
                                        : localizationController.tr("log.empty")
                                    color: "#eff6ff"
                                    font.family: "Segoe UI"
                                    font.pixelSize: 20
                                    font.bold: true
                                    elide: Text.ElideRight
                                }

                                TextArea {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    readOnly: true
                                    wrapMode: Text.WrapAnywhere
                                    text: shellController.activeLogText
                                    color: "#d8f3ff"
                                    selectionColor: "#1f4b6b"
                                    selectedTextColor: "#ffffff"
                                    font.family: "Cascadia Mono"
                                    font.pixelSize: 13
                                    background: Rectangle {
                                        radius: 8
                                        color: "#0b1524"
                                        border.color: "#22314a"
                                    }
                                }
                            }
                        }

                        ColumnLayout {
                            anchors.centerIn: parent
                            visible: !shellController.logConsoleActive && !shellController.activeProcessReady
                            spacing: 10

                            Rectangle {
                                Layout.alignment: Qt.AlignHCenter
                                Layout.preferredWidth: 34
                                Layout.preferredHeight: 34
                                radius: 17
                                color: theme.accentSoft
                                border.color: "#c9d9ff"
                                Text {
                                    anchors.centerIn: parent
                                    text: "…"
                                    color: theme.accent
                                    font.pixelSize: 20
                                    font.bold: true
                                }
                            }
                            Text {
                                Layout.alignment: Qt.AlignHCenter
                                text: localizationController.tr("content.loading")
                                color: theme.muted
                                font.family: "Segoe UI"
                                font.pixelSize: 16
                            }
                        }
                    }
                }
            }
        }
    }
}
