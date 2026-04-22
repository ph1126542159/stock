import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window
    width: 1480
    height: 920
    minimumWidth: 1180
    minimumHeight: 760
    visible: false
    title: "stok"
    color: "#181a1f"
    readonly property real sidebarRatio: 2
    readonly property real contentRatio: 7
    readonly property real ratioTotal: sidebarRatio + contentRatio

    Component.onCompleted: showMaximized()

    QtObject {
        id: theme
        readonly property color background: "#181a1f"
        readonly property color panel: "#1f232a"
        readonly property color panelAlt: "#252930"
        readonly property color border: "#30363d"
        readonly property color text: "#d7dae0"
        readonly property color muted: "#8b949e"
        readonly property color accent: "#0e639c"
        readonly property color positive: "#4ec9b0"
        readonly property color negative: "#f14c4c"
        readonly property color warning: "#d7ba7d"
    }

    background: Rectangle {
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#20242b" }
            GradientStop { position: 0.6; color: theme.background }
            GradientStop { position: 1.0; color: "#121417" }
        }
    }

    header: Rectangle {
        height: 72
        color: "#16181d"
        border.color: theme.border

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 24
            anchors.rightMargin: 24
            spacing: 18

            Rectangle {
                width: 46
                height: 46
                radius: 12
                color: theme.accent
                border.color: Qt.lighter(theme.accent, 1.3)

                Text {
                    anchors.centerIn: parent
                    text: "S"
                    color: "white"
                    font.family: "Cascadia Code"
                    font.pixelSize: 24
                    font.bold: true
                }
            }

            ColumnLayout {
                spacing: 2

                Text {
                    text: "stok desktop shell"
                    color: theme.text
                    font.family: "Segoe UI Variable"
                    font.pixelSize: 24
                    font.bold: true
                }

                Text {
                    text: "Qt6 + QML container / VS Code inspired workspace"
                    color: theme.muted
                    font.family: "Segoe UI Variable"
                    font.pixelSize: 13
                }
            }

            Item { Layout.fillWidth: true }

            Rectangle {
                radius: 14
                color: "#20262d"
                border.color: theme.border
                implicitWidth: 220
                implicitHeight: 36

                Text {
                    anchors.centerIn: parent
                    text: shellController.telemetryMode
                    color: theme.text
                    font.family: "Segoe UI Variable"
                    font.pixelSize: 13
                }
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.topMargin: header.height
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredWidth: window.width * window.sidebarRatio / window.ratioTotal
            Layout.fillHeight: true
            color: "#171a20"
            border.color: theme.border

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 16

                Text {
                    text: "Workspace"
                    color: theme.muted
                    font.family: "Cascadia Code"
                    font.pixelSize: 12
                    font.letterSpacing: 1.2
                }

                Rectangle {
                    Layout.fillWidth: true
                    radius: 14
                    color: theme.panel
                    border.color: theme.border
                    implicitHeight: 116

                    Column {
                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 10

                        Text {
                            text: shellController.serviceName
                            color: theme.text
                            font.family: "Segoe UI Variable"
                            font.pixelSize: 20
                            font.bold: true
                        }

                        Text {
                            text: "Feed: " + shellController.feedState
                            color: theme.text
                            font.family: "Cascadia Code"
                            font.pixelSize: 13
                        }

                        Text {
                            text: "DDS Topic: " + shellController.topicName
                            color: theme.muted
                            font.family: "Cascadia Code"
                            font.pixelSize: 12
                            wrapMode: Text.WrapAnywhere
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    radius: 14
                    color: theme.panelAlt
                    border.color: theme.border
                    implicitHeight: 180

                    Column {
                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 12

                        Text {
                            text: "Runtime Diagnostics"
                            color: theme.text
                            font.family: "Segoe UI Variable"
                            font.pixelSize: 16
                            font.bold: true
                        }

                        Text {
                            text: "Matched publishers: " + shellController.matchedPublishers
                            color: theme.text
                            font.family: "Cascadia Code"
                            font.pixelSize: 13
                        }

                        Text {
                            text: "Quotes tracked: " + shellController.quoteCount
                            color: theme.text
                            font.family: "Cascadia Code"
                            font.pixelSize: 13
                        }

                        Text {
                            text: "Last route: " + shellController.lastUpdated
                            color: theme.muted
                            font.family: "Cascadia Code"
                            font.pixelSize: 12
                            wrapMode: Text.WrapAnywhere
                        }

                        Rectangle {
                            width: parent.width
                            height: 1
                            color: theme.border
                        }

                        Text {
                            text: shellController.statusLine
                            color: theme.warning
                            font.family: "Segoe UI Variable"
                            font.pixelSize: 13
                            wrapMode: Text.WordWrap
                        }
                    }
                }

                Item { Layout.fillHeight: true }

                Text {
                    text: "services/market-data-service -> Fast-DDS SHM -> services/desktop-shell"
                    color: theme.muted
                    font.family: "Cascadia Code"
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredWidth: window.width * window.contentRatio / window.ratioTotal
            Layout.fillHeight: true
            color: "transparent"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 22
                spacing: 18

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 16

                    Repeater {
                        model: [
                            { title: "Market Bus", value: shellController.feedState, accent: theme.accent },
                            { title: "Publishers", value: shellController.matchedPublishers, accent: theme.warning },
                            { title: "Watchlist", value: shellController.quoteCount, accent: theme.positive }
                        ]

                        delegate: Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: 108
                            radius: 16
                            color: theme.panel
                            border.color: theme.border

                            Column {
                                anchors.fill: parent
                                anchors.margins: 18
                                spacing: 12

                                Text {
                                    text: modelData.title
                                    color: theme.muted
                                    font.family: "Cascadia Code"
                                    font.pixelSize: 12
                                }

                                Text {
                                    text: modelData.value
                                    color: modelData.accent
                                    font.family: "Segoe UI Variable"
                                    font.pixelSize: 28
                                    font.bold: true
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 18
                    color: theme.panel
                    border.color: theme.border

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 14

                        RowLayout {
                            Layout.fillWidth: true

                            ColumnLayout {
                                spacing: 4

                                Text {
                                    text: "Live Watchlist"
                                    color: theme.text
                                    font.family: "Segoe UI Variable"
                                    font.pixelSize: 22
                                    font.bold: true
                                }

                                Text {
                                    text: "Synthetic stock feed routed from the launcher-managed service mesh."
                                    color: theme.muted
                                    font.family: "Segoe UI Variable"
                                    font.pixelSize: 13
                                }
                            }

                            Item { Layout.fillWidth: true }

                            Rectangle {
                                radius: 14
                                color: "#20262d"
                                border.color: theme.border
                                implicitWidth: 190
                                implicitHeight: 36

                                Text {
                                    anchors.centerIn: parent
                                    text: "Last status: " + shellController.feedState
                                    color: theme.text
                                    font.family: "Cascadia Code"
                                    font.pixelSize: 12
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: theme.border
                        }

                        ListView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            spacing: 8
                            model: shellController.quotesModel

                            delegate: Rectangle {
                                width: ListView.view.width
                                height: 82
                                radius: 14
                                color: index % 2 === 0 ? "#20242b" : "#1c2026"
                                border.color: theme.border

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: 16
                                    spacing: 18

                                    ColumnLayout {
                                        Layout.preferredWidth: 220
                                        spacing: 4

                                        Text {
                                            text: symbol
                                            color: theme.text
                                            font.family: "Cascadia Code"
                                            font.pixelSize: 19
                                            font.bold: true
                                        }

                                        Text {
                                            text: name + "  ·  " + market
                                            color: theme.muted
                                            font.family: "Segoe UI Variable"
                                            font.pixelSize: 12
                                            elide: Text.ElideRight
                                        }
                                    }

                                    Rectangle {
                                        width: 1
                                        Layout.fillHeight: true
                                        color: theme.border
                                    }

                                    ColumnLayout {
                                        Layout.preferredWidth: 160
                                        spacing: 4

                                        Text {
                                            text: live ? Number(price).toLocaleString(Qt.locale(), "f", 2) : "--"
                                            color: theme.text
                                            font.family: "Segoe UI Variable"
                                            font.pixelSize: 24
                                            font.bold: true
                                        }

                                        Text {
                                            text: timestampText
                                            color: theme.muted
                                            font.family: "Cascadia Code"
                                            font.pixelSize: 11
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.preferredWidth: 180
                                        spacing: 4

                                        Text {
                                            text: live ? ((change >= 0 ? "+" : "") + Number(change).toLocaleString(Qt.locale(), "f", 2)) : "Waiting"
                                            color: change >= 0 ? theme.positive : theme.negative
                                            font.family: "Cascadia Code"
                                            font.pixelSize: 18
                                            font.bold: true
                                        }

                                        Text {
                                            text: live ? ((percent >= 0 ? "+" : "") + Number(percent).toLocaleString(Qt.locale(), "f", 2) + "%") : "No flow"
                                            color: change >= 0 ? theme.positive : theme.negative
                                            font.family: "Cascadia Code"
                                            font.pixelSize: 12
                                        }
                                    }

                                    Item { Layout.fillWidth: true }

                                    ColumnLayout {
                                        Layout.alignment: Qt.AlignRight
                                        spacing: 6

                                        Rectangle {
                                            radius: 10
                                            color: live ? "#173028" : "#31363f"
                                            border.color: live ? theme.positive : theme.border
                                            implicitWidth: 88
                                            implicitHeight: 28

                                            Text {
                                                anchors.centerIn: parent
                                                text: live ? "STREAMING" : "IDLE"
                                                color: live ? theme.positive : theme.muted
                                                font.family: "Cascadia Code"
                                                font.pixelSize: 11
                                            }
                                        }

                                        Text {
                                            text: "Vol " + volume
                                            color: theme.muted
                                            font.family: "Cascadia Code"
                                            font.pixelSize: 11
                                            horizontalAlignment: Text.AlignRight
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
