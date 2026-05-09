import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window
    width: 1400
    height: 900
    minimumWidth: 1120
    minimumHeight: 760
    visible: false
    title: localizationController.tr("window.title")
    color: "#0d131a"
    flags: Qt.Tool | Qt.FramelessWindowHint

    property int selectedInstitutionRow: -1
    property int selectedFundRow: 0
    property int selectedStockRow: -1
    property var domesticIndexRows: [
        { name: "上证指数", code: "000001.SH", value: "3,087.42", change: 0.62, breadth: 58, tone: "温和修复", note: "权重蓝筹企稳，成交量较前一交易日放大。", series: [48, 49, 51, 50, 53, 55, 54, 56, 58, 60, 59, 61] },
        { name: "深证成指", code: "399001.SZ", value: "9,654.18", change: 0.94, breadth: 61, tone: "风险偏好回升", note: "成长板块回暖，资金向科技和先进制造扩散。", series: [42, 44, 43, 47, 50, 52, 55, 54, 57, 59, 62, 64] },
        { name: "创业板指", code: "399006.SZ", value: "1,901.35", change: 1.18, breadth: 64, tone: "弹性增强", note: "新能源、医药和成长风格同步反弹。", series: [40, 39, 43, 46, 45, 49, 52, 55, 57, 60, 63, 66] },
        { name: "科创50", code: "000688.SH", value: "812.76", change: 1.47, breadth: 67, tone: "科技强势", note: "半导体、AI 算力链带动科创情绪走强。", series: [35, 37, 42, 45, 48, 52, 55, 59, 61, 65, 69, 72] },
        { name: "沪深300", code: "000300.SH", value: "3,642.09", change: 0.38, breadth: 53, tone: "核心资产企稳", note: "金融、消费权重保持稳定，组合底仓信号改善。", series: [51, 50, 52, 53, 54, 53, 55, 56, 57, 56, 58, 59] },
        { name: "中证500", code: "000905.SH", value: "5,612.44", change: 0.81, breadth: 60, tone: "中盘扩散", note: "中盘景气方向回升，市场赚钱效应扩散。", series: [45, 46, 48, 47, 50, 52, 53, 55, 56, 58, 61, 62] },
        { name: "恒生科技", code: "HSTECH.HK", value: "3,982.11", change: -0.42, breadth: 44, tone: "港股承压", note: "互联网权重震荡，关注南向资金和美元利率。", series: [62, 61, 59, 60, 57, 55, 56, 53, 51, 52, 50, 49] },
        { name: "中证全指", code: "000985.SH", value: "4,728.36", change: 0.73, breadth: 59, tone: "整体偏暖", note: "全市场宽度改善，适合提高观察仓位。", series: [44, 45, 47, 49, 50, 52, 51, 54, 56, 58, 57, 60] }
    ]
    property string domesticIndexStatus: "正在获取真实行情"
    property var capitalFlowRows: seedCapitalFlowRows()
    property var rotationRows: seedRotationRows()
    property var valuationRows: seedValuationRows()
    property var earningsRows: seedEarningsRows()
    property var fundLookthroughRows: seedFundLookthroughRows()
    property var riskAlertRows: seedRiskAlertRows()
    property var portfolioDiagnosticRows: seedPortfolioDiagnosticRows()
    property var tradePlanRows: seedTradePlanRows()
    property var sectorMoneyFlowRows: []
    property var cninfoAnnouncementRows: []
    property string sectorMoneyFlowStatus: "行业资金流：正在获取东方财富真实数据"
    property string cninfoStatus: "财报公告：正在获取巨潮资讯公开公告"
    property var marketBoardPages: [
        { key: "mb.tab.institutions", color: "#58b7ff", bg: "#102b3f", soft: "#132331" },
        { key: "mb.tab.usWatch",      color: "#7cc7ff", bg: "#133149", soft: "#142532" },
        { key: "mb.tab.indices",      color: "#5cd7a7", bg: "#123629", soft: "#142820" },
        { key: "mb.tab.flow",         color: "#42d392", bg: "#113525", soft: "#13271f" },
        { key: "mb.tab.rotation",     color: "#ffd56a", bg: "#3a3216", soft: "#292617" },
        { key: "mb.tab.valuation",    color: "#ffb86b", bg: "#3b2818", soft: "#2a2219" },
        { key: "mb.tab.earnings",     color: "#b18cff", bg: "#2b2444", soft: "#211f32" },
        { key: "mb.tab.fundLookthrough", color: "#6ee7f9", bg: "#15363b", soft: "#16282c" },
        { key: "mb.tab.riskAlerts",   color: "#ff8f7b", bg: "#3b201d", soft: "#2a1f1d" },
        { key: "mb.tab.diagnostics",  color: "#9ee493", bg: "#24351e", soft: "#1d2a1b" },
        { key: "mb.tab.tradePlans",   color: "#f78fb3", bg: "#3a2030", soft: "#2a1d26" }
    ]

    function formatIndexValue(value) {
        return Number(value).toLocaleString(Qt.locale("en_US"), "f", 2)
    }

    function indexRiskLabel(change, breadth) {
        if (change < -1.2 || breadth < 38) return "偏高"
        if (change > 1.0 && breadth > 65) return "追高"
        if (breadth >= 52) return "可控"
        return "观察"
    }

    function tomorrowBias(change, breadth, momentum) {
        if (change >= 0.8 && breadth >= 60 && momentum >= 62) return "明日偏强"
        if (change <= -0.8 || breadth <= 42) return "明日防守"
        if (change >= 0.2 && breadth >= 52) return "震荡上行"
        return "震荡整理"
    }

    function metricColor(value, goodLine, weakLine) {
        return value >= goodLine ? theme.positive : (value <= weakLine ? theme.negative : theme.caution)
    }

    function referenceStatus(value) {
        if (String(value).indexOf("待接入") >= 0 || String(value).indexOf("预警") >= 0) return "warn"
        if (String(value).indexOf("卖出") >= 0 || String(value).indexOf("清仓") >= 0 || String(value).indexOf("防守") >= 0) return "danger"
        if (String(value).indexOf("加仓") >= 0 || String(value).indexOf("强") >= 0 || String(value).indexOf("健康") >= 0) return "good"
        return "neutral"
    }

    function referenceColor(status) {
        if (status === "good") return theme.positive
        if (status === "danger") return theme.negative
        if (status === "warn") return theme.caution
        return theme.accent
    }

    function boundedScore(value) {
        return Math.max(0, Math.min(100, Math.round(Number(value || 0))))
    }

    function scoreStatus(score) {
        if (score >= 75) return "good"
        if (score <= 45) return "danger"
        if (score <= 60) return "warn"
        return "neutral"
    }

    function metric(label, value) {
        return { label: label, value: value }
    }

    function seedCapitalFlowRows() {
        return [
            { title: "北向资金", value: "待接入沪深港通", action: "暂不决策", score: 0, detail: "需接入沪深港通真实资金源，未接入前不参与买卖评分。", status: "warn", source: "待接入真实资金源", metrics: [metric("当日净买入", "待接入"), metric("5日趋势", "待接入"), metric("行业集中", "待接入")] },
            { title: "主力净流入", value: "待接入逐笔资金", action: "等待确认", score: 0, detail: "接入后按个股、行业、宽基展示净流入、连续性、尾盘回流强度。", status: "warn", source: "待接入真实资金流", metrics: [metric("净流入率", "待接入"), metric("连续性", "待接入"), metric("尾盘强度", "待接入")] },
            { title: "市场热度", value: "等待行情刷新", action: "控制仓位", score: 50, detail: "由真实指数上涨家数/下跌家数、涨跌幅和宽基成交额推导。", status: "neutral", source: "等待东方财富行情", metrics: [metric("市场宽度", "-"), metric("平均涨跌", "-"), metric("成交额样本", "-")] },
            { title: "行业资金热度", value: "等待行业资金流", action: "等待确认", score: 50, detail: "东方财富行业资金流返回后展示主力净流入、净占比、超大单。", status: "neutral", source: "等待东方财富资金流", metrics: [metric("强行业", "-"), metric("弱行业", "-"), metric("样本数", "-")] }
        ]
    }

    function seedRotationRows() {
        const rows = []
        for (let i = 0; i < domesticIndexRows.length; ++i) {
            const row = domesticIndexRows[i]
            rows.push({
                title: row.name,
                value: row.tone || "轮动观察",
                action: Number(row.change || 0) >= 0 ? "跟进观察" : "等待修复",
                score: boundedScore(Number(row.breadth || 50)),
                detail: "按涨跌幅、宽度、动量、拥挤度判断强弱方向和轮动动作。",
                status: Number(row.change || 0) >= 0 ? "neutral" : "warn",
                source: "本地基准，等待真实行情覆盖",
                metrics: [metric(localizationController.tr("mb.col.change"), Number(row.change || 0).toFixed(2) + "%"), metric("宽度", (row.breadth || 0) + "%"), metric("拥挤度", "待刷新")]
            })
        }
        return rows
    }

    function seedValuationRows() {
        return [
            { title: "PE 分位", value: "待接入真实估值源", action: "不参与估值评分", score: 0, detail: "PE 分位 <25% 低估，25%-75% 合理，>75% 高估。", status: "warn", source: "待接入指数估值库", metrics: [metric("PE", "待接入"), metric("分位", "待接入"), metric("结论", "待接入")] },
            { title: "PB 分位", value: "待接入真实估值源", action: "等待估值源", score: 0, detail: "PB 分位结合 ROE 趋势，避免低 PB 价值陷阱。", status: "warn", source: "待接入指数估值库", metrics: [metric("PB", "待接入"), metric("ROE", "待接入"), metric("安全边际", "待接入")] },
            { title: "股债性价比", value: "待接入利率数据", action: "等待确认", score: 0, detail: "盈利收益率 - 10 年国债收益率，差值越高股票吸引力越强。", status: "warn", source: "待接入利率/盈利收益率", metrics: [metric("盈利收益率", "待接入"), metric("10Y国债", "待接入"), metric("利差", "待接入")] },
            { title: "宽基温度", value: "等待行情刷新", action: "分批观察", score: 50, detail: "真实估值源接入前，以市场宽度和涨跌幅做临时温度计。", status: "neutral", source: "等待真实行情推导", metrics: [metric("市场宽度", "-"), metric("强指数", "-"), metric("热度分", "-")] }
        ]
    }

    function seedEarningsRows() {
        return [
            { title: "业绩预告", value: "等待巨潮公告", action: "事件前降风险", score: 50, detail: "预增/扭亏提高评分，预减/亏损扩大触发减仓观察。", status: "neutral", source: "巨潮资讯公开公告", metrics: [metric("预告类型", "等待"), metric("同比", "等待"), metric("影响", "等待")] },
            { title: "财报发布日期", value: "等待公告日历", action: "生成提醒", score: 50, detail: "未来 7 天披露的重仓标的提高事件权重，仓位不宜过满。", status: "neutral", source: "巨潮资讯公开公告", metrics: [metric("最近披露", "等待"), metric("覆盖持仓", "等待"), metric("提醒", "等待")] },
            { title: "超预期标记", value: "待接入一致预期", action: "等待对比", score: 0, detail: "实际利润/收入超一致预期且指引上修，允许跟进或加仓。", status: "warn", source: "待接入一致预期", metrics: [metric("收入偏离", "待接入"), metric("利润偏离", "待接入"), metric("指引", "待接入")] },
            { title: "低预期处理", value: "跌破支撑减仓", action: "触发风控", score: 55, detail: "公告低于预期且价格跌破支撑，先控制损失，再等待二次评估。", status: "warn", source: "规则引擎", metrics: [metric("价格", "破支撑"), metric("财报", "低预期"), metric("动作", "减仓")] }
        ]
    }

    function seedFundLookthroughRows() {
        return [
            { title: "基金重仓行业", value: "请选择基金", action: "先选基金", score: 0, detail: "在价值投资榜选择基金后，结合基金赛道和季报持仓做穿透。", status: "warn", source: "当前基金信息", metrics: [metric("基金", "未选"), metric("赛道", "未选"), metric(localizationController.tr("mb.label.valueScore"), "-")] },
            { title: "重仓股票", value: "待接入真实季报持仓", action: "不伪造持仓", score: 0, detail: "接入后展示名称、涨跌幅、持仓占比、较上季度变化。", status: "warn", source: "待接入基金季报", metrics: [metric("前十持仓", "待接入"), metric(localizationController.tr("mb.col.change"), "待接入"), metric("季度变化", "待接入")] },
            { title: "持仓集中度", value: "待接入", action: "识别抱团风险", score: 0, detail: "前十重仓占比 >60% 记为集中，第一大重仓 >10% 提示风险。", status: "warn", source: "待接入基金季报", metrics: [metric("前十占比", "待接入"), metric("第一重仓", "待接入"), metric("风险等级", "待接入")] },
            { title: "风格漂移", value: "待持仓验证", action: "检查一致性", score: 0, detail: "基金名称/赛道与真实重仓行业偏离过大时提示风格漂移风险。", status: "warn", source: "待接入基金季报", metrics: [metric("申明赛道", "-"), metric("真实行业", "待接入"), metric("漂移", "待计算")] }
        ]
    }

    function seedRiskAlertRows() {
        const rows = []
        for (let i = 0; i < domesticIndexRows.length && i < 6; ++i) {
            const row = domesticIndexRows[i]
            const risk = indexRiskLabel(Number(row.change || 0), Number(row.breadth || 0))
            rows.push({
                title: row.name,
                value: risk,
                action: risk === "偏高" || risk === "追高" ? "降风险" : "可持有",
                score: risk === "可控" ? 75 : 55,
                detail: "风控逻辑：跌破支撑、宽度下降、追高拥挤、趋势破位时触发减仓。",
                status: risk === "偏高" || risk === "追高" ? "danger" : (risk === "观察" ? "warn" : "good"),
                source: "本地基准，等待真实行情覆盖",
                metrics: [metric("支撑", row.support || "-"), metric("压力", row.resistance || "-"), metric("宽度", (row.breadth || 0) + "%")]
            })
        }
        return rows
    }

    function seedPortfolioDiagnosticRows() {
        return [
            { title: "当前选择", value: "未选择标的", action: "普通观察", score: 0, detail: "价值分、近 1 年表现、市场状态共同决定组合动作。", status: "warn", source: "价值投资榜", metrics: [metric(localizationController.tr("mb.label.valueScore"), "-"), metric("近1年", "-"), metric("类型", "-")] },
            { title: "市场匹配度", value: "等待行情刷新", action: "控制暴露", score: 50, detail: "市场宽度越高，组合进攻仓位越容易获得胜率。", status: "neutral", source: "等待真实行情推导", metrics: [metric("宽度", "-"), metric("平均涨跌", "-"), metric("热度分", "-")] },
            { title: "相关性", value: "待接入持仓明细", action: "计算集中风险", score: 0, detail: "需要组合全部持仓和历史收益序列计算相关性矩阵。", status: "warn", source: "待接入组合持仓", metrics: [metric("相关性", "待接入"), metric("行业暴露", "待接入"), metric("集中度", "待接入")] },
            { title: "仓位建议", value: "中性仓", action: "控制仓", score: 50, detail: "仓位由市场热度和标的质量共同决定。", status: "neutral", source: "规则引擎", metrics: [metric("热度分", "-"), metric(localizationController.tr("mb.label.valueScore"), "-"), metric("仓位", "控制")] }
        ]
    }

    function seedTradePlanRows() {
        return [
            { title: "动作建议", value: "跟进", action: "跟进", score: 50, detail: "基于价值分、近 1 年收益、市场宽度生成。", status: "neutral", source: "规则引擎", metrics: [metric(localizationController.tr("mb.label.valueScore"), "-"), metric("市场宽度", "-"), metric("近1年", "-")] },
            { title: "卖出条件", value: "破位或价值分 < 62", action: "卖出", score: 35, detail: "跌破关键支撑、市场宽度低于 38%、基本面恶化时触发。", status: "danger", source: "规则", metrics: [metric("价值阈值", "<62"), metric("宽度阈值", "<38%"), metric("趋势", "破位")] },
            { title: "跟进条件", value: "价值分 70-85", action: "跟进", score: 65, detail: "等待回踩支撑、资金确认、财报风险释放。", status: "neutral", source: "规则", metrics: [metric("价值区间", "70-85"), metric("资金", "确认"), metric("事件", "释放")] },
            { title: "加仓条件", value: "价值分 > 86 且市场偏强", action: "加仓", score: 70, detail: "分批执行，单次加仓不超过计划仓位上限。", status: "good", source: "规则", metrics: [metric("价值阈值", ">86"), metric("宽度阈值", ">=58%"), metric("方式", "分批")] }
        ]
    }

    function averageMarketBreadth() {
        if (!domesticIndexRows.length) return 0
        let total = 0
        for (let i = 0; i < domesticIndexRows.length; ++i) total += Number(domesticIndexRows[i].breadth || 0)
        return Math.round(total / domesticIndexRows.length)
    }

    function averageMarketChange() {
        if (!domesticIndexRows.length) return 0
        let total = 0
        for (let i = 0; i < domesticIndexRows.length; ++i) total += Number(domesticIndexRows[i].change || 0)
        return total / domesticIndexRows.length
    }

    function rebuildInvestmentReferencePages() {
        const breadth = averageMarketBreadth()
        const change = averageMarketChange()
        const selectedName = localizationController.trCn(marketBoardController.selectedAssetName) || "未选择标的"
        const selectedScore = Number(marketBoardController.selectedAssetScore || 0)
        const selectedReturn = Number(marketBoardController.selectedAssetOneYearReturn || 0)
        const selectedKind = localizationController.trCn(marketBoardController.selectedAssetKind) || ""
        const marketBias = breadth >= 58 && change >= 0.2 ? "市场偏强" : (breadth <= 42 || change <= -0.8 ? "市场防守" : "震荡观察")
        let totalTurnover = 0
        let strongCount = 0
        let weakCount = 0
        let positiveCount = 0
        let bestIndexName = "-"
        let worstIndexName = "-"
        let bestIndexChange = -999
        let worstIndexChange = 999
        for (let rowIndex = 0; rowIndex < domesticIndexRows.length; ++rowIndex) {
            const row = domesticIndexRows[rowIndex]
            const rowChange = Number(row.change || 0)
            const rowBreadth = Number(row.breadth || 0)
            totalTurnover += Number(row.turnover || 0)
            if (rowChange > 0) positiveCount += 1
            if (rowChange > 0.5 && rowBreadth >= 55) strongCount += 1
            if (rowChange < -0.5 || rowBreadth <= 42) weakCount += 1
            if (rowChange > bestIndexChange) {
                bestIndexChange = rowChange
                bestIndexName = row.name
            }
            if (rowChange < worstIndexChange) {
                worstIndexChange = rowChange
                worstIndexName = row.name
            }
        }
        const heatScore = boundedScore(breadth * 0.62 + Math.max(-2, Math.min(2, change)) * 12 + Math.min(18, totalTurnover / 1200))
        const attackDefenseRatio = domesticIndexRows.length > 0 ? Math.round(positiveCount * 100 / domesticIndexRows.length) : 0

        capitalFlowRows = [
            { title: "北向资金", value: "待接入沪深港通", action: "暂不决策", score: 0, detail: "指标逻辑：当日净买入、5 日连续性、买入行业集中度；未接入前不参与买卖评分。", status: "warn", source: "待接入真实资金源", metrics: [metric("当日净买入", "待接入"), metric("5日趋势", "待接入"), metric("行业集中", "待接入")] },
            { title: "主力净流入", value: "待接入逐笔资金", action: "等待确认", score: 0, detail: "指标逻辑：主力净流入额/成交额、连续流入天数、尾盘回流强度。", status: "warn", source: "待接入真实资金流", metrics: [metric("净流入率", "待接入"), metric("连续性", "待接入"), metric("尾盘强度", "待接入")] },
            { title: "市场热度", value: breadth + "% 宽度", action: heatScore >= 70 ? "提高仓位" : (heatScore <= 45 ? "降低仓位" : "控制仓位"), score: heatScore, detail: marketBias + "，由真实指数上涨家数/下跌家数、涨跌幅和宽基成交额推导。", status: scoreStatus(heatScore), source: "东方财富行情推导", metrics: [metric("市场宽度", breadth + "%"), metric("平均涨跌", change.toFixed(2) + "%"), metric("成交额样本", totalTurnover + "亿")] },
            { title: "行业资金热度", value: strongCount + " 强 / " + weakCount + " 弱", action: strongCount > weakCount ? "跟随强势" : "偏防守", score: boundedScore(50 + (strongCount - weakCount) * 9 + change * 8), detail: "用宽基和主题指数替代行业资金热度初筛，后续可接入真实行业资金流。", status: strongCount > weakCount ? "good" : (weakCount > strongCount ? "danger" : "neutral"), source: "真实行情推导", metrics: [metric("强指数", strongCount), metric("弱指数", weakCount), metric("样本数", domesticIndexRows.length)] },
            { title: "攻防比例", value: attackDefenseRatio + "% 上涨样本", action: attackDefenseRatio >= 60 ? "偏进攻" : (attackDefenseRatio <= 40 ? "偏防守" : "均衡"), score: boundedScore(attackDefenseRatio), detail: "用上涨宽基数量/样本数量衡量风险偏好，帮助判断是否适合追随资金。", status: scoreStatus(attackDefenseRatio), source: "真实行情推导", metrics: [metric("上涨样本", positiveCount), metric("总样本", domesticIndexRows.length), metric("最强", bestIndexName)] },
            { title: "资金强弱锚", value: bestIndexName + " / " + worstIndexName, action: "强换弱", score: boundedScore(50 + (bestIndexChange - worstIndexChange) * 10), detail: "最强指数作为跟踪方向，最弱指数作为回避或等待修复方向。", status: bestIndexChange - worstIndexChange > 1.2 ? "good" : "neutral", source: "真实行情推导", metrics: [metric("最强涨跌", bestIndexChange.toFixed(2) + "%"), metric("最弱涨跌", worstIndexChange.toFixed(2) + "%"), metric("强弱差", (bestIndexChange - worstIndexChange).toFixed(2) + "%")] }
        ]
        for (let flowIndex = 0; flowIndex < sectorMoneyFlowRows.length && flowIndex < 8; ++flowIndex) {
            const flow = sectorMoneyFlowRows[flowIndex]
            const flowScore = boundedScore(50 + Number(flow.netMainPct || 0) * 8 + (flowIndex < 3 ? 10 : 0))
            capitalFlowRows.push({
                title: "行业资金：" + flow.name,
                value: (Number(flow.netMain || 0) / 100000000).toFixed(2) + "亿",
                action: Number(flow.netMain || 0) >= 0 ? "资金流入跟踪" : "资金流出回避",
                score: flowScore,
                detail: "主力净流入、超大单、大单和净占比来自公开资金流接口，用于识别当日资金偏好。",
                status: Number(flow.netMain || 0) >= 0 ? scoreStatus(flowScore) : "danger",
                source: sectorMoneyFlowStatus,
                metrics: [
                    metric("净占比", Number(flow.netMainPct || 0).toFixed(2) + "%"),
                    metric("超大单", (Number(flow.superFlow || 0) / 100000000).toFixed(2) + "亿"),
                    metric("排名", flow.rank)
                ]
            })
        }

        const sortedRows = domesticIndexRows.slice().sort(function(a, b) {
            return (Number(b.change || 0) + Number(b.breadth || 0) / 100) - (Number(a.change || 0) + Number(a.breadth || 0) / 100)
        })
        rotationRows = sortedRows.map(function(row, index) {
            const crowding = Number(row.breadth || 0) >= 68 && Number(row.change || 0) >= 1.0 ? "偏拥挤" : (Number(row.breadth || 0) <= 42 ? "低迷" : "适中")
            const direction = index < 2 ? "强势跟踪" : (index >= sortedRows.length - 2 ? "弱势规避" : "轮动观察")
            const score = boundedScore(Number(row.breadth || 0) * 0.55 + Number(row.momentum || 50) * 0.30 + Number(row.change || 0) * 10 - (crowding === "偏拥挤" ? 8 : 0))
            return {
                title: row.name,
                value: direction,
                action: direction === "强势跟踪" ? "跟进/低吸" : (direction === "弱势规避" ? "回避/转仓" : "观察"),
                score: score,
                detail: "强弱排序依据：涨跌幅、市场宽度、动量、拥挤度；连续性由实时序列斜率补充判断。",
                status: scoreStatus(score),
                source: "真实行情推导",
                metrics: [metric(localizationController.tr("mb.col.change"), Number(row.change || 0).toFixed(2) + "%"), metric("宽度", (row.breadth || 0) + "%"), metric("拥挤度", crowding)]
            }
        })

        valuationRows = [
            { title: "PE 分位", value: "待接入真实估值源", action: "不参与估值评分", score: 0, detail: "业务逻辑：PE 分位 < 25% 低估，25%-75% 合理，> 75% 高估；未接入前不编造。", status: "warn", source: "待接入指数估值库", metrics: [metric("PE", "待接入"), metric("分位", "待接入"), metric("结论", "待接入")] },
            { title: "PB 分位", value: "待接入真实估值源", action: "等待估值源", score: 0, detail: "业务逻辑：PB 分位结合 ROE 趋势，避免低 PB 价值陷阱。", status: "warn", source: "待接入指数估值库", metrics: [metric("PB", "待接入"), metric("ROE", "待接入"), metric("安全边际", "待接入")] },
            { title: "股债性价比", value: "待接入利率数据", action: "等待确认", score: 0, detail: "业务逻辑：盈利收益率 - 10 年国债收益率，差值越高股票吸引力越强。", status: "warn", source: "待接入利率/盈利收益率", metrics: [metric("盈利收益率", "待接入"), metric("10Y国债", "待接入"), metric("利差", "待接入")] },
            { title: "宽基估值温度替代", value: marketBias, action: marketBias === "市场偏强" ? "只低吸不追高" : (marketBias === "市场防守" ? "等待估值确认" : "分批观察"), score: heatScore, detail: "当前仅以真实行情热度做临时温度计，真正估值分位接入后替换。", status: scoreStatus(heatScore), source: "真实行情推导", metrics: [metric("市场宽度", breadth + "%"), metric("强指数", strongCount), metric("热度分", heatScore)] },
            { title: "追高风险", value: bestIndexChange > 1.2 && breadth > 64 ? "偏拥挤" : "可观察", action: bestIndexChange > 1.2 && breadth > 64 ? "等待回踩" : "分批跟踪", score: boundedScore(100 - Math.max(0, bestIndexChange - 0.5) * 18), detail: "涨幅大且宽度过高时，短线性价比下降，避免在情绪峰值一次性加仓。", status: bestIndexChange > 1.2 && breadth > 64 ? "warn" : "neutral", source: "真实行情推导", metrics: [metric("最强涨幅", bestIndexChange.toFixed(2) + "%"), metric("市场宽度", breadth + "%"), metric("动作", bestIndexChange > 1.2 ? "等回踩" : "观察")] },
            { title: "低吸窗口", value: worstIndexName, action: worstIndexChange > -0.8 && breadth >= 48 ? "观察修复" : "暂缓", score: boundedScore(50 + breadth * 0.4 - Math.abs(worstIndexChange) * 10), detail: "弱指数未破坏且市场宽度不差时，可作为轮动低吸候选；破位则暂缓。", status: worstIndexChange > -0.8 && breadth >= 48 ? "neutral" : "warn", source: "真实行情推导", metrics: [metric("弱项", worstIndexName), metric("弱项涨跌", worstIndexChange.toFixed(2) + "%"), metric("宽度", breadth + "%")] }
        ]

        earningsRows = [
            { title: "业绩预告", value: "待接入交易所/巨潮", action: "事件前降风险", score: 0, detail: "业务逻辑：预增/扭亏提高评分，预减/亏损扩大触发减仓观察。", status: "warn", source: "待接入真实公告源", metrics: [metric("预告类型", "待接入"), metric("同比", "待接入"), metric("影响", "待接入")] },
            { title: "财报发布日期", value: "待接入财报日历", action: "生成提醒", score: 0, detail: "业务逻辑：未来 7 天披露的重仓标的提高事件权重，仓位不宜过满。", status: "warn", source: "待接入财报日历", metrics: [metric("最近披露", "待接入"), metric("覆盖持仓", "待接入"), metric("提醒", "待接入")] },
            { title: "超预期标记", value: "待接入一致预期", action: "等待对比", score: 0, detail: "业务逻辑：实际利润/收入超一致预期且指引上修，允许跟进或加仓。", status: "warn", source: "待接入一致预期", metrics: [metric("收入偏离", "待接入"), metric("利润偏离", "待接入"), metric("指引", "待接入")] },
            { title: "持仓影响", value: selectedName, action: selectedKind === "fund" ? "穿透后计算" : "待财报源", score: selectedScore, detail: selectedKind === "fund" ? "基金需要穿透重仓股后计算财报冲击。" : "股票可在接入财报源后直接计算事件风险。", status: selectedScore >= 80 ? "good" : "neutral", source: "当前选择", metrics: [metric("标的类型", selectedKind || "未选"), metric(localizationController.tr("mb.label.valueScore"), selectedScore.toFixed(1)), metric("近1年", selectedReturn.toFixed(1) + "%")] },
            { title: "财报前仓位规则", value: selectedScore >= 80 ? "可轻仓持有" : "减少事件暴露", action: selectedScore >= 80 ? "保留观察仓" : "降仓等待", score: boundedScore(selectedScore * 0.8), detail: "财报未公布前，高价值分保留观察仓，低价值分避免重仓赌业绩。", status: selectedScore >= 80 ? "good" : "warn", source: "规则引擎", metrics: [metric(localizationController.tr("mb.label.valueScore"), selectedScore.toFixed(1)), metric("事件风险", "中"), metric("仓位", selectedScore >= 80 ? "观察仓" : "轻仓")] },
            { title: "低预期处理", value: "跌破支撑减仓", action: "触发风控", score: 55, detail: "公告低于预期且价格跌破支撑，先控制损失，再等待二次评估。", status: "warn", source: "规则引擎", metrics: [metric("价格", "破支撑"), metric("财报", "低预期"), metric("动作", "减仓")] }
        ]
        for (let annIndex = 0; annIndex < cninfoAnnouncementRows.length && annIndex < 8; ++annIndex) {
            const ann = cninfoAnnouncementRows[annIndex]
            const annTitle = ann.title || "公告"
            const isRisk = annTitle.indexOf("亏") >= 0 || annTitle.indexOf("减") >= 0 || annTitle.indexOf("风险") >= 0
            const isGood = annTitle.indexOf("增") >= 0 || annTitle.indexOf("预盈") >= 0 || annTitle.indexOf("利润") >= 0
            earningsRows.push({
                title: ann.stock || "财报公告",
                value: annTitle,
                action: isRisk ? "降低事件风险" : (isGood ? "跟踪超预期" : "加入日历"),
                score: isRisk ? 35 : (isGood ? 76 : 58),
                detail: "来自巨潮资讯公开公告，作为财报日历和业绩预告事件输入。",
                status: isRisk ? "danger" : (isGood ? "good" : "neutral"),
                source: cninfoStatus,
                metrics: [
                    metric("代码", ann.code || "-"),
                    metric("日期", ann.date || "-"),
                    metric("来源", "巨潮")
                ]
            })
        }

        fundLookthroughRows = [
            { title: "基金重仓行业", value: selectedKind === "fund" ? localizationController.trCn(marketBoardController.selectedAssetCategory) : "请选择基金", action: selectedKind === "fund" ? "查看赛道风险" : "先选基金", score: selectedKind === "fund" ? selectedScore : 0, detail: selectedKind === "fund" ? "已显示基金赛道，行业穿透需接入季报持仓。" : "在价值投资榜选择基金后查看。", status: selectedKind === "fund" ? scoreStatus(selectedScore) : "warn", source: "当前基金信息", metrics: [metric("基金", selectedKind === "fund" ? selectedName : "未选"), metric("赛道", selectedKind === "fund" ? localizationController.trCn(marketBoardController.selectedAssetCategory) : "未选"), metric(localizationController.tr("mb.label.valueScore"), selectedKind === "fund" ? selectedScore.toFixed(1) : "-")] },
            { title: "重仓股票", value: "待接入真实季报持仓", action: "不伪造持仓", score: 0, detail: "接入后展示名称、涨跌幅、持仓占比、较上季度变化，并联动实时涨跌幅。", status: "warn", source: "待接入基金季报", metrics: [metric("前十持仓", "待接入"), metric(localizationController.tr("mb.col.change"), "待接入"), metric("季度变化", "待接入")] },
            { title: "持仓集中度", value: "待接入", action: "识别抱团风险", score: 0, detail: "前十重仓占比 > 60% 记为集中，第一大重仓 > 10% 提示单一标的风险。", status: "warn", source: "待接入基金季报", metrics: [metric("前十占比", "待接入"), metric("第一重仓", "待接入"), metric("风险等级", "待接入")] },
            { title: "季度变化", value: "待接入", action: "识别调仓方向", score: 0, detail: "比较本季度与上季度加仓、减仓、新进、退出，用于判断基金经理最新意图。", status: "warn", source: "待接入基金季报", metrics: [metric("加仓", "待接入"), metric("减仓", "待接入"), metric("新进/退出", "待接入")] },
            { title: "基金风格漂移", value: selectedKind === "fund" ? "待持仓验证" : "请选择基金", action: "检查一致性", score: selectedKind === "fund" ? boundedScore(selectedScore * 0.7) : 0, detail: "基金名称/赛道与真实重仓行业偏离过大时提示风格漂移风险。", status: selectedKind === "fund" ? "neutral" : "warn", source: "当前基金 + 待接入季报", metrics: [metric("申明赛道", selectedKind === "fund" ? localizationController.trCn(marketBoardController.selectedAssetCategory) : "-"), metric("真实行业", "待接入"), metric("漂移", "待计算")] },
            { title: "重仓股实时冲击", value: "待穿透后计算", action: "联动行情", score: 0, detail: "前十重仓股票接入后，会按实时涨跌幅和持仓占比计算基金当日冲击。", status: "warn", source: "待接入基金持仓 + 实时行情", metrics: [metric("权重涨跌", "待接入"), metric("贡献", "待接入"), metric("风险股", "待接入")] }
        ]

        riskAlertRows = domesticIndexRows.slice(0, 6).map(function(row) {
            const risk = row.risk || indexRiskLabel(row.change, row.breadth)
            const score = boundedScore(100 - Math.max(0, Number(row.change || 0) > 1.2 ? 16 : 0) - Math.max(0, 52 - Number(row.breadth || 0)) - (risk === "追高" ? 20 : 0))
            return {
                title: row.name,
                value: risk,
                action: risk === "偏高" || risk === "追高" ? "降风险" : "可持有",
                score: score,
                detail: "风控逻辑：跌破支撑、宽度下降、追高拥挤、趋势破位时触发减仓。",
                status: risk === "偏高" || risk === "追高" ? "danger" : (risk === "观察" ? "warn" : "good"),
                source: "真实行情推导",
                metrics: [metric("支撑", row.support || "-"), metric("压力", row.resistance || "-"), metric("宽度", (row.breadth || 0) + "%")]
            }
        })

        portfolioDiagnosticRows = [
            { title: "当前选择", value: selectedName, action: selectedScore >= 80 ? "核心观察" : "普通观察", score: selectedScore, detail: "价值分、近 1 年表现、市场状态共同决定组合动作。", status: selectedScore >= 80 ? "good" : (selectedScore < 65 ? "warn" : "neutral"), source: "价值投资榜", metrics: [metric(localizationController.tr("mb.label.valueScore"), selectedScore.toFixed(1)), metric("近1年", selectedReturn.toFixed(1) + "%"), metric("类型", selectedKind || "-")] },
            { title: "市场匹配度", value: marketBias, action: breadth >= 58 ? "顺势配置" : "控制暴露", score: heatScore, detail: "市场宽度越高，组合进攻仓位越容易获得胜率；低宽度时强调现金和防守。", status: scoreStatus(heatScore), source: "真实行情推导", metrics: [metric("宽度", breadth + "%"), metric("平均涨跌", change.toFixed(2) + "%"), metric("热度分", heatScore)] },
            { title: "相关性", value: "待接入持仓明细", action: "计算集中风险", score: 0, detail: "需要组合全部持仓和历史收益序列计算相关性矩阵，识别同涨同跌风险。", status: "warn", source: "待接入组合持仓", metrics: [metric("相关性", "待接入"), metric("行业暴露", "待接入"), metric("集中度", "待接入")] },
            { title: "风险收益比", value: selectedScore >= 78 && breadth >= 52 ? "可跟进" : "谨慎评估", action: selectedScore >= 78 && breadth >= 52 ? "跟进" : "等待", score: boundedScore(selectedScore * 0.62 + breadth * 0.38), detail: "由价值分、市场宽度、趋势风险综合给出。", status: selectedScore >= 78 && breadth >= 52 ? "good" : "warn", source: "模型推导", metrics: [metric("价值权重", "62%"), metric("市场权重", "38%"), metric("动作", selectedScore >= 78 && breadth >= 52 ? "跟进" : "等待")] },
            { title: "仓位建议", value: heatScore >= 70 && selectedScore >= 80 ? "中高仓" : (heatScore <= 45 ? "低仓" : "中性仓"), action: heatScore >= 70 && selectedScore >= 80 ? "分批加" : "控制仓", score: boundedScore((heatScore + selectedScore) / 2), detail: "仓位由市场热度和标的质量共同决定，避免只看单个股票或基金。", status: heatScore >= 70 && selectedScore >= 80 ? "good" : (heatScore <= 45 ? "danger" : "neutral"), source: "规则引擎", metrics: [metric("热度分", heatScore), metric(localizationController.tr("mb.label.valueScore"), selectedScore.toFixed(1)), metric("仓位", heatScore >= 70 && selectedScore >= 80 ? "中高" : "控制")] },
            { title: "风格暴露", value: localizationController.trCn(marketBoardController.selectedAssetCategory) || "未选择", action: "检查集中", score: selectedKind.length > 0 ? 62 : 0, detail: "选择基金或股票后，用赛道/行业作为初步暴露，后续接入组合明细计算总暴露。", status: selectedKind.length > 0 ? "neutral" : "warn", source: "价值投资榜", metrics: [metric("标的", selectedName), metric("赛道", localizationController.trCn(marketBoardController.selectedAssetCategory) || "-"), metric("类型", selectedKind || "-")] }
        ]

        let action = "跟进"
        let actionStatus = "neutral"
        if (selectedScore >= 86 && breadth >= 58 && selectedReturn < 35) {
            action = "加仓"
            actionStatus = "good"
        } else if (selectedScore < 62 || breadth <= 38) {
            action = "减仓/清仓"
            actionStatus = "danger"
        } else if (selectedReturn > 55 && breadth < 52) {
            action = "止盈转仓"
            actionStatus = "warn"
        }
        tradePlanRows = [
            { title: "动作建议", value: action, action: action, score: boundedScore(selectedScore * 0.55 + breadth * 0.45), detail: "基于价值分、近 1 年收益、市场宽度生成，需结合个人仓位执行。", status: actionStatus, source: "规则引擎", metrics: [metric(localizationController.tr("mb.label.valueScore"), selectedScore.toFixed(1)), metric("市场宽度", breadth + "%"), metric("近1年", selectedReturn.toFixed(1) + "%")] },
            { title: "卖出条件", value: "破位或价值分 < 62", action: "卖出", score: selectedScore < 62 ? 85 : 35, detail: "跌破关键支撑、市场宽度低于 38%、基本面恶化时触发。", status: "danger", source: "规则", metrics: [metric("价值阈值", "<62"), metric("宽度阈值", "<38%"), metric("趋势", "破位")] },
            { title: "跟进条件", value: "价值分 70-85", action: "跟进", score: selectedScore >= 70 && selectedScore <= 85 ? 78 : 45, detail: "等待回踩支撑、资金确认、财报风险释放。", status: "neutral", source: "规则", metrics: [metric("价值区间", "70-85"), metric("资金", "确认"), metric("事件", "释放")] },
            { title: "加仓条件", value: "价值分 > 86 且市场偏强", action: "加仓", score: selectedScore > 86 && breadth >= 58 ? 90 : 48, detail: "分批执行，单次加仓不超过计划仓位上限。", status: "good", source: "规则", metrics: [metric("价值阈值", ">86"), metric("宽度阈值", ">=58%"), metric("方式", "分批")] },
            { title: "转仓条件", value: "高估/拥挤/弱势", action: "转仓", score: weakCount > strongCount ? 76 : 42, detail: "从弱势或拥挤方向转向低估、趋势改善、风险收益比更优方向。", status: "warn", source: "规则", metrics: [metric("弱指数", weakCount), metric("强指数", strongCount), metric("目标", "强势低估")] },
            { title: "清仓条件", value: "基本面失效", action: "清仓", score: selectedScore < 55 && breadth < 40 ? 92 : 30, detail: "财报低预期、风险事件、趋势破位且反弹无量时执行。", status: "danger", source: "规则", metrics: [metric(localizationController.tr("mb.label.valueScore"), "<55"), metric("市场宽度", "<40%"), metric("事件", "失效")] }
        ]
    }

    function appendRealIndexPoint(row, price) {
        const nextSeries = (row.series || []).slice(Math.max(0, (row.series || []).length - 23))
        nextSeries.push(price)
        return nextSeries.length >= 2 ? nextSeries : [price, price]
    }

    function applyRealIndexQuote(code, name, price, changePct, amount, upCount, downCount, flatCount) {
        const nextRows = domesticIndexRows.slice()
        let rowIndex = -1
        for (let i = 0; i < nextRows.length; ++i) {
            if (nextRows[i].code === code) {
                rowIndex = i
                break
            }
        }
        if (rowIndex < 0 || !isFinite(price) || price <= 0) return

        const row = nextRows[rowIndex]
        const activeCount = Math.max(1, Number(upCount || 0) + Number(downCount || 0) + Number(flatCount || 0))
        const breadth = Math.round(Math.max(0, Math.min(100, Number(upCount || 0) * 100 / activeCount)))
        const momentum = Math.max(1, Math.min(99, Math.round((breadth * 0.58) + ((Number(changePct || 0) + 2.5) * 11))))
        const turnover = Math.max(0, Math.round(Number(amount || 0) / 100000000))
        const support = price * (1 - Math.max(0.006, Math.min(0.025, (100 - breadth) / 3600)))
        const resistance = price * (1 + Math.max(0.008, Math.min(0.032, breadth / 3000)))
        row.value = formatIndexValue(price)
        row.change = Number(changePct || 0)
        row.breadth = breadth
        row.momentum = momentum
        row.turnover = turnover
        row.support = formatIndexValue(support)
        row.resistance = formatIndexValue(resistance)
        row.risk = indexRiskLabel(row.change, breadth)
        row.tomorrow = tomorrowBias(row.change, breadth, momentum)
        row.tone = row.change >= 1.0 ? "强势上行" : (row.change >= 0.2 ? "震荡偏强" : (row.change > -0.6 ? "横盘观察" : "风险降温"))
        row.name = name || row.name
        row.series = appendRealIndexPoint(row, price)
        nextRows[rowIndex] = row
        domesticIndexRows = nextRows
        rebuildInvestmentReferencePages()
    }

    function applySectorMoneyFlow(rows) {
        const nextRows = []
        for (let i = 0; i < rows.length; ++i) {
            const item = rows[i]
            const name = item.f14 || item.name || ""
            if (!name.length) continue
            nextRows.push({
                rank: i + 1,
                code: item.f12 || "",
                name: name,
                netMain: Number(item.f62 || 0),
                netMainPct: Number(item.f184 || 0),
                superFlow: Number(item.f66 || 0),
                bigFlow: Number(item.f72 || 0),
                updatedAt: item.f124 || 0
            })
        }
        sectorMoneyFlowRows = nextRows
        sectorMoneyFlowStatus = "行业资金流：真实数据 " + Qt.formatDateTime(new Date(), "HH:mm:ss") + " 东方财富"
        rebuildInvestmentReferencePages()
    }

    function fetchEastmoneySectorMoneyFlow() {
        const request = new XMLHttpRequest()
        request.onreadystatechange = function() {
            if (request.readyState !== XMLHttpRequest.DONE) return
            if (request.status < 200 || request.status >= 300) {
                sectorMoneyFlowStatus = "行业资金流：获取失败 " + request.status
                rebuildInvestmentReferencePages()
                return
            }
            try {
                const payload = JSON.parse(request.responseText)
                const rows = payload && payload.data && payload.data.diff ? payload.data.diff : []
                applySectorMoneyFlow(rows)
            } catch (error) {
                sectorMoneyFlowStatus = "行业资金流：解析失败"
                rebuildInvestmentReferencePages()
            }
        }
        // Plain http for the same TLS-handshake reason as fetchEastmoneyIndexes.
        request.open("GET", "http://push2.eastmoney.com/api/qt/clist/get?fid=f62&po=1&pz=20&pn=1&np=1&fltt=2&invt=2&fs=m:90+t:2&fields=f12,f14,f62,f184,f66,f72,f124")
        request.send()
    }

    function applyCninfoAnnouncements(rows) {
        const nextRows = []
        for (let i = 0; i < rows.length; ++i) {
            const item = rows[i]
            const title = item.announcementTitle || item.title || ""
            if (!title.length) continue
            const secName = item.secName || item.stockName || ""
            const secCode = item.secCode || item.stockCode || ""
            const timeValue = Number(item.announcementTime || 0)
            const dateText = timeValue > 0 ? Qt.formatDateTime(new Date(timeValue), "yyyy-MM-dd") : ""
            nextRows.push({
                title: title.replace(/<[^>]*>/g, ""),
                stock: secName,
                code: secCode,
                date: dateText
            })
        }
        cninfoAnnouncementRows = nextRows
        cninfoStatus = "财报公告：真实公告 " + Qt.formatDateTime(new Date(), "HH:mm:ss") + " 巨潮资讯"
        rebuildInvestmentReferencePages()
    }

    function fetchCninfoAnnouncements() {
        const request = new XMLHttpRequest()
        request.onreadystatechange = function() {
            if (request.readyState !== XMLHttpRequest.DONE) return
            if (request.status < 200 || request.status >= 300) {
                cninfoStatus = "财报公告：获取失败 " + request.status
                rebuildInvestmentReferencePages()
                return
            }
            try {
                const payload = JSON.parse(request.responseText)
                const rows = payload && payload.announcements ? payload.announcements : []
                applyCninfoAnnouncements(rows)
            } catch (error) {
                cninfoStatus = "财报公告：解析失败"
                rebuildInvestmentReferencePages()
            }
        }
        const body = "pageNum=1&pageSize=20&column=szse&tabName=fulltext&plate=&stock=&searchkey=&secid=&category=category_yjyg_szsh;category_ndbg_szsh;category_bndbg_szsh;category_sjdbg_szsh&trade=&seDate=&sortName=&sortType=&isHLtitle=true"
        request.open("POST", "https://www.cninfo.com.cn/new/hisAnnouncement/query")
        request.setRequestHeader("Content-Type", "application/x-www-form-urlencoded; charset=UTF-8")
        request.setRequestHeader("Accept", "application/json, text/javascript, */*; q=0.01")
        request.send(body)
    }

    function applyFreeDataProviderSnapshot(snapshot) {
        if (!snapshot) return
        const flowRows = snapshot.sectorMoneyFlow || []
        if (flowRows.length > 0) {
            const nextFlowRows = []
            for (let i = 0; i < flowRows.length; ++i) {
                const item = flowRows[i]
                nextFlowRows.push({
                    rank: item.rank || (i + 1),
                    code: item.code || "",
                    name: item.name || "",
                    netMain: Number(item.netMain || 0),
                    netMainPct: Number(item.netMainPct || 0),
                    superFlow: Number(item.superFlow || 0),
                    bigFlow: Number(item.bigFlow || 0),
                    updatedAt: item.updatedAt || 0
                })
            }
            sectorMoneyFlowRows = nextFlowRows
            sectorMoneyFlowStatus = marketBoardController.freeDataStatus.length > 0
                ? localizationController.trCn(marketBoardController.freeDataStatus)
                : "行业资金流：本地免费数据源"
        }

        const announcements = snapshot.announcements || []
        if (announcements.length > 0) {
            cninfoAnnouncementRows = announcements
            cninfoStatus = marketBoardController.freeDataStatus.length > 0
                ? localizationController.trCn(marketBoardController.freeDataStatus)
                : "财报公告：本地免费数据源"
        }
        rebuildInvestmentReferencePages()
    }

    function fetchEastmoneyIndexes(secids) {
        const request = new XMLHttpRequest()
        request.onreadystatechange = function() {
            if (request.readyState !== XMLHttpRequest.DONE) return
            if (request.status < 200 || request.status >= 300) {
                domesticIndexStatus = "真实行情获取失败：" + request.status
                return
            }
            try {
                const payload = JSON.parse(request.responseText)
                const rows = payload && payload.data && payload.data.diff ? payload.data.diff : []
                for (let i = 0; i < rows.length; ++i) {
                    const item = rows[i]
                    const code = item.f12 === "HSTECH" ? "HSTECH.HK" : (item.f12 === "399001" || item.f12 === "399006" ? item.f12 + ".SZ" : item.f12 + ".SH")
                    applyRealIndexQuote(code, item.f14, Number(item.f2), Number(item.f3), Number(item.f6), Number(item.f104), Number(item.f105), Number(item.f106))
                }
                domesticIndexStatus = "真实行情：" + Qt.formatDateTime(new Date(), "HH:mm:ss") + " 东方财富"
            } catch (error) {
                domesticIndexStatus = "真实行情解析失败"
            }
        }
        // Use http://: Eastmoney's https push2 endpoint occasionally fails the
        // TLS handshake from Qt's bundled OpenSSL (the same issue we hit from
        // Python urllib in tools/free_data_provider.py). The response is
        // public read-only market data, so plain HTTP is acceptable.
        request.open("GET", "http://push2.eastmoney.com/api/qt/ulist.np/get?fltt=2&invt=2&fields=f12,f14,f2,f3,f4,f6,f104,f105,f106&secids=" + secids)
        request.send()
    }

    function refreshDomesticIndexes() {
        fetchEastmoneyIndexes("1.000001,0.399001,0.399006,1.000688,1.000300,1.000905")
        fetchEastmoneyIndexes("100.HSTECH")
    }

    Component.onCompleted: {
        rebuildInvestmentReferencePages()
        refreshDomesticIndexes()
        fetchEastmoneySectorMoneyFlow()
        fetchCninfoAnnouncements()
    }

    Connections {
        target: marketBoardController
        function onSelectedAssetChanged() {
            window.rebuildInvestmentReferencePages()
        }
        function onFreeDataSnapshotChanged() {
            window.applyFreeDataProviderSnapshot(marketBoardController.freeDataSnapshot)
        }
    }

    Timer {
        interval: 1000
        running: true
        repeat: true
        onTriggered: window.refreshDomesticIndexes()
    }

    Timer {
        interval: 60000
        running: true
        repeat: true
        onTriggered: window.fetchEastmoneySectorMoneyFlow()
    }

    Timer {
        interval: 600000
        running: true
        repeat: true
        onTriggered: window.fetchCninfoAnnouncements()
    }

    function openValueBoard(row) {
        selectedInstitutionRow = row
        selectedFundRow = 0
        selectedStockRow = -1
        marketBoardController.openValueBoard(row)
    }

    QtObject {
        id: theme
        readonly property color background: "#0d131a"
        readonly property color panel: "#121b24"
        readonly property color panelRaised: "#182430"
        readonly property color rowEven: "#101821"
        readonly property color rowOdd: "#15202b"
        readonly property color border: "#2a3847"
        readonly property color text: "#eef5ff"
        readonly property color muted: "#97a9bc"
        readonly property color accent: "#58b7ff"
        readonly property color accentSoft: "#14354d"
        readonly property color selection: "#173a57"
        readonly property color positive: "#5cd7a7"
        readonly property color caution: "#ffd56a"
        readonly property color negative: "#ff8f7b"
        readonly property color scrollbarTrack: "#0f1821"
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

    component Sparkline : Canvas {
        id: spark
        property var series: []
        property bool positive: true
        antialiasing: true

        // Canvas does not repaint automatically when bound properties change,
        // so we have to drive requestPaint() ourselves whenever the series or
        // direction tone updates. Without this the realtime curve only paints
        // on first show and never reflects the per-second quote pushes.
        onSeriesChanged: requestPaint()
        onPositiveChanged: requestPaint()

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.clearRect(0, 0, width, height)
            const values = series || []
            if (values.length < 2 || width <= 0 || height <= 0) return

            let minValue = Number(values[0])
            let maxValue = Number(values[0])
            for (let i = 1; i < values.length; ++i) {
                const v = Number(values[i])
                minValue = Math.min(minValue, v)
                maxValue = Math.max(maxValue, v)
            }
            if (Math.abs(maxValue - minValue) < 0.0001) {
                maxValue += 1
                minValue -= 1
            }

            const top = 6
            const bottom = height - 6
            const left = 2
            const right = width - 2
            const chartWidth = Math.max(1, right - left)
            const chartHeight = Math.max(1, bottom - top)
            const points = []
            for (let i = 0; i < values.length; ++i) {
                const ratio = values.length === 1 ? 0 : i / (values.length - 1)
                const normalized = (Number(values[i]) - minValue) / (maxValue - minValue)
                points.push({ x: left + ratio * chartWidth, y: bottom - normalized * chartHeight })
            }

            ctx.beginPath()
            ctx.moveTo(points[0].x, bottom)
            for (let i = 0; i < points.length; ++i) ctx.lineTo(points[i].x, points[i].y)
            ctx.lineTo(points[points.length - 1].x, bottom)
            ctx.closePath()
            const fill = ctx.createLinearGradient(0, top, 0, bottom)
            fill.addColorStop(0, positive ? "rgba(92, 215, 167, 0.34)" : "rgba(255, 143, 123, 0.34)")
            fill.addColorStop(1, "rgba(13, 19, 26, 0.02)")
            ctx.fillStyle = fill
            ctx.fill()

            ctx.strokeStyle = positive ? theme.positive : theme.negative
            ctx.lineWidth = 2
            ctx.beginPath()
            ctx.moveTo(points[0].x, points[0].y)
            for (let i = 1; i < points.length; ++i) ctx.lineTo(points[i].x, points[i].y)
            ctx.stroke()
        }
    }

    component MetricChip : Rectangle {
        id: chip
        required property string labelText
        required property string valueText
        property color valueColor: theme.text

        radius: 18
        color: theme.panelRaised
        border.color: theme.border
        implicitHeight: 88

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 6

            Text {
                text: chip.labelText
                color: theme.muted
                font.family: "Segoe UI Variable"
                font.pixelSize: 15
                font.bold: true
            }

            Text {
                text: chip.valueText
                color: chip.valueColor
                font.family: "Segoe UI Variable"
                font.pixelSize: 24
                font.bold: true
                elide: Text.ElideRight
            }
        }
    }

    component TrendChart : Rectangle {
        id: chartCard
        required property string titleText
        property string subtitleText: ""
        property var series: []
        property color lineColor: theme.accent

        radius: 18
        color: theme.panelRaised
        border.color: theme.border
        implicitHeight: 220

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 10

            ColumnLayout {
                spacing: 2

                Text {
                    text: chartCard.titleText
                    color: theme.text
                    font.family: "Segoe UI Variable"
                    font.pixelSize: 22
                    font.bold: true
                }

                Text {
                    text: chartCard.subtitleText
                    color: theme.muted
                    font.family: "Segoe UI Variable"
                    font.pixelSize: 15
                }
            }

            Canvas {
                id: canvas
                Layout.fillWidth: true
                Layout.fillHeight: true
                antialiasing: true

                onPaint: {
                    const context = getContext("2d")
                    context.reset()
                    context.clearRect(0, 0, width, height)

                    const values = chartCard.series || []
                    if (!values.length) {
                        context.fillStyle = theme.muted
                        context.font = "18px 'Segoe UI Variable'"
                        context.fillText("暂无曲线数据", 18, height / 2)
                        return
                    }

                    let minValue = Number(values[0])
                    let maxValue = Number(values[0])
                    for (let index = 1; index < values.length; ++index) {
                        const value = Number(values[index])
                        minValue = Math.min(minValue, value)
                        maxValue = Math.max(maxValue, value)
                    }

                    if (Math.abs(maxValue - minValue) < 0.0001) {
                        maxValue += 1
                        minValue -= 1
                    }

                    const left = 16
                    const top = 10
                    const right = width - 10
                    const bottom = height - 18
                    const chartWidth = Math.max(right - left, 1)
                    const chartHeight = Math.max(bottom - top, 1)

                    context.strokeStyle = "rgba(151, 169, 188, 0.18)"
                    context.lineWidth = 1
                    context.beginPath()
                    for (let grid = 0; grid < 4; ++grid) {
                        const y = top + chartHeight * grid / 3
                        context.moveTo(left, y)
                        context.lineTo(right, y)
                    }
                    context.stroke()

                    const points = []
                    for (let pointIndex = 0; pointIndex < values.length; ++pointIndex) {
                        const ratio = values.length === 1 ? 0 : pointIndex / (values.length - 1)
                        const normalized = (Number(values[pointIndex]) - minValue) / (maxValue - minValue)
                        points.push({
                            x: left + ratio * chartWidth,
                            y: bottom - normalized * chartHeight
                        })
                    }

                    context.beginPath()
                    context.moveTo(points[0].x, bottom)
                    for (let pointIndex = 0; pointIndex < points.length; ++pointIndex) {
                        context.lineTo(points[pointIndex].x, points[pointIndex].y)
                    }
                    context.lineTo(points[points.length - 1].x, bottom)
                    context.closePath()
                    const area = context.createLinearGradient(0, top, 0, bottom)
                    area.addColorStop(0, "rgba(88, 183, 255, 0.30)")
                    area.addColorStop(1, "rgba(88, 183, 255, 0.00)")
                    context.fillStyle = area
                    context.fill()

                    context.strokeStyle = chartCard.lineColor
                    context.lineWidth = 2.6
                    context.beginPath()
                    for (let pointIndex = 0; pointIndex < points.length; ++pointIndex) {
                        if (pointIndex === 0) {
                            context.moveTo(points[pointIndex].x, points[pointIndex].y)
                        } else {
                            context.lineTo(points[pointIndex].x, points[pointIndex].y)
                        }
                    }
                    context.stroke()

                    const lastPoint = points[points.length - 1]
                    context.fillStyle = chartCard.lineColor
                    context.beginPath()
                    context.arc(lastPoint.x, lastPoint.y, 3.8, 0, Math.PI * 2)
                    context.fill()
                }
            }
        }

        onSeriesChanged: canvas.requestPaint()
        onWidthChanged: canvas.requestPaint()
        onHeightChanged: canvas.requestPaint()
    }

    component ReferenceCard : Rectangle {
        id: refCard
        required property string titleText
        required property string valueText
        property string detailText: ""
        property string sourceText: ""
        property string actionText: ""
        property string status: "neutral"
        property int score: 0
        property var metrics: []

        radius: 10
        color: theme.panelRaised
        border.color: window.referenceColor(refCard.status)
        clip: true

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 8

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    Layout.fillWidth: true
                    text: refCard.titleText
                    color: theme.text
                    font.family: "Segoe UI Variable"
                    font.pixelSize: 18
                    font.bold: true
                    elide: Text.ElideRight
                }

                Rectangle {
                    Layout.preferredWidth: 74
                    Layout.preferredHeight: 24
                    radius: 7
                    color: Qt.rgba(window.referenceColor(refCard.status).r, window.referenceColor(refCard.status).g, window.referenceColor(refCard.status).b, 0.14)
                    border.color: window.referenceColor(refCard.status)

                    Text {
                        anchors.centerIn: parent
                        text: refCard.status === "good" ? "良好" : (refCard.status === "danger" ? "风险" : (refCard.status === "warn" ? "待确认" : "参考"))
                        color: window.referenceColor(refCard.status)
                        font.family: "Segoe UI Variable"
                        font.pixelSize: 12
                        font.bold: true
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                text: refCard.valueText
                color: window.referenceColor(refCard.status)
                font.family: "Segoe UI Variable"
                font.pixelSize: 21
                font.bold: true
                wrapMode: Text.WrapAnywhere
                maximumLineCount: 2
                elide: Text.ElideRight
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Rectangle {
                    Layout.preferredWidth: 86
                    Layout.preferredHeight: 28
                    radius: 7
                    color: Qt.rgba(window.referenceColor(refCard.status).r, window.referenceColor(refCard.status).g, window.referenceColor(refCard.status).b, 0.12)
                    border.color: window.referenceColor(refCard.status)

                    Text {
                        anchors.centerIn: parent
                        text: localizationController.tr("mb.label.score") + " " + refCard.score
                        color: window.referenceColor(refCard.status)
                        font.family: "Segoe UI Variable"
                        font.pixelSize: 12
                        font.bold: true
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: refCard.actionText.length > 0 ? "动作：" + refCard.actionText : "动作：观察"
                    color: theme.text
                    font.family: "Segoe UI Variable"
                    font.pixelSize: 13
                    font.bold: true
                    elide: Text.ElideRight
                }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 3
                rowSpacing: 6
                columnSpacing: 6

                Repeater {
                    model: refCard.metrics

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 42
                        radius: 7
                        color: theme.background
                        border.color: theme.border

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 7
                            anchors.rightMargin: 7
                            anchors.topMargin: 4
                            anchors.bottomMargin: 4
                            spacing: 0

                            Text {
                                Layout.fillWidth: true
                                text: modelData.label
                                color: theme.muted
                                font.family: "Segoe UI Variable"
                                font.pixelSize: 10
                                elide: Text.ElideRight
                            }

                            Text {
                                Layout.fillWidth: true
                                text: modelData.value
                                color: theme.text
                                font.family: "Segoe UI Variable"
                                font.pixelSize: 12
                                font.bold: true
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                text: refCard.detailText
                color: theme.muted
                font.family: "Segoe UI Variable"
                font.pixelSize: 12
                lineHeight: 1.15
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }

            Text {
                Layout.fillWidth: true
                text: refCard.sourceText
                color: theme.accent
                font.family: "Consolas"
                font.pixelSize: 11
                elide: Text.ElideRight
            }
        }
    }

    component ReferencePage : Rectangle {
        id: refPage
        required property string titleText
        required property string subtitleText
        property string statusText: domesticIndexStatus
        property var rows: []
        property var fallbackRows: [
            {
                title: "数据刷新中",
                value: statusText && statusText.length > 0 ? statusText : "等待真实数据源返回",
                action: "自动刷新",
                score: 50,
                detail: "页面已连接真实数据源；如果网络源限流或暂时不可达，会在下一轮刷新后自动补齐。",
                status: "warn",
                source: "本地免费数据提供器 / 公开数据源",
                metrics: [
                    { label: "东方财富", value: "行业资金" },
                    { label: "巨潮资讯", value: "公告财报" },
                    { label: "刷新", value: "自动" }
                ]
            }
        ]
        property var displayRows: rows && rows.length > 0 ? rows : fallbackRows

        radius: 10
        color: theme.panel
        border.color: theme.border
        clip: true

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 14

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(64, pageHeader.implicitHeight)
                spacing: 12

                ColumnLayout {
                    id: pageHeader
                    Layout.fillWidth: true
                    spacing: 3

                    Text {
                        Layout.fillWidth: true
                        text: refPage.titleText
                        color: theme.text
                        font.family: "Segoe UI Variable"
                        font.pixelSize: 24
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    Text {
                        Layout.fillWidth: true
                        text: refPage.subtitleText
                        color: theme.muted
                        font.family: "Segoe UI Variable"
                        font.pixelSize: 14
                        elide: Text.ElideRight
                    }
                }

                Text {
                    Layout.preferredWidth: 280
                    text: refPage.statusText
                    color: theme.accent
                    font.family: "Segoe UI Variable"
                    font.pixelSize: 13
                    font.bold: true
                    horizontalAlignment: Text.AlignRight
                    wrapMode: Text.WordWrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                }
            }

            GridLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                columns: width >= 1200 ? 4 : (width >= 840 ? 3 : 2)
                rowSpacing: 12
                columnSpacing: 12

                Repeater {
                    model: refPage.displayRows

                    ReferenceCard {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 236
                        titleText: modelData.title
                        valueText: modelData.value
                        detailText: modelData.detail
                        sourceText: modelData.source
                        actionText: modelData.action
                        status: modelData.status
                        score: modelData.score || 0
                        metrics: modelData.metrics || []
                    }
                }
            }
        }
    }

    component BoardTableCard : Rectangle {
        id: card
        property var boardModel: null
        property var columnRatios: []
        property string titleText: ""
        property string subtitleText: ""
        property string statusText: ""
        property string actionHint: ""
        property int selectedRow: -1
        property int minimumBodyWidth: 920
        property int chartColumn: -1
        signal rowSelected(int row)
        signal rowActivated(int row)

        function columnWidth(column) {
            const ratios = card.columnRatios && card.columnRatios.length > 0 ? card.columnRatios : [1]
            let total = 0
            for (let index = 0; index < ratios.length; ++index) {
                total += ratios[index]
            }

            const compact = tableView.width > 0 && tableView.width < card.minimumBodyWidth
            const available = Math.max(tableView.width - 20, compact ? 760 : card.minimumBodyWidth)
            if (column === ratios.length - 1) {
                let used = 0
                for (let index = 0; index < ratios.length - 1; ++index) {
                    used += Math.floor(available * ratios[index] / total)
                }
                return Math.max(available - used, 120)
            }

            return Math.max(Math.floor(available * ratios[column] / total), 92)
        }

        function displayColor(column, textValue) {
            if (column === 0) {
                return theme.accent
            }
            const stringValue = String(textValue)
            if (stringValue.startsWith("+")) {
                return theme.positive
            }
            if (stringValue.startsWith("-")) {
                return theme.negative
            }
            return theme.text
        }

        radius: 10
        color: theme.panel
        border.color: theme.border
        clip: true

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 12

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(62, cardHeader.implicitHeight)
                spacing: 12

                ColumnLayout {
                    id: cardHeader
                    Layout.fillWidth: true
                    spacing: 2

                    Text {
                        Layout.fillWidth: true
                        text: card.titleText
                        color: theme.text
                        font.family: "Segoe UI Variable"
                        font.pixelSize: 24
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    Text {
                        Layout.fillWidth: true
                        text: card.subtitleText
                        color: theme.muted
                        font.family: "Segoe UI Variable"
                        font.pixelSize: 14
                        elide: Text.ElideRight
                    }
                }

                ColumnLayout {
                    Layout.preferredWidth: 170
                    spacing: 2

                    Text {
                        Layout.fillWidth: true
                        text: card.statusText
                        color: theme.text
                        font.family: "Segoe UI Variable"
                        font.pixelSize: 14
                        horizontalAlignment: Text.AlignRight
                        elide: Text.ElideRight
                    }

                    Text {
                        Layout.fillWidth: true
                        text: card.actionHint
                        color: theme.accent
                        font.family: "Segoe UI Variable"
                        font.pixelSize: 14
                        font.bold: true
                        horizontalAlignment: Text.AlignRight
                        elide: Text.ElideRight
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 8
                color: theme.background
                border.color: theme.border
                clip: true

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    HorizontalHeaderView {
                        id: headerView
                        Layout.fillWidth: true
                        Layout.preferredHeight: 52
                        syncView: tableView
                        model: card.boardModel
                        clip: true

                        delegate: Rectangle {
                            implicitHeight: 52
                            color: theme.panelRaised
                            border.color: theme.border

                            Text {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 12
                                verticalAlignment: Text.AlignVCenter
                                text: typeof display !== "undefined" ? localizationController.trCn(String(display)) : ""
                                color: theme.text
                                font.family: "Microsoft YaHei UI"
                                font.pixelSize: 14
                                font.bold: true
                                elide: Text.ElideRight
                            }
                        }
                    }

                    TableView {
                        id: tableView
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds
                        reuseItems: true
                        model: card.boardModel

                        columnWidthProvider: function(column) {
                            return card.columnWidth(column)
                        }

                        rowHeightProvider: function() {
                            return 60
                        }

                        delegate: Rectangle {
                            implicitWidth: card.columnWidth(column)
                            implicitHeight: 60
                            color: card.selectedRow === row
                                ? theme.selection
                                : (row % 2 === 0 ? theme.rowEven : theme.rowOdd)
                            border.color: card.selectedRow === row ? theme.accent : theme.border
                            border.width: card.selectedRow === row ? 1.3 : 1

                            Text {
                                id: cellText
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 12
                                verticalAlignment: Text.AlignVCenter
                                visible: column !== card.chartColumn
                                text: typeof display !== "undefined" ? localizationController.trCn(String(display)) : ""
                                color: card.displayColor(column, cellText.text)
                                font.family: "Microsoft YaHei UI"
                                font.pixelSize: 14
                                font.bold: column === 0 || column === 1
                                wrapMode: Text.WordWrap
                                maximumLineCount: 2
                                elide: Text.ElideRight
                            }

                            Sparkline {
                                anchors.fill: parent
                                anchors.leftMargin: 14
                                anchors.rightMargin: 14
                                anchors.topMargin: 10
                                anchors.bottomMargin: 10
                                visible: column === card.chartColumn
                                series: typeof realtimeTrend === "undefined" ? [] : realtimeTrend
                                positive: typeof oneHourChangePct === "undefined" ? true : oneHourChangePct >= 0
                            }

                            MouseArea {
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: card.rowSelected(row)
                                onDoubleClicked: card.rowActivated(row)
                            }
                        }

                        ScrollBar.vertical: StyledScrollBar {
                            policy: ScrollBar.AlwaysOff
                            visible: false
                        }

                        ScrollBar.horizontal: StyledScrollBar {
                            policy: ScrollBar.AlwaysOff
                            visible: false
                        }
                    }
                }
            }
        }
    }

    background: Rectangle {
        color: theme.background
    }

    Rectangle {
        anchors.fill: parent
        color: theme.background

        StackLayout {
            anchors.fill: parent
            anchors.margins: 12
            currentIndex: marketBoardController.currentPage

            Item {
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 14

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 92
                        radius: 10
                        color: theme.panelRaised
                        border.color: theme.border

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 18
                            anchors.rightMargin: 18
                            spacing: 12

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 6

                                Text {
                                    Layout.fillWidth: true
                                    text: localizationController.trCn(marketBoardController.institutionBoardTitle)
                                    color: theme.text
                                    font.family: "Segoe UI Variable"
                                    font.pixelSize: 28
                                    font.bold: true
                                    elide: Text.ElideRight
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: localizationController.tr("mb.help.intro")
                                    color: theme.muted
                                    font.family: "Segoe UI Variable"
                                    font.pixelSize: 14
                                    elide: Text.ElideRight
                                }
                            }

                            Rectangle {
                                radius: 8
                                color: theme.accentSoft
                                border.color: theme.accent
                                implicitWidth: 132
                                implicitHeight: 40

                                Text {
                                    anchors.centerIn: parent
                                    text: localizationController.tr("mb.label.liveSync")
                                    color: theme.accent
                                    font.family: "Segoe UI Variable"
                                    font.pixelSize: 15
                                    font.bold: true
                                }
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 124
                        radius: 10
                        color: theme.panel
                        border.color: theme.border

                        GridLayout {
                            id: institutionTabs
                            property int currentIndex: 0
                            anchors.fill: parent
                            anchors.margins: 8
                            columns: 6
                            rowSpacing: 8
                            columnSpacing: 8

                            Repeater {
                                model: window.marketBoardPages

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    Layout.preferredHeight: 48
                                    radius: 10
                                    color: institutionTabs.currentIndex === index ? modelData.bg : modelData.soft
                                    border.color: modelData.color
                                    border.width: institutionTabs.currentIndex === index ? 2 : 1
                                    opacity: institutionTabs.currentIndex === index ? 1.0 : 0.82

                                    Text {
                                        anchors.centerIn: parent
                                        anchors.leftMargin: 8
                                        anchors.rightMargin: 8
                                        text: localizationController.tr(modelData.key)
                                        color: institutionTabs.currentIndex === index ? modelData.color : theme.text
                                        font.family: "Segoe UI Variable"
                                        font.pixelSize: 16
                                        font.bold: true
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                        elide: Text.ElideRight
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: institutionTabs.currentIndex = index
                                        onEntered: parent.opacity = 1.0
                                        onExited: parent.opacity = institutionTabs.currentIndex === index ? 1.0 : 0.82
                                    }
                                }
                            }
                        }
                    }

                    StackLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        currentIndex: institutionTabs.currentIndex

                        Item {
                            BoardTableCard {
                                anchors.fill: parent
                                boardModel: institutionRankingModel
                                columnRatios: [0.08, 0.16, 0.18, 0.24, 0.22, 0.12]
                                minimumBodyWidth: 960
                                titleText: localizationController.tr("mb.section.institutions")
                                subtitleText: localizationController.tr("mb.section.institutions.subtitle")
                                statusText: localizationController.trCn(marketBoardController.institutionStatus)
                                actionHint: localizationController.tr("mb.action.update.1h")
                                selectedRow: window.selectedInstitutionRow

                                onRowSelected: function(row) {
                                    window.selectedInstitutionRow = row
                                }

                                onRowActivated: function(row) {
                                    window.openValueBoard(row)
                                }
                            }
                        }

                        Item {
                            Rectangle {
                                anchors.fill: parent
                                radius: 10
                                color: theme.panel
                                border.color: theme.border

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 16
                                    spacing: 14

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 12

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 4

                                            Text {
                                                text: localizationController.tr("mb.label.usWatch")
                                                color: theme.text
                                                font.family: "Segoe UI Variable"
                                                font.pixelSize: 24
                                                font.bold: true
                                            }

                                            Text {
                                                text: localizationController.trCn(marketBoardController.usMarketStatus)
                                                color: theme.muted
                                                font.family: "Segoe UI Variable"
                                                font.pixelSize: 14
                                                wrapMode: Text.Wrap
                                            }
                                        }

                                        TextField {
                                            id: usSymbolInput
                                            Layout.preferredWidth: 280
                                            Layout.minimumWidth: 180
                                            placeholderText: "输入代码或名称，例如 AAPL / Apple / 纳指"
                                            font.family: "Segoe UI Variable"
                                            font.pixelSize: 16
                                            color: theme.text
                                            selectByMouse: true
                                            onAccepted: {
                                                if (marketBoardController.addUsWatchSymbol(text)) {
                                                    clear()
                                                }
                                            }

                                            background: Rectangle {
                                                radius: 16
                                                color: theme.background
                                                border.color: usSymbolInput.activeFocus ? theme.accent : theme.border
                                            }
                                        }

                                        Button {
                                            id: addUsButton
                                            text: localizationController.tr("mb.action.add")
                                            implicitWidth: 86
                                            implicitHeight: 48
                                            font.family: "Segoe UI Variable"
                                            font.pixelSize: 17
                                            font.bold: true
                                            onClicked: {
                                                if (marketBoardController.addUsWatchSymbol(usSymbolInput.text)) {
                                                    usSymbolInput.clear()
                                                }
                                            }

                                            background: Rectangle {
                                                radius: 16
                                                color: theme.accentSoft
                                                border.color: theme.accent
                                            }

                                            contentItem: Text {
                                                text: addUsButton.text
                                                color: theme.accent
                                                font: addUsButton.font
                                                horizontalAlignment: Text.AlignHCenter
                                                verticalAlignment: Text.AlignVCenter
                                            }
                                        }
                                    }

                                    BoardTableCard {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        boardModel: usMarketModel
                                        columnRatios: [0.13, 0.22, 0.17, 0.13, 0.14, 0.21]
                                        minimumBodyWidth: 1180
                                        chartColumn: 5
                                        titleText: localizationController.tr("mb.section.usWatch")
                                        subtitleText: localizationController.tr("mb.section.usWatch.subtitle")
                                        statusText: localizationController.trCn(marketBoardController.usMarketStatus)
                                        actionHint: localizationController.tr("mb.action.realtime")
                                        selectedRow: -1
                                        onRowSelected: function(row) { }
                                        onRowActivated: function(row) { }
                                    }
                                }
                            }
                        }

                        Item {
                            Rectangle {
                                anchors.fill: parent
                                radius: 10
                                color: theme.panel
                                border.color: theme.border

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 16
                                    spacing: 14

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 12

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 4

                                            Text {
                                                Layout.fillWidth: true
                                                text: localizationController.tr("mb.label.indices")
                                                color: theme.text
                                                font.family: "Segoe UI Variable"
                                                font.pixelSize: 24
                                                font.bold: true
                                                elide: Text.ElideRight
                                            }

                                            Text {
                                                Layout.fillWidth: true
                                                text: domesticIndexStatus
                                                color: theme.muted
                                                font.family: "Segoe UI Variable"
                                                font.pixelSize: 14
                                                elide: Text.ElideRight
                                            }
                                        }

                                        Rectangle {
                                            Layout.preferredWidth: 132
                                            Layout.preferredHeight: 40
                                            radius: 8
                                            color: theme.accentSoft
                                            border.color: theme.accent

                                            Text {
                                                anchors.centerIn: parent
                                                text: localizationController.tr("mb.label.realQuotes")
                                                color: theme.accent
                                                font.family: "Segoe UI Variable"
                                                font.pixelSize: 15
                                                font.bold: true
                                            }
                                        }
                                    }

                                    GridLayout {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        columns: 4
                                        rowSpacing: 12
                                        columnSpacing: 12

                                        Repeater {
                                            model: window.domesticIndexRows

                                            Rectangle {
                                                id: indexCard
                                                property var indexRow: modelData

                                                Layout.fillWidth: true
                                                Layout.preferredHeight: 262
                                                Layout.minimumHeight: 246
                                                Layout.maximumHeight: 280
                                                radius: 10
                                                color: theme.panelRaised
                                                border.color: theme.border
                                                clip: true

                                                ColumnLayout {
                                                    anchors.fill: parent
                                                    anchors.margins: 14
                                                    spacing: 8

                                                    RowLayout {
                                                        Layout.fillWidth: true
                                                        spacing: 8

                                                        ColumnLayout {
                                                            Layout.fillWidth: true
                                                            spacing: 2

                                                            Text {
                                                                Layout.fillWidth: true
                                                                text: modelData.name
                                                                color: theme.text
                                                                font.family: "Segoe UI Variable"
                                                                font.pixelSize: 20
                                                                font.bold: true
                                                                elide: Text.ElideRight
                                                            }

                                                            Text {
                                                                Layout.fillWidth: true
                                                                text: modelData.code
                                                                color: theme.muted
                                                                font.family: "Consolas"
                                                                font.pixelSize: 12
                                                                elide: Text.ElideRight
                                                            }
                                                        }

                                                        Rectangle {
                                                            Layout.preferredWidth: 82
                                                            Layout.preferredHeight: 28
                                                            radius: 7
                                                            color: modelData.change >= 0
                                                                ? Qt.rgba(theme.positive.r, theme.positive.g, theme.positive.b, 0.13)
                                                                : Qt.rgba(theme.negative.r, theme.negative.g, theme.negative.b, 0.13)
                                                            border.color: modelData.change >= 0 ? theme.positive : theme.negative

                                                            Text {
                                                                anchors.centerIn: parent
                                                                text: (modelData.change >= 0 ? "+" : "") + Number(modelData.change).toFixed(2) + "%"
                                                                color: modelData.change >= 0 ? theme.positive : theme.negative
                                                                font.family: "Segoe UI Variable"
                                                                font.pixelSize: 13
                                                                font.bold: true
                                                            }
                                                        }
                                                    }

                                                    RowLayout {
                                                        Layout.fillWidth: true
                                                        spacing: 10

                                                        Text {
                                                            Layout.fillWidth: true
                                                            text: modelData.value
                                                            color: theme.text
                                                            font.family: "Segoe UI Variable"
                                                            font.pixelSize: 28
                                                            font.bold: true
                                                            elide: Text.ElideRight
                                                        }

                                                        Text {
                                                            text: modelData.tone
                                                            color: theme.accent
                                                            font.family: "Segoe UI Variable"
                                                            font.pixelSize: 13
                                                            font.bold: true
                                                        }
                                                    }

                                                    Sparkline {
                                                        Layout.fillWidth: true
                                                        Layout.preferredHeight: 44
                                                        series: modelData.series
                                                        positive: modelData.change >= 0
                                                    }

                                                    GridLayout {
                                                        Layout.fillWidth: true
                                                        columns: 3
                                                        rowSpacing: 8
                                                        columnSpacing: 8

                                                        Repeater {
                                                            model: [
                                                                { label: "宽度", value: indexCard.indexRow.breadth + "%", color: window.metricColor(indexCard.indexRow.breadth, 58, 42) },
                                                                { label: "动量", value: (indexCard.indexRow.momentum || 55), color: window.metricColor(indexCard.indexRow.momentum || 55, 62, 40) },
                                                                { label: "量能", value: (indexCard.indexRow.turnover || 100) + "%", color: window.metricColor(indexCard.indexRow.turnover || 100, 110, 75) },
                                                                { label: "支撑", value: indexCard.indexRow.support || "-", color: theme.muted },
                                                                { label: "压力", value: indexCard.indexRow.resistance || "-", color: theme.muted },
                                                                { label: "风险", value: indexCard.indexRow.risk || window.indexRiskLabel(indexCard.indexRow.change, indexCard.indexRow.breadth), color: indexCard.indexRow.risk === "偏高" || indexCard.indexRow.risk === "追高" ? theme.negative : theme.positive }
                                                            ]

                                                            Rectangle {
                                                                Layout.fillWidth: true
                                                                Layout.preferredHeight: 42
                                                                radius: 8
                                                                color: theme.background
                                                                border.color: theme.border

                                                                ColumnLayout {
                                                                    anchors.fill: parent
                                                                    anchors.leftMargin: 8
                                                                    anchors.rightMargin: 8
                                                                    anchors.topMargin: 5
                                                                    anchors.bottomMargin: 5
                                                                    spacing: 0

                                                                    Text {
                                                                        Layout.fillWidth: true
                                                                        text: modelData.label
                                                                        color: theme.muted
                                                                        font.family: "Segoe UI Variable"
                                                                        font.pixelSize: 10
                                                                        elide: Text.ElideRight
                                                                    }

                                                                    Text {
                                                                        Layout.fillWidth: true
                                                                        text: modelData.value
                                                                        color: modelData.color
                                                                        font.family: "Segoe UI Variable"
                                                                        font.pixelSize: 13
                                                                        font.bold: true
                                                                        elide: Text.ElideRight
                                                                    }
                                                                }
                                                            }
                                                        }
                                                    }

                                                    Rectangle {
                                                        Layout.fillWidth: true
                                                        Layout.preferredHeight: 30
                                                        radius: 8
                                                        color: Qt.rgba(theme.accent.r, theme.accent.g, theme.accent.b, 0.10)
                                                        border.color: theme.accent

                                                        Text {
                                                            anchors.fill: parent
                                                            anchors.leftMargin: 10
                                                            anchors.rightMargin: 10
                                                            text: (modelData.tomorrow || window.tomorrowBias(modelData.change, modelData.breadth, modelData.momentum || 55)) + " / " + modelData.note
                                                            color: theme.accent
                                                            font.family: "Segoe UI Variable"
                                                            font.pixelSize: 12
                                                            font.bold: true
                                                            verticalAlignment: Text.AlignVCenter
                                                            elide: Text.ElideRight
                                                        }
                                                    }

                                                    Text {
                                                        Layout.fillWidth: true
                                                        text: localizationController.tr("mb.help.disclaimer")
                                                        color: theme.muted
                                                        font.family: "Segoe UI Variable"
                                                        font.pixelSize: 11
                                                        wrapMode: Text.NoWrap
                                                        maximumLineCount: 1
                                                        elide: Text.ElideRight
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        Item {
                            ReferencePage {
                                anchors.fill: parent
                                titleText: localizationController.tr("mb.section.flow")
                                subtitleText: localizationController.tr("mb.section.flow.subtitle")
                                rows: window.capitalFlowRows
                            }
                        }

                        Item {
                            ReferencePage {
                                anchors.fill: parent
                                titleText: localizationController.tr("mb.section.rotation")
                                subtitleText: localizationController.tr("mb.section.rotation.subtitle")
                                rows: window.rotationRows
                            }
                        }

                        Item {
                            ReferencePage {
                                anchors.fill: parent
                                titleText: localizationController.tr("mb.section.valuation")
                                subtitleText: localizationController.tr("mb.section.valuation.subtitle")
                                rows: window.valuationRows
                            }
                        }

                        Item {
                            ReferencePage {
                                anchors.fill: parent
                                titleText: localizationController.tr("mb.section.earnings")
                                subtitleText: localizationController.tr("mb.section.earnings.subtitle")
                                rows: window.earningsRows
                            }
                        }

                        Item {
                            ReferencePage {
                                anchors.fill: parent
                                titleText: localizationController.tr("mb.section.fund")
                                subtitleText: localizationController.tr("mb.section.fund.subtitle")
                                rows: window.fundLookthroughRows
                            }
                        }

                        Item {
                            ReferencePage {
                                anchors.fill: parent
                                titleText: localizationController.tr("mb.section.risk")
                                subtitleText: localizationController.tr("mb.section.risk.subtitle")
                                rows: window.riskAlertRows
                            }
                        }

                        Item {
                            ReferencePage {
                                anchors.fill: parent
                                titleText: localizationController.tr("mb.section.diag")
                                subtitleText: localizationController.tr("mb.section.diag.subtitle")
                                rows: window.portfolioDiagnosticRows
                            }
                        }

                        Item {
                            ReferencePage {
                                anchors.fill: parent
                                titleText: localizationController.tr("mb.section.plans")
                                subtitleText: localizationController.tr("mb.section.plans.subtitle")
                                rows: window.tradePlanRows
                            }
                        }
                    }
                }
            }

            Item {
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 14

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 96
                        radius: 10
                        color: theme.panelRaised
                        border.color: theme.border

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 16
                            anchors.rightMargin: 16
                            spacing: 16

                            Button {
                                id: backButton
                                text: localizationController.tr("mb.action.backToList")
                                implicitHeight: 52
                                implicitWidth: 118
                                font.family: "Segoe UI Variable"
                                font.pixelSize: 16
                                font.bold: true
                                onClicked: marketBoardController.backToInstitutionBoard()

                                background: Rectangle {
                                    radius: 16
                                    color: theme.accentSoft
                                    border.color: theme.accent
                                }

                                contentItem: Text {
                                    text: backButton.text
                                    color: theme.accent
                                    font: backButton.font
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 4

                                Text {
                                    Layout.fillWidth: true
                                    text: localizationController.trCn(marketBoardController.valueBoardTitle)
                                    color: theme.text
                                    font.family: "Segoe UI Variable"
                                    font.pixelSize: 26
                                    font.bold: true
                                    elide: Text.ElideRight
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: marketBoardController.selectedInstitutionTitle.length > 0
                                        ? "入口机构：" + localizationController.trCn(marketBoardController.selectedInstitutionTitle)
                                        : "入口机构：未选择"
                                    color: theme.muted
                                    font.family: "Segoe UI Variable"
                                    font.pixelSize: 14
                                    elide: Text.ElideRight
                                }
                            }

                            Item { Layout.fillWidth: true }

                            ColumnLayout {
                                spacing: 4

                                Text {
                                    text: localizationController.trCn(marketBoardController.valueStatus)
                                    color: theme.text
                                    font.family: "Segoe UI Variable"
                                    font.pixelSize: 16
                                    horizontalAlignment: Text.AlignRight
                                }

                                Text {
                                    text: localizationController.tr("mb.label.cache")
                                    color: theme.accent
                                    font.family: "Segoe UI Variable"
                                    font.pixelSize: 16
                                    font.bold: true
                                    horizontalAlignment: Text.AlignRight
                                }
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 14

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            spacing: 14

                            BoardTableCard {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                Layout.preferredHeight: 400
                                boardModel: fundRankingModel
                                columnRatios: [0.08, 0.24, 0.14, 0.10, 0.12, 0.10, 0.22]
                                minimumBodyWidth: 900
                                titleText: localizationController.tr("mb.section.topFunds")
                                subtitleText: localizationController.tr("mb.action.tapForDetail")
                                statusText: "基金榜"
                                actionHint: localizationController.tr("mb.action.update.5m")
                                selectedRow: window.selectedFundRow

                                onRowSelected: function(row) {
                                    window.selectedFundRow = row
                                    window.selectedStockRow = -1
                                    marketBoardController.selectFund(row)
                                }

                                onRowActivated: function(row) {
                                    window.selectedFundRow = row
                                    window.selectedStockRow = -1
                                    marketBoardController.selectFund(row)
                                }
                            }

                            BoardTableCard {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                Layout.preferredHeight: 300
                                boardModel: stockRankingModel
                                columnRatios: [0.08, 0.24, 0.14, 0.10, 0.12, 0.10, 0.22]
                                minimumBodyWidth: 900
                                titleText: localizationController.tr("mb.section.topStocks")
                                subtitleText: localizationController.tr("mb.action.tapForDetail")
                                statusText: "股票榜"
                                actionHint: localizationController.tr("mb.action.update.5m")
                                selectedRow: window.selectedStockRow

                                onRowSelected: function(row) {
                                    window.selectedStockRow = row
                                    window.selectedFundRow = -1
                                    marketBoardController.selectStock(row)
                                }

                                onRowActivated: function(row) {
                                    window.selectedStockRow = row
                                    window.selectedFundRow = -1
                                    marketBoardController.selectStock(row)
                                }
                            }
                        }

                        Rectangle {
                            Layout.preferredWidth: 420
                            Layout.minimumWidth: 360
                            Layout.maximumWidth: 680
                            Layout.fillHeight: true
                            radius: 10
                            color: theme.panel
                            border.color: theme.border
                            clip: true

                            Flickable {
                                id: detailsScroll
                                anchors.fill: parent
                                anchors.margins: 18
                                clip: true
                                boundsBehavior: Flickable.StopAtBounds
                                contentWidth: width
                                contentHeight: detailsColumn.implicitHeight

                                Column {
                                    id: detailsColumn
                                    width: detailsScroll.width
                                    spacing: 14

                                    Rectangle {
                                        width: parent.width
                                        height: Math.max(96, selectedAssetHeaderLayout.implicitHeight + 36)
                                        radius: 20
                                        color: theme.panelRaised
                                        border.color: theme.border

                                        ColumnLayout {
                                            id: selectedAssetHeaderLayout
                                            anchors.fill: parent
                                            anchors.margins: 18
                                            spacing: 12

                                            Text {
                                                Layout.fillWidth: true
                                                text: marketBoardController.selectedAssetName.length > 0
                                                    ? localizationController.trCn(marketBoardController.selectedAssetName) + "  " + localizationController.trCn(marketBoardController.selectedAssetCode)
                                                    : "请选择基金或股票"
                                                color: theme.text
                                                font.family: "Segoe UI Variable"
                                                font.pixelSize: 28
                                                font.bold: true
                                                wrapMode: Text.WrapAnywhere
                                            }

                                            Text {
                                                Layout.fillWidth: true
                                                text: marketBoardController.selectedAssetCategory.length > 0
                                                    ? localizationController.trCn(marketBoardController.selectedAssetCategory) + " | " + localizationController.trCn(marketBoardController.selectedAssetProvider)
                                                    : "价值投资详情"
                                                color: theme.muted
                                                font.family: "Segoe UI Variable"
                                                font.pixelSize: 18
                                                wrapMode: Text.WrapAnywhere
                                            }
                                        }
                                    }

                                    ColumnLayout {
                                        width: parent.width
                                        spacing: 12

                                        MetricChip {
                                            Layout.fillWidth: true
                                            labelText: localizationController.tr("mb.label.valueScore")
                                            valueText: Number(marketBoardController.selectedAssetScore).toFixed(1)
                                            valueColor: theme.accent
                                        }

                                        MetricChip {
                                            Layout.fillWidth: true
                                            labelText: localizationController.tr("mb.label.navOrPrice")
                                            valueText: Number(marketBoardController.selectedAssetLatestPrice).toFixed(3)
                                        }

                                        MetricChip {
                                            Layout.fillWidth: true
                                            labelText: localizationController.tr("mb.label.return1y")
                                            valueText: Number(marketBoardController.selectedAssetOneYearReturn).toFixed(1) + "%"
                                            valueColor: marketBoardController.selectedAssetOneYearReturn >= 0
                                                ? theme.positive
                                                : theme.negative
                                        }
                                    }

                                    Rectangle {
                                        width: parent.width
                                        height: Math.max(150, fundConfigLayout.implicitHeight + 32)
                                        radius: 18
                                        color: theme.panelRaised
                                        border.color: theme.border
                                        visible: localizationController.trCn(marketBoardController.selectedAssetKind) === "fund"

                                        ColumnLayout {
                                            id: fundConfigLayout
                                            anchors.fill: parent
                                            anchors.margins: 16
                                            spacing: 12

                                            Text {
                                                text: localizationController.tr("mb.label.detailCard")
                                                color: theme.text
                                                font.family: "Segoe UI Variable"
                                                font.pixelSize: 20
                                                font.bold: true
                                            }

                                            GridLayout {
                                                Layout.fillWidth: true
                                                columns: 2
                                                rowSpacing: 8
                                                columnSpacing: 8

                                                Repeater {
                                                    model: marketBoardController.selectedFundConfigCards

                                                    Rectangle {
                                                        Layout.fillWidth: true
                                                        Layout.preferredHeight: 48
                                                        radius: 8
                                                        color: theme.background
                                                        border.color: theme.border

                                                        ColumnLayout {
                                                            anchors.fill: parent
                                                            anchors.leftMargin: 10
                                                            anchors.rightMargin: 10
                                                            anchors.topMargin: 6
                                                            anchors.bottomMargin: 6
                                                            spacing: 0

                                                            Text {
                                                                Layout.fillWidth: true
                                                                text: modelData.label
                                                                color: theme.muted
                                                                font.family: "Segoe UI Variable"
                                                                font.pixelSize: 11
                                                                elide: Text.ElideRight
                                                            }

                                                            Text {
                                                                Layout.fillWidth: true
                                                                text: modelData.value
                                                                color: theme.text
                                                                font.family: "Segoe UI Variable"
                                                                font.pixelSize: 14
                                                                font.bold: true
                                                                elide: Text.ElideRight
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }

                                    Rectangle {
                                        width: parent.width
                                        height: Math.max(132, fundHoldingLayout.implicitHeight + 32)
                                        radius: 18
                                        color: theme.panelRaised
                                        border.color: theme.border
                                        visible: localizationController.trCn(marketBoardController.selectedAssetKind) === "fund"

                                        ColumnLayout {
                                            id: fundHoldingLayout
                                            anchors.fill: parent
                                            anchors.margins: 16
                                            spacing: 12

                                            Text {
                                                text: localizationController.tr("mb.label.fundCard")
                                                color: theme.text
                                                font.family: "Segoe UI Variable"
                                                font.pixelSize: 20
                                                font.bold: true
                                            }

                                            GridLayout {
                                                Layout.fillWidth: true
                                                columns: 2
                                                rowSpacing: 8
                                                columnSpacing: 8

                                                Repeater {
                                                    model: marketBoardController.selectedFundHoldingCards

                                                    Rectangle {
                                                        Layout.fillWidth: true
                                                        Layout.preferredHeight: 46
                                                        radius: 8
                                                        color: theme.background
                                                        border.color: theme.border

                                                        RowLayout {
                                                            anchors.fill: parent
                                                            anchors.leftMargin: 10
                                                            anchors.rightMargin: 10
                                                            spacing: 8

                                                            Text {
                                                                text: modelData.label
                                                                color: theme.muted
                                                                font.family: "Segoe UI Variable"
                                                                font.pixelSize: 12
                                                                Layout.preferredWidth: 74
                                                                elide: Text.ElideRight
                                                            }

                                                            Text {
                                                                Layout.fillWidth: true
                                                                text: modelData.value
                                                                color: theme.text
                                                                font.family: "Segoe UI Variable"
                                                                font.pixelSize: 13
                                                                font.bold: true
                                                                elide: Text.ElideRight
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }

                                    Rectangle {
                                        width: parent.width
                                        height: Math.max(174, fundTopHoldingLayout.implicitHeight + 32)
                                        radius: 18
                                        color: theme.panelRaised
                                        border.color: theme.border
                                        visible: localizationController.trCn(marketBoardController.selectedAssetKind) === "fund"

                                        ColumnLayout {
                                            id: fundTopHoldingLayout
                                            anchors.fill: parent
                                            anchors.margins: 16
                                            spacing: 10

                                            Text {
                                                text: localizationController.tr("mb.label.topHoldings")
                                                color: theme.text
                                                font.family: "Segoe UI Variable"
                                                font.pixelSize: 20
                                                font.bold: true
                                            }

                                            Rectangle {
                                                Layout.fillWidth: true
                                                Layout.preferredHeight: 30
                                                radius: 7
                                                color: theme.background
                                                border.color: theme.border

                                                RowLayout {
                                                    anchors.fill: parent
                                                    anchors.leftMargin: 10
                                                    anchors.rightMargin: 10
                                                    spacing: 8
                                                    Text { text: localizationController.tr("mb.col.name"); color: theme.muted; font.pixelSize: 12; font.bold: true; Layout.fillWidth: true }
                                                    Text { text: localizationController.tr("mb.col.change"); color: theme.muted; font.pixelSize: 12; font.bold: true; Layout.preferredWidth: 62; horizontalAlignment: Text.AlignRight }
                                                    Text { text: localizationController.tr("mb.col.weight"); color: theme.muted; font.pixelSize: 12; font.bold: true; Layout.preferredWidth: 52; horizontalAlignment: Text.AlignRight }
                                                    Text { text: localizationController.tr("mb.col.qoqDelta"); color: theme.muted; font.pixelSize: 12; font.bold: true; Layout.preferredWidth: 62; horizontalAlignment: Text.AlignRight }
                                                }
                                            }

                                            Repeater {
                                                model: marketBoardController.selectedFundTopHoldings

                                                Rectangle {
                                                    width: fundTopHoldingLayout.width
                                                    height: 34
                                                    radius: 7
                                                    color: index % 2 === 0 ? theme.rowEven : theme.rowOdd
                                                    border.color: theme.border

                                                    RowLayout {
                                                        anchors.fill: parent
                                                        anchors.leftMargin: 10
                                                        anchors.rightMargin: 10
                                                        spacing: 8
                                                        Text { text: modelData.name; color: theme.text; font.pixelSize: 12; font.bold: true; Layout.fillWidth: true; elide: Text.ElideRight }
                                                        Text { text: modelData.changePct; color: String(modelData.changePct).indexOf("-") === 0 ? theme.negative : theme.positive; font.pixelSize: 12; Layout.preferredWidth: 62; horizontalAlignment: Text.AlignRight }
                                                        Text { text: modelData.weight; color: theme.text; font.pixelSize: 12; Layout.preferredWidth: 52; horizontalAlignment: Text.AlignRight }
                                                        Text { text: modelData.delta; color: String(modelData.delta).indexOf("-") === 0 ? theme.negative : theme.positive; font.pixelSize: 12; Layout.preferredWidth: 62; horizontalAlignment: Text.AlignRight }
                                                    }
                                                }
                                            }

                                            Text {
                                                Layout.fillWidth: true
                                                visible: marketBoardController.selectedFundTopHoldings.length === 0
                                                text: localizationController.tr("mb.empty.fund")
                                                color: theme.muted
                                                font.family: "Segoe UI Variable"
                                                font.pixelSize: 13
                                                wrapMode: Text.WordWrap
                                            }
                                        }
                                    }

                                    TrendChart {
                                        width: parent.width
                                        height: implicitHeight
                                        titleText: localizationController.tr("mb.section.trend1y")
                                        subtitleText: localizationController.tr("mb.section.trend1y.subtitle")
                                        series: marketBoardController.selectedAssetOneYearTrend
                                        lineColor: theme.accent
                                    }

                                    TrendChart {
                                        width: parent.width
                                        height: implicitHeight
                                        titleText: localizationController.tr("mb.section.live1h")
                                        subtitleText: localizationController.tr("mb.section.live1h.subtitle")
                                        series: marketBoardController.selectedAssetOneHourDrawdown
                                        lineColor: theme.caution
                                    }

                                    Rectangle {
                                        width: parent.width
                                        height: Math.max(170, investmentAnalysisLayout.implicitHeight + 36)
                                        radius: 18
                                        color: theme.panelRaised
                                        border.color: theme.border

                                        ColumnLayout {
                                            id: investmentAnalysisLayout
                                            anchors.fill: parent
                                            anchors.margins: 18
                                            spacing: 10

                                            Text {
                                                text: localizationController.tr("mb.section.valueAnalysis")
                                                color: theme.text
                                                font.family: "Segoe UI Variable"
                                                font.pixelSize: 22
                                                font.bold: true
                                            }

                                            Text {
                                                Layout.fillWidth: true
                                                text: marketBoardController.selectedAssetInvestmentAnalysis.length > 0
                                                    ? localizationController.trCn(marketBoardController.selectedAssetInvestmentAnalysis)
                                                    : "选择左侧条目后，这里会展示投资价值分析。"
                                                color: theme.text
                                                font.family: "Segoe UI Variable"
                                                font.pixelSize: 18
                                                wrapMode: Text.WordWrap
                                            }
                                        }
                                    }

                                    Rectangle {
                                        width: parent.width
                                        height: Math.max(170, forecastAnalysisLayout.implicitHeight + 36)
                                        radius: 18
                                        color: theme.panelRaised
                                        border.color: theme.border

                                        ColumnLayout {
                                            id: forecastAnalysisLayout
                                            anchors.fill: parent
                                            anchors.margins: 18
                                            spacing: 10

                                            Text {
                                                text: localizationController.tr("mb.section.forecast6m")
                                                color: theme.text
                                                font.family: "Segoe UI Variable"
                                                font.pixelSize: 22
                                                font.bold: true
                                            }

                                            Text {
                                                Layout.fillWidth: true
                                                text: marketBoardController.selectedAssetSixMonthForecast.length > 0
                                                    ? localizationController.trCn(marketBoardController.selectedAssetSixMonthForecast)
                                                    : "选择左侧条目后，这里会展示未来 6 个月预测分析。"
                                                color: theme.text
                                                font.family: "Segoe UI Variable"
                                                font.pixelSize: 18
                                                wrapMode: Text.WordWrap
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
}
