import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window
    width: 1280
    height: 860
    visible: false
    title: localizationController.tr("window.title")
    color: theme.background
    flags: Qt.Tool | Qt.FramelessWindowHint

    function toneColor(tone) {
        if (tone === "green") return theme.green
        if (tone === "amber") return theme.amber
        if (tone === "red") return theme.red
        return theme.accent
    }

    QtObject {
        id: theme
        readonly property color background: "#090f17"
        readonly property color panel: "#101926"
        readonly property color panel2: "#0d1622"
        readonly property color line: "#24354a"
        readonly property color text: "#eef6ff"
        readonly property color muted: "#8fa6bd"
        readonly property color faint: "#5d7289"
        readonly property color accent: "#4ea1ff"
        readonly property color green: "#20d69b"
        readonly property color amber: "#f2b84b"
        readonly property color red: "#ff6b6b"
    }

    component MetricCard: Rectangle {
        required property var item
        radius: 8
        color: theme.panel
        border.color: theme.line
        implicitHeight: 104
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 6
            Text { text: item.label; color: theme.muted; font.pixelSize: 13; Layout.fillWidth: true; elide: Text.ElideRight }
            Text { text: item.value; color: toneColor(item.tone); font.pixelSize: 28; font.bold: true; Layout.fillWidth: true; elide: Text.ElideRight }
            Text { text: item.note; color: theme.faint; font.pixelSize: 12; Layout.fillWidth: true; elide: Text.ElideRight }
        }
    }

    component SourceCard: Rectangle {
        required property var item
        radius: 8
        color: theme.panel2
        border.color: theme.line
        implicitHeight: 128
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 15
            spacing: 8
            RowLayout {
                Layout.fillWidth: true
                Text { text: item.name; color: theme.text; font.pixelSize: 18; font.bold: true; Layout.fillWidth: true; elide: Text.ElideRight }
                Rectangle { Layout.preferredWidth: 9; Layout.preferredHeight: 9; radius: 5; color: toneColor(item.tone) }
            }
            Text { text: item.type; color: theme.muted; font.pixelSize: 13; Layout.fillWidth: true; elide: Text.ElideRight }
            RowLayout {
                Layout.fillWidth: true
                Text { text: item.latency; color: toneColor(item.tone); font.pixelSize: 24; font.bold: true; Layout.fillWidth: true; elide: Text.ElideRight }
                Text { text: item.status; color: theme.muted; font.pixelSize: 13; elide: Text.ElideRight }
            }
            Text { text: (item.qualityScore ? localizationController.tr("source.quality.prefix") + item.qualityScore + localizationController.tr("source.coverage.prefix") + item.coveragePct : item.freshness || ""); color: theme.faint; font.pixelSize: 12; Layout.fillWidth: true; elide: Text.ElideRight }
        }
    }

    component EventRow: Rectangle {
        required property var item
        property string leftText: item.name || item.title
        property string rightText: item.latency || item.tag
        implicitHeight: 66
        radius: 8
        color: theme.panel2
        border.color: theme.line
        RowLayout {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 12
            Text { text: leftText; color: theme.text; font.pixelSize: 15; font.bold: true; Layout.preferredWidth: 120; elide: Text.ElideRight }
            Text { text: (item.dataAction ? item.dataAction + "  " : "") + item.detail; color: theme.muted; font.pixelSize: 13; Layout.fillWidth: true; wrapMode: Text.WordWrap; maximumLineCount: 2 }
            Text { text: rightText; color: toneColor(item.tone); font.pixelSize: 14; font.bold: true; Layout.preferredWidth: 76; horizontalAlignment: Text.AlignRight }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 16

        RowLayout {
            Layout.fillWidth: true
            spacing: 18
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6
                Text { text: localizationController.tr("window.title"); color: theme.text; font.pixelSize: 26; font.bold: true; Layout.fillWidth: true; elide: Text.ElideRight }
                Text { text: localizationController.tr("window.description"); color: theme.muted; font.pixelSize: 14; Layout.fillWidth: true; elide: Text.ElideRight }
            }
            Rectangle {
                Layout.preferredWidth: 200
                Layout.preferredHeight: 44
                radius: 8
                color: "#10233a"
                border.color: "#2b6fad"
                Text { anchors.centerIn: parent; text: pageController.status; color: theme.accent; font.pixelSize: 13; font.bold: true }
            }
            Button {
                Layout.preferredWidth: 92
                Layout.preferredHeight: 44
                text: localizationController.tr("action.refresh")
                onClicked: pageController.refresh()
                background: Rectangle { radius: 8; color: parent.down ? "#1f5d82" : "#132b45"; border.color: theme.accent }
                contentItem: Text { text: parent.text; color: theme.text; font.pixelSize: 14; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: window.width < 1160 ? 2 : 4
            columnSpacing: 12
            Repeater {
                model: pageController.scoreCards
                delegate: MetricCard {
                    required property var modelData
                    Layout.fillWidth: true
                    item: modelData
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 16

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 8
                color: theme.panel
                border.color: theme.line
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 12
                    RowLayout {
                        Layout.fillWidth: true
                        Text { text: localizationController.tr("datahub.section.sources"); color: theme.text; font.pixelSize: 22; font.bold: true; Layout.fillWidth: true }
                        Text { text: localizationController.tr("datahub.section.sources.subtitle"); color: theme.muted; font.pixelSize: 13 }
                    }
                    GridLayout {
                        Layout.fillWidth: true
                        columns: 3
                        columnSpacing: 12
                        rowSpacing: 12
                        Repeater {
                            model: pageController.sourceRows
                            delegate: SourceCard {
                                required property var modelData
                                Layout.fillWidth: true
                                item: modelData
                            }
                        }
                    }
                    Item { Layout.fillHeight: true }
                }
            }

            ColumnLayout {
                Layout.preferredWidth: 390
                Layout.fillHeight: true
                spacing: 16

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 8
                    color: theme.panel
                    border.color: theme.line
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 10
                        Text { text: localizationController.tr("datahub.section.latency"); color: theme.text; font.pixelSize: 22; font.bold: true }
                        Repeater {
                            model: pageController.latencyRows
                            delegate: EventRow {
                                required property var modelData
                                Layout.fillWidth: true
                                item: modelData
                            }
                        }
                        Item { Layout.fillHeight: true }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.max(340, governanceContent.implicitHeight + 36)
                    Layout.minimumHeight: governanceContent.implicitHeight + 36
                    radius: 8
                    color: theme.panel
                    border.color: theme.line
                    ColumnLayout {
                        id: governanceContent
                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 10
                        Text { text: localizationController.tr("datahub.section.governance"); color: theme.text; font.pixelSize: 22; font.bold: true }
                        Repeater {
                            model: pageController.governanceRows
                            delegate: EventRow {
                                required property var modelData
                                Layout.fillWidth: true
                                item: modelData
                            }
                        }
                    }
                }
            }
        }
    }
}
