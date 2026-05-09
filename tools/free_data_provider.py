#!/usr/bin/env python3
"""Fetch free public market data and emit a normalized JSON snapshot.

The provider prefers no-key public sources that are already visible to retail
investors. If AKShare is installed, the script can be extended to enrich the
snapshot, but it never fabricates missing data.
"""

from __future__ import annotations

import argparse
import json
import sys
import time
import urllib.parse
import urllib.request


def request_json(url: str, *, method: str = "GET", data: str | None = None, headers: dict[str, str] | None = None, retries: int = 3) -> dict:
    body = data.encode("utf-8") if data is not None else None
    req = urllib.request.Request(url, data=body, method=method)
    req.add_header("User-Agent", "Mozilla/5.0 stok-free-data-provider")
    req.add_header("Accept", "application/json,text/plain,*/*")
    if headers:
        for key, value in headers.items():
            req.add_header(key, value)
    last_exc: Exception | None = None
    # Eastmoney push2 occasionally returns 502 / 503 / 504 mid-day. Retry a
    # couple of times with backoff so a single transient failure doesn't
    # blank out the UI.
    for attempt in range(max(1, retries)):
        try:
            with urllib.request.urlopen(req, timeout=12) as resp:
                return json.loads(resp.read().decode("utf-8", errors="replace"))
        except urllib.error.HTTPError as exc:
            last_exc = exc
            if exc.code not in (500, 502, 503, 504):
                raise
        except Exception as exc:
            last_exc = exc
        time.sleep(0.8 * (attempt + 1))
    if last_exc is not None:
        raise last_exc
    return {}


def eastmoney_sector_money_flow(limit: int) -> list[dict]:
    # Use plain HTTP: Eastmoney's HTTPS endpoint occasionally fails the TLS
    # handshake from Python's urllib on this host while curl/Invoke-WebRequest
    # succeed. The endpoint is read-only public data, so HTTP is acceptable.
    url = (
        "http://push2.eastmoney.com/api/qt/clist/get?"
        "fid=f62&po=1&pn=1&np=1&fltt=2&invt=2&fs=m:90+t:2&"
        f"pz={limit}&fields=f12,f14,f62,f184,f66,f72,f124"
    )
    payload = request_json(url)
    rows = payload.get("data", {}).get("diff", []) or []
    result: list[dict] = []
    for index, row in enumerate(rows, start=1):
        result.append(
            {
                "rank": index,
                "code": row.get("f12", ""),
                "name": row.get("f14", ""),
                "netMain": row.get("f62", 0),
                "netMainPct": row.get("f184", 0),
                "superFlow": row.get("f66", 0),
                "bigFlow": row.get("f72", 0),
                "updatedAt": row.get("f124", 0),
                "source": "东方财富行业资金流",
            }
        )
    return result


def cninfo_announcements(limit: int) -> list[dict]:
    url = "https://www.cninfo.com.cn/new/hisAnnouncement/query"
    form = {
        "pageNum": "1",
        "pageSize": str(limit),
        "column": "szse",
        "tabName": "fulltext",
        "plate": "",
        "stock": "",
        "searchkey": "",
        "secid": "",
        "category": "category_yjyg_szsh;category_ndbg_szsh;category_bndbg_szsh;category_sjdbg_szsh",
        "trade": "",
        "seDate": "",
        "sortName": "",
        "sortType": "",
        "isHLtitle": "true",
    }
    payload = request_json(
        url,
        method="POST",
        data=urllib.parse.urlencode(form),
        headers={
            "Content-Type": "application/x-www-form-urlencoded; charset=UTF-8",
            "Referer": "https://www.cninfo.com.cn/new/commonUrl/pageOfSearch",
        },
    )
    rows = payload.get("announcements", []) or []
    result: list[dict] = []
    for row in rows:
        title = (row.get("announcementTitle") or "").replace("<em>", "").replace("</em>", "")
        ts = row.get("announcementTime") or 0
        date = time.strftime("%Y-%m-%d", time.localtime(ts / 1000)) if ts else ""
        result.append(
            {
                "code": row.get("secCode", ""),
                "stock": row.get("secName", ""),
                "title": title,
                "date": date,
                "url": row.get("adjunctUrl", ""),
                "source": "巨潮资讯公开公告",
            }
        )
    return result


def akshare_status() -> dict:
    try:
        import akshare as ak  # type: ignore

        return {"available": True, "version": getattr(ak, "__version__", "unknown")}
    except Exception as exc:  # pragma: no cover - defensive runtime report
        return {"available": False, "error": str(exc)}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--json", action="store_true", help="Emit JSON only")
    parser.add_argument("--limit", type=int, default=20)
    args = parser.parse_args()

    snapshot = {
        "schema": "stok.free-data.snapshot.v1",
        "generatedAt": int(time.time()),
        "sources": [
            {"name": "东方财富行业资金流", "kind": "public-web", "authorized": True},
            {"name": "巨潮资讯公开公告", "kind": "official-disclosure", "authorized": True},
            {"name": "AKShare", "kind": "local-open-source-wrapper", **akshare_status()},
        ],
        "sectorMoneyFlow": [],
        "announcements": [],
        "errors": [],
    }

    for key, fn in (
        ("sectorMoneyFlow", eastmoney_sector_money_flow),
        ("announcements", cninfo_announcements),
    ):
        try:
            snapshot[key] = fn(args.limit)
        except Exception as exc:
            snapshot["errors"].append({"source": key, "message": str(exc)})

    print(json.dumps(snapshot, ensure_ascii=False, separators=(",", ":")))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
