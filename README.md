# stok 投资工作台

一个面向中国大陆个人投资者的桌面投资工作台。
基于 macchina.io / Poco / Qt 6 QML，宿主多个独立子服务（行情榜、价值投资榜、持仓、交易提醒、风险回测、估值研究、数据中心、配置中心），通过 Fast-DDS 总线互联。

> ⚠ 25% 收益与 5% 风险只作为回测和验收目标，**不作为收益承诺**。任何 UI、报告和提示都不会出现"保证收益、稳赚、必涨"等确定性表述。

## 运行方式

构建后的产物位于 `build-vs2022/bin/`，主入口是 `macchina.exe`：

```powershell
cmd /c start "" /D "e:\stock\build-vs2022\bin" "e:\stock\build-vs2022\bin\macchina.exe"
```

`macchina.exe` 会按 `macchina.properties` 自动拉起 9 个 stok-* 子服务（desktop-shell, market-board, market-data-service, portfolio-board, valuation-research, risk-backtest, trade-alerts, data-hub, config-center）。桌面壳窗口标题为 `Stok 投资工作台`。

⚠ 不要用 PowerShell 的 `Start-Process` 启动，desktop-shell 拿不到交互桌面 session 会自己 `exit 0`。

## 关键修改记录（本批次）

### 真实数据接入（替代 LLM 与示例数据）

- 价值投资榜的"基金 + 股票"评分改成由 [`tools/value_board_provider.py`](tools/value_board_provider.py) 通过 [AKShare](https://github.com/akfamily/akshare) 抓真实净值/行情，按透明的多因子公式打分（25% 1y 回报 + 20% 最大回撤 + 20% 夏普 + 15% 波动率 + 20% 估值，权重在脚本顶部 `SCORING_WEIGHTS`）。前 10 支基金 + 5 支股票来自一个手工筛选的蓝筹候选池，所有候选都是公开存在的代码（`110020 易方达沪深300ETF联接A` / `600519 贵州茅台` 等）。
- 启动期 `purgeNonRealValueBoardData()` 清空 `value_board_assets` 表，杜绝任何残留示例数据。脚本失败时**保留库内上一次成功数据**，绝不退回到内置假数据。
- 美股观察的实时曲线由 [`tools/us_history_provider.py`](tools/us_history_provider.py) 启动期一次性 backfill 60 个分钟级真实历史点（个股用 akshare `stock_us_hist_min_em`，指数用 `index_us_stock_sina` 把日 K 收盘价合成等距 timestamp）。
- 占位页面（持仓 / 交易提醒 / 风控回测 / 估值研究）顶部加红色横幅 "本页为占位示例，未接入真实数据，不构成投资建议"。

### 自包含 Python 运行时

- 新增 [`cmake/StokPythonBundle.cmake`](cmake/StokPythonBundle.cmake) 在 configure 阶段下载 embeddable Python 3.11.9，bootstrap pip，安装 AKShare 全部依赖到 `<build>/python-embed/`，install 时复制到 `<prefix>/bin/python-embed/`。
- market-board controller `resolve_python_command()` 优先查找相对 exe 路径下的 `python-embed/python.exe`，所以**用户机器零依赖**，无需自行安装 Python 或 AKShare。
- 安装尺寸：剥离 `__pycache__/tests/*.pyc` 后约 154 MB。

### Bug 修复

- **腾讯 GBK 行情解码**：Qt 6 的 `QStringConverter::encodingForName("GB18030")` 永远返回 nullopt（Qt6 删了 textcodec module），导致基金/股票名称在 SQLite 与 UI 上呈现 `Æ»¹û` (应为"苹果")、`µÀÇíË¹` (应为"道琼斯") 之类的 mojibake。改用 Win32 `MultiByteToWideChar(936, ...)` 显式 GBK 解码。
- **腾讯字段索引 off-by-one**：`fields[31]` 是涨跌额（美元），`fields[32]` 才是涨跌幅 (%)；之前读 [31] 把道指当日下跌 313.62 USD 渲染成了 `-313.62%`，会让客户做出灾难性决策。同时 `fields[29]` 改 `fields[30]` 取时间戳。
- **美股观察实时曲线静止**：QML Sparkline (Canvas) 需要显式 `onSeriesChanged: requestPaint()`，且当 `lastPrice` 不变时 series 数组内容不变，binding 不会触发重绘。`syncUsMarketModel` 每 tick 给 `realtimeTrend` 末尾追加一个 `lastPrice`（cap 64 点），让 series 长度每秒变化、QML 必触发 onSeriesChanged，曲线视觉上每秒滚动；盘中真实价变化时直接体现，盘后曲线左滚但保留前段真实分钟历史。
- **大盘行情曲线不刷新**：QML `fetchEastmoneyIndexes` 与 controller 的腾讯行情请求都用 `https://`，Qt 6 自带 OpenSSL 在 `push2.eastmoney.com` 与 `qt.gtimg.cn` 上 TLS 握手不稳；改成 `http://`（公开行情数据，可以走 HTTP）。
- **默认语言改为中文**：`LocalizationController` 与 `LocalizationClient` 默认值从 `English` 改成 `Chinese`，shell 启动直接广播 `zh`。
- **切换页面被拉回主页**：`DesktopShellController::registerHostedProcess` 之前每次子进程窗口重建（HWND 改变）就把焦点抢回 default view，用户根本切不出去。改成只在当前 active view 真的不可用时才回退到 default。

### 工程结构

```
agent/                    # 三 agent 协作流程文档（编码/研究/测试）
cmake/StokPythonBundle.cmake   # 内嵌 Python + AKShare 打包脚本
platform/                 # 厂内 macchina.io 平台扩展源
server/                   # macchina 主进程
services/
  common/                 # LocalizationClient, HostedViewPublisher 等
  config/                 # 各服务运行时配置
  config-center/          # 配置中心 (DDS publisher)
  data-hub/               # 数据中心
  desktop-shell/          # 桌面壳 (主窗口/导航/日志)
  log-viewer/             # 日志查看
  market-board/           # 行情榜 + 价值投资榜
  market-data-service/    # 行情接收
  portfolio-board/        # 持仓 (占位)
  trade-alerts/           # 交易提醒 (占位)
  risk-backtest/          # 风险回测 (占位)
  valuation-research/     # 估值研究 (占位)
  feature-page/           # 通用功能页运行器
tools/
  free_data_provider.py   # 行业资金流 + 巨潮公告抓取
  value_board_provider.py # 价值投资榜真实数据 + 多因子评分
  us_history_provider.py  # 美股观察分钟级历史 backfill
```

## 已知边界 / 待办

- 价值评分中的 valuation 因子目前是占位常数（基金 60、股票 55），真正接 PE 分位 / 股息率需要更长时间的 akshare `stock_a_indicator_lg`。
- QML 里 `tr()` 是 `Q_INVOKABLE` 不是 NOTIFY 属性，运行时切换语言不会自动重新计算所有绑定文本——目前默认就是中文，绕开了这个限制。彻底修复需要给每个子服务的 Main.qml 上百处 `tr(...)` 调用绑定一个 `revision` 属性。
- 持仓 / 交易提醒 / 风控回测 / 估值研究四页仍是占位示例，已在顶部加红色警告横幅。
