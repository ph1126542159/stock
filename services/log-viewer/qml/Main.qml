import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window
    width: 1480
    height: 920
    visible: false
    title: localizationController.tr("window.title")
    color: "#0d131a"
    flags: Qt.Tool | Qt.FramelessWindowHint

    QtObject {
        id: theme
        readonly property color background: "#0d131a"
        readonly property color panel: "#121b24"
        readonly property color panelRaised: "#182430"
        readonly property color border: "#2a3847"
        readonly property color text: "#edf4ff"
        readonly property color muted: "#97a9bc"
        readonly property color accent: "#58b7ff"
        readonly property color accentSoft: "#14354d"
        readonly property color scrollbarTrack: "#0a1118"
    }

    component StyledScrollBar : ScrollBar {
        padding: 0
        width: 0
        height: 0
        implicitWidth: 0
        implicitHeight: 0
        opacity: 0
        visible: false
        enabled: false
        interactive: false
        policy: ScrollBar.AlwaysOff
        background: Item {}
        contentItem: Item {}
    }

    background: Rectangle { color: theme.background }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 14

        Rectangle {
            Layout.preferredWidth: 320
            Layout.fillHeight: true
            radius: 8
            color: theme.panel
            border.color: theme.border

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 18
                spacing: 12

                Text {
                    text: localizationController.tr("window.title")
                    color: theme.text
                    font.family: "Segoe UI Variable"
                    font.pixelSize: 32
                    font.bold: true
                }

                Text {
                    text: logViewerController.status
                    color: theme.accent
                    font.family: "Segoe UI Variable"
                    font.pixelSize: 15
                    wrapMode: Text.WordWrap
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 8
                    color: theme.panelRaised
                    border.color: theme.border

                    ListView {
                        anchors.fill: parent
                        anchors.margins: 10
                        clip: true
                        spacing: 8
                        model: logViewerController.processModel

                        delegate: Rectangle {
                            width: ListView.view.width
                            implicitHeight: 78
                            radius: 8
                            color: active ? theme.accentSoft : theme.panel
                            border.color: active ? theme.accent : theme.border

                            MouseArea {
                                anchors.fill: parent
                                onClicked: logViewerController.selectProcess(index)
                            }

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 14
                                spacing: 4

                                Text {
                                    text: name
                                    color: theme.text
                                    font.family: "Segoe UI Variable"
                                    font.pixelSize: 18
                                    font.bold: true
                                    elide: Text.ElideRight
                                }

                                Text {
                                    text: level + " | " + preview
                                    color: theme.muted
                                    font.family: "Segoe UI Variable"
                                    font.pixelSize: 13
                                    maximumLineCount: 2
                                    wrapMode: Text.WordWrap
                                    elide: Text.ElideRight
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
            radius: 8
            color: theme.panel
            border.color: theme.border

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 18
                spacing: 12

                Text {
                    text: logViewerController.activeProcessName.length > 0
                        ? logViewerController.activeProcessName
                        : localizationController.tr("lv.placeholder.waitingForProcess")
                    color: theme.text
                    font.family: "Segoe UI Variable"
                    font.pixelSize: 30
                    font.bold: true
                }

                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    background: Rectangle { color: "transparent" }
                    ScrollBar.vertical: StyledScrollBar { policy: ScrollBar.AlwaysOff; visible: false }
                    ScrollBar.horizontal: StyledScrollBar { policy: ScrollBar.AlwaysOff; visible: false }

                    TextArea {
                        readOnly: true
                        wrapMode: Text.WrapAnywhere
                        text: logViewerController.activeLogText
                        color: theme.text
                        selectionColor: theme.accentSoft
                        selectedTextColor: theme.text
                        font.family: "Consolas"
                        font.pixelSize: 14
                        background: Rectangle {
                            radius: 8
                            color: theme.panelRaised
                            border.color: theme.border
                        }
                    }
                }
            }
        }
    }
}
