import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window
    width: 1500
    height: 860
    visible: false
    title: localizationController.tr("window.title")
    color: theme.background
    flags: Qt.Tool | Qt.FramelessWindowHint

    property string selectedInstitution: portfolioController.institutionOptions.length > 0
        ? String(portfolioController.institutionOptions[0])
        : ""

    QtObject {
        id: theme
        readonly property color background: "#070d14"
        readonly property color panel: "#0d1722"
        readonly property color panelRaised: "#111f2c"
        readonly property color panelSoft: "#0a141f"
        readonly property color rowEven: "#0d1824"
        readonly property color rowOdd: "#102030"
        readonly property color border: "#21344a"
        readonly property color line: "#1a2a3c"
        readonly property color text: "#f1f7ff"
        readonly property color muted: "#91a4ba"
        readonly property color faint: "#63758a"
        readonly property color accent: "#52a7ff"
        readonly property color cyan: "#33d7c2"
        readonly property color positive: "#22c993"
        readonly property color negative: "#ff6972"
    }

    component HiddenScrollBar: ScrollBar {
        policy: ScrollBar.AlwaysOff
        implicitWidth: 0
        implicitHeight: 0
        width: 0
        height: 0
        opacity: 0
        interactive: false
        background: Item {}
        contentItem: Item {}
    }

    component MetricTile: Rectangle {
        id: tile
        required property string label
        required property string value
        property color valueColor: theme.text

        implicitWidth: 160
        implicitHeight: 78
        radius: 8
        color: theme.panelRaised
        border.color: theme.border

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 4

            Text {
                Layout.fillWidth: true
                text: tile.label
                color: theme.muted
                font.family: "Segoe UI Variable"
                font.pixelSize: 12
                elide: Text.ElideRight
            }
            Text {
                Layout.fillWidth: true
                text: tile.value
                color: tile.valueColor
                font.family: "Segoe UI Variable"
                font.pixelSize: 22
                font.bold: true
                elide: Text.ElideRight
            }
        }
    }

    component TrendCard: Rectangle {
        id: chartCard
        required property string titleText
        property var series: []
        property color lineColor: theme.accent

        radius: 8
        color: theme.panelRaised
        border.color: theme.border
        clip: true

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 8

            Text {
                Layout.fillWidth: true
                text: chartCard.titleText
                color: theme.text
                font.family: "Segoe UI Variable"
                font.pixelSize: 16
                font.bold: true
                elide: Text.ElideRight
            }

            Canvas {
                id: canvas
                Layout.fillWidth: true
                Layout.fillHeight: true
                antialiasing: true

                onPaint: {
                    const ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    const values = chartCard.series || []
                    if (!values.length) {
                        ctx.fillStyle = theme.faint
                        ctx.font = "13px 'Segoe UI Variable'"
                        ctx.fillText(localizationController.tr("pb.empty"), 12, height / 2)
                        return
                    }

                    let minValue = Number(values[0])
                    let maxValue = Number(values[0])
                    for (let i = 1; i < values.length; ++i) {
                        const value = Number(values[i])
                        minValue = Math.min(minValue, value)
                        maxValue = Math.max(maxValue, value)
                    }
                    if (Math.abs(maxValue - minValue) < 0.0001) {
                        maxValue += 1
                        minValue -= 1
                    }

                    const left = 10
                    const top = 6
                    const right = width - 8
                    const bottom = height - 10
                    const chartWidth = Math.max(right - left, 1)
                    const chartHeight = Math.max(bottom - top, 1)

                    ctx.strokeStyle = "rgba(145, 164, 186, 0.15)"
                    ctx.lineWidth = 1
                    ctx.beginPath()
                    for (let g = 0; g < 4; ++g) {
                        const y = top + chartHeight * g / 3
                        ctx.moveTo(left, y)
                        ctx.lineTo(right, y)
                    }
                    ctx.stroke()

                    const points = []
                    for (let i = 0; i < values.length; ++i) {
                        const ratio = values.length === 1 ? 0 : i / (values.length - 1)
                        const normalized = (Number(values[i]) - minValue) / (maxValue - minValue)
                        points.push({
                            x: left + ratio * chartWidth,
                            y: bottom - normalized * chartHeight
                        })
                    }

                    ctx.beginPath()
                    ctx.moveTo(points[0].x, bottom)
                    for (let i = 0; i < points.length; ++i) ctx.lineTo(points[i].x, points[i].y)
                    ctx.lineTo(points[points.length - 1].x, bottom)
                    ctx.closePath()
                    const fill = ctx.createLinearGradient(0, top, 0, bottom)
                    fill.addColorStop(0, "rgba(82, 167, 255, 0.24)")
                    fill.addColorStop(1, "rgba(51, 215, 194, 0.00)")
                    ctx.fillStyle = fill
                    ctx.fill()

                    ctx.strokeStyle = chartCard.lineColor
                    ctx.lineWidth = 2.2
                    ctx.beginPath()
                    for (let i = 0; i < points.length; ++i) {
                        if (i === 0) ctx.moveTo(points[i].x, points[i].y)
                        else ctx.lineTo(points[i].x, points[i].y)
                    }
                    ctx.stroke()

                    const lastPoint = points[points.length - 1]
                    ctx.fillStyle = chartCard.lineColor
                    ctx.beginPath()
                    ctx.arc(lastPoint.x, lastPoint.y, 3.4, 0, Math.PI * 2)
                    ctx.fill()
                }
            }
        }

        onSeriesChanged: canvas.requestPaint()
        onWidthChanged: canvas.requestPaint()
        onHeightChanged: canvas.requestPaint()
    }

    component InfoBlock: Rectangle {
        id: block
        required property string titleText
        required property string bodyText

        radius: 8
        color: theme.panelRaised
        border.color: theme.border
        implicitHeight: Math.max(132, infoContent.implicitHeight + 28)

        ColumnLayout {
            id: infoContent
            anchors.fill: parent
            anchors.margins: 14
            spacing: 8

            Text {
                Layout.fillWidth: true
                text: block.titleText
                color: theme.text
                font.family: "Segoe UI Variable"
                font.pixelSize: 16
                font.bold: true
                elide: Text.ElideRight
            }
            Text {
                Layout.fillWidth: true
                text: block.bodyText
                color: theme.muted
                font.family: "Segoe UI Variable"
                font.pixelSize: 13
                lineHeight: 1.25
                wrapMode: Text.WordWrap
            }
        }
    }

    component CompactMetric: Rectangle {
        id: compactMetric
        required property string label
        required property string value
        property color valueColor: theme.text

        radius: 8
        color: theme.panelSoft
        border.color: theme.line

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 3

            Text {
                Layout.fillWidth: true
                text: compactMetric.label
                color: theme.muted
                font.family: "Segoe UI Variable"
                font.pixelSize: 11
                elide: Text.ElideRight
            }
            Text {
                Layout.fillWidth: true
                text: compactMetric.value
                color: compactMetric.valueColor
                font.family: "Segoe UI Variable"
                font.pixelSize: 16
                font.bold: true
                elide: Text.ElideRight
            }
        }
    }

    component DecisionCard: Rectangle {
        id: decisionCard

        function actionColor(action) {
            if (action === localizationController.tr("pb.action.add")) return theme.positive
            if (action === localizationController.tr("pb.action.sell") || action === localizationController.tr("pb.action.exit")) return theme.negative
            if (action === localizationController.tr("pb.action.rotate")) return "#ffbf4d"
            return theme.accent
        }

        radius: 8
        color: theme.panelRaised
        border.color: theme.border
        implicitHeight: Math.max(332, decisionContent.implicitHeight + 28)

        ColumnLayout {
            id: decisionContent
            anchors.fill: parent
            anchors.margins: 14
            spacing: 10

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    Layout.fillWidth: true
                    text: localizationController.tr("pb.label.recommendation")
                    color: theme.text
                    font.family: "Segoe UI Variable"
                    font.pixelSize: 16
                    font.bold: true
                    elide: Text.ElideRight
                }

                Rectangle {
                    Layout.preferredWidth: 76
                    Layout.preferredHeight: 28
                    radius: 8
                    color: Qt.rgba(decisionCard.actionColor(localizationController.trCn(portfolioController.selectedAction)).r,
                                   decisionCard.actionColor(localizationController.trCn(portfolioController.selectedAction)).g,
                                   decisionCard.actionColor(localizationController.trCn(portfolioController.selectedAction)).b,
                                   0.16)
                    border.color: decisionCard.actionColor(localizationController.trCn(portfolioController.selectedAction))

                    Text {
                        anchors.centerIn: parent
                        text: localizationController.trCn(portfolioController.selectedAction)
                        color: decisionCard.actionColor(localizationController.trCn(portfolioController.selectedAction))
                        font.family: "Segoe UI Variable"
                        font.pixelSize: 14
                        font.bold: true
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                text: localizationController.trCn(portfolioController.selectedPositionPlan)
                color: theme.muted
                font.family: "Segoe UI Variable"
                font.pixelSize: 13
                lineHeight: 1.2
                wrapMode: Text.WordWrap
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 8
                rowSpacing: 8

                CompactMetric {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 58
                    label: localizationController.tr("pb.label.tech")
                    value: Number(portfolioController.selectedTechnicalScore).toFixed(0)
                    valueColor: theme.accent
                }
                CompactMetric {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 58
                    label: localizationController.tr("pb.label.fundamental")
                    value: Number(portfolioController.selectedFundamentalScore).toFixed(0)
                    valueColor: theme.cyan
                }
                CompactMetric {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 58
                    label: localizationController.tr("pb.label.risk")
                    value: Number(portfolioController.selectedRiskScore).toFixed(0)
                    valueColor: portfolioController.selectedRiskScore >= 60 ? theme.positive : theme.negative
                }
                CompactMetric {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 58
                    label: localizationController.tr("pb.label.drawdown")
                    value: Number(portfolioController.selectedMaxDrawdownPct).toFixed(1) + "%"
                    valueColor: portfolioController.selectedMaxDrawdownPct <= -5 ? theme.negative : theme.positive
                }
                CompactMetric {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 58
                    label: localizationController.tr("pb.label.volatility")
                    value: Number(portfolioController.selectedVolatilityPct).toFixed(1) + "%"
                    valueColor: theme.text
                }
                CompactMetric {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 58
                    label: localizationController.tr("pb.label.ai")
                    value: Number(portfolioController.selectedAiScore).toFixed(0)
                    valueColor: theme.cyan
                }
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
        color: "#3a0a0a"
        border.color: "#ff4d4d"
        border.width: 1

        Text {
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            text: "⚠ 本页为占位示例，未接入真实账户与持仓数据，所有数字仅作界面演示，不构成投资建议。"
            color: "#ff9b9b"
            font.family: "Segoe UI Variable"
            font.pixelSize: 13
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }

    RowLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: mockBanner.bottom
        anchors.bottom: parent.bottom
        anchors.margins: 14
        spacing: 14

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredWidth: 980
            Layout.minimumWidth: 620
            Layout.fillHeight: true
            radius: 10
            color: theme.panel
            border.color: theme.border
            clip: true

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 18
                spacing: 14

                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 64
                    Layout.minimumHeight: 64
                    Layout.maximumHeight: 64
                    spacing: 16

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        Text {
                            Layout.fillWidth: true
                            text: localizationController.tr("window.title")
                            color: theme.text
                            font.family: "Segoe UI Variable"
                            font.pixelSize: 24
                            font.bold: true
                            elide: Text.ElideRight
                        }
                        Text {
                            Layout.fillWidth: true
                            text: localizationController.trCn(portfolioController.status)
                            color: theme.muted
                            font.family: "Segoe UI Variable"
                            font.pixelSize: 13
                            elide: Text.ElideRight
                        }
                    }

                    Rectangle {
                        Layout.preferredWidth: 142
                        Layout.preferredHeight: 36
                        radius: 8
                        color: "#10283b"
                        border.color: "#245d8f"
                        Text {
                            anchors.centerIn: parent
                            text: localizationController.tr("pb.section.live")
                            color: theme.accent
                            font.family: "Segoe UI Variable"
                            font.pixelSize: 13
                            font.bold: true
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 78
                    Layout.minimumHeight: 78
                    Layout.maximumHeight: 78
                    spacing: 10

                    MetricTile {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 78
                        Layout.maximumHeight: 78
                        label: localizationController.tr("pb.label.symbol")
                        value: portfolioController.selectedSymbol.length > 0 ? portfolioController.selectedSymbol : "--"
                        valueColor: theme.accent
                    }
                    MetricTile {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 78
                        Layout.maximumHeight: 78
                        label: localizationController.tr("pb.label.last")
                        value: Number(portfolioController.selectedLastPrice).toFixed(3)
                    }
                    MetricTile {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 78
                        Layout.maximumHeight: 78
                        label: localizationController.tr("pb.label.recent1h")
                        value: (portfolioController.selectedOneHourChangePct >= 0 ? "+" : "")
                            + Number(portfolioController.selectedOneHourChangePct).toFixed(2) + "%"
                        valueColor: portfolioController.selectedOneHourChangePct >= 0 ? theme.positive : theme.negative
                    }
                    MetricTile {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 78
                        Layout.maximumHeight: 78
                        label: localizationController.tr("pb.label.aiScore")
                        value: Number(portfolioController.selectedAiScore).toFixed(1)
                        valueColor: theme.cyan
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 8
                    color: theme.panelSoft
                    border.color: theme.border
                    clip: true

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 0

                        HorizontalHeaderView {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 50
                            syncView: holdingsTable
                            model: portfolioController.holdingsModel
                            clip: true

                            delegate: Rectangle {
                                implicitHeight: 50
                                color: "#0b1622"
                                border.color: theme.line

                                Text {
                                    anchors.fill: parent
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 12
                                    verticalAlignment: Text.AlignVCenter
                                    text: localizationController.trCn(String(display))
                                    color: theme.muted
                                    font.family: "Segoe UI Variable"
                                    font.pixelSize: 13
                                    font.bold: true
                                    elide: Text.ElideRight
                                }
                            }
                        }

                        TableView {
                            id: holdingsTable
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            model: portfolioController.holdingsModel
                            boundsBehavior: Flickable.StopAtBounds
                            ScrollBar.vertical: HiddenScrollBar {}
                            ScrollBar.horizontal: HiddenScrollBar {}

                            columnWidthProvider: function(column) {
                                const ratios = [0.16, 0.11, 0.08, 0.13, 0.10, 0.09, 0.07, 0.08, 0.18]
                                let total = 0
                                for (let i = 0; i < ratios.length; ++i) total += ratios[i]
                                return Math.max(width * ratios[column] / total, 70)
                            }
                            rowHeightProvider: function() { return 56 }

                            delegate: Rectangle {
                                implicitWidth: holdingsTable.columnWidthProvider(column)
                                implicitHeight: 56
                                color: row % 2 === 0 ? theme.rowEven : theme.rowOdd
                                border.color: theme.line

                                Text {
                                    anchors.fill: parent
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 12
                                    verticalAlignment: Text.AlignVCenter
                                    text: localizationController.trCn(String(display))
                                    color: String(display).startsWith("+") ? theme.positive
                                        : (String(display).startsWith("-") ? theme.negative : theme.text)
                                    font.family: "Segoe UI Variable"
                                    font.pixelSize: 13
                                    elide: Text.ElideRight
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: portfolioController.selectHolding(row)
                                }
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.preferredWidth: 340
            Layout.minimumWidth: 300
            Layout.maximumWidth: 380
            Layout.fillHeight: true
            radius: 10
            color: theme.panel
            border.color: theme.border
            clip: true

            Flickable {
                id: detailsView
                anchors.fill: parent
                anchors.margins: 14
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                contentWidth: width
                contentHeight: detailsColumn.implicitHeight
                ScrollBar.vertical: HiddenScrollBar {}

                Column {
                    id: detailsColumn
                    width: detailsView.width
                    spacing: 12

                    Rectangle {
                        id: addForm
                        width: parent.width
                        height: addForm.implicitHeight
                        implicitHeight: Math.max(154, addFormContent.implicitHeight + 24)
                        radius: 8
                        color: theme.panelRaised
                        border.color: theme.border

                        ColumnLayout {
                            id: addFormContent
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 10

                            Text {
                                Layout.fillWidth: true
                                text: localizationController.tr("pb.section.add")
                                color: theme.text
                                font.family: "Segoe UI Variable"
                                font.pixelSize: 15
                                font.bold: true
                                elide: Text.ElideRight
                            }

                            TextField {
                                id: symbolInput
                                Layout.fillWidth: true
                                Layout.preferredHeight: 38
                                placeholderText: localizationController.tr("pb.field.symbol")
                                placeholderTextColor: theme.faint
                                selectByMouse: true
                                color: theme.text
                                font.family: "Segoe UI Variable"
                                font.pixelSize: 13
                                leftPadding: 10
                                rightPadding: 8
                                background: Rectangle {
                                    radius: 8
                                    color: theme.background
                                    border.color: symbolInput.activeFocus ? theme.accent : theme.border
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 38
                                spacing: 8

                                ComboBox {
                                    id: institutionBox
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    model: portfolioController.institutionOptions
                                    onCurrentTextChanged: window.selectedInstitution = currentText
                                    font.family: "Segoe UI Variable"
                                    font.pixelSize: 12
                                    contentItem: Text {
                                        text: institutionBox.displayText
                                        color: theme.text
                                        font: institutionBox.font
                                        leftPadding: 10
                                        rightPadding: 20
                                        verticalAlignment: Text.AlignVCenter
                                        elide: Text.ElideRight
                                    }
                                    indicator: Text {
                                        x: institutionBox.width - width - 8
                                        y: (institutionBox.height - height) / 2
                                        text: "▾"
                                        color: theme.muted
                                        font.pixelSize: 12
                                    }
                                    background: Rectangle {
                                        radius: 8
                                        color: theme.background
                                        border.color: institutionBox.activeFocus ? theme.accent : theme.border
                                    }
                                    delegate: ItemDelegate {
                                        width: institutionBox.width
                                        height: 34
                                        padding: 0
                                        highlighted: institutionBox.highlightedIndex === index
                                        contentItem: Text {
                                            leftPadding: 10
                                            rightPadding: 10
                                            text: modelData
                                            color: highlighted ? "#ffffff" : theme.text
                                            font.family: "Segoe UI Variable"
                                            font.pixelSize: 13
                                            verticalAlignment: Text.AlignVCenter
                                            elide: Text.ElideRight
                                        }
                                        background: Rectangle {
                                            color: highlighted ? "#1578c9" : (hovered ? "#14263a" : theme.panelRaised)
                                        }
                                    }
                                    popup: Popup {
                                        y: institutionBox.height + 4
                                        width: institutionBox.width
                                        implicitHeight: Math.min(contentItem.implicitHeight + 2, 260)
                                        padding: 1
                                        modal: false
                                        dim: false
                                        background: Rectangle {
                                            radius: 8
                                            color: theme.panelRaised
                                            border.color: theme.accent
                                        }
                                        contentItem: ListView {
                                            clip: true
                                            implicitHeight: contentHeight
                                            model: institutionBox.popup.visible ? institutionBox.delegateModel : null
                                            currentIndex: institutionBox.highlightedIndex
                                            boundsBehavior: Flickable.StopAtBounds
                                            highlightMoveDuration: 0
                                            highlightResizeDuration: 0
                                            Rectangle {
                                                anchors.fill: parent
                                                z: -1
                                                color: theme.panelRaised
                                            }
                                            ScrollBar.vertical: HiddenScrollBar {}
                                        }
                                    }
                                }

                                Button {
                                    id: addHoldingButton
                                    Layout.preferredWidth: 72
                                    Layout.fillHeight: true
                                    text: localizationController.tr("pb.action.new")
                                    font.family: "Segoe UI Variable"
                                    font.pixelSize: 13
                                    font.bold: true
                                    onClicked: {
                                        if (portfolioController.addHolding(symbolInput.text, institutionBox.currentText)) {
                                            symbolInput.clear()
                                        }
                                    }
                                    background: Rectangle {
                                        radius: 8
                                        color: addHoldingButton.down ? "#1c6da1" : "#13314a"
                                        border.color: theme.accent
                                    }
                                    contentItem: Text {
                                        text: addHoldingButton.text
                                        color: theme.text
                                        font: addHoldingButton.font
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                }
                            }
                        }
                    }

                    Rectangle {
                        width: parent.width
                        height: 116
                        radius: 8
                        color: theme.panelRaised
                        border.color: theme.border

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 8

                            Text {
                                Layout.fillWidth: true
                                text: portfolioController.selectedName.length > 0
                                    ? localizationController.trCn(portfolioController.selectedName) + "  " + portfolioController.selectedSymbol
                                    : localizationController.tr("pb.placeholder.select")
                                color: theme.text
                                font.family: "Segoe UI Variable"
                                font.pixelSize: 20
                                font.bold: true
                                elide: Text.ElideRight
                            }
                            Text {
                                Layout.fillWidth: true
                                text: portfolioController.selectedType.length > 0
                                    ? localizationController.trCn(portfolioController.selectedType) + " | " + localizationController.trCn(portfolioController.selectedInstitution)
                                    : localizationController.tr("pb.section.detail")
                                color: theme.muted
                                font.family: "Segoe UI Variable"
                                font.pixelSize: 13
                                elide: Text.ElideRight
                            }
                            Text {
                                Layout.fillWidth: true
                                text: localizationController.tr("pb.prefix.suggest") + (portfolioController.selectedSuggestion.length > 0
                                    ? localizationController.trCn(portfolioController.selectedSuggestion)
                                    : localizationController.tr("pb.placeholder.waiting"))
                                color: theme.cyan
                                font.family: "Segoe UI Variable"
                                font.pixelSize: 13
                                font.bold: true
                                elide: Text.ElideRight
                            }
                        }
                    }

                    DecisionCard {
                        width: parent.width
                        height: implicitHeight
                    }

                    TrendCard {
                        width: parent.width
                        height: 156
                        titleText: localizationController.tr("pb.section.live1hCurve")
                        lineColor: theme.accent
                        series: portfolioController.selectedOneHourTrend
                    }

                    TrendCard {
                        width: parent.width
                        height: 156
                        titleText: localizationController.tr("pb.section.industryNext1m")
                        lineColor: theme.positive
                        series: portfolioController.selectedOneMonthIndustryTrend
                    }

                    InfoBlock {
                        width: parent.width
                        height: implicitHeight
                        titleText: localizationController.tr("pb.section.analysis")
                        bodyText: portfolioController.selectedAnalysis.length > 0
                            ? localizationController.trCn(portfolioController.selectedAnalysis)
                            : localizationController.tr("pb.placeholder.analysis")
                    }

                    InfoBlock {
                        width: parent.width
                        height: implicitHeight
                        titleText: localizationController.tr("pb.section.industry")
                        bodyText: portfolioController.selectedIndustryOutlook.length > 0
                            ? localizationController.trCn(portfolioController.selectedIndustryOutlook)
                            : localizationController.tr("pb.placeholder.industry")
                    }

                    InfoBlock {
                        width: parent.width
                        height: implicitHeight
                        titleText: localizationController.tr("pb.section.fundamentals")
                        bodyText: localizationController.trCn(portfolioController.selectedFinancialSignal)
                    }

                    InfoBlock {
                        width: parent.width
                        height: implicitHeight
                        titleText: localizationController.tr("pb.section.market")
                        bodyText: localizationController.trCn(portfolioController.selectedMarketSignal)
                    }

                    InfoBlock {
                        width: parent.width
                        height: implicitHeight
                        titleText: localizationController.tr("pb.section.forecast")
                        bodyText: localizationController.trCn(portfolioController.selectedForecastSignal) + "\n" + localizationController.trCn(portfolioController.selectedStopLossPlan) + "\n" + localizationController.trCn(portfolioController.selectedTakeProfitPlan)
                    }
                }
            }
        }
    }
}
