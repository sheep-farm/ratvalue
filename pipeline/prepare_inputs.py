#!/usr/bin/env python3
"""
prepare_inputs.py
=================
Reads multi-year CSVs from inputs/ and derives all financial parameters.

CSV format:
  income.csv / balance.csv : field | 2024-12-31 | 2023-12-31 | ...  (multi-year)
  market.csv               : field | value | source
  macro.csv                : field | value | source

Usage (module):
  from prepare_inputs import load
  p = load(base, "WEGE3")

  p.ebit, p.fcff, p.wacc, ...          # most recent year
  p.history.years                       # ['2024-12-31', '2023-12-31', ...]
  p.history.ebit                        # [8.0B, 7.2B, ...]
  p.history.fcff                        # [7.0B, 5.8B, ...]
  p.history.revenue                     # [40.8B, 34.1B, ...]
  p.history.fcff_cagr                   # FCFF CAGR over the full series
  p.history.ebit_cagr                   # EBIT CAGR

Usage (diagnostic):
  python3 prepare_inputs.py TICKER [path/to/inputs]
"""

import csv
import pathlib
import sys
from types import SimpleNamespace


def load(base: pathlib.Path, ticker: str) -> SimpleNamespace:
    pfx = ticker.upper() + "_"

    # ── multi-year readers ───────────────────────────────────────────
    def _years(name):
        """Return list of year labels from the CSV."""
        with open(base / f"{pfx}{name}.csv", newline="", encoding="utf-8") as f:
            header = next(csv.reader(f))
        return [h for h in header if h != "field"]

    def _kv(name, year_idx=0):
        """Read a specific year from a multi-year CSV (0 = most recent)."""
        data = {}
        with open(base / f"{pfx}{name}.csv", newline="", encoding="utf-8") as f:
            reader = csv.DictReader(f)
            years = [k for k in reader.fieldnames if k != "field"]
            if not years:
                return data
            col = years[min(year_idx, len(years) - 1)]
            for row in reader:
                try:
                    data[row["field"].strip()] = float(row[col])
                except (ValueError, KeyError):
                    pass
        return data

    def _kv_single(name):
        """Read CSV with a 'value' column (market.csv, macro.csv)."""
        data = {}
        with open(base / f"{pfx}{name}.csv", newline="", encoding="utf-8") as f:
            for row in csv.DictReader(f):
                try:
                    data[row["field"].strip()] = float(row["value"])
                except (ValueError, KeyError):
                    pass
        return data

    def _bs_pair(year_idx=0):
        """Return (current, prior) for a pair of years in the balance sheet."""
        cur, pri = {}, {}
        with open(base / f"{pfx}balance.csv", newline="", encoding="utf-8") as f:
            reader = csv.DictReader(f)
            years = [k for k in reader.fieldnames if k != "field"]
            if not years:
                return cur, pri
            col_cur = years[min(year_idx,     len(years) - 1)]
            col_pri = years[min(year_idx + 1, len(years) - 1)]
            for row in reader:
                try:
                    cur[row["field"].strip()] = float(row[col_cur])
                    pri[row["field"].strip()] = float(row[col_pri])
                except (ValueError, KeyError):
                    pass
        return cur, pri

    def _all_years_kv(name):
        """Return {field: [v_year0, v_year1, ...]} for all years."""
        result = {}
        with open(base / f"{pfx}{name}.csv", newline="", encoding="utf-8") as f:
            reader = csv.DictReader(f)
            years = [k for k in reader.fieldnames if k != "field"]
            for row in reader:
                field = row["field"].strip()
                vals  = []
                for y in years:
                    try:
                        vals.append(float(row[y]))
                    except (ValueError, KeyError):
                        vals.append(None)
                result[field] = vals
        return result

    # ── base read ────────────────────────────────────────────────────
    _macro   = _kv_single("macro")
    _inc     = _kv("income", year_idx=0)
    _mkt     = _kv_single("market")
    _bs_cur, _bs_pri = _bs_pair(year_idx=0)

    # ── macro ────────────────────────────────────────────────────────
    _cdi       = _macro["cdi"]
    _tax_rate  = _macro["tax_rate"]
    _risk_prem = _macro["risk_premium"]
    _spread    = _macro["bank_spread"]

    # ── derivation: income statement (most recent year) ──────────────
    _net_revenue = _inc["net_revenue"]
    _ebit        = (_net_revenue + _inc["cogs"] + _inc["selling_expenses"]
                    + _inc["ga_expenses"] + _inc["other_op_income"])
    _da          = _inc["da"]
    _fin_expenses = _inc["fin_expenses"]
    _nopat       = _ebit * (1 - _tax_rate)
    _capex       = _inc["capex"]

    # ── derivation: balance sheet (most recent year) ─────────────────
    _cash    = _bs_cur["cash_equiv"] + _bs_cur["short_term_investments"]
    _fin_debt = (_bs_cur["short_term_loans"] + _bs_cur["short_term_leasing"]
                 + _bs_cur["long_term_loans"] + _bs_cur["long_term_leasing"])
    _net_debt_val = _fin_debt - _cash

    _nwc_cur = ((_bs_cur["accounts_receivable"] + _bs_cur["inventories"] + _bs_cur["other_current_assets"])
                - (_bs_cur["trade_payables"] + _bs_cur["tax_liabilities"] + _bs_cur["other_current_liabilities"]))
    _nwc_pri = ((_bs_pri["accounts_receivable"] + _bs_pri["inventories"] + _bs_pri["other_current_assets"])
                - (_bs_pri["trade_payables"] + _bs_pri["tax_liabilities"] + _bs_pri["other_current_liabilities"]))
    _delta_nwc = _nwc_cur - _nwc_pri

    _fcff = _nopat + _da - _capex - _delta_nwc

    # ── derivation: cost of capital ──────────────────────────────────
    _price    = _mkt["price"]
    _shares   = _mkt["shares"]
    _div_per_share = _mkt["dividend_per_share"]
    _mktcap   = _price * _shares
    _dv       = max(0.0, _fin_debt / (_fin_debt + _mktcap)) if (_fin_debt + _mktcap) > 0 else 0.0
    _ev       = 1.0 - _dv
    _kd       = _cdi + _risk_prem + _spread
    _ku       = _cdi + _risk_prem * _ev
    _ke       = _ku + (_ku - _kd * (1 - _tax_rate)) * (_dv / _ev) if _ev > 0 else _ku
    _wacc     = _ev * _ke + _dv * _kd * (1 - _tax_rate)
    _dy       = _div_per_share / _price if _price else 0.0

    # ── full history ─────────────────────────────────────────────────
    try:
        inc_years   = _years("income")
        bs_years    = _years("balance")
        inc_all     = _all_years_kv("income")
        n_hist      = len(inc_years)

        def _safe(lst, i):
            return lst[i] if lst[i] is not None else 0.0

        bs_all = _all_years_kv("balance")

        B = 1e9
        hist_ebit, hist_fcff, hist_revenue, hist_net_income = [], [], [], []

        for i in range(n_hist):
            inc_i = {k: _safe(v, i) for k, v in inc_all.items()}
            ebit_i = (inc_i.get("net_revenue", 0) + inc_i.get("cogs", 0) +
                      inc_i.get("selling_expenses", 0) + inc_i.get("ga_expenses", 0) +
                      inc_i.get("other_op_income", 0))
            nopat_i = ebit_i * (1 - _tax_rate)
            da_i    = inc_i.get("da", 0)
            capex_i = inc_i.get("capex", 0)

            # NWC delta: balance year i vs balance year i+1
            if i + 1 < len(bs_years):
                nwc_cur  = sum(_safe(bs_all.get(c, []), i) for c in
                               ["accounts_receivable", "inventories", "other_current_assets"]) - \
                           sum(_safe(bs_all.get(c, []), i) for c in
                               ["trade_payables", "tax_liabilities", "other_current_liabilities"])
                nwc_prev = sum(_safe(bs_all.get(c, []), i+1) for c in
                               ["accounts_receivable", "inventories", "other_current_assets"]) - \
                           sum(_safe(bs_all.get(c, []), i+1) for c in
                               ["trade_payables", "tax_liabilities", "other_current_liabilities"])
                delta_nwc_i = nwc_cur - nwc_prev
            else:
                delta_nwc_i = 0.0

            fcff_i = nopat_i + da_i - capex_i - delta_nwc_i

            hist_ebit.append(ebit_i / B)
            hist_fcff.append(fcff_i / B)
            hist_revenue.append(inc_i.get("net_revenue", 0) / B)
            hist_net_income.append(inc_i.get("net_income", 0) / B)

        def _cagr(vals):
            """CAGR of series (most recent = index 0)."""
            clean = [v for v in vals if v and v > 0]
            if len(clean) < 2:
                return None
            n = len(clean) - 1
            return (clean[0] / clean[-1]) ** (1 / n) - 1

        history = SimpleNamespace(
            years        = inc_years,
            ebit         = hist_ebit,
            fcff         = hist_fcff,
            revenue      = hist_revenue,
            net_income   = hist_net_income,
            ebit_cagr    = _cagr(hist_ebit),
            fcff_cagr    = _cagr(hist_fcff),
            revenue_cagr = _cagr(hist_revenue),
        )
    except Exception:
        history = SimpleNamespace(
            years=[], ebit=[], fcff=[], revenue=[], net_income=[],
            ebit_cagr=None, fcff_cagr=None, revenue_cagr=None,
        )

    B = 1e9
    return SimpleNamespace(
        ticker           = ticker.upper(),
        cdi              = _cdi,
        tax_rate         = _tax_rate,
        risk_premium     = _risk_prem,
        spread           = _spread,
        ku               = _ku,
        kd               = _kd,
        ke               = _ke,
        wacc             = _wacc,
        dv               = _dv,
        ev               = _ev,
        shares           = int(_shares),
        price            = _price,
        dividend_per_share = _div_per_share,
        dy               = _dy,
        ebit             = _ebit / B,
        da               = _da   / B,
        capex            = _capex / B,
        delta_nwc        = _delta_nwc / B,
        fcff             = _fcff / B,
        net_debt         = _net_debt_val / B,
        growth_high      = _macro["growth_high"],
        growth_mid       = _macro["growth_mid"],
        growth_low       = _macro["growth_low"],
        growth_terminal  = _macro["growth_terminal"],
        history          = history,
        # internal
        _fin_expenses    = _fin_expenses,
        _ebit_raw        = _ebit,
    )


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit("Usage: python3 prepare_inputs.py TICKER [path/to/inputs]")
    ticker = sys.argv[1].upper()
    base   = pathlib.Path(sys.argv[2]) if len(sys.argv) > 2 else pathlib.Path(__file__).parent / "inputs"
    p = load(base, ticker)
    icr = p._ebit_raw / p._fin_expenses if p._fin_expenses else float("inf")

    print(f"inputs: {base}  ticker: {p.ticker}\n")
    print(f"  EBIT       R${p.ebit:.2f}B     FCFF  R${p.fcff:.2f}B")
    print(f"  WACC       {p.wacc:.2%}       D/V   {p.dv:.2%}")
    print(f"  Net debt   R${p.net_debt:.2f}B    DY    {p.dy:.2%}")
    print(f"  ICR        {'∞' if icr == float('inf') else f'{icr:.1f}×'}")

    if p.history.years:
        print(f"\n  History ({len(p.history.years)} years):")
        print(f"  {'Period':<14} {'Revenue':>10} {'EBIT':>10} {'FCFF':>10} {'Net Inc':>10}")
        print(f"  {'-'*54}")
        for i, yr in enumerate(p.history.years):
            print(f"  {yr:<14} "
                  f"{p.history.revenue[i]:>9.2f}B "
                  f"{p.history.ebit[i]:>9.2f}B "
                  f"{p.history.fcff[i]:>9.2f}B "
                  f"{p.history.net_income[i]:>9.2f}B")
        print(f"\n  CAGR revenue: {p.history.revenue_cagr:.1%}" if p.history.revenue_cagr else "")
        print(f"  CAGR EBIT:    {p.history.ebit_cagr:.1%}" if p.history.ebit_cagr else "")
        print(f"  CAGR FCFF:    {p.history.fcff_cagr:.1%}" if p.history.fcff_cagr else "")
