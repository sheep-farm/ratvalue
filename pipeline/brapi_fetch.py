#!/usr/bin/env python3
"""
brapi_fetch.py
==============
Fetches financial data from brapi.dev and writes/updates CSVs in inputs/.
Naming: inputs/<TICKER>_income.csv, inputs/<TICKER>_balance.csv, etc.

Multi-year CSVs: one column per fiscal year (e.g. 2024-12-31, 2023-12-31 ...).
The raw JSON is preserved in inputs/<TICKER>_raw.json.

Usage:
  BRAPI_TOKEN=xxx python3 brapi_fetch.py WEGE3
  python3 brapi_fetch.py WEGE3 --token TOKEN
  python3 brapi_fetch.py WEGE3 --period quarterly
"""

import os, sys, csv, json, pathlib, argparse, urllib.request, urllib.parse

ap = argparse.ArgumentParser()
ap.add_argument("ticker")
ap.add_argument("--token",  default=os.getenv("BRAPI_TOKEN", ""))
ap.add_argument("--out",    default=str(pathlib.Path(__file__).parent / "inputs"))
ap.add_argument("--period", choices=["annual", "quarterly"], default="annual")
args = ap.parse_args()

TICKER = args.ticker.upper()
OUT    = pathlib.Path(args.out)
OUT.mkdir(parents=True, exist_ok=True)
PFX    = TICKER + "_"

# ── API fetch ─────────────────────────────────────────────────────────
MODULES = ",".join([
    "balanceSheetHistory",
    "balanceSheetHistoryQuarterly",
    "incomeStatementHistory",
    "incomeStatementHistoryQuarterly",
    "cashflowHistory",
    "cashflowHistoryQuarterly",
    "defaultKeyStatistics",
])
params = {"modules": MODULES}
if args.token:
    params["token"] = args.token

url = f"https://brapi.dev/api/quote/{TICKER}?" + urllib.parse.urlencode(params)
print(f"Fetching {TICKER} from brapi.dev...")

try:
    with urllib.request.urlopen(url, timeout=15) as resp:
        raw_bytes = resp.read()
except urllib.error.HTTPError as e:
    if e.code == 401:
        sys.exit("Invalid or missing token. Set BRAPI_TOKEN or use --token.")
    raise

(OUT / f"{PFX}raw.json").write_bytes(raw_bytes)

data = json.loads(raw_bytes)
if not data.get("results"):
    sys.exit(f"Ticker not found: {TICKER}")

r  = data["results"][0]
ks = r.get("defaultKeyStatistics", {})

# ── helpers ───────────────────────────────────────────────────────────
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

# ── annual / quarterly series ─────────────────────────────────────────
sfx       = "Quarterly" if args.period == "quarterly" else ""
bs_list   = r.get(f"balanceSheetHistory{sfx}",    [])
inc_list  = r.get(f"incomeStatementHistory{sfx}", [])
cf_list   = r.get(f"cashflowHistory{sfx}",        [])

if not bs_list:
    sys.exit(f"No balance sheet data for {TICKER}. Check your token.")

# number of complete years (balance sheet, income, and CF all available)
n_full = min(len(bs_list), len(inc_list), len(cf_list))
# balance sheet uses one extra year for the first-year ΔNWC
n_bs   = len(bs_list)

print(f"  Available series: {n_full} years (income/CF) | {n_bs} years (balance)")

def date_label(lst, i):
    return lst[i].get("endDate", f"year_{i}") if i < len(lst) else f"year_{i}"

inc_dates = [date_label(inc_list, i) for i in range(n_full)]
bs_dates  = [date_label(bs_list,  i) for i in range(n_bs)]

# ── extract balance sheet fields for each year ────────────────────────
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

bs_years = [_bs_fields(bs_list[i]) for i in range(n_bs)]
bs_field_keys = list(bs_years[0].keys())

# ── extract income statement + cash flow for each year ───────────────
def _income_year(inc, cf):
    net_revenue   = g(inc, "totalRevenue")
    cogs          = g(inc, "costOfRevenue")
    selling_exp   = g(inc, "salesExpenses")
    ga_exp        = g(inc, "sellingGeneralAdministrative") - g(inc, "salesExpenses")
    ebit          = g(inc, "ebit")
    other_op      = ebit - (net_revenue + cogs + selling_exp + ga_exp)

    da = g(cf, "incomeFromOperations") - g(inc, "netIncome")
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

inc_years = [_income_year(inc_list[i], cf_list[i]) for i in range(n_full)]
inc_field_keys = list(inc_years[0].keys())

# ── write multi-year CSVs ─────────────────────────────────────────────
# income: field | 2024-12-31 | 2023-12-31 | ...
write_csv(OUT / f"{PFX}income.csv",
    ["field"] + inc_dates,
    [[k] + [inc_years[i][k] for i in range(n_full)] for k in inc_field_keys])

# balance: field | 2024-12-31 | 2023-12-31 | ...  (one extra year vs income)
write_csv(OUT / f"{PFX}balance.csv",
    ["field"] + bs_dates,
    [[k] + [bs_years[i][k] for i in range(n_bs)] for k in bs_field_keys])

# market (current point — no long historical series in this endpoint)
price    = g(r,  "regularMarketPrice")
shares   = g(ks, "sharesOutstanding")
dy_pct   = g(ks, "dividendYield")

write_csv(OUT / f"{PFX}market.csv",
    ["field", "value", "source"],
    [
        ["price",             price,          "brapi regularMarketPrice"],
        ["shares",            shares,         "brapi sharesOutstanding"],
        ["dividend_per_share",dy_pct * price, "brapi dividendYield × price"],
    ])

# ── macro: try to fetch CDI/SELIC and IPCA from brapi v2 ─────────────
def _fetch_macro():
    cdi = ipca = None
    token_param = f"&token={args.token}" if args.token else ""
    try:
        url_pr = f"https://brapi.dev/api/v2/prime-rate?sortBy=date&sortOrder=desc&limit=1{token_param}"
        with urllib.request.urlopen(url_pr, timeout=10) as resp:
            d = json.loads(resp.read())
        rates = d.get("prime-rate", [])
        if rates:
            cdi = float(rates[0]["value"]) / 100
    except Exception:
        pass
    try:
        url_inf = f"https://brapi.dev/api/v2/inflation?sortBy=date&sortOrder=desc&limit=1{token_param}"
        with urllib.request.urlopen(url_inf, timeout=10) as resp:
            d = json.loads(resp.read())
        inflations = d.get("inflation", [])
        if inflations:
            ipca = float(inflations[0]["value"]) / 100
    except Exception:
        pass
    return cdi, ipca

_cdi_api, _ipca_api = _fetch_macro()

macro_path = OUT / f"{PFX}macro.csv"
if not macro_path.exists():
    _cdi_val  = _cdi_api  if _cdi_api  is not None else 0.1148
    _ipca_val = _ipca_api if _ipca_api is not None else 0.05
    write_csv(macro_path,
        ["field", "value", "source"],
        [
            ["cdi",            _cdi_val,  "brapi prime-rate" if _cdi_api else "BCB — update manually"],
            ["tax_rate",       0.34,      "CIT + CSLL"],
            ["risk_premium",   0.03,      "analyst judgment"],
            ["bank_spread",    0.03,      "analyst judgment"],
            ["growth_high",    0.08,      "phase 1"],
            ["growth_mid",     0.05,      "phase 2"],
            ["growth_low",     0.03,      "phase 3"],
            ["growth_terminal",_ipca_val, "brapi inflation" if _ipca_api else "IPCA — update manually"],
        ])
    print(f"  {PFX}macro.csv created  (CDI={'brapi' if _cdi_api else 'manual'})")
elif _cdi_api is not None:
    rows = []
    with open(macro_path, newline="", encoding="utf-8") as f:
        rows = list(csv.reader(f))
    header = rows[0]
    updated = []
    for row in rows[1:]:
        if row[0] == "cdi":
            row = ["cdi", str(_cdi_api), "brapi prime-rate"]
        if row[0] == "growth_terminal" and _ipca_api is not None:
            row = ["growth_terminal", str(_ipca_api), "brapi inflation"]
        updated.append(row)
    with open(macro_path, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(header)
        w.writerows(updated)
    print(f"  CDI updated: {_cdi_api:.2%} (brapi prime-rate)")

# ── summary ───────────────────────────────────────────────────────────
inc0 = inc_years[0]
print(f"\n  Files written to {OUT}/")
print(f"  {TICKER}  |  {n_full} years: {inc_dates[0]}  →  {inc_dates[-1]}")
print(f"  Revenue: R${inc0['net_revenue']/1e9:.1f}B  "
      f"EBIT: R${(inc0['net_revenue']+inc0['cogs']+inc0['selling_expenses']+inc0['ga_expenses']+inc0['other_op_income'])/1e9:.1f}B  "
      f"Net income: R${inc0['net_income']/1e9:.1f}B")
print(f"  Price: R${price:.2f}  CapEx: R${inc0['capex']/1e9:.1f}B  "
      f"D&A (proxy): R${inc0['da']/1e9:.1f}B")
