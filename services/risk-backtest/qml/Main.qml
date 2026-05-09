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

    component ExposureRow: Rectangle {
        required property var item
        implicitHeight: 74
        radius: 8
        color: theme.panel2
        border.color: theme.line
        RowLayout {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 12
            Text { text: item.group; color: theme.faint; font.pixelSize: 12; Layout.preferredWidth: 54; elide: Text.ElideRight }
            Text { text: item.name; color: theme.text; font.pixelSize: 15; font.bold: true; Layout.preferredWidth: 110; elide: Text.ElideRight }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 8
                radius: 4
                color: "#0a121c"
                Rectangle {
                    width: Math.min(parent.width, parent.width * Number(item.usage))
                    height: parent.height
                    radius: 4
                    color: toneColor(item.tone)
                }
            }
            ColumnLayout {
                Layout.preferredWidth: 142
                spacing: 2
                Text { text: Number(item.value).toFixed(1) + "% / " + Number(item.limit).toFixed(0) + "%"; color: toneColor(item.tone); font.pixelSize: 13; font.bold: true; Layout.fillWidth: true; horizontalAlignment: Text.AlignRight }
                Text { text: (item.action || "") + " " + (item.suggestedDelta || "") + (item.riskInstruction ? " / " + item.riskInstruction : ""); color: theme.muted; font.pixelSize: 11; Layout.fillWidth: true; horizontalAlignment: Text.AlignRight; elide: Text.ElideRight }
            }
        }
    }

    component SimpleRow: Rectangle {
        required property var item
        property string leftText: item.pair || item.name || item.title
        property string rightText: item.value !== undefined ? Number(item.value).toFixed(2) : (item.sharpe || item.tag || "")
        property string detail: (item.riskInstruction ? item.riskInstruction + " / " : "") + (item.detail || item.advice || item.rebalanceRule || "")
        property string tone: item.tone || "blue"
        implicitHeight: 64
        radius: 8
        color: theme.panel2
        border.color: theme.line
        RowLayout {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 12
            Text { text: leftText; color: theme.text; font.pixelSize: 15; font.bold: true; Layout.preferredWidth: 138; elide: Text.ElideRight }
            Text { text: (item.action ? item.action + "  " : "") + detail; color: theme.muted; font.pixelSize: 13; Layout.fillWidth: true; wrapMode: Text.WordWrap; maximumLineCount: 2 }
            Text { text: rightText; color: toneColor(tone); font.pixelSize: 14; font.bold: true; Layout.preferredWidth: 76; horizontalAlignment: Text.AlignRight }
        }
    }

    Canvas {
        id: backdrop
        anchors.fill: parent
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.strokeStyle = "rgba(78,161,255,0.05)"
            ctx.lineWidth = 1
            for (let x = 0; x < width; x += 42) { ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, height); ctx.stroke() }
            for (let y = 0; y < height; y += 42) { ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(width, y); ctx.stroke() }
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
            text: "⚠ 本页为占位示例，回测/风控指标基于内置场景，未接入真实持仓与历史行情，不构成投资建议。"
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
                Layout.preferredWidth: Math.min(420, Math.max(220, pageController.status.length * 12))
                Layout.maximumWidth: 420
                Layout.preferredHeight: 44
                radius: 8
                color: "#10233a"
                border.color: "#2b6fad"
                Text {
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14
                    text: pageController.status
                    color: theme.accent
                    font.pixelSize: 13
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
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
                Layout.preferredWidth: 400
                Layout.fillHeight: true
                radius: 8
                color: theme.panel
                border.color: theme.line
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 10
                    Text { text: localizationController.tr("rb.section.exposure"); color: theme.text; font.pixelSize: 22; font.bold: true }
                    Text { text: localizationController.tr("rb.section.exposure.subtitle"); color: theme.muted; font.pixelSize: 13 }
                    Repeater {
                        model: pageController.exposureRows
                        delegate: ExposureRow {
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
                Layout.fillHeight: true
                radius: 8
                color: theme.panel
                border.color: theme.line
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 14
                    RowLayout {
                        Layout.fillWidth: true
                        Text { text: localizationController.tr("rb.section.correlation"); color: theme.text; font.pixelSize: 22; font.bold: true; Layout.fillWidth: true }
                        Text { text: localizationController.tr("rb.section.correlation.subtitle"); color: theme.muted; font.pixelSize: 13 }
                    }
                    Repeater {
                        model: pageController.correlationRows
                        delegate: SimpleRow {
                            required property var modelData
                            Layout.fillWidth: true
                            item: modelData
                        }
                    }

                    Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: theme.line }
                    RowLayout {
                        Layout.fillWidth: true
                        Text { text: localizationController.tr("rb.section.backtest"); color: theme.text; font.pixelSize: 22; font.bold: true; Layout.fillWidth: true }
                        Text { text: localizationController.tr("rb.section.backtest.subtitle"); color: theme.muted; font.pixelSize: 13 }
                    }
                    Repeater {
                        model: pageController.backtestRows
                        delegate: Rectangle {
                            required property var modelData
                            Layout.fillWidth: true
                            implicitHeight: 70
                            radius: 8
                            color: theme.panel2
                            border.color: theme.line
                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 14
                                spacing: 12
                                Text { text: modelData.name; color: theme.text; font.pixelSize: 15; font.bold: true; Layout.preferredWidth: 150; elide: Text.ElideRight }
                                Text { text: localizationController.tr("rb.label.winrate") + modelData.winRate; color: theme.green; font.pixelSize: 14; Layout.preferredWidth: 92 }
                                Text { text: localizationController.tr("rb.label.drawdown") + modelData.maxDrawdown; color: theme.amber; font.pixelSize: 14; Layout.preferredWidth: 104 }
                                Text { text: "Sharpe " + modelData.sharpe; color: theme.accent; font.pixelSize: 14; Layout.preferredWidth: 96 }
                                Text { text: modelData.advice; color: theme.muted; font.pixelSize: 13; Layout.fillWidth: true; elide: Text.ElideRight }
                            }
                        }
                    }
                    Item { Layout.fillHeight: true }
                }
            }
        }
    }
}
