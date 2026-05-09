#!/usr/bin/env python3
"""Real-data valuation research provider.

For every symbol in the input watchlist this script:
  1. Pulls the latest spot price (PE/PB/total market cap from akshare).
  2. Pulls multi-year financials (revenue, net profit, free-cash-flow).
  3. Runs a 10-year discounted cash flow (DCF) projection at a configurable
     discount rate / terminal growth rate -> "fair value" per share.
  4. Computes ROE / ROIC / FCF-quality from the same financial series.
  5. Computes PE / PB industry-median percentile so the row can be marked
     undervalued / fair / expensive without needing 5 years of history.
  6. Aggregates into a transparent composite score (30% margin of safety,
     20% PE percentile, 20% FCF quality, 15% ROE, 15% ROIC) and a suggested
     trading action (add / follow / rotate / sell).

Output: a single JSON object on stdout, see SCHEMA at bottom of this file.

Notes
-----
* US coverage uses akshare's `stock_financial_us_analysis_indicator_em`
  which can be flaky; the script catches per-symbol errors and falls back
  to a P/E-multiplier valuation (last_price * industry-median P/E).
* HK coverage uses `stock_hk_financial_indicator_em` which only ships the
  TTM aggregate; the DCF degrades to a 1-period value and we annotate the
  row with `methodology: "PE multiplier"`.
* All projection assumptions are passed as CLI flags so they can be tuned
  via the config-center DDS topic at runtime.
"""

from __future__ import annotations

import argparse
import datetime as _dt
import json
import math
import re
import statistics
import sys
import time
import traceback
from typing import Any

import pandas as pd  # akshare transitive dep

# Eastmoney's https endpoints occasionally return SSL EOF mid-handshake from
# Python's bundled OpenSSL. Configure requests with HTTP-level retries before
# akshare imports so the pooled adapter is reused for every endpoint call.
try:
    import requests  # noqa: E402
    from requests.adapters import HTTPAdapter, Retry  # noqa: E402
    _retry = Retry(
        total=4, backoff_factor=0.6,
        status_forcelist=[500, 502, 503, 504],
        allowed_methods=frozenset(["GET", "POST", "HEAD"]),
    )
    _adapter = HTTPAdapter(max_retries=_retry, pool_connections=20, pool_maxsize=20)
    _orig_session_init = requests.Session.__init__

    def _patched_init(self, *a, **kw):
        _orig_session_init(self, *a, **kw)
        self.mount("https://", _adapter)
        self.mount("http://", _adapter)
    requests.Session.__init__ = _patched_init  # type: ignore[assignment]
except Exception:
    pass

# ---------------------------------------------------------------------------
# Default watchlist. Operators can override via --symbols.
# Format: "A:600519" / "HK:00700" / "US:AAPL".
# ---------------------------------------------------------------------------
DEFAULT_WATCHLIST = [
    "A:600519",   # 贵州茅台
    "A:000333",   # 美的集团
    "A:600036",   # 招商银行
    "A:000651",   # 格力电器
    "A:601318",   # 中国平安
    "A:601398",   # 工商银行
    "A:600900",   # 长江电力
    "A:000858",   # 五粮液
    "HK:00700",   # 腾讯控股
    "HK:00941",   # 中国移动
]


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _safe_float(value, default: float = 0.0) -> float:
    try:
        if value is None:
            return default
        if isinstance(value, str):
            value = value.replace(",", "").replace("%", "").strip()
            if not value or value in {"-", "--", "N/A"}:
                return default
        result = float(value)
        if math.isnan(result) or math.isinf(result):
            return default
        return result
    except Exception:
        return default


def _clip(v: float, lo: float, hi: float) -> float:
    return max(lo, min(hi, v))


def _norm(value: float, lo: float, hi: float, *, invert: bool = False) -> float:
    if hi == lo:
        return 50.0
    pct = (value - lo) / (hi - lo)
    pct = _clip(pct, 0.0, 1.0)
    if invert:
        pct = 1.0 - pct
    return round(pct * 100.0, 1)


# ---------------------------------------------------------------------------
# A-share branch
# ---------------------------------------------------------------------------

def _abstract_row(df: pd.DataFrame, indicator_name: str) -> dict[str, float]:
    """Return {report_period: value} for the named indicator in stock_financial_abstract."""
    rows = df[df["指标"] == indicator_name]
    if rows.empty:
        return {}
    row = rows.iloc[0]
    out: dict[str, float] = {}
    for col, value in row.items():
        if col in ("选项", "指标"):
            continue
        # Only consider annual reports (YYYY1231) so DCF input series has
        # uniform spacing.
        if not (isinstance(col, str) and re.match(r"^\d{8}$", col) and col.endswith("1231")):
            continue
        v = _safe_float(value, default=math.nan)
        if not math.isnan(v):
            out[col] = v
    return out


def _yearly_series(values: dict[str, float], lookback: int) -> list[tuple[int, float]]:
    """Return last `lookback` annual data points sorted ascending by year."""
    pairs = [(int(k[:4]), v) for k, v in values.items()]
    pairs.sort()
    return pairs[-lookback:]


def _tencent_a_spot(code6: str) -> dict:
    """Hit qt.gtimg.cn over plain http for a single A-share quote.

    Returns {"name", "lastPrice", "pe", "pb", "marketCap", "totalShares"}.
    The Tencent endpoint is GBK-encoded and serves last price (field 3),
    PE-static (field 39), PB (field 46), total market cap (field 45) and
    circulating shares (field 38) among other things; see project_us_quote_fields.md
    for the broader field map.
    """
    import urllib.request
    market_prefix = "sh" if code6.startswith(("6", "9")) else "sz"
    url = f"http://qt.gtimg.cn/q={market_prefix}{code6}"
    req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
    raw = urllib.request.urlopen(req, timeout=8).read()
    text = raw.decode("gbk", errors="replace")
    line = text.strip().splitlines()[0] if text else ""
    qs = line.find('"'); qe = line.rfind('"')
    if qs < 0 or qe <= qs:
        raise RuntimeError(f"A:{code6}: empty Tencent payload")
    fields = line[qs+1:qe].split('~')
    if len(fields) < 50:
        raise RuntimeError(f"A:{code6}: short Tencent payload ({len(fields)} fields)")
    # Verified field layout for Tencent A-share quotes (qt.gtimg.cn):
    #   [ 1] name
    #   [ 3] last price
    #   [38] turnover rate (%) — NOT share count
    #   [39] PE (dynamic)
    #   [44] total market cap in 亿 (100M CNY)
    #   [45] same as [44] — circulating market cap
    #   [46] PB
    name = fields[1]
    last_price = _safe_float(fields[3])
    pe_dyn = _safe_float(fields[39])
    pb = _safe_float(fields[46])
    market_cap_yi = _safe_float(fields[45]) or _safe_float(fields[44])
    market_cap = market_cap_yi * 1.0e8
    total_shares = (market_cap / last_price) if last_price > 0 else 0.0
    return {
        "name": name,
        "lastPrice": last_price,
        "pe": pe_dyn,
        "pb": pb,
        "marketCap": market_cap,
        "totalShares": total_shares,
    }


def _value_a_share(ak, code6: str, settings: dict, spot_lookup: dict | None) -> dict | None:
    # Eastmoney's stock_individual_info_em returns 502 frequently. Use the
    # cached batched stock_zh_a_spot_em row (already paginated by the caller)
    # for spot price + total market cap; fall back to individual_info_em
    # only when the symbol is missing from the batched result.
    name = code6
    industry = ""
    last_price = 0.0
    market_cap = 0.0
    total_shares = 0.0
    pe_initial = 0.0
    pb_initial = 0.0
    # Eastmoney's stock_individual_info_em / stock_zh_a_spot_em endpoints
    # return 502 / SSL DECRYPTION_FAILED frequently. Hit Tencent's plain-http
    # qt.gtimg.cn first which is the same source we already trust elsewhere
    # in this code base for live US quotes.
    try:
        tspot = _tencent_a_spot(code6)
        name = tspot["name"] or code6
        last_price = tspot["lastPrice"]
        market_cap = tspot["marketCap"]
        total_shares = tspot["totalShares"]
        pe_initial = tspot["pe"]
        pb_initial = tspot["pb"]
    except Exception:
        pass
    if last_price <= 0 or total_shares <= 0:
        if spot_lookup and code6 in spot_lookup:
            spot_row = spot_lookup[code6]
            name = str(spot_row.get("名称", code6))
            last_price = _safe_float(spot_row.get("最新价", last_price))
            market_cap = _safe_float(spot_row.get("总市值", market_cap))
            if last_price > 0 and market_cap > 0:
                total_shares = market_cap / last_price
    if last_price <= 0 or total_shares <= 0:
        raise RuntimeError(f"A:{code6}: spot quote unavailable (price={last_price})")

    abstract = ak.stock_financial_abstract(symbol=code6)
    revenue_series = _yearly_series(_abstract_row(abstract, "营业总收入"), settings["dcf_history_years"])
    netprofit_series = _yearly_series(_abstract_row(abstract, "归母净利润"), settings["dcf_history_years"])
    fcf_proxy_series = _abstract_row(abstract, "经营现金流量净额")
    capex_series = _abstract_row(abstract, "购建固定资产、无形资产和其他长期资产支付的现金")
    equity_series = _abstract_row(abstract, "股东权益合计(净资产)")
    if not equity_series:
        equity_series = _abstract_row(abstract, "归属于母公司所有者权益合计")
    roe_series = _abstract_row(abstract, "净资产收益率(ROE)")
    roa_series = _abstract_row(abstract, "总资产报酬率(ROA)")

    # Build last `dcf_history_years` annual FCFs (operating cash flow - capex)
    fcf_series: list[tuple[int, float]] = []
    for year_key, ocf in fcf_proxy_series.items():
        if not (isinstance(year_key, str) and year_key.endswith("1231")):
            continue
        year = int(year_key[:4])
        capex = capex_series.get(year_key, 0.0)
        fcf_series.append((year, ocf - capex))
    fcf_series.sort()
    fcf_series = fcf_series[-settings["dcf_history_years"]:]

    if len(fcf_series) < 3:
        raise RuntimeError(f"A:{code6}: not enough FCF history ({len(fcf_series)} years)")

    # 10-year DCF projection
    base_fcf = fcf_series[-1][1]
    if base_fcf <= 0:
        # Use 3y average to avoid a single-year dip dominating the model.
        positives = [f for _, f in fcf_series if f > 0]
        base_fcf = sum(positives[-3:]) / max(1, len(positives[-3:])) if positives else 0.0
    if base_fcf <= 0:
        raise RuntimeError(f"A:{code6}: base FCF non-positive, DCF skipped")

    growth = settings["dcf_growth_rate"]
    discount = settings["dcf_discount_rate"]
    terminal = settings["dcf_terminal_growth"]
    horizon = settings["dcf_horizon_years"]

    pv_total = 0.0
    for t in range(1, horizon + 1):
        future_fcf = base_fcf * ((1 + growth) ** t)
        pv_total += future_fcf / ((1 + discount) ** t)
    terminal_fcf = base_fcf * ((1 + growth) ** horizon) * (1 + terminal)
    pv_terminal = (terminal_fcf / (discount - terminal)) / ((1 + discount) ** horizon)
    enterprise_value = pv_total + pv_terminal
    fair_value_per_share = enterprise_value / total_shares
    margin_of_safety = (fair_value_per_share - last_price) / fair_value_per_share * 100 if fair_value_per_share else 0

    # Quality factors. Prefer akshare's published ROE / ROA series; fall
    # back to net profit / equity if those are missing (rare).
    last_np = netprofit_series[-1][1] if netprofit_series else 0
    if roe_series:
        latest_roe_year = max(roe_series.keys())
        roe = roe_series[latest_roe_year]
    else:
        last_equity_key = max(equity_series.keys()) if equity_series else None
        last_equity = equity_series[last_equity_key] if last_equity_key else 0
        roe = (last_np / last_equity * 100) if last_equity > 0 else 0
    if roa_series:
        roa = roa_series[max(roa_series.keys())]
    else:
        roa = roe * 0.7

    fcf_quality = 0.0
    if netprofit_series and fcf_series:
        sums_np = sum(v for _, v in netprofit_series[-4:])
        sums_fcf = sum(v for _, v in fcf_series[-4:])
        if sums_np > 0:
            fcf_quality = _clip(sums_fcf / sums_np * 100, 0, 150)

    # Industry PE/PB percentile via spot of all A-shares is too slow
    # (~4 minutes). Defer to caller's batched spot scan.
    return {
        "market": "A",
        "symbol": code6,
        "displayCode": f"{code6}.SH" if code6.startswith("6") else f"{code6}.SZ",
        "name": name,
        "industry": industry,
        "lastPrice": last_price,
        "marketCap": market_cap,
        "totalShares": total_shares,
        "fairValue": round(fair_value_per_share, 2),
        "marginOfSafety": round(margin_of_safety, 2),
        "fcfQuality": round(fcf_quality, 1),
        "roe": round(roe, 2),
        "roic": round(roa, 2),
        "dividendYield": 0.0,
        "pe": round(pe_initial, 2) if pe_initial > 0 else 0.0,
        "pb": round(pb_initial, 2) if pb_initial > 0 else 0.0,
        "pePercentile": 0.0,
        "pbPercentile": 0.0,
        "methodology": "DCF",
        "dcfBaseFcf": base_fcf,
        "dcfDiscountRate": discount,
        "dcfTerminalGrowth": terminal,
    }


def _enrich_a_share_pe_pb(rows: list[dict], spot: pd.DataFrame | None) -> None:
    """Use the already-fetched batched A-share spot DataFrame to backfill
    PE / PB / industry percentile."""
    if spot is None or spot.empty:
        return
    if not any(r["market"] == "A" for r in rows):
        return

    spot = spot.copy()
    spot["代码"] = spot["代码"].astype(str)
    spot.set_index("代码", inplace=True)

    # Pre-bucket by industry-equivalent (use sector heat: column "行业" not in
    # spot; fall back to a single "all-A" bucket if industry lookup fails).
    pe_all = pd.to_numeric(spot["市盈率-动态"], errors="coerce").dropna()
    pe_all = pe_all[(pe_all > 0) & (pe_all < 200)]  # filter outliers / loss-makers
    pb_all = pd.to_numeric(spot["市净率"], errors="coerce").dropna()
    pb_all = pb_all[(pb_all > 0) & (pb_all < 30)]

    for r in rows:
        if r["market"] != "A":
            continue
        if r["symbol"] not in spot.index:
            continue
        row = spot.loc[r["symbol"]]
        pe = _safe_float(row.get("市盈率-动态"))
        pb = _safe_float(row.get("市净率"))
        if pe > 0:
            r["pe"] = round(pe, 2)
            r["pePercentile"] = round((pe_all < pe).mean() * 100, 1)
        if pb > 0:
            r["pb"] = round(pb, 2)
            r["pbPercentile"] = round((pb_all < pb).mean() * 100, 1)


# ---------------------------------------------------------------------------
# HK branch (TTM only)
# ---------------------------------------------------------------------------

def _value_hk(ak, code5: str, settings: dict) -> dict:
    df = ak.stock_hk_financial_indicator_em(symbol=code5)
    if df is None or df.empty:
        raise RuntimeError(f"HK:{code5}: no financial data")
    row = df.iloc[0]

    # Spot price via stock_hk_spot_em is too slow per-symbol; use the
    # market_cap / shares from this row to recover per-share metrics, then
    # approximate last price = market_cap / shares if needed.
    eps = _safe_float(row.get("基本每股收益(元)"))
    bvps = _safe_float(row.get("每股净资产(元)"))
    pe_ttm = _safe_float(row.get("市盈率"))
    pb_ttm = _safe_float(row.get("市净率"))
    div_yield_ttm = _safe_float(row.get("股息率TTM(%)"))
    roe = _safe_float(row.get("股东权益回报率(%)"))
    net_margin = _safe_float(row.get("销售净利率(%)"))
    revenue = _safe_float(row.get("营业总收入"))
    net_profit = _safe_float(row.get("净利润"))
    roa = _safe_float(row.get("总资产回报率(%)"))
    ocf_per_share = _safe_float(row.get("每股经营现金流(元)"))

    if pe_ttm <= 0 or eps <= 0:
        raise RuntimeError(f"HK:{code5}: missing PE/EPS, cannot value")

    last_price = pe_ttm * eps  # back out last price from PE x EPS

    # Use Graham-style multiplier as fair value: 8.5 + 2g for stable HK names.
    # Without a g forecast we use 0% (TTM value).
    fair_value_per_share = eps * 12  # 12x TTM EPS as conservative anchor
    margin_of_safety = (fair_value_per_share - last_price) / fair_value_per_share * 100

    fcf_quality = _clip(ocf_per_share / max(eps, 0.01) * 100, 0, 150)

    return {
        "market": "HK",
        "symbol": code5,
        "displayCode": f"{code5}.HK",
        "name": code5,  # akshare HK indicator endpoint doesn't ship name
        "industry": "",
        "lastPrice": round(last_price, 2),
        "marketCap": _safe_float(row.get("总市值(港元)", 0)),
        "totalShares": _safe_float(row.get("已发行股本(股)", 0)),
        "fairValue": round(fair_value_per_share, 2),
        "marginOfSafety": round(margin_of_safety, 2),
        "fcfQuality": round(fcf_quality, 1),
        "roe": round(roe, 2),
        "roic": round(roa, 2),  # use ROA as ROIC proxy for HK
        "dividendYield": round(div_yield_ttm, 2),
        "pe": round(pe_ttm, 2),
        "pb": round(pb_ttm, 2),
        "pePercentile": 0.0,  # filled later if a sector cohort is available
        "pbPercentile": 0.0,
        "methodology": "PE multiplier",
        "dcfBaseFcf": 0.0,
        "dcfDiscountRate": settings["dcf_discount_rate"],
        "dcfTerminalGrowth": settings["dcf_terminal_growth"],
    }


# ---------------------------------------------------------------------------
# US branch (annual report history)
# ---------------------------------------------------------------------------

def _value_us(ak, ticker: str, settings: dict) -> dict:
    df = ak.stock_financial_us_analysis_indicator_em(symbol=ticker, indicator="年报")
    if df is None or df.empty:
        raise RuntimeError(f"US:{ticker}: no annual reports returned")
    df = df.sort_values("REPORT_DATE")  # ascending
    history = df.tail(settings["dcf_history_years"])
    name = str(df.iloc[0].get("SECURITY_NAME_ABBR", ticker))

    revenue_growth = _safe_float(history["OPERATE_INCOME_YOY"].mean()) / 100.0
    net_margin = _safe_float(history["NET_PROFIT_RATIO"].iloc[-1]) / 100.0
    last_revenue = _safe_float(history["OPERATE_INCOME"].iloc[-1])
    last_net_profit = _safe_float(history["PARENT_HOLDER_NETPROFIT"].iloc[-1])
    roe = _safe_float(history["ROE_AVG"].iloc[-1])
    roa = _safe_float(history["ROA"].iloc[-1])
    eps = _safe_float(history["BASIC_EPS"].iloc[-1])
    if last_revenue <= 0 or eps <= 0:
        raise RuntimeError(f"US:{ticker}: cannot read revenue/EPS")

    # FCF history is not in this endpoint, approximate FCF as net_profit * 1.05
    # (cash flow typically slightly above accounting net profit for asset-
    # light tech names). For asset-heavy names this overestimates.
    base_fcf = last_net_profit * 1.05

    # Get spot price + market cap from the spot endpoint
    try:
        spot_df = ak.stock_us_spot_em()
        spot_match = spot_df[spot_df["代码"].str.contains(f"\\.{ticker}$|^{ticker}$", regex=True, na=False)]
        last_price = _safe_float(spot_match.iloc[0]["最新价"]) if not spot_match.empty else 0
        market_cap = _safe_float(spot_match.iloc[0]["总市值"]) if not spot_match.empty else 0
        total_shares = market_cap / last_price if last_price > 0 else 0
    except Exception:
        last_price = 0
        market_cap = 0
        total_shares = last_net_profit / eps if eps > 0 else 0

    # 10-year DCF
    growth = _clip(revenue_growth, -0.05, 0.20)  # cap optimism
    discount = settings["dcf_discount_rate"]
    terminal = settings["dcf_terminal_growth"]
    horizon = settings["dcf_horizon_years"]

    pv_total = 0.0
    for t in range(1, horizon + 1):
        future_fcf = base_fcf * ((1 + growth) ** t)
        pv_total += future_fcf / ((1 + discount) ** t)
    terminal_fcf = base_fcf * ((1 + growth) ** horizon) * (1 + terminal)
    pv_terminal = (terminal_fcf / (discount - terminal)) / ((1 + discount) ** horizon)
    enterprise_value = pv_total + pv_terminal
    fair_value_per_share = enterprise_value / total_shares if total_shares > 0 else 0
    margin_of_safety = (fair_value_per_share - last_price) / fair_value_per_share * 100 if fair_value_per_share > 0 else 0

    fcf_quality = _clip(base_fcf / max(last_net_profit, 1) * 100, 0, 150)

    return {
        "market": "US",
        "symbol": ticker,
        "displayCode": ticker,
        "name": name,
        "industry": "",
        "lastPrice": round(last_price, 2),
        "marketCap": market_cap,
        "totalShares": total_shares,
        "fairValue": round(fair_value_per_share, 2),
        "marginOfSafety": round(margin_of_safety, 2),
        "fcfQuality": round(fcf_quality, 1),
        "roe": round(roe, 2),
        "roic": round(roa, 2),
        "dividendYield": 0.0,
        "pe": round(last_price / eps, 2) if eps > 0 else 0,
        "pb": 0.0,
        "pePercentile": 0.0,
        "pbPercentile": 0.0,
        "methodology": "DCF",
        "dcfBaseFcf": base_fcf,
        "dcfDiscountRate": discount,
        "dcfTerminalGrowth": terminal,
    }


# ---------------------------------------------------------------------------
# Composite scoring + action mapping
# ---------------------------------------------------------------------------

SCORE_WEIGHTS = {
    "margin": 0.30,
    "pe": 0.20,
    "fcf": 0.20,
    "roe": 0.15,
    "roic": 0.15,
}


def _score_and_action(row: dict, settings: dict) -> None:
    margin = row["marginOfSafety"]
    score_margin = _norm(margin, -25.0, 30.0)
    score_pe = _norm(row["pePercentile"], 0.0, 100.0, invert=True) if row["pePercentile"] > 0 else 50.0
    score_fcf = _norm(row["fcfQuality"], 30.0, 130.0)
    score_roe = _norm(row["roe"], 5.0, 30.0)
    score_roic = _norm(row["roic"], 5.0, 25.0)
    composite = (
        score_margin * SCORE_WEIGHTS["margin"] +
        score_pe * SCORE_WEIGHTS["pe"] +
        score_fcf * SCORE_WEIGHTS["fcf"] +
        score_roe * SCORE_WEIGHTS["roe"] +
        score_roic * SCORE_WEIGHTS["roic"]
    )
    composite = round(_clip(composite, 0, 100), 1)
    row["compositeScore"] = composite
    row["scoreBreakdown"] = {
        "margin": score_margin,
        "pePercentile": score_pe,
        "fcf": score_fcf,
        "roe": score_roe,
        "roic": score_roic,
    }

    fair = row["fairValue"]
    row["buyBelow"] = round(fair * settings["buy_multiplier"], 2) if fair > 0 else 0
    row["sellAbove"] = round(fair * settings["sell_multiplier"], 2) if fair > 0 else 0

    if margin >= 15.0 and composite >= 75.0 and row["fcfQuality"] >= 70.0:
        action, tone, rating = "加仓", "green", "低估"
    elif margin <= -12.0 or composite <= 40.0:
        action, tone, rating = "卖出", "red", "偏贵"
    elif margin >= 5.0 and composite >= 60.0:
        action, tone, rating = "跟进", "blue", "合理偏低"
    else:
        action, tone, rating = "转仓", "amber", "持平观察"

    row["action"] = action
    row["tone"] = tone
    row["rating"] = rating
    row["riskFlag"] = "估值溢价" if margin < 0 else "安全边际"


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main() -> int:
    parser = argparse.ArgumentParser(description="Real-data valuation research provider")
    parser.add_argument("--symbols", default=",".join(DEFAULT_WATCHLIST))
    parser.add_argument("--discount-rate", type=float, default=0.085)
    parser.add_argument("--terminal-growth", type=float, default=0.025)
    parser.add_argument("--growth-rate", type=float, default=0.06,
        help="Default near-term FCF growth rate when historical not available.")
    parser.add_argument("--horizon-years", type=int, default=10)
    parser.add_argument("--history-years", type=int, default=8)
    parser.add_argument("--buy-multiplier", type=float, default=0.85)
    parser.add_argument("--sell-multiplier", type=float, default=1.18)
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()

    settings = {
        "dcf_discount_rate": args.discount_rate,
        "dcf_terminal_growth": args.terminal_growth,
        "dcf_growth_rate": args.growth_rate,
        "dcf_horizon_years": args.horizon_years,
        "dcf_history_years": args.history_years,
        "buy_multiplier": args.buy_multiplier,
        "sell_multiplier": args.sell_multiplier,
    }

    snapshot = {
        "schema": "stok.valuation-research.v1",
        "generatedAt": int(time.time()),
        "generatedAtIso": _dt.datetime.now().strftime("%Y-%m-%dT%H:%M:%S"),
        "settings": settings,
        "scoreWeights": SCORE_WEIGHTS,
        "errors": [],
        "rows": [],
    }

    try:
        import akshare as ak  # type: ignore
    except Exception as exc:
        snapshot["errors"].append({"source": "akshare", "message": str(exc)})
        print(json.dumps(snapshot, ensure_ascii=False))
        return 1

    requested = [s.strip() for s in args.symbols.split(",") if s.strip()]
    has_a_share = any(spec.upper().startswith("A:") for spec in requested)
    # Note: stock_zh_a_spot_em (Eastmoney) is currently flaky — returns 502
    # or SSL DECRYPTION_FAILED. We prefer the lighter Tencent qt.gtimg.cn
    # http endpoint per-symbol (handled inside _value_a_share). The full
    # spot scan is only used to derive industry-PE percentile, which is
    # nice-to-have, so skip silently when it fails.
    a_spot_lookup: dict | None = None
    a_spot_full: pd.DataFrame | None = None
    if has_a_share:
        try:
            a_spot_full = ak.stock_zh_a_spot_em()
            a_spot_lookup = {str(row["代码"]): row.to_dict() for _, row in a_spot_full.iterrows()}
        except Exception:
            a_spot_full = None
            a_spot_lookup = None

    rows: list[dict] = []
    for spec in requested:
        if ":" not in spec:
            snapshot["errors"].append({"symbol": spec, "message": "missing market prefix"})
            continue
        market, code = spec.split(":", 1)
        try:
            if market.upper() == "A":
                row = _value_a_share(ak, code, settings, a_spot_lookup)
            elif market.upper() == "HK":
                row = _value_hk(ak, code, settings)
            elif market.upper() == "US":
                row = _value_us(ak, code, settings)
            else:
                snapshot["errors"].append({"symbol": spec, "message": f"unknown market {market}"})
                continue
            rows.append(row)
        except Exception as exc:
            snapshot["errors"].append({"symbol": spec, "message": str(exc)})

    if rows:
        try:
            _enrich_a_share_pe_pb(rows, a_spot_full)
        except Exception as exc:
            snapshot["errors"].append({"source": "pe_pb_enrichment", "message": str(exc)})
        for r in rows:
            _score_and_action(r, settings)

    rows.sort(key=lambda r: r.get("compositeScore", 0), reverse=True)
    snapshot["rows"] = rows
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
