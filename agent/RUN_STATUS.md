# 三 Agent 运行入口与状态

更新时间：2026-04-25

## 当前状态

- Agent 1：已启动，负责编码与编译。
- Agent 2：已启动，负责投研与风控研究。
- Agent 3：已启动，负责测试与验收。

本目录是三 agent 的轻量协作入口。下一轮继续工作时，先阅读本文件确认当前分工和构建命令，再按各自职责打开对应说明文件。

## 职责分工

- Agent 1 编码与编译：把研究需求拆成可编译、可验证的最小增量，优先沿用现有 Qt/QML 与 Controller 数据结构，完成后记录构建结果。
- Agent 2 投研与风控研究：提出可落地的投研、组合管理、交易提醒、估值研究和风险控制需求，说明用户价值、数据来源、触发规则、展示位置与风险提示。
- Agent 3 测试与验收：验证可用性、数据可信度、回测目标、风控目标和 UI 表现；未达标时把阻塞项回传给 Agent 1 或 Agent 2。

## 下一轮如何继续

1. Agent 2 先给出本轮最高价值需求，必须包含数据需求、触发规则、UI 展示位置和风险提示。
2. Agent 1 将需求拆为最小可编译增量，只改必要代码，并运行本轮构建命令。
3. Agent 3 根据构建结果和界面表现验收，明确通过项、阻塞项和下一轮建议。
4. 如果存在阻塞，下一轮从阻塞项开始；如果通过，下一轮继续补充更高价值的投研或风控能力。

## 本轮构建命令

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build-vs2022 --target stok_portfolio_board stok_data_hub stok_trade_alerts stok_risk_backtest stok_valuation_research --config RelWithDebInfo
```

## 2026-04-25 Agent 1 本轮修复

- 已修复 Agent 3 验收风险：风险回测在最大回撤/压力损失超过 -5% 时显示“不达标/需降风险/阻塞原因”，不再暗示达标。
- 已统一交易/投资动作枚举：UI action 字段仅使用“卖出/跟进/加仓/转仓/清仓”；“对冲/降波动/暂停加仓/限加仓/禁止同向加仓”等迁移到 riskInstruction/detail。
- 已给 Portfolio AI score schema 增加 minimum/maximum 0-100，并在接收 payload、加载/保存和表格展示路径 clamp 到 0-100。
- 构建通过：`stok_portfolio_board`、`stok_trade_alerts`、`stok_risk_backtest`，命令返回 0。构建日志仍出现 Qt deploy 阶段环境提示（`pwsh.exe` 不存在、`VCINSTALLDIR` 未设置 warning），未阻塞产物生成。

## 2026-04-25 Agent 1 P0/P1 门禁修复

- trade-alerts 增加本轮静态统一门禁：数据质量/风控未达标时不再显示“可执行加仓 2”，AAPL 从“加仓”降级为“跟进”，仓位动作改为“0% / 等待数据与风控达标”，并在 riskInstruction/detail 写明“数据质量/风控门禁阻塞强动作”。
- portfolio-board 增加本轮静态门禁：selectedAction、持仓表动作列和 action_for_entry 统一阻塞“加仓/卖出/清仓”等强动作，降级为“跟进”或“转仓”；selectedPositionPlan 显示数据质量模拟门禁或风控分未达标原因。
- data-hub 将非投资动作字段 `action` 改名为 `dataAction`，QML 同步读取 `dataAction`，避免被投资动作验收误判。
- portfolio-board 右侧 DecisionCard/InfoBlock 与 trade-alerts PlanCard 改为基于 implicitHeight 的动态高度，降低文字裁剪风险。
- 构建通过：`stok_portfolio_board`、`stok_trade_alerts`、`stok_data_hub`，命令返回 0。构建日志仍出现 Qt deploy 阶段环境提示（`pwsh.exe` 不存在、`VCINSTALLDIR` 未设置 warning），未阻塞产物生成。

## 交付约束

- 25% 收益和 5% 风险只能作为回测、压力测试或验收目标，不能写成收益承诺。
- 高风险动作，例如卖出、转仓、清仓、加仓，必须给出触发原因、风险依据和失效条件。
- 每轮必须产出可见产品改进，或明确说明为什么本轮无法构建。
- 多 agent 可能同时工作，不撤销其他 agent 的改动。
