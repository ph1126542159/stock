#!/usr/bin/env python3
"""Real-data fund holdings (look-through) provider.

For a given mainland Chinese mutual fund code, return:
  * Top 10 underlying stocks (most recent quarterly disclosure)
  * Top sector / industry allocation breakdown
  * Concentration metrics so the UI can flag crowding risk

Output schema (printed as JSON to stdout):

  {
    "schema": "stok.fund-holdings.v1",
    "generatedAt": <unix-seconds>,
    "fundCode": "110020",
    "asOf": "2025-12-31",
    "topHoldings": [
      {"rank": 1, "code": "300750", "name": "宁德时代",
       "weightPct": 3.0, "marketValueWan": 569.62},
      ...
    ],
    "industryAllocation": [
      {"name": "制造业", "weightPct": 46.0, "marketValueWan": 8755.96},
      ...
    ],
    "concentration": {
      "topTenPct": 28.5,            # sum of top-10 weight (concentration metric)
      "firstHoldingPct": 3.0,       # single-name concentration
      "industryCount": 12,
      "topIndustryPct": 46.0
    },
    "errors": [...]
  }

The script honors `--funds A,B,C` so the controller can warm a small
batch of cached funds, but in practice the UI calls it on demand for
the currently-selected fund.
"""

from __future__ import annotations

import argparse
import datetime as _dt
import json
import sys
import time
import traceback


def _safe_float(v, default: float = 0.0) -> float:
    try:
        if v is None: return default
        if isinstance(v, str):
            v = v.replace(",", "").replace("%", "").strip()
            if not v or v in {"-", "--", "N/A"}: return default
        return float(v)
    except Exception:
        return default


def fetch_one(ak, fund_code: str) -> dict:
    out: dict = {
        "fundCode": fund_code,
        "asOf": "",
        "topHoldings": [],
        "industryAllocation": [],
        "concentration": {},
        "errors": [],
    }

    # Try the most recent year, then fall back if empty.
    today = _dt.date.today()
    candidate_years = [str(today.year), str(today.year - 1)]
    holdings_df = None
    for year in candidate_years:
        try:
            df = ak.fund_portfolio_hold_em(symbol=fund_code, date=year)
            if df is not None and not df.empty:
                holdings_df = df
                break
        except Exception as exc:
            out["errors"].append({"source": f"holdings:{year}", "message": str(exc)})

    if holdings_df is not None and not holdings_df.empty:
        # Pick the latest reporting period present in the data.
        if "季度" in holdings_df.columns:
            latest_period = holdings_df["季度"].iloc[0]
            holdings_df = holdings_df[holdings_df["季度"] == latest_period]
            out["asOf"] = str(latest_period)
        # Top 10 ordered by 占净值比例 desc.
        weight_col = "占净值比例"
        if weight_col in holdings_df.columns:
            holdings_df = holdings_df.sort_values(weight_col, ascending=False)
        for _, row in holdings_df.head(10).iterrows():
            weight_raw = _safe_float(row.get(weight_col, 0))
            # Eastmoney usually returns weight as either fraction (0.03) or
            # percent (3.0). Normalize to percent.
            weight_pct = weight_raw * 100 if weight_raw < 1.5 else weight_raw
            out["topHoldings"].append({
                "rank": int(row.get("序号", len(out["topHoldings"]) + 1)),
                "code": str(row.get("股票代码", "")),
                "name": str(row.get("股票名称", "")),
                "weightPct": round(weight_pct, 2),
                "marketValueWan": round(_safe_float(row.get("持仓市值", 0)), 2),
            })

    # Industry allocation
    industry_df = None
    for year in candidate_years:
        try:
            df = ak.fund_portfolio_industry_allocation_em(symbol=fund_code, date=year)
            if df is not None and not df.empty:
                industry_df = df
                break
        except Exception as exc:
            out["errors"].append({"source": f"industry:{year}", "message": str(exc)})

    if industry_df is not None and not industry_df.empty:
        if "截止时间" in industry_df.columns:
            latest_date = industry_df["截止时间"].iloc[0]
            industry_df = industry_df[industry_df["截止时间"] == latest_date]
            if not out["asOf"]:
                out["asOf"] = str(latest_date)
        if "占净值比例" in industry_df.columns:
            industry_df = industry_df.sort_values("占净值比例", ascending=False)
        for _, row in industry_df.head(8).iterrows():
            weight_raw = _safe_float(row.get("占净值比例", 0))
            weight_pct = weight_raw * 100 if weight_raw < 1.5 else weight_raw
            if weight_pct < 0.5:  # filter trivial buckets
                continue
            out["industryAllocation"].append({
                "name": str(row.get("行业类别", "")),
                "weightPct": round(weight_pct, 2),
                "marketValueWan": round(_safe_float(row.get("市值", 0)), 2),
            })

    # Concentration metrics
    top_ten = sum(h["weightPct"] for h in out["topHoldings"])
    first = out["topHoldings"][0]["weightPct"] if out["topHoldings"] else 0.0
    top_industry = out["industryAllocation"][0]["weightPct"] if out["industryAllocation"] else 0.0
    out["concentration"] = {
        "topTenPct": round(top_ten, 2),
        "firstHoldingPct": round(first, 2),
        "industryCount": len(out["industryAllocation"]),
        "topIndustryPct": round(top_industry, 2),
    }
    return out


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--funds", required=False, default="",
        help="Comma-separated fund codes (e.g. 110020,000248).")
    parser.add_argument("--fund", required=False, default="",
        help="Single fund code (preferred for on-demand UI calls).")
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()

    requested: list[str] = []
    if args.fund:
        requested.append(args.fund.strip())
    if args.funds:
        for code in args.funds.split(","):
            code = code.strip()
            if code and code not in requested:
                requested.append(code)
    if not requested:
        print(json.dumps({"schema": "stok.fund-holdings.v1", "errors": [{"source": "args", "message": "no fund code given"}]}, ensure_ascii=False))
        return 1

    snapshot = {
        "schema": "stok.fund-holdings.v1",
        "generatedAt": int(time.time()),
        "generatedAtIso": _dt.datetime.now().strftime("%Y-%m-%dT%H:%M:%S"),
        "funds": [],
        "errors": [],
    }

    try:
        import akshare as ak  # type: ignore
    except Exception as exc:
        snapshot["errors"].append({"source": "akshare", "message": str(exc)})
        print(json.dumps(snapshot, ensure_ascii=False))
        return 1

    for code in requested:
        try:
            snapshot["funds"].append(fetch_one(ak, code))
        except Exception as exc:
            snapshot["errors"].append({"fund": code, "message": str(exc)})

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
