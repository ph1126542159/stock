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
        required property string label
        required property string value
        required property string note
        required property string tone
        radius: 8
        color: theme.panel
        border.color: theme.line
        implicitHeight: 104

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 6
            Text { text: label; color: theme.muted; font.pixelSize: 13; elide: Text.ElideRight; Layout.fillWidth: true }
            Text { text: value; color: toneColor(tone); font.pixelSize: 28; font.bold: true; elide: Text.ElideRight; Layout.fillWidth: true }
            Text { text: note; color: theme.faint; font.pixelSize: 12; elide: Text.ElideRight; Layout.fillWidth: true }
        }
    }

    component WorkRow: Rectangle {
        required property var item
        radius: 6
        color: index % 2 === 0 ? "#0c1520" : "#111c2a"
        border.color: "#203148"
        implicitHeight: 68
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            spacing: 14
            Text { text: item.symbol; color: theme.text; font.pixelSize: 15; font.bold: true; Layout.preferredWidth: 76; elide: Text.ElideRight }
            Text { text: item.name; color: theme.text; font.pixelSize: 14; Layout.preferredWidth: 130; elide: Text.ElideRight }
            Text { text: Number(item.price).toFixed(1); color: theme.muted; font.pixelSize: 14; Layout.preferredWidth: 82 }
            Text { text: Number(item.fairValue).toFixed(1); color: theme.muted; font.pixelSize: 14; Layout.preferredWidth: 82 }
            Text { text: Number(item.margin).toFixed(1) + "%"; color: toneColor(item.tone); font.pixelSize: 15; font.bold: true; Layout.preferredWidth: 82 }
            Text { text: Number(item.pePercentile).toFixed(0) + " / " + Number(item.pbPercentile).toFixed(0); color: theme.muted; font.pixelSize: 14; Layout.preferredWidth: 96 }
            Text { text: Number(item.roe).toFixed(0) + " / " + Number(item.roic).toFixed(0); color: theme.muted; font.pixelSize: 14; Layout.preferredWidth: 86 }
            Text { text: item.buyBelow ? (localizationController.tr("vr.band.buy") + " " + Number(item.buyBelow).toFixed(0) + " / " + localizationController.tr("vr.band.sell") + " " + Number(item.sellAbove).toFixed(0)) : "--"; color: theme.muted; font.pixelSize: 13; Layout.preferredWidth: 118; elide: Text.ElideRight }
            Rectangle {
                Layout.preferredWidth: 92
                Layout.preferredHeight: 26
                radius: 6
                color: Qt.rgba(toneColor(item.tone).r, toneColor(item.tone).g, toneColor(item.tone).b, 0.12)
                border.color: toneColor(item.tone)
                Text { anchors.centerIn: parent; text: item.action || item.rating; color: toneColor(item.tone); font.pixelSize: 13; font.bold: true }
            }
        }
    }

    component InfoRow: Rectangle {
        required property var item
        implicitHeight: 70
        radius: 8
        color: theme.panel2
        border.color: theme.line
        RowLayout {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 12
            Rectangle { Layout.preferredWidth: 4; Layout.fillHeight: true; radius: 2; color: toneColor(item.tone || "blue") }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4
                Text { text: item.title; color: theme.text; font.pixelSize: 15; font.bold: true; Layout.fillWidth: true; elide: Text.ElideRight }
                Text { text: (item.evidence ? item.evidence + "  " : "") + item.detail; color: theme.muted; font.pixelSize: 13; Layout.fillWidth: true; wrapMode: Text.WordWrap; maximumLineCount: 2 }
            }
            Text { text: item.tag || item.step; color: toneColor(item.tone || "blue"); font.pixelSize: 13; font.bold: true; Layout.preferredWidth: 52; horizontalAlignment: Text.AlignRight }
        }
    }

    background: Rectangle { color: theme.background }

    Rectangle {
        id: mockBanner
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 28
        z: 1000
        color: "#3a0a0a"
        border.color: "#ff4d4d"
        border.width: 1
        Text {
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            text: "⚠ 本页为占位示例，估值指标基于内置场景，未接入真实财报/行情数据，不构成投资建议。"
            color: "#ff9b9b"
            font.family: "Segoe UI Variable"
            font.pixelSize: 13
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }

    ColumnLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: mockBanner.bottom
        anchors.bottom: parent.bottom
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
                Text { anchors.centerIn: parent; text: pageController.status; color: theme.accent; font.pixelSize: 13; font.bold: true; elide: Text.ElideRight }
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
            rowSpacing: 12
            Repeater {
                model: pageController.scoreCards
                delegate: MetricCard {
                    required property var modelData
                    Layout.fillWidth: true
                    label: modelData.label
                    value: modelData.value
                    note: modelData.note
                    tone: modelData.tone
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
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true
                        Text { text: localizationController.tr("vr.section.matrix"); color: theme.text; font.pixelSize: 22; font.bold: true; Layout.fillWidth: true }
                        Text { text: localizationController.tr("vr.section.matrix.subtitle"); color: theme.muted; font.pixelSize: 13 }
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 34
                        radius: 6
                        color: "#0b1320"
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 14
                            anchors.rightMargin: 14
                            spacing: 14
                            Repeater {
                                model: [localizationController.tr("vr.col.symbol"), localizationController.tr("vr.col.name"), localizationController.tr("vr.col.price"), localizationController.tr("vr.col.fair"), localizationController.tr("vr.col.margin"), "PE/PB", "ROE/ROIC", localizationController.tr("vr.col.bands"), localizationController.tr("vr.col.action")]
                                delegate: Text {
                                    required property string modelData
                                    text: modelData
                                    color: theme.muted
                                    font.pixelSize: 12
                                    font.bold: true
                                    Layout.preferredWidth: [76,130,82,82,82,96,96,92][index]
                                }
                            }
                        }
                    }
                    Column {
                        Layout.fillWidth: true
                        spacing: 6
                        Repeater {
                            model: pageController.valuationRows
                            delegate: WorkRow {
                                required property int index
                                required property var modelData
                                width: parent.width
                                item: modelData
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.preferredWidth: 360
                Layout.fillHeight: true
                spacing: 14
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 8
                    color: theme.panel
                    border.color: theme.line
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 10
                        Text { text: localizationController.tr("vr.section.research"); color: theme.text; font.pixelSize: 21; font.bold: true }
                        Repeater {
                            model: pageController.researchRows
                            delegate: InfoRow {
                                required property var modelData
                                Layout.fillWidth: true
                                item: modelData
                            }
                        }
                    }
                }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.max(260, thesisContent.implicitHeight + 36)
                    Layout.minimumHeight: thesisContent.implicitHeight + 36
                    radius: 8
                    color: theme.panel
                    border.color: theme.line
                    ColumnLayout {
                        id: thesisContent
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 10
                        Text { text: localizationController.tr("vr.section.thesis"); color: theme.text; font.pixelSize: 21; font.bold: true }
                        Repeater {
                            model: pageController.thesisRows
                            delegate: InfoRow {
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
