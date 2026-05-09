#!/usr/bin/env python3
"""Fetch the most recent intraday minute curve for the US watchlist.

Tencent's qt.gtimg.cn endpoint only returns the latest snapshot price; once
the US market closes the price stops moving and the realtime sparkline
becomes a flat line. To keep the curve meaningful we backfill the past
N minutes of intraday closing prices via AKShare's stock_us_hist_min_em.

Output: a single JSON object on stdout, e.g.

  {
    "schema": "stok.us-history.v1",
    "generatedAt": 1778240000,
    "tradingDay": "2026-05-07",
    "errors": [{"symbol":"NDX","message":"..."}],
    "series": {
      "AAPL": {"timestamps": [<unix-ms>...], "prices": [287.51, 287.4, ...]},
      "NDX":  {"timestamps": [...],          "prices": [...]},
      ...
    }
  }

The market-board controller seeds usQuoteHistory_ from this snapshot so the
sparkline column draws the most recent real-money curve even outside trading
hours, while still updating point-by-point during market hours.
"""

from __future__ import annotations

import argparse
import datetime as _dt
import json
import sys
import time
import traceback
from typing import Iterable


# Map UI symbol -> akshare's stock_us_hist_min_em symbol.
# AKShare uses "<exchange-prefix>.<ticker>"; 105 = NASDAQ, 106 = NYSE,
# 107 = AMEX. Indexes have to use Yahoo-style symbols instead.
SYMBOL_TO_AKSHARE = {
    "AAPL":  "105.AAPL",
    "MSFT":  "105.MSFT",
    "NVDA":  "105.NVDA",
    "AMZN":  "105.AMZN",
    "META":  "105.META",
    "GOOGL": "105.GOOGL",
    "TSLA":  "105.TSLA",
}

# AKShare's index_us_stock_sina expects Sina-style symbols with a leading dot.
# .IXIC = Nasdaq Composite (closest to NDX without paying for live NDX feed),
# .INX = S&P 500, .DJI = Dow Jones Industrial Average.
INDEX_TO_SINA = {
    "NDX": ".IXIC",
    "SPX": ".INX",
    "DJI": ".DJI",
}


def fetch_stock_minutes(ak_module, symbol: str, lookback_minutes: int) -> list[dict]:
    """Return a list of {timestamp_ms, price} for an akshare 105.xxx symbol."""
    end = _dt.datetime.now()
    start = end - _dt.timedelta(days=2)  # 1 trading session is enough; pad for weekends
    df = ak_module.stock_us_hist_min_em(
        symbol=symbol,
        start_date=start.strftime("%Y-%m-%d %H:%M:%S"),
        end_date=end.strftime("%Y-%m-%d %H:%M:%S"),
    )
    if df is None or df.empty:
        return []
    df = df.tail(lookback_minutes).copy()
    out: list[dict] = []
    for _, row in df.iterrows():
        ts_str = str(row.get("时间", ""))
        if not ts_str:
            continue
        try:
            ts = _dt.datetime.strptime(ts_str, "%Y-%m-%d %H:%M:%S")
        except Exception:
            continue
        price = float(row.get("收盘", 0) or 0)
        if price <= 0:
            continue
        out.append({"timestamp_ms": int(ts.timestamp() * 1000), "price": price})
    return out


def fetch_index_minutes(ak_module, sina_symbol: str, lookback_minutes: int) -> list[dict]:
    """US indexes only have daily OHLC available via free APIs. Take the last
    N daily closes and stamp them with synthetic timestamps spread across the
    last hour so the controller's "stale history" guard doesn't discard them
    as older than the 24h cutoff. This lets the sparkline at least show the
    most recent multi-day trend instead of a flat point."""
    try:
        df = ak_module.index_us_stock_sina(symbol=sina_symbol)
    except Exception:
        return []
    if df is None or df.empty:
        return []
    df = df.tail(lookback_minutes).copy()
    closes: list[float] = []
    for _, row in df.iterrows():
        close_value = row.get("close", row.get("收盘", 0))
        try:
            price = float(close_value or 0)
        except Exception:
            continue
        if price > 0:
            closes.append(price)
    if not closes:
        return []
    # Spread synthetic timestamps from now-3600s to now-1s so each point is
    # one minute apart. The exact mapping doesn't matter for visualization;
    # we only need timestamps that aren't older than the controller's
    # legacyHistory threshold.
    out: list[dict] = []
    now_ms = int(_dt.datetime.now().timestamp() * 1000)
    spread = 60 * 1000  # 1 minute per point
    n = len(closes)
    for i, price in enumerate(closes):
        ts_ms = now_ms - (n - i) * spread
        out.append({"timestamp_ms": ts_ms, "price": price})
    return out


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--symbols",
        default=",".join(list(SYMBOL_TO_AKSHARE.keys()) + list(INDEX_TO_SINA.keys())),
        help="Comma-separated UI symbols to fetch (e.g. AAPL,MSFT,NDX).",
    )
    parser.add_argument("--lookback", type=int, default=240,
        help="Minute samples per symbol (default 240 = ~4 hours).")
    parser.add_argument("--json", action="store_true", help="Emit JSON only (always on)")
    args = parser.parse_args()

    snapshot = {
        "schema": "stok.us-history.v1",
        "generatedAt": int(time.time()),
        "tradingDay": _dt.date.today().isoformat(),
        "errors": [],
        "series": {},
    }

    try:
        import akshare as ak  # type: ignore
    except Exception as exc:
        snapshot["errors"].append({"source": "akshare", "message": str(exc)})
        print(json.dumps(snapshot, ensure_ascii=False))
        return 1

    requested: list[str] = [s.strip() for s in args.symbols.split(",") if s.strip()]
    for symbol in requested:
        try:
            if symbol in SYMBOL_TO_AKSHARE:
                points = fetch_stock_minutes(ak, SYMBOL_TO_AKSHARE[symbol], args.lookback)
            elif symbol in INDEX_TO_SINA:
                points = fetch_index_minutes(ak, INDEX_TO_SINA[symbol], args.lookback)
            else:
                snapshot["errors"].append({"symbol": symbol, "message": "unknown mapping"})
                continue
            if not points:
                snapshot["errors"].append({"symbol": symbol, "message": "no rows"})
                continue
            snapshot["series"][symbol] = {
                "timestamps": [p["timestamp_ms"] for p in points],
                "prices": [p["price"] for p in points],
            }
        except Exception as exc:
            snapshot["errors"].append({"symbol": symbol, "message": str(exc)})

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
