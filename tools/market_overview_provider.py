#!/usr/bin/env python3
"""Unified provider for the market-board overview page.

This script bypasses the flaky eastmoney push2 endpoint entirely and pulls
every dataset the overview tabs need from healthier sources:

  * Tencent qt.gtimg.cn  -> realtime spot for the 8 main A-share / HK indices
  * Tencent ifzq.gtimg.cn -> 30-day daily-K so each sparkline has a unique
    real shape even after market close
  * AKShare stock_sector_spot (Sina) -> 49 industry sector boards with
    average price / change / leader stock
  * AKShare stock_hsgt_fund_flow_summary_em -> northbound + southbound
    (深港通 / 沪港通) net inflow today
  * AKShare stock_index_pe_lg -> historical PE for 上证50 / 沪深300 / 中证500
    so we can compute a real 5-year PE percentile (not a fabricated 50)
  * AKShare stock_yjyg_em -> latest earnings preannouncements
  * AKShare stock_yjkb_em -> latest earnings flash reports

Everything ends up in one JSON snapshot consumed by MarketBoardController and
fanned out to each ReferencePage tab.
"""

from __future__ import annotations

import argparse
import datetime as _dt
import json
import os
import re
import sys
import time
import traceback
import urllib.parse
import urllib.request

# Suppress tqdm progress bars on stderr -- when this script is launched as
# a QProcess child the parent's stderr pipe can fill up and block akshare
# mid-fetch. Setting TQDM_DISABLE alone isn't enough on some versions; also
# monkey-patch tqdm to a no-op once the module loads.
os.environ.setdefault("TQDM_DISABLE", "1")
try:
    import tqdm  # type: ignore
    def _silent_tqdm(iterable=None, *args, **kwargs):
        return iterable if iterable is not None else iter(())
    tqdm.tqdm = _silent_tqdm  # type: ignore
    tqdm.trange = lambda *a, **kw: range(*a)  # type: ignore
except Exception:
    pass


def _safe_float(value, default: float = 0.0) -> float:
    try:
        if value is None:
            return default
        if isinstance(value, str):
            value = value.replace(",", "").replace("%", "").strip()
            if not value or value in {"-", "--", "N/A"}:
                return default
        result = float(value)
        if result != result:  # NaN
            return default
        return result
    except Exception:
        return default


# ---------------------------------------------------------------------------
# 1. Indices (spot + 30d daily-K) via Tencent
# ---------------------------------------------------------------------------

INDICES = [
    # (Tencent secid, display code, friendly name)
    ("sh000001", "000001.SH", "上证指数"),
    ("sz399001", "399001.SZ", "深证成指"),
    ("sz399006", "399006.SZ", "创业板指"),
    ("sh000688", "000688.SH", "科创50"),
    ("sh000300", "000300.SH", "沪深300"),
    ("sh000905", "000905.SH", "中证500"),
    ("sh000985", "000985.SH", "中证全指"),
    ("hkHSTECH", "HSTECH.HK", "恒生科技"),
]


def _tencent_index_spot() -> list[dict]:
    secids = ",".join(s[0] for s in INDICES)
    url = f"http://qt.gtimg.cn/q={secids}"
    req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
    text = urllib.request.urlopen(req, timeout=8).read().decode("gbk", errors="replace")
    out = []
    for line in text.strip().splitlines():
        if "=" not in line:
            continue
        m = re.search(r'"([^"]*)"', line)
        if not m:
            continue
        fields = m.group(1).split("~")
        if len(fields) < 35:
            continue
        secid_match = re.search(r"v_(\w+)=", line)
        if not secid_match:
            continue
        secid = secid_match.group(1)
        meta = next((s for s in INDICES if s[0] == secid), None)
        if not meta:
            continue
        last_price = _safe_float(fields[3])
        change_pct = _safe_float(fields[32])  # day change %
        up_count = _safe_float(fields[34], 0)
        down_count = _safe_float(fields[35], 0) if len(fields) > 35 else 0
        # Tencent A-share pkt: f6 turnover-yuan at index 6, but for indices it's
        # the cumulative turnover. Different positions per market, fall back to 0.
        amount = 0.0
        for idx in (37, 36, 6):
            if idx < len(fields):
                v = _safe_float(fields[idx])
                if v > 1_000_000:  # heuristic: turnover in CNY
                    amount = v
                    break
        out.append({
            "secid": secid,
            "code": meta[1],
            "name": fields[1] or meta[2],
            "lastPrice": last_price,
            "changePct": change_pct,
            "amount": amount,
            "upCount": up_count,
            "downCount": down_count,
            "tradingTime": fields[30] if len(fields) > 30 else "",
        })
    return out


def _tencent_index_kline_30d(secid: str) -> list[float]:
    url = f"http://web.ifzq.gtimg.cn/appstock/app/fqkline/get?param={secid},day,,,30,"
    text = urllib.request.urlopen(url, timeout=8).read().decode("utf-8", errors="replace")
    payload = json.loads(text)
    days = (payload.get("data", {}).get(secid, {}) or {}).get("day", [])
    closes = []
    for row in days:
        try:
            closes.append(float(row[2]))  # [date, open, close, high, low, vol]
        except Exception:
            continue
    return closes


def fetch_indices() -> list[dict]:
    rows = _tencent_index_spot()
    by_secid = {r["secid"]: r for r in rows}
    for secid, _, _ in INDICES:
        if secid not in by_secid:
            continue
        try:
            by_secid[secid]["closes30d"] = _tencent_index_kline_30d(secid)
        except Exception:
            by_secid[secid]["closes30d"] = []
    return list(by_secid.values())


# ---------------------------------------------------------------------------
# 2. Industry sectors (Sina via AKShare)
# ---------------------------------------------------------------------------

def fetch_sectors(ak_module) -> list[dict]:
    df = ak_module.stock_sector_spot()
    rows = []
    for _, row in df.iterrows():
        rows.append({
            "name": str(row.get("板块", "")),
            "label": str(row.get("label", "")),
            "stockCount": int(_safe_float(row.get("公司家数", 0))),
            "avgPrice": _safe_float(row.get("平均价格", 0)),
            "changePct": _safe_float(row.get("涨跌幅", 0)),
            "turnover": _safe_float(row.get("总成交额", 0)),
            "leaderCode": str(row.get("股票代码", "")),
            "leaderName": str(row.get("股票名称", "")),
            "leaderChange": _safe_float(row.get("个股-涨跌幅", 0)),
            "leaderPrice": _safe_float(row.get("个股-当前价", 0)),
        })
    rows.sort(key=lambda r: r["changePct"], reverse=True)
    return rows


# ---------------------------------------------------------------------------
# 3. Northbound / Southbound flow
# ---------------------------------------------------------------------------

def fetch_hsgt(ak_module) -> dict:
    try:
        df = ak_module.stock_hsgt_fund_flow_summary_em()
    except Exception as exc:
        return {"error": str(exc)}
    out = {"flows": [], "tradingDay": ""}
    for _, row in df.iterrows():
        out["tradingDay"] = str(row.get("交易日", out["tradingDay"]))
        out["flows"].append({
            "type": str(row.get("类型", "")),
            "board": str(row.get("板块", "")),
            "direction": str(row.get("资金方向", "")),
            "netBuy": _safe_float(row.get("成交净买额", 0)),
            "netInflow": _safe_float(row.get("资金净流入", 0)),
            "upCount": int(_safe_float(row.get("上涨数", 0))),
            "flatCount": int(_safe_float(row.get("持平数", 0))),
            "downCount": int(_safe_float(row.get("下跌数", 0))),
            "indexName": str(row.get("相关指数", "")),
            "indexChange": _safe_float(row.get("指数涨跌幅", 0)),
        })
    return out


# ---------------------------------------------------------------------------
# 4. Real PE percentile for the major indices
# ---------------------------------------------------------------------------

PE_INDEX_NAMES = ["上证50", "沪深300", "中证500", "上证180", "创业板50"]


def fetch_pe_percentile(ak_module) -> list[dict]:
    out = []
    for name in PE_INDEX_NAMES:
        try:
            df = ak_module.stock_index_pe_lg(symbol=name)
        except Exception as exc:
            out.append({"index": name, "error": str(exc)})
            continue
        if df is None or df.empty:
            continue
        # Use the latest 5 years of rolling PE (滚动市盈率) for the percentile
        df = df.sort_values("日期")
        cutoff = _dt.date.today() - _dt.timedelta(days=365 * 5)
        recent = df[df["日期"] >= cutoff]
        col = "滚动市盈率" if "滚动市盈率" in recent.columns else "等权滚动市盈率"
        if col not in recent.columns or recent.empty:
            continue
        series = recent[col].dropna()
        if series.empty:
            continue
        latest = float(series.iloc[-1])
        # historical percentile = fraction of past values <= latest
        pct = float((series <= latest).mean()) * 100
        out.append({
            "index": name,
            "currentPe": round(latest, 2),
            "percentile5y": round(pct, 1),
            "median5y": round(float(series.median()), 2),
            "min5y": round(float(series.min()), 2),
            "max5y": round(float(series.max()), 2),
            "samples": int(len(series)),
        })
    return out


# ---------------------------------------------------------------------------
# 5. Earnings calendar (preannouncements + flash reports)
# ---------------------------------------------------------------------------

def fetch_earnings(ak_module, limit: int = 30) -> dict:
    out: dict = {"preannouncements": [], "flashReports": []}
    today = _dt.date.today()
    quarter_dates = [
        today.replace(month=12, day=31, year=today.year - 1),
        today.replace(month=9, day=30) if today.month >= 9 else today.replace(month=9, day=30, year=today.year - 1),
    ]
    for d in quarter_dates:
        try:
            df = ak_module.stock_yjyg_em(date=d.strftime("%Y%m%d"))
        except Exception:
            continue
        if df is None or df.empty:
            continue
        # Take rows with a recent announcement date
        if "公告日期" in df.columns:
            df = df.sort_values("公告日期", ascending=False)
        for _, row in df.head(limit).iterrows():
            out["preannouncements"].append({
                "code": str(row.get("股票代码", "")),
                "name": str(row.get("股票简称", "")),
                "metric": str(row.get("预测指标", "")),
                "type": str(row.get("预告类型", "")),
                "summary": str(row.get("业绩变动", ""))[:120],
                "yoyPct": _safe_float(row.get("业绩变动幅度", 0)),
                "noticeDate": str(row.get("公告日期", "")),
            })
        if out["preannouncements"]:
            break

    try:
        df_kb = ak_module.stock_yjkb_em(date=quarter_dates[0].strftime("%Y%m%d"))
        if df_kb is not None and not df_kb.empty:
            if "公告日期" in df_kb.columns:
                df_kb = df_kb.sort_values("公告日期", ascending=False)
            for _, row in df_kb.head(limit).iterrows():
                out["flashReports"].append({
                    "code": str(row.get("股票代码", "")),
                    "name": str(row.get("股票简称", "")),
                    "industry": str(row.get("所处行业", "")),
                    "revenueYoyPct": _safe_float(row.get("营业收入-同比增长", 0)),
                    "netProfitYoyPct": _safe_float(row.get("净利润-同比增长", 0)),
                    "roe": _safe_float(row.get("净资产收益率", 0)),
                    "noticeDate": str(row.get("公告日期", "")),
                })
    except Exception:
        pass
    return out


# ---------------------------------------------------------------------------
# CLI entrypoint
# ---------------------------------------------------------------------------

def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--skip", default="", help="comma-separated sections to skip (sectors,hsgt,pe,earnings)")
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()
    skip = {s.strip() for s in args.skip.split(",") if s.strip()}

    snapshot = {
        "schema": "stok.market-overview.v1",
        "generatedAt": int(time.time()),
        "generatedAtIso": _dt.datetime.now().strftime("%Y-%m-%dT%H:%M:%S"),
        "indices": [],
        "sectors": [],
        "hsgt": {},
        "pePercentile": [],
        "earnings": {"preannouncements": [], "flashReports": []},
        "errors": [],
    }

    # Indices first since they don't need akshare
    try:
        snapshot["indices"] = fetch_indices()
    except Exception as exc:
        snapshot["errors"].append({"source": "indices", "message": str(exc)})

    try:
        import akshare as ak  # type: ignore
    except Exception as exc:
        snapshot["errors"].append({"source": "akshare", "message": str(exc)})
        print(json.dumps(snapshot, ensure_ascii=False, separators=(",", ":")))
        return 0

    if "sectors" not in skip:
        try:
            snapshot["sectors"] = fetch_sectors(ak)
        except Exception as exc:
            snapshot["errors"].append({"source": "sectors", "message": str(exc)})

    if "hsgt" not in skip:
        try:
            snapshot["hsgt"] = fetch_hsgt(ak)
        except Exception as exc:
            snapshot["errors"].append({"source": "hsgt", "message": str(exc)})

    if "pe" not in skip:
        try:
            snapshot["pePercentile"] = fetch_pe_percentile(ak)
        except Exception as exc:
            snapshot["errors"].append({"source": "pe", "message": str(exc)})

    if "earnings" not in skip:
        try:
            snapshot["earnings"] = fetch_earnings(ak)
        except Exception as exc:
            snapshot["errors"].append({"source": "earnings", "message": str(exc)})

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
