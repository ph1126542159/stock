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

    component PlanCard: Rectangle {
        required property var item
        radius: 8
        color: theme.panel2
        border.color: theme.line
        implicitHeight: Math.max(238, planContent.implicitHeight + 30)
        ColumnLayout {
            id: planContent
            anchors.fill: parent
            anchors.margins: 15
            spacing: 8
            RowLayout {
                Layout.fillWidth: true
                Text { text: item.symbol; color: theme.text; font.pixelSize: 22; font.bold: true; Layout.fillWidth: true }
                Rectangle {
                    Layout.preferredWidth: 56
                    Layout.preferredHeight: 24
                    radius: 6
                    color: "#0a121c"
                    border.color: toneColor(item.tone)
                    Text { anchors.centerIn: parent; text: item.action || localizationController.tr("ta.label.plan"); color: toneColor(item.tone); font.pixelSize: 12; font.bold: true }
                }
            }
            Text { text: item.thesis; color: theme.muted; font.pixelSize: 13; Layout.fillWidth: true; wrapMode: Text.WordWrap; maximumLineCount: 2 }
            Text { text: localizationController.tr("ta.label.priority") + (item.priority || "--") + localizationController.tr("ta.label.positionDelta") + (item.positionDelta || "--"); color: theme.accent; font.pixelSize: 13; Layout.fillWidth: true; elide: Text.ElideRight }
            Text {
                visible: Boolean(item.riskInstruction || item.detail)
                text: (item.riskInstruction ? item.riskInstruction + " / " : "") + (item.detail || "")
                color: theme.amber
                font.pixelSize: 13
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }
            Text { text: localizationController.tr("ta.label.buy") + item.buy; color: theme.green; font.pixelSize: 13; Layout.fillWidth: true; elide: Text.ElideRight }
            Text { text: localizationController.tr("ta.label.sell") + item.sell; color: theme.amber; font.pixelSize: 13; Layout.fillWidth: true; elide: Text.ElideRight }
            Text { text: localizationController.tr("ta.label.trigger") + (item.triggerPrice || "--") + localizationController.tr("ta.label.stoploss") + (item.stopLoss || "--") + localizationController.tr("ta.label.takeprofit") + (item.takeProfit || "--"); color: theme.muted; font.pixelSize: 13; Layout.fillWidth: true; elide: Text.ElideRight }
            Text { text: localizationController.tr("ta.label.invalid") + item.invalid; color: theme.red; font.pixelSize: 13; Layout.fillWidth: true; elide: Text.ElideRight }
        }
    }

    component AlertRow: Rectangle {
        required property var item
        required property int rowIndex
        implicitHeight: 66
        radius: 8
        color: theme.panel2
        border.color: theme.line
        RowLayout {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 12
            Rectangle {
                Layout.preferredWidth: 64
                Layout.preferredHeight: 28
                radius: 6
                color: "#0a121c"
                border.color: toneColor(item.tone)
                Text { anchors.centerIn: parent; text: item.type; color: toneColor(item.tone); font.pixelSize: 12; font.bold: true }
            }
            Text { text: item.target; color: theme.text; font.pixelSize: 16; font.bold: true; Layout.preferredWidth: 76; elide: Text.ElideRight }
            Text { text: (item.action ? item.action + "  " : "") + (item.riskInstruction ? item.riskInstruction + " / " : "") + item.detail; color: theme.muted; font.pixelSize: 13; Layout.fillWidth: true; elide: Text.ElideRight }
            Button {
                Layout.preferredWidth: 74
                Layout.preferredHeight: 30
                text: item.status
                onClicked: pageController.acknowledgeAlert(rowIndex)
                background: Rectangle { radius: 7; color: parent.down ? "#1f5d82" : "#10233a"; border.color: toneColor(item.tone) }
                contentItem: Text { text: parent.text; color: toneColor(item.tone); font.pixelSize: 12; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
            }
        }
    }

    component ReviewRow: Rectangle {
        required property var item
        implicitHeight: 68
        radius: 8
        color: theme.panel2
        border.color: theme.line
        RowLayout {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 12
            Text { text: item.date; color: theme.faint; font.pixelSize: 12; Layout.preferredWidth: 82 }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4
                Text { text: item.title; color: theme.text; font.pixelSize: 15; font.bold: true; Layout.fillWidth: true; elide: Text.ElideRight }
                Text { text: item.detail; color: theme.muted; font.pixelSize: 13; Layout.fillWidth: true; elide: Text.ElideRight }
            }
            Rectangle { Layout.preferredWidth: 8; Layout.preferredHeight: 8; radius: 4; color: toneColor(item.tone) }
        }
    }

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
            text: "⚠ 本页为占位示例，未接入真实交易/风控/行情数据，所有提醒仅作演示，不构成投资建议。"
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
                Layout.preferredWidth: 420
                Layout.fillHeight: true
                radius: 8
                color: theme.panel
                border.color: theme.line
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 12
                    Text { text: localizationController.tr("ta.section.plans"); color: theme.text; font.pixelSize: 22; font.bold: true }
                    Repeater {
                        model: pageController.planRows
                        delegate: PlanCard {
                            required property var modelData
                            Layout.fillWidth: true
                            item: modelData
                        }
                    }
                    Item { Layout.fillHeight: true }
                }
            }

            ColumnLayout {
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
                            Text { text: localizationController.tr("ta.section.alerts"); color: theme.text; font.pixelSize: 22; font.bold: true; Layout.fillWidth: true }
                            Text { text: localizationController.tr("ta.section.alerts.subtitle"); color: theme.muted; font.pixelSize: 13 }
                        }
                        Repeater {
                            model: pageController.alertRows
                            delegate: AlertRow {
                                required property int index
                                required property var modelData
                                Layout.fillWidth: true
                                item: modelData
                                rowIndex: index
                            }
                        }
                        Item { Layout.fillHeight: true }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.max(250, reviewContent.implicitHeight + 36)
                    Layout.minimumHeight: reviewContent.implicitHeight + 36
                    radius: 8
                    color: theme.panel
                    border.color: theme.line
                    ColumnLayout {
                        id: reviewContent
                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 10
                        Text { text: localizationController.tr("ta.section.review"); color: theme.text; font.pixelSize: 22; font.bold: true }
                        Repeater {
                            model: pageController.reviewRows
                            delegate: ReviewRow {
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
