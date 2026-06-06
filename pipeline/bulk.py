#!/usr/bin/env python3
"""
bulk.py — Mass valuation of all B3 tickers
===========================================
Usage:
  ./bulk.py --token TOKEN
  ./bulk.py --token TOKEN --workers 10
  ./bulk.py --token TOKEN --limit 100      # test with a subset
  ./bulk.py --token TOKEN --skip-fetch     # valuation only (already downloaded)
  ./bulk.py --token TOKEN --refetch        # force re-download

Output:
  inputs/summary.csv       — all tickers sorted by upside
  inputs/macro_default.csv — created automatically; edit to change defaults
"""

import os, sys, csv, json, pathlib, argparse, time, shutil, threading
import urllib.request, urllib.parse
from concurrent.futures import ThreadPoolExecutor, as_completed

import ratvalue as rv

sys.path.insert(0, str(pathlib.Path(__file__).parent))
from prepare_inputs import load

ap = argparse.ArgumentParser()
ap.add_argument("--token",      default=os.getenv("BRAPI_TOKEN", ""))
ap.add_argument("--out",        default=str(pathlib.Path(__file__).parent / "inputs"))
ap.add_argument("--workers",    type=int, default=8)
ap.add_argument("--limit",      type=int, default=0,  help="0 = no limit")
ap.add_argument("--skip-fetch", action="store_true",  help="skip download, use cached CSVs")
ap.add_argument("--refetch",    action="store_true",  help="force re-download even with cache")
args = ap.parse_args()

if not args.token:
    sys.exit("--token required or set BRAPI_TOKEN")

OUT   = pathlib.Path(args.out)
OUT.mkdir(parents=True, exist_ok=True)
TOKEN = args.token

# ── helpers (same as brapi_fetch.py) ─────────────────────────────────
def g(obj, *keys, default=0.0):
    for k in keys:
        if not isinstance(obj, dict):
            return default
        obj = obj.get(k)
        if obj is None:
            return default
    return float(obj) if obj is not None else default

def write_csv(path, header, rows):
    with open(path, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(header)
        w.writerows(rows)

def _bs_fields(b):
    return {
        "cash_equiv":             g(b, "cash"),
        "short_term_investments": g(b, "shortTermInvestments"),
        "accounts_receivable":    g(b, "netReceivables"),
        "inventories":            g(b, "inventory"),
        "other_current_assets":   g(b, "otherCurrentAssets"),
        "short_term_loans":       g(b, "loansAndFinancing"),
        "short_term_leasing":     g(b, "leaseFinancing"),
        "trade_payables":         g(b, "providers"),
        "tax_liabilities":        g(b, "taxObligations"),
        "other_current_liabilities": g(b, "socialAndLaborObligations") + g(b, "otherObligations"),
        "long_term_loans":        g(b, "longTermLoansAndFinancing"),
        "long_term_leasing":      g(b, "longTermLeaseFinancing"),
        "net_ppe":                g(b, "propertyPlantEquipment"),
    }

def _income_year(inc, cf):
    net_revenue = g(inc, "totalRevenue")
    cogs        = g(inc, "costOfRevenue")
    selling_exp = g(inc, "salesExpenses")
    ga_exp      = g(inc, "sellingGeneralAdministrative") - g(inc, "salesExpenses")
    ebit        = g(inc, "ebit")
    other_op    = ebit - (net_revenue + cogs + selling_exp + ga_exp)
    da          = g(cf, "incomeFromOperations") - g(inc, "netIncome")
    if da <= 0:
        da = abs(ebit) * 0.40
    capex = abs(g(cf, "operatingCashFlow") - g(cf, "freeCashFlow"))
    if capex <= 0:
        capex = abs(g(cf, "investmentCashFlow"))
    return {
        "net_revenue":      net_revenue,
        "cogs":             cogs,
        "selling_expenses": selling_exp,
        "ga_expenses":      ga_exp,
        "other_op_income":  other_op,
        "da":               da,
        "fin_expenses":     abs(g(inc, "financialExpenses")),
        "fin_income":       g(inc, "financialIncome"),
        "income_tax":       g(inc, "incomeTaxExpense"),
        "net_income":       g(inc, "netIncome"),
        "capex":            capex,
    }

MODULES = ",".join([
    "balanceSheetHistory",
    "incomeStatementHistory",
    "cashflowHistory",
    "defaultKeyStatistics",
])

# ── macro_default.csv ──────────────────────────────────────────────────
DEFAULT_MACRO = OUT / "macro_default.csv"
if not DEFAULT_MACRO.exists():
    write_csv(DEFAULT_MACRO,
        ["field", "value", "source"],
        [
            ["cdi",            0.1450, "BCB Jun/2026"],
            ["tax_rate",       0.34,   "CIT + CSLL"],
            ["risk_premium",   0.03,   "default"],
            ["bank_spread",    0.03,   "default"],
            ["growth_high",    0.08,   "phase 1 — conservative default"],
            ["growth_mid",     0.05,   "phase 2"],
            ["growth_low",     0.03,   "phase 3"],
            ["growth_terminal",0.04,   "terminal"],
        ])
    print(f"  macro_default.csv created at {DEFAULT_MACRO}")
    print("  Edit to change defaults applied to all tickers.\n")

# ── fetch a single ticker ─────────────────────────────────────────────
def fetch_ticker(ticker):
    """Download data and write CSVs. Returns (True, info) or (False, reason)."""
    pfx      = ticker + "_"
    raw_path = OUT / f"{pfx}raw.json"

    if raw_path.exists() and not args.refetch:
        raw_bytes = raw_path.read_bytes()
    else:
        params = {"modules": MODULES, "token": TOKEN}
        url    = f"https://brapi.dev/api/quote/{ticker}?" + urllib.parse.urlencode(params)
        try:
            with urllib.request.urlopen(url, timeout=20) as resp:
                raw_bytes = resp.read()
            raw_path.write_bytes(raw_bytes)
        except urllib.error.HTTPError as e:
            return False, f"HTTP {e.code}"
        except Exception as e:
            return False, str(e)[:60]

    try:
        data = json.loads(raw_bytes)
    except Exception:
        return False, "invalid JSON"

    if not data.get("results"):
        return False, "no results"

    r  = data["results"][0]
    ks = r.get("defaultKeyStatistics", {})

    bs_list  = r.get("balanceSheetHistory",    [])
    inc_list = r.get("incomeStatementHistory", [])
    cf_list  = r.get("cashflowHistory",        [])

    if not (bs_list and inc_list and cf_list):
        return False, "no financial statements"

    n_full = min(len(bs_list), len(inc_list), len(cf_list))
    n_bs   = len(bs_list)

    def dlabel(lst, i):
        return lst[i].get("endDate", f"year_{i}") if i < len(lst) else f"year_{i}"

    inc_dates = [dlabel(inc_list, i) for i in range(n_full)]
    bs_dates  = [dlabel(bs_list,  i) for i in range(n_bs)]
    bs_years  = [_bs_fields(bs_list[i])           for i in range(n_bs)]
    inc_years = [_income_year(inc_list[i], cf_list[i]) for i in range(n_full)]

    bs_keys  = list(bs_years[0].keys())
    inc_keys = list(inc_years[0].keys())

    write_csv(OUT / f"{pfx}income.csv",
        ["field"] + inc_dates,
        [[k] + [inc_years[i][k] for i in range(n_full)] for k in inc_keys])

    write_csv(OUT / f"{pfx}balance.csv",
        ["field"] + bs_dates,
        [[k] + [bs_years[i][k] for i in range(n_bs)] for k in bs_keys])

    price  = g(r,  "regularMarketPrice")
    shares = g(ks, "sharesOutstanding")
    dy_pct = g(ks, "dividendYield")

    write_csv(OUT / f"{pfx}market.csv",
        ["field", "value", "source"],
        [
            ["price",             price,          "brapi"],
            ["shares",            shares,         "brapi"],
            ["dividend_per_share",dy_pct * price, "brapi"],
        ])

    macro_path = OUT / f"{pfx}macro.csv"
    if not macro_path.exists():
        shutil.copy(DEFAULT_MACRO, macro_path)

    name = r.get("longName") or r.get("shortName") or ticker
    return True, {"n_years": n_full, "price": price, "name": name}

# ── valuation of a single ticker ─────────────────────────────────────
def valuation_ticker(ticker):
    """Run DCF. Returns dict with results."""
    base = {"ticker": ticker}
    try:
        p = load(OUT, ticker)

        if p.price <= 0 or p.shares <= 0:
            return {**base, "status": "no_price"}
        if p.ebit <= 0:
            return {**base, "status": "negative_ebit",
                    "price": p.price, "ebit_bn": round(p.ebit, 2)}
        if p.growth_terminal >= p.wacc:
            return {**base, "status": "g_ge_wacc",
                    "price": p.price, "wacc_pct": round(p.wacc*100, 2)}

        fcff0 = rv.compute_fcff(
            ebit_b=p.ebit, tax_rate=p.tax_rate,
            da_b=p.da, capex_b=p.capex, delta_nwc_b=p.delta_nwc)
        fcffs = rv.project_fcff(
            fcff0,
            stages=[(p.growth_high, 3), (p.growth_mid, 3), (p.growth_low, 2)])
        dcf = rv.compute_dcf(
            projected_fcff_billions=fcffs, wacc=p.wacc,
            terminal_growth=p.growth_terminal,
            net_debt_billions=p.net_debt, shares=p.shares)

        upside = dcf.price_per_share / p.price - 1
        rec    = "BUY" if upside >= 0.15 else ("HOLD" if upside >= -0.15 else "SELL")

        return {
            "ticker":        ticker,
            "status":        "ok",
            "price":         round(p.price, 2),
            "fair_value":    round(dcf.price_per_share, 2),
            "upside_pct":    round(upside * 100, 1),
            "recommendation":rec,
            "wacc_pct":      round(p.wacc * 100, 2),
            "ev_bn":         round(dcf.enterprise_value, 2),
            "equity_bn":     round(dcf.equity_value, 2),
            "fcff_bn":       round(p.fcff, 2),
            "ebit_bn":       round(p.ebit, 2),
            "revenue_bn":    round(p.history.revenue[0], 2) if p.history.revenue else 0,
            "dv_pct":        round(p.dv * 100, 1),
            "dy_pct":        round(p.dy * 100, 2),
            "n_years":       len(p.history.years),
            "fcff_cagr_pct": round(p.history.fcff_cagr * 100, 1) if p.history.fcff_cagr else "",
            "ebit_cagr_pct": round(p.history.ebit_cagr * 100, 1) if p.history.ebit_cagr else "",
            "revenue_cagr_pct": round(p.history.revenue_cagr * 100, 1) if p.history.revenue_cagr else "",
        }
    except Exception as e:
        return {**base, "status": f"error: {str(e)[:80]}"}

# ── ticker list ────────────────────────────────────────────────────────
print("Fetching B3 ticker list...")
with urllib.request.urlopen(
        f"https://brapi.dev/api/available?token={TOKEN}", timeout=15) as resp:
    available = json.loads(resp.read())
tickers = available.get("stocks", [])
if args.limit:
    tickers = tickers[:args.limit]
print(f"  {len(tickers)} tickers")

# ── parallel download ──────────────────────────────────────────────────
if not args.skip_fetch:
    print(f"\n── Download ({args.workers} workers) ──────────────────────────")
    lock   = threading.Lock()
    counts = {"ok": 0, "err": 0, "done": 0}

    def _fetch_and_report(ticker):
        ok, info = fetch_ticker(ticker)
        with lock:
            counts["done"] += 1
            if ok:
                counts["ok"] += 1
            else:
                counts["err"] += 1
            if counts["done"] % 100 == 0:
                print(f"  [{counts['done']:4d}/{len(tickers)}]  "
                      f"ok={counts['ok']}  err={counts['err']}")
        return ticker, ok

    with ThreadPoolExecutor(max_workers=args.workers) as ex:
        list(ex.map(_fetch_and_report, tickers))

    print(f"  Download complete: {counts['ok']} ok  |  {counts['err']} no data")

# ── sequential valuation ───────────────────────────────────────────────
print(f"\n── Valuation ────────────────────────────────────────────────")
results = []
ok_v = 0
for i, ticker in enumerate(tickers, 1):
    r = valuation_ticker(ticker)
    results.append(r)
    if r.get("status") == "ok":
        ok_v += 1
    if i % 200 == 0:
        print(f"  [{i:4d}/{len(tickers)}]  valuations ok: {ok_v}")

# ── summary.csv ────────────────────────────────────────────────────────
results_ok    = sorted(
    [r for r in results if r.get("status") == "ok"],
    key=lambda r: r.get("upside_pct", -9999), reverse=True)
results_other = [r for r in results if r.get("status") != "ok"]
all_results   = results_ok + results_other

SUMMARY = OUT / "summary.csv"
if all_results:
    fields = list(dict.fromkeys(k for r in all_results for k in r))
    with open(SUMMARY, "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=fields, extrasaction="ignore")
        w.writeheader()
        w.writerows(all_results)

# ── final summary ──────────────────────────────────────────────────────
buy_c  = sum(1 for r in results_ok if r.get("recommendation") == "BUY")
hold_c = sum(1 for r in results_ok if r.get("recommendation") == "HOLD")
sell_c = sum(1 for r in results_ok if r.get("recommendation") == "SELL")

print(f"\n{'═'*62}")
print(f"  Tickers processed  : {len(tickers)}")
print(f"  Valuation OK       : {ok_v}  "
      f"(BUY {buy_c} | HOLD {hold_c} | SELL {sell_c})")
print(f"  No data / error    : {len(results_other)}")
print(f"  Summary            : {SUMMARY}")
print(f"{'═'*62}")

if results_ok:
    print(f"\n  TOP 15 HIGHEST UPSIDES:")
    print(f"  {'Ticker':<8} {'Price':>8} {'Target':>8} {'Upside':>8} "
          f"{'Rec':>5} {'WACC':>7} {'EBIT':>8}")
    print(f"  {'-'*60}")
    for r in results_ok[:15]:
        print(f"  {r['ticker']:<8} R${r['price']:>6.2f}  R${r['fair_value']:>6.2f}  "
              f"{r['upside_pct']:>+6.1f}%  {r['recommendation']:>5}  "
              f"{r['wacc_pct']:>5.1f}%  R${r['ebit_bn']:>5.1f}B")

    print(f"\n  TOP 10 HIGHEST DOWNSIDES (SELL):")
    sell_sorted = sorted([r for r in results_ok if r.get("recommendation") == "SELL"],
                          key=lambda r: r.get("upside_pct", 0))
    print(f"  {'Ticker':<8} {'Price':>8} {'Target':>8} {'Upside':>8}")
    print(f"  {'-'*40}")
    for r in sell_sorted[:10]:
        print(f"  {r['ticker']:<8} R${r['price']:>6.2f}  R${r['fair_value']:>6.2f}  "
              f"{r['upside_pct']:>+6.1f}%")
