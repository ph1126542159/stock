#!/usr/bin/env python3
"""Real-data value-investment board provider.

Pulls fund NAV, A-share spot, and HK spot from public sources via AKShare,
scores each asset with a transparent multi-factor formula and emits a JSON
snapshot in the schema the market-board service expects.

Output shape (also documented in market-board MarketBoardController::parseValueBoard):

    {
      "schema": "stok.value-board.v1",
      "generatedAt": <unix-seconds>,
      "institutionId": "<id>",
      "institutionName": "<display name>",
      "scoringWeights": { ... },        # transparency: weights printed in UI
      "errors": [ ... ],
      "funds": [
        {
          "id": "fund_<code>", "rank": 1,
          "name": "...", "code": "000248",
          "provider": "...", "category": "...",
          "score": 84.6,
          "latestPrice": 4.18,
          "oneYearReturnPct": 8.6,
          "investmentAnalysis": "...",
          "sixMonthForecast": "...",
          "oneYearTrend": [<24 sampled normalized points>],
          "oneHourDrawdown": [<12 zero placeholders for funds>],
          "factorBreakdown": {"return1y":..., "drawdown":..., "volatility":..., "sharpe":..., "valuation":...}
        }, ...
      ],
      "stocks": [ similar ]
    }

Scoring is intentionally simple and explainable, NOT a guarantee of return.
"""

from __future__ import annotations

import argparse
import datetime as _dt
import json
import math
import sys
import time
import traceback
from typing import Iterable

import numpy as np  # akshare's transitive dependency
import pandas as pd  # akshare's transitive dependency


# ---------------------------------------------------------------------------
# Curated pools. Picking from a quality-screened pool keeps real-data scoring
# fast (no full-market crawl) and keeps the recommendations auditable: every
# candidate is a name an investor could reasonably recognise.
# ---------------------------------------------------------------------------

# Funds: a mix of large-AUM index, dividend-style, value-style and bond-style
# funds available on every major Chinese wealth platform.
DEFAULT_FUND_POOL = [
    # (code, display name, category)
    ("110020", "易方达沪深300ETF联接A", "指数基金"),
    ("000248", "汇添富中证主要消费ETF联接A", "指数基金"),
    ("012102", "富国中证红利指数增强A", "红利指数"),
    ("110003", "易方达上证50增强A", "指数增强"),
    ("519712", "交银阿尔法核心混合A", "价值混合"),
    ("217022", "招商产业债券A", "债券型"),
    ("005827", "易方达蓝筹精选混合", "价值混合"),
    ("161725", "招商中证白酒指数(LOF)A", "行业指数"),
    ("003095", "中欧医疗健康混合A", "行业混合"),
    ("400015", "东方红睿丰混合", "价值混合"),
    ("000961", "天弘沪深300ETF联接A", "指数基金"),
    ("009777", "易方达蓝筹混合", "价值混合"),
    ("050026", "博时医疗保健行业混合A", "行业混合"),
    ("001102", "前海开源国家比较优势混合", "成长混合"),
    ("004997", "广发沪深300ETF联接C", "指数基金"),
]

# Stocks: blue-chip / dividend names tradeable on A and HK markets.
# Symbols use AKShare's spot conventions (6-digit for A-share, 5-digit for HK).
DEFAULT_STOCK_POOL = [
    # (a-share-code OR hk-code, display name, market label)
    ("600519", "贵州茅台", "A 股"),
    ("000333", "美的集团", "A 股"),
    ("601318", "中国平安", "A 股"),
    ("600036", "招商银行", "A 股"),
    ("000651", "格力电器", "A 股"),
    ("601398", "工商银行", "A 股"),
    ("600900", "长江电力", "A 股"),
    ("000858", "五粮液", "A 股"),
    ("00700", "腾讯控股", "港股"),
    ("00941", "中国移动", "港股"),
    ("00883", "中国海洋石油", "港股"),
    ("01299", "友邦保险", "港股"),
]

# Per-institution narrative tweaks. The numerical data is identical (the fund
# universe is universal) — institution selection only colors the analysis text.
INSTITUTION_STYLE = {
    "cmb-wealth": ("招商银行财富", "稳健配置"),
    "icbc-wealth": ("工商银行财富", "稳健配置"),
    "alipay-wealth": ("支付宝财富", "宽基定投"),
    "wechat-licaitong": ("微信理财通", "稳健底仓"),
    "eastmoney-fund": ("天天基金", "进取均衡"),
}


# ---------------------------------------------------------------------------
# Scoring weights. Documented for the UI so users see HOW each score is built.
# ---------------------------------------------------------------------------

SCORING_WEIGHTS = {
    "return1y": 0.25,    # 1-year total return (higher is better)
    "drawdown": 0.20,    # 1-year max drawdown (smaller is better)
    "volatility": 0.15,  # 1-year annualized volatility (smaller is better)
    "sharpe": 0.20,      # 1-year sharpe (higher is better)
    "valuation": 0.20,   # PE-percentile-style score (lower PE is better, capped)
}


def _now_iso() -> str:
    return _dt.datetime.now().strftime("%Y-%m-%dT%H:%M:%S")


def _clip(value: float, lo: float, hi: float) -> float:
    if math.isnan(value) or math.isinf(value):
        return (lo + hi) / 2
    return max(lo, min(hi, value))


def _norm(value: float, lo: float, hi: float, *, invert: bool = False) -> float:
    """Map raw value into 0-100 by linear interpolation between lo and hi."""
    if math.isnan(value):
        return 50.0
    if hi == lo:
        return 50.0
    pct = (value - lo) / (hi - lo)
    pct = _clip(pct, 0.0, 1.0)
    if invert:
        pct = 1.0 - pct
    return round(pct * 100, 1)


def _resample_24(series: list[float]) -> list[float]:
    """Reduce a long net-value time series down to 24 normalized samples (50-100)."""
    if not series:
        return []
    arr = np.array(series, dtype=float)
    if len(arr) > 24:
        idx = np.linspace(0, len(arr) - 1, 24).astype(int)
        arr = arr[idx]
    base = float(arr[0]) or 1.0
    normalized = [round(50 + (v / base - 1) * 100, 2) for v in arr]
    return normalized


# ---------------------------------------------------------------------------
# Fund scoring
# ---------------------------------------------------------------------------

def score_fund(code: str, ak_module) -> dict:
    """Pull a fund's daily NAV history, compute factors, return a dict the UI can consume."""
    import warnings
    warnings.filterwarnings("ignore")

    nav_df = ak_module.fund_open_fund_info_em(symbol=code, indicator="单位净值走势")
    if nav_df is None or nav_df.empty:
        raise RuntimeError(f"fund {code}: empty NAV history")
    nav_df = nav_df.dropna(subset=["净值日期", "单位净值"]).copy()
    nav_df["净值日期"] = pd.to_datetime(nav_df["净值日期"])
    nav_df = nav_df.sort_values("净值日期")
    nav_df["nav"] = pd.to_numeric(nav_df["单位净值"], errors="coerce")
    nav_df = nav_df.dropna(subset=["nav"])

    if len(nav_df) < 20:
        raise RuntimeError(f"fund {code}: not enough history ({len(nav_df)} rows)")

    latest_nav = float(nav_df.iloc[-1]["nav"])
    latest_date = nav_df.iloc[-1]["净值日期"].date().isoformat()

    # Last 252 trading days (about 1 year)
    one_year = nav_df.tail(252)
    start_nav = float(one_year.iloc[0]["nav"])
    one_year_return_pct = (latest_nav / start_nav - 1) * 100 if start_nav else 0.0

    # Daily returns (1-year window)
    daily_ret = one_year["nav"].pct_change().dropna()
    if daily_ret.empty:
        volatility = 0.0
        sharpe = 0.0
        max_drawdown = 0.0
    else:
        volatility = float(daily_ret.std() * math.sqrt(252) * 100)  # %
        # Risk-free ~2.0% (rough RMB short rate); not a guarantee, just a normaliser.
        excess = float(daily_ret.mean() * 252 * 100) - 2.0
        sharpe = excess / volatility if volatility > 1e-6 else 0.0
        cum = (1 + daily_ret).cumprod()
        peak = cum.cummax()
        max_drawdown = float(((cum - peak) / peak).min() * 100)  # negative %

    score_return = _norm(one_year_return_pct, -10.0, 25.0)
    score_drawdown = _norm(max_drawdown, -25.0, 0.0)              # bigger (less neg) is better
    score_volatility = _norm(volatility, 5.0, 30.0, invert=True)
    score_sharpe = _norm(sharpe, -0.5, 1.5)
    # Funds don't have PE; valuation factor uses a flat neutral 60 to keep weighting honest.
    score_valuation = 60.0

    overall = (
        score_return * SCORING_WEIGHTS["return1y"] +
        score_drawdown * SCORING_WEIGHTS["drawdown"] +
        score_volatility * SCORING_WEIGHTS["volatility"] +
        score_sharpe * SCORING_WEIGHTS["sharpe"] +
        score_valuation * SCORING_WEIGHTS["valuation"]
    )
    overall = round(_clip(overall, 0, 100), 1)

    trend_24 = _resample_24(one_year["nav"].astype(float).tolist())

    return {
        "code": code,
        "score": overall,
        "latestPrice": round(latest_nav, 4),
        "latestDate": latest_date,
        "oneYearReturnPct": round(one_year_return_pct, 2),
        "factorBreakdown": {
            "return1y": score_return,
            "drawdown": score_drawdown,
            "volatility": score_volatility,
            "sharpe": score_sharpe,
            "valuation": score_valuation,
        },
        "rawFactors": {
            "return1yPct": round(one_year_return_pct, 2),
            "maxDrawdownPct": round(max_drawdown, 2),
            "annualVolPct": round(volatility, 2),
            "sharpe": round(sharpe, 2),
        },
        "oneYearTrend": trend_24,
    }


def build_funds(institution_id: str, ak_module, errors: list, limit: int) -> list[dict]:
    out: list[dict] = []
    for code, name, category in DEFAULT_FUND_POOL[:limit]:
        try:
            scored = score_fund(code, ak_module)
        except Exception as exc:  # pragma: no cover — depend on remote
            errors.append({"source": f"fund:{code}", "message": str(exc)})
            continue
        analysis = (
            f"近 1 年实际收益 {scored['rawFactors']['return1yPct']}%，"
            f"年化波动 {scored['rawFactors']['annualVolPct']}%，"
            f"最大回撤 {scored['rawFactors']['maxDrawdownPct']}%，"
            f"夏普 {scored['rawFactors']['sharpe']}。"
        )
        forecast = (
            "评分基于真实净值，非收益承诺；后续 6 个月收益区间应结合权益市场情绪自行判断。"
        )
        out.append({
            "id": f"fund_{code}",
            "rank": 0,  # filled after sort
            "name": name,
            "code": code,
            "provider": INSTITUTION_STYLE.get(institution_id, ("--", ""))[0],
            "category": category,
            "score": scored["score"],
            "latestPrice": scored["latestPrice"],
            "oneYearReturnPct": scored["oneYearReturnPct"],
            "investmentAnalysis": analysis,
            "sixMonthForecast": forecast,
            "oneYearTrend": scored["oneYearTrend"],
            "oneHourDrawdown": [0.0] * 12,  # funds publish daily NAV, no intraday curve
            "factorBreakdown": scored["factorBreakdown"],
            "rawFactors": scored["rawFactors"],
            "latestDate": scored["latestDate"],
        })
    out.sort(key=lambda r: r["score"], reverse=True)
    for i, r in enumerate(out, start=1):
        r["rank"] = i
    return out


# ---------------------------------------------------------------------------
# Stock scoring (uses pre-fetched A-share + HK spot, dispatched by code)
# ---------------------------------------------------------------------------

def _hk_history(code5: str, ak_module) -> pd.DataFrame:
    # Daily history for HK
    end = _dt.datetime.now().strftime("%Y%m%d")
    start = (_dt.datetime.now() - _dt.timedelta(days=400)).strftime("%Y%m%d")
    return ak_module.stock_hk_hist(symbol=code5, period="daily", start_date=start, end_date=end, adjust="qfq")


def _a_history(code6: str, ak_module) -> pd.DataFrame:
    end = _dt.datetime.now().strftime("%Y%m%d")
    start = (_dt.datetime.now() - _dt.timedelta(days=400)).strftime("%Y%m%d")
    return ak_module.stock_zh_a_hist(symbol=code6, period="daily", start_date=start, end_date=end, adjust="qfq")


def score_stock(code: str, market: str, ak_module) -> dict:
    if market == "A 股":
        df = _a_history(code, ak_module)
        close_col = "收盘"
    else:
        df = _hk_history(code, ak_module)
        close_col = "收盘"

    if df is None or df.empty:
        raise RuntimeError(f"stock {code}: no history")
    df = df.dropna(subset=[close_col]).copy()
    df["date"] = pd.to_datetime(df["日期"])
    df = df.sort_values("date").tail(252)
    if len(df) < 30:
        raise RuntimeError(f"stock {code}: history too short ({len(df)})")

    close = pd.to_numeric(df[close_col], errors="coerce").dropna()
    latest = float(close.iloc[-1])
    start = float(close.iloc[0])
    one_year_return_pct = (latest / start - 1) * 100 if start else 0.0
    daily_ret = close.pct_change().dropna()
    volatility = float(daily_ret.std() * math.sqrt(252) * 100)
    excess = float(daily_ret.mean() * 252 * 100) - 2.0
    sharpe = excess / volatility if volatility > 1e-6 else 0.0
    cum = (1 + daily_ret).cumprod()
    peak = cum.cummax()
    max_drawdown = float(((cum - peak) / peak).min() * 100)

    score_return = _norm(one_year_return_pct, -10.0, 35.0)
    score_drawdown = _norm(max_drawdown, -35.0, 0.0)
    score_volatility = _norm(volatility, 10.0, 60.0, invert=True)
    score_sharpe = _norm(sharpe, -0.5, 1.5)
    # Without PE percentile we use a static neutral; left as placeholder for upgrade.
    score_valuation = 55.0

    overall = (
        score_return * SCORING_WEIGHTS["return1y"] +
        score_drawdown * SCORING_WEIGHTS["drawdown"] +
        score_volatility * SCORING_WEIGHTS["volatility"] +
        score_sharpe * SCORING_WEIGHTS["sharpe"] +
        score_valuation * SCORING_WEIGHTS["valuation"]
    )
    overall = round(_clip(overall, 0, 100), 1)

    trend_24 = _resample_24(close.astype(float).tolist())
    # 12 most recent points as the "1-hour drawdown" curve placeholder. The UI
    # accepts any 12-point series; intraday minute bars can replace this later.
    last12 = close.tail(12).astype(float).tolist()
    base = last12[0] if last12 else 1.0
    drawdown12 = [round((v / base - 1) * 100, 2) for v in last12]

    return {
        "code": code,
        "score": overall,
        "latestPrice": round(latest, 2),
        "latestDate": df.iloc[-1]["date"].date().isoformat(),
        "oneYearReturnPct": round(one_year_return_pct, 2),
        "factorBreakdown": {
            "return1y": score_return,
            "drawdown": score_drawdown,
            "volatility": score_volatility,
            "sharpe": score_sharpe,
            "valuation": score_valuation,
        },
        "rawFactors": {
            "return1yPct": round(one_year_return_pct, 2),
            "maxDrawdownPct": round(max_drawdown, 2),
            "annualVolPct": round(volatility, 2),
            "sharpe": round(sharpe, 2),
        },
        "oneYearTrend": trend_24,
        "oneHourDrawdown": drawdown12,
    }


def build_stocks(ak_module, errors: list, limit: int) -> list[dict]:
    out: list[dict] = []
    for code, name, market in DEFAULT_STOCK_POOL[:limit]:
        try:
            scored = score_stock(code, market, ak_module)
        except Exception as exc:
            errors.append({"source": f"stock:{code}", "message": str(exc)})
            continue
        analysis = (
            f"近 1 年实际收益 {scored['rawFactors']['return1yPct']}%，"
            f"年化波动 {scored['rawFactors']['annualVolPct']}%，"
            f"最大回撤 {scored['rawFactors']['maxDrawdownPct']}%，"
            f"夏普 {scored['rawFactors']['sharpe']}。"
        )
        forecast = "评分基于近 1 年真实价格序列，非收益承诺；高风险动作请结合个人风险承受能力判断。"
        out.append({
            "id": f"stock_{code}",
            "rank": 0,
            "name": name,
            "code": code if "." in code or len(code) != 6 else (
                f"{code}.SH" if code.startswith("6") else f"{code}.SZ"
            ),
            "provider": "公开行情",
            "category": "蓝筹标的" if market == "A 股" else "蓝筹标的",
            "score": scored["score"],
            "latestPrice": scored["latestPrice"],
            "oneYearReturnPct": scored["oneYearReturnPct"],
            "investmentAnalysis": analysis,
            "sixMonthForecast": forecast,
            "oneYearTrend": scored["oneYearTrend"],
            "oneHourDrawdown": scored["oneHourDrawdown"],
            "factorBreakdown": scored["factorBreakdown"],
            "rawFactors": scored["rawFactors"],
            "latestDate": scored["latestDate"],
        })
    out.sort(key=lambda r: r["score"], reverse=True)
    for i, r in enumerate(out, start=1):
        r["rank"] = i
    return out


# ---------------------------------------------------------------------------
# CLI entrypoint
# ---------------------------------------------------------------------------

def main() -> int:
    parser = argparse.ArgumentParser(description="Real-data value-investment board provider")
    parser.add_argument("--institution-id", default="cmb-wealth")
    parser.add_argument("--institution-name", default="")
    parser.add_argument("--funds", type=int, default=10, help="Funds to score (limit pool)")
    parser.add_argument("--stocks", type=int, default=5, help="Stocks to score (limit pool)")
    parser.add_argument("--json", action="store_true", help="Print JSON only (default true)")
    args = parser.parse_args()

    institution_id = args.institution_id.strip() or "cmb-wealth"
    institution_name = args.institution_name.strip() or INSTITUTION_STYLE.get(
        institution_id, ("--", ""))[0]

    snapshot = {
        "schema": "stok.value-board.v1",
        "generatedAt": int(time.time()),
        "generatedAtIso": _now_iso(),
        "institutionId": institution_id,
        "institutionName": institution_name,
        "scoringWeights": SCORING_WEIGHTS,
        "errors": [],
        "funds": [],
        "stocks": [],
    }

    try:
        import akshare as ak  # type: ignore
    except Exception as exc:
        snapshot["errors"].append({"source": "akshare", "message": str(exc)})
        print(json.dumps(snapshot, ensure_ascii=False))
        return 1

    snapshot["funds"] = build_funds(institution_id, ak, snapshot["errors"], args.funds)
    snapshot["stocks"] = build_stocks(ak, snapshot["errors"], args.stocks)

    print(json.dumps(snapshot, ensure_ascii=False, separators=(",", ":")))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except SystemExit:
        raise
    except Exception:
        traceback.print_exc(file=sys.stderr)
        sys.exit(2)
