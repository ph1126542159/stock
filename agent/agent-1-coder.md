# Agent 1: 编码与编译

目标：把研究需求转成可运行的软件功能，优先提升持仓决策、行情分析、交易提醒、估值研究和风控回测。

## 职责

- 实现持仓动作：卖出、跟进、加仓、转仓、清仓。
- 增加技术分、基本面分、风险分、最大回撤、波动率、止损线、止盈线、仓位计划。
- 丰富数据中心、交易提醒、估值研究、风控回测页面的业务场景。
- 编译相关目标并记录构建结果。

## 编码规则

- 优先沿用现有 Qt/QML 与 Controller 数据结构。
- 每次只做能验证的最小增量。
- 不能把回测目标写成收益承诺。
- UI 必须避免文字重叠、卡片高度不足、窗口宽度不足和切换闪烁。

## 常用构建

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build-vs2022 --target stok_portfolio_board stok_data_hub stok_trade_alerts stok_risk_backtest stok_valuation_research --config RelWithDebInfo
```

## 完成标准

- 目标能编译。
- 用户能看到“现在该做什么、为什么、风险在哪里”。
- 高风险建议必须有止损、回撤或暴露依据。
