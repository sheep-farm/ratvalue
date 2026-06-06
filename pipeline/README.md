# pipeline/

Damodaran multi-stage DCF valuation for B3 stocks, powered by **brapi.dev** and **ratvalue**.

## What it does

1. Fetches financial statements (income, balance sheet, cash flow) from brapi.dev for any B3 ticker.
2. Derives FCFF, WACC, and growth parameters from the raw data.
3. Runs the full Damodaran DCF model via the `ratvalue` C++ library (Python bindings).
4. Produces a terminal report and a 5-page PDF with charts, sensitivity analysis, and financial data.

## Quick start

```bash
./valuation.sh --ticker WEGE3 --token YOUR_BRAPI_TOKEN
```

This runs all four steps sequentially and produces:
- `inputs/WEGE3_*.csv` — raw financial data
- `reports/WEGE3_valuation.png` — quick chart
- `reports/WEGE3_report.pdf` — full 5-page report

## Scripts

| Script | Purpose |
|---|---|
| `brapi_fetch.py` | Download data from brapi.dev; write CSVs |
| `prepare_inputs.py` | Read CSVs; derive all model parameters |
| `demo.py` | Run DCF; print summary; save chart |
| `report.py` | Generate full PDF report (5 pages) |
| `bulk.py` | Mass valuation of all B3 tickers; output `summary.csv` |
| `valuation.sh` | Orchestrate steps 1–4 in sequence |

## File structure

```
inputs/
  <TICKER>_income.csv    — income statement (multi-year, field × year)
  <TICKER>_balance.csv   — balance sheet   (multi-year, field × year)
  <TICKER>_market.csv    — market data     (price, shares, DPS)
  <TICKER>_macro.csv     — macro inputs    (CDI, tax rate, growth rates)
  <TICKER>_raw.json      — raw API response (preserved for auditing)
  summary.csv            — bulk run results sorted by upside

reports/
  <TICKER>_valuation.png — quick chart (demo.py)
  <TICKER>_report.pdf    — full PDF report (report.py)
```

## CSV format

**Multi-year CSVs** (`income.csv`, `balance.csv`):
```
field,2024-12-31,2023-12-31,2022-12-31
net_revenue,50000000000,45000000000,38000000000
cogs,-30000000000,-28000000000,-22000000000
...
```

**Single-value CSVs** (`market.csv`, `macro.csv`):
```
field,value,source
price,38.50,brapi regularMarketPrice
cdi,0.1450,BCB Jun/2026
tax_rate,0.34,CIT + CSLL
...
```

## Growth stages (macro.csv)

Edit `<TICKER>_macro.csv` (or `macro_default.csv` for bulk) to tune growth assumptions:

| Field | Description |
|---|---|
| `growth_high` | Phase 1 growth rate (years 1–3) |
| `growth_mid` | Phase 2 growth rate (years 4–6) |
| `growth_low` | Phase 3 growth rate (years 7–8) |
| `growth_terminal` | Perpetual growth rate (Gordon TV) |
| `risk_premium` | Equity risk premium added to CDI |
| `bank_spread` | Credit spread added to CDI for Kd |

## Dependencies

- **ratvalue** — installed as Python package (`pip install .` from repo root)
- **matplotlib** — charts and PDF generation
- **numpy** — sensitivity grid computation
- Python standard library only for data fetching (no requests)

## Notes

- All monetary values are in **Brazilian Reais (R$)**.
- The CDI-based WACC model: `Ku = CDI + risk_premium × E/V`, `Ke` derived via Modigliani-Miller.
- For educational purposes only — not investment advice.
