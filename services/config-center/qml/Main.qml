import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window
    width: 1360
    height: 900
    visible: false
    title: localizationController.tr("window.title")
    color: "#0e141b"
    flags: Qt.Tool | Qt.FramelessWindowHint

    QtObject {
        id: theme
        readonly property color background: "#0e141b"
        readonly property color panel: "#121b24"
        readonly property color panelRaised: "#182430"
        readonly property color border: "#2a3847"
        readonly property color text: "#edf4ff"
        readonly property color muted: "#96abc1"
        readonly property color accent: "#59b8ff"
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

    component ConfigSection : Rectangle {
        id: section
        required property string titleText
        required property string subtitleText
        property bool expanded: true

        color: theme.panel
        border.color: theme.border
        radius: 8
        implicitHeight: 36 + headerBox.implicitHeight + (section.expanded ? contentColumn.implicitHeight + 14 : 0)

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 14

            Rectangle {
                id: headerBox
                Layout.fillWidth: true
                implicitHeight: 76
                radius: 8
                color: theme.panelRaised
                border.color: theme.border

                MouseArea {
                    anchors.fill: parent
                    onClicked: section.expanded = !section.expanded
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    anchors.topMargin: 10
                    anchors.bottomMargin: 10
                    spacing: 14

                    Text {
                        Layout.preferredWidth: 38
                        Layout.fillHeight: true
                        text: section.expanded ? "▾" : "▸"
                        color: theme.accent
                        font.pixelSize: 18
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 2

                        Text {
                            Layout.fillWidth: true
                            text: section.titleText
                            color: theme.text
                            font.family: "Segoe UI Variable"
                            font.pixelSize: 22
                            font.bold: true
                            elide: Text.ElideRight
                            verticalAlignment: Text.AlignBottom
                        }

                        Text {
                            Layout.fillWidth: true
                            text: section.subtitleText
                            color: theme.muted
                            font.family: "Segoe UI Variable"
                            font.pixelSize: 13
                            elide: Text.ElideRight
                            verticalAlignment: Text.AlignTop
                        }
                    }
                }
            }

            ColumnLayout {
                id: contentColumn
                visible: section.expanded
                Layout.fillWidth: true
                spacing: 12
            }
        }

        default property alias content: contentColumn.data
    }

    component ConfigRow : Rectangle {
        id: rowCard
        required property string labelText
        required property string keyText
        required property string targetText
        required property string valueText

        color: theme.panelRaised
        border.color: theme.border
        radius: 8
        implicitHeight: Math.max(104, rowContent.implicitHeight + 28)

        RowLayout {
            id: rowContent
            anchors.fill: parent
            anchors.margins: 16
            spacing: 14

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Text {
                    text: rowCard.labelText
                    color: theme.text
                    font.family: "Segoe UI Variable"
                    font.pixelSize: 18
                    font.bold: true
                }

                Text {
                    text: rowCard.keyText + " / " + rowCard.targetText
                    color: theme.muted
                    font.family: "Consolas"
                    font.pixelSize: 13
                    wrapMode: Text.WrapAnywhere
                }
            }

            TextField {
                id: valueField
                Layout.preferredWidth: 220
                text: rowCard.valueText
                selectByMouse: true
                color: theme.text
                font.family: "Segoe UI Variable"
                font.pixelSize: 16

                background: Rectangle {
                    radius: 8
                    color: theme.background
                    border.color: valueField.activeFocus ? theme.accent : theme.border
                }
            }

            Button {
                id: applyButton
                text: localizationController.tr("cc.label.apply")
                implicitWidth: 112
                implicitHeight: 46
                onClicked: {
                    if (configController.publishUpdate(rowCard.targetText, rowCard.keyText, valueField.text)) {
                        rowCard.valueText = valueField.text
                    }
                }

                background: Rectangle {
                    radius: 8
                    color: theme.accentSoft
                    border.color: theme.accent
                }

                contentItem: Text {
                    text: applyButton.text
                    color: theme.accent
                    font: applyButton.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }

    background: Rectangle {
        color: theme.background
    }

    ScrollView {
        id: configScroll
        anchors.fill: parent
        anchors.margins: 14
        clip: true
        background: Rectangle { color: "transparent" }
        contentWidth: availableWidth
        ScrollBar.vertical: StyledScrollBar { policy: ScrollBar.AlwaysOff; visible: false }
        ScrollBar.horizontal: StyledScrollBar { policy: ScrollBar.AlwaysOff; visible: false }

        ColumnLayout {
            width: configScroll.availableWidth
            spacing: 14

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 126
                radius: 8
                color: theme.panelRaised
                border.color: theme.border

                ColumnLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 20
                    anchors.rightMargin: 20
                    anchors.topMargin: 16
                    anchors.bottomMargin: 16
                    spacing: 6

                    Text {
                        Layout.fillWidth: true
                        text: localizationController.tr("window.title")
                        color: theme.text
                        font.family: "Segoe UI Variable"
                        font.pixelSize: 30
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    Text {
                        Layout.fillWidth: true
                        text: localizationController.tr("cc.label.intro")
                        color: theme.muted
                        font.family: "Segoe UI Variable"
                        font.pixelSize: 15
                        elide: Text.ElideRight
                    }

                    Text {
                        Layout.fillWidth: true
                        text: configController.status
                        color: theme.accent
                        font.family: "Segoe UI Variable"
                        font.pixelSize: 15
                        font.bold: true
                        elide: Text.ElideRight
                    }
                }
            }

            ConfigSection {
                Layout.fillWidth: true
                titleText: localizationController.tr("cc.section.process")
                subtitleText: localizationController.tr("cc.section.process.subtitle")

                ConfigRow {
                    Layout.fillWidth: true
                    labelText: localizationController.tr("cc.field.keepalive.enabled")
                    keyText: "stok.services.keepAlive.enabled"
                    targetText: "macchina"
                    valueText: "true"
                }

                ConfigRow {
                    Layout.fillWidth: true
                    labelText: localizationController.tr("cc.field.keepalive.delayMs")
                    keyText: "stok.services.keepAlive.restartDelayMs"
                    targetText: "macchina"
                    valueText: "0"
                }

                ConfigRow {
                    Layout.fillWidth: true
                    labelText: localizationController.tr("cc.field.keepalive.skipShell")
                    keyText: "stok.services.desktop-shell.keepAlive"
                    targetText: "macchina"
                    valueText: "false"
                }
            }

            ConfigSection {
                Layout.fillWidth: true
                titleText: localizationController.tr("cc.section.market")
                subtitleText: localizationController.tr("cc.section.market.subtitle")

                ConfigRow {
                    Layout.fillWidth: true
                    labelText: localizationController.tr("cc.field.market.institutions")
                    keyText: "update.institutions.intervalMs"
                    targetText: "market-board"
                    valueText: "3600000"
                }

                ConfigRow {
                    Layout.fillWidth: true
                    labelText: localizationController.tr("cc.field.market.value")
                    keyText: "update.valueBoard.intervalMs"
                    targetText: "market-board"
                    valueText: "300000"
                }

                ConfigRow {
                    Layout.fillWidth: true
                    labelText: localizationController.tr("cc.field.market.us")
                    keyText: "update.usMarket.intervalMs"
                    targetText: "market-board"
                    valueText: "1000"
                }

                ConfigRow {
                    Layout.fillWidth: true
                    labelText: "免费数据脚本调用间隔 ms (东财行业资金/巨潮公告)"
                    keyText: "update.freeData.intervalMs"
                    targetText: "market-board"
                    valueText: "300000"
                }
            }

            ConfigSection {
                Layout.fillWidth: true
                titleText: localizationController.tr("cc.section.portfolio")
                subtitleText: localizationController.tr("cc.section.portfolio.subtitle")

                ConfigRow {
                    Layout.fillWidth: true
                    labelText: localizationController.tr("cc.field.portfolio.curves")
                    keyText: "update.quotes.intervalMs"
                    targetText: "portfolio-board"
                    valueText: "1000"
                }

                ConfigRow {
                    Layout.fillWidth: true
                    labelText: localizationController.tr("cc.field.portfolio.ai")
                    keyText: "update.analysis.intervalMs"
                    targetText: "portfolio-board"
                    valueText: "300000"
                }
            }
        }
    }
}
