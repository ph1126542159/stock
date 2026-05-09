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
        id: infoRow
        required property var item
        implicitHeight: infoCol.implicitHeight + 24
        radius: 8
        color: theme.panel2
        border.color: theme.line
        RowLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 12
            Rectangle { Layout.preferredWidth: 4; Layout.fillHeight: true; radius: 2; color: toneColor(infoRow.item.tone || "blue") }
            ColumnLayout {
                id: infoCol
                Layout.fillWidth: true
                spacing: 6
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Text { text: infoRow.item.title; color: theme.text; font.family: "Microsoft YaHei UI"; font.pixelSize: 15; font.bold: true; Layout.fillWidth: true; elide: Text.ElideRight }
                    Rectangle {
                        Layout.preferredWidth: 36
                        Layout.preferredHeight: 22
                        radius: 11
                        color: Qt.rgba(toneColor(infoRow.item.tone || "blue").r, toneColor(infoRow.item.tone || "blue").g, toneColor(infoRow.item.tone || "blue").b, 0.18)
                        border.color: toneColor(infoRow.item.tone || "blue")
                        Text { anchors.centerIn: parent; text: infoRow.item.tag || infoRow.item.step || ""; color: toneColor(infoRow.item.tone || "blue"); font.family: "Microsoft YaHei UI"; font.pixelSize: 12; font.bold: true }
                    }
                }
                Text { text: infoRow.item.detail; color: theme.text; font.family: "Microsoft YaHei UI"; font.pixelSize: 13; Layout.fillWidth: true; wrapMode: Text.WordWrap; lineHeight: 1.3 }
                Text { visible: !!infoRow.item.evidence; text: "▸ " + (infoRow.item.evidence || ""); color: theme.muted; font.family: "Microsoft YaHei UI"; font.pixelSize: 12; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            }
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
        color: "#1a2638"
        border.color: "#3878c2"
        border.width: 1
        Text {
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            text: "估值数据来自公开财报与行情；评分为透明因子模型，不构成投资建议。" + "  " + (pageController.assumptionSummary || "")
            color: "#9bc8ff"
            font.family: "Microsoft YaHei UI"
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
                Layout.preferredWidth: 320
                Layout.preferredHeight: 44
                radius: 8
                color: "#10233a"
                border.color: "#2b6fad"
                Text { anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 12; verticalAlignment: Text.AlignVCenter; text: pageController.status + "  " + (pageController.lastUpdatedText || ""); color: theme.accent; font.family: "Microsoft YaHei UI"; font.pixelSize: 13; font.bold: true; elide: Text.ElideRight }
            }
            Button {
                Layout.preferredWidth: 100
                Layout.preferredHeight: 44
                text: pageController.refreshing ? "刷新中..." : localizationController.tr("action.refresh")
                enabled: !pageController.refreshing
                onClicked: pageController.refresh()
                background: Rectangle { radius: 8; color: parent.down ? "#1f5d82" : "#132b45"; border.color: theme.accent; opacity: parent.enabled ? 1.0 : 0.5 }
                contentItem: Text { text: parent.text; color: theme.text; font.family: "Microsoft YaHei UI"; font.pixelSize: 14; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
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

            ScrollView {
                id: rightScroll
                Layout.preferredWidth: 380
                Layout.fillHeight: true
                clip: true
                contentWidth: rightCol.width
                ScrollBar.vertical.policy: ScrollBar.AsNeeded
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                background: Item {}

            ColumnLayout {
                id: rightCol
                width: rightScroll.width - 12
                spacing: 14

                // Watchlist editor
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: watchlistContent.implicitHeight + 36
                    Layout.minimumHeight: 180
                    radius: 8
                    color: theme.panel
                    border.color: theme.line
                    ColumnLayout {
                        id: watchlistContent
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 10

                        Text { text: "标的池"; color: theme.text; font.family: "Microsoft YaHei UI"; font.pixelSize: 21; font.bold: true }
                        Text { text: "格式 A:600519 / HK:00700 / US:AAPL"; color: theme.muted; font.family: "Microsoft YaHei UI"; font.pixelSize: 12 }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            TextField {
                                id: addInput
                                Layout.fillWidth: true
                                Layout.preferredHeight: 36
                                placeholderText: "A:600519"
                                color: theme.text
                                font.family: "Microsoft YaHei UI"
                                font.pixelSize: 14
                                background: Rectangle { radius: 6; color: theme.panel2; border.color: addInput.activeFocus ? theme.accent : theme.line }
                                onAccepted: { if (pageController.addSymbol(text)) text = "" }
                            }
                            Button {
                                id: addBtn
                                Layout.preferredWidth: 64
                                Layout.preferredHeight: 36
                                text: "添加"
                                onClicked: { if (pageController.addSymbol(addInput.text)) addInput.text = "" }
                                background: Rectangle { radius: 6; color: addBtn.down ? "#1f5d82" : "#132b45"; border.color: theme.accent }
                                contentItem: Text { text: addBtn.text; color: theme.text; font.family: "Microsoft YaHei UI"; font.pixelSize: 13; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            }
                        }

                        ListView {
                            id: watchlistView
                            Layout.fillWidth: true
                            // Let the ListView grow to fit all rows; the
                            // outer ScrollView provides vertical scrolling.
                            Layout.preferredHeight: contentHeight
                            interactive: false
                            clip: false
                            spacing: 6
                            model: pageController.watchlistRich
                            delegate: Rectangle {
                                id: watchRow
                                required property var modelData
                                width: ListView.view.width
                                implicitHeight: watchRowCol.implicitHeight + 16
                                radius: 6
                                color: "#0c1520"
                                border.color: watchRow.modelData.tone === "red" ? Qt.rgba(theme.red.r, theme.red.g, theme.red.b, 0.45)
                                            : watchRow.modelData.tone === "green" ? Qt.rgba(theme.green.r, theme.green.g, theme.green.b, 0.45)
                                            : "#1c2a3c"
                                ColumnLayout {
                                    id: watchRowCol
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    spacing: 4

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 6
                                        Text {
                                            text: watchRow.modelData.displayCode || watchRow.modelData.spec
                                            color: theme.accent
                                            font.family: "Consolas"
                                            font.pixelSize: 12
                                            font.bold: true
                                            Layout.preferredWidth: 78
                                        }
                                        Text {
                                            text: watchRow.modelData.pending ? "（等待估值）" : (watchRow.modelData.name || "")
                                            color: theme.text
                                            font.family: "Microsoft YaHei UI"
                                            font.pixelSize: 13
                                            font.bold: true
                                            Layout.fillWidth: true
                                            elide: Text.ElideRight
                                        }
                                        Button {
                                            id: removeBtn
                                            Layout.preferredWidth: 38
                                            Layout.preferredHeight: 20
                                            text: "✕"
                                            onClicked: pageController.removeSymbol(watchRow.modelData.spec)
                                            background: Rectangle { radius: 4; color: removeBtn.down ? "#5a1f1f" : "#2a1414"; border.color: theme.red }
                                            contentItem: Text { text: removeBtn.text; color: theme.red; font.family: "Microsoft YaHei UI"; font.pixelSize: 12; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                        }
                                    }
                                    RowLayout {
                                        visible: !watchRow.modelData.pending
                                        Layout.fillWidth: true
                                        spacing: 4
                                        Text {
                                            text: "现价 " + Number(watchRow.modelData.lastPrice || 0).toFixed(2)
                                            color: theme.muted; font.family: "Microsoft YaHei UI"; font.pixelSize: 11
                                            Layout.preferredWidth: 90
                                        }
                                        Text {
                                            text: "合理 " + Number(watchRow.modelData.fairValue || 0).toFixed(2)
                                            color: theme.muted; font.family: "Microsoft YaHei UI"; font.pixelSize: 11
                                            Layout.fillWidth: true
                                        }
                                    }
                                    RowLayout {
                                        visible: !watchRow.modelData.pending
                                        Layout.fillWidth: true
                                        spacing: 4
                                        Rectangle {
                                            Layout.preferredWidth: 70
                                            Layout.preferredHeight: 22
                                            radius: 4
                                            color: Qt.rgba(toneColor(watchRow.modelData.tone || "blue").r, toneColor(watchRow.modelData.tone || "blue").g, toneColor(watchRow.modelData.tone || "blue").b, 0.18)
                                            border.color: toneColor(watchRow.modelData.tone || "blue")
                                            Text {
                                                anchors.centerIn: parent
                                                text: (Number(watchRow.modelData.margin || 0) >= 0 ? "+" : "") + Number(watchRow.modelData.margin || 0).toFixed(1) + "%"
                                                color: toneColor(watchRow.modelData.tone || "blue")
                                                font.family: "Microsoft YaHei UI"; font.pixelSize: 12; font.bold: true
                                            }
                                        }
                                        Text {
                                            text: "PE " + Number(watchRow.modelData.pe || 0).toFixed(1)
                                            color: theme.muted; font.family: "Microsoft YaHei UI"; font.pixelSize: 11
                                            Layout.preferredWidth: 56
                                        }
                                        Text {
                                            text: "ROE " + Number(watchRow.modelData.roe || 0).toFixed(1) + "%"
                                            color: theme.muted; font.family: "Microsoft YaHei UI"; font.pixelSize: 11
                                            Layout.preferredWidth: 76
                                        }
                                        Text {
                                            text: watchRow.modelData.action || ""
                                            color: toneColor(watchRow.modelData.tone || "blue")
                                            font.family: "Microsoft YaHei UI"; font.pixelSize: 11; font.bold: true
                                            Layout.fillWidth: true
                                            horizontalAlignment: Text.AlignRight
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: researchContent.implicitHeight + 36
                    radius: 8
                    color: theme.panel
                    border.color: theme.line
                    ColumnLayout {
                        id: researchContent
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
                    Layout.preferredHeight: thesisContent.implicitHeight + 36
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
}
