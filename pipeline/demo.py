#!/usr/bin/env python3
"""
demo.py — Damodaran Valuation (any ticker)
==========================================
Usage:
  python3 demo.py WEGE3
  python3 demo.py ABEV3 --inputs /other/path/inputs

Prerequisite:
  BRAPI_TOKEN=... python3 brapi_fetch.py WEGE3
"""

import pathlib
import sys
import argparse

import ratvalue as rv
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
from prepare_inputs import load

ap = argparse.ArgumentParser()
ap.add_argument("ticker")
ap.add_argument("--inputs", default=str(pathlib.Path(__file__).parent / "inputs"))
args = ap.parse_args()

TICKER  = args.ticker.upper()
INPUTS  = pathlib.Path(args.inputs)
REPORTS = pathlib.Path(__file__).parent / "reports"
REPORTS.mkdir(exist_ok=True)
OUT_PNG = REPORTS / f"{TICKER}_valuation.png"

p = load(INPUTS, TICKER)

# ─────────────────────────────────────────────────────────────────────
print("=" * 62)
print(f"  VALUATION — {TICKER}")
print(f"  Source: brapi.dev  |  CDI-based model")
print("=" * 62)

print("\n── Cost of Capital (CDI-based) ──────────────────────────────")
print(f"  CDI:   {p.cdi:.2%}   Premium: {p.risk_premium:.2%}   Spread: {p.spread:.2%}")
print(f"  Ku:    {p.ku:.2%}   Ke: {p.ke:.2%}   Kd: {p.kd:.2%}")
print(f"  WACC:  {p.wacc:.2%}  (E/V={p.ev:.1%}, D/V={p.dv:.1%})")

print("\n── FCFF ─────────────────────────────────────────────────────")
fcff0 = rv.compute_fcff(
    ebit_b      = p.ebit,
    tax_rate    = p.tax_rate,
    da_b        = p.da,
    capex_b     = p.capex,
    delta_nwc_b = p.delta_nwc,
)
print(f"  EBIT × (1-t)  R${p.ebit*(1-p.tax_rate):.2f}B")
print(f"  + D&A         R${p.da:.2f}B")
print(f"  − CapEx       R${p.capex:.2f}B")
print(f"  − Δ NWC       R${p.delta_nwc:.2f}B")
print(f"  {'─'*36}")
print(f"  Base FCFF     R${fcff0:.2f}B")

fcffs = rv.project_fcff(
    fcff0,
    stages = [(p.growth_high, 3), (p.growth_mid, 3), (p.growth_low, 2)],
)
print(f"\n  Projection ({p.growth_high:.0%}×3y | {p.growth_mid:.0%}×3y | {p.growth_low:.0%}×2y):")
for i, f in enumerate(fcffs, 1):
    print(f"    Year {i:02d}: R${f:.2f}B")

if p.history.years:
    h = p.history
    n = len(h.years)
    print(f"\n── History ({h.years[-1][:4]}–{h.years[0][:4]}, {n} years) ──────────────────────")
    print(f"  {'Period':<13} {'Revenue':>9} {'EBIT':>9} {'FCFF':>9} {'Net Inc':>9}")
    print(f"  {'─'*52}")
    for i, yr in enumerate(h.years):
        print(f"  {yr:<13} {h.revenue[i]:>8.2f}B {h.ebit[i]:>8.2f}B "
              f"{h.fcff[i]:>8.2f}B {h.net_income[i]:>8.2f}B")
    print(f"  {'─'*52}")
    if h.revenue_cagr: print(f"  CAGR Revenue: {h.revenue_cagr:.1%}  |  "
                             f"EBIT: {h.ebit_cagr:.1%}  |  "
                             f"FCFF: {h.fcff_cagr:.1%}  ({n-1} years)")
    print(f"  Assumed projection:  phase 1 {p.growth_high:.0%}  "
          f"phase 2 {p.growth_mid:.0%}  phase 3 {p.growth_low:.0%}")

print("\n── DCF ──────────────────────────────────────────────────────")
dcf = rv.compute_dcf(
    projected_fcff_billions = fcffs,
    wacc                    = p.wacc,
    terminal_growth         = p.growth_terminal,
    net_debt_billions       = p.net_debt,
    shares                  = p.shares,
)
pct_tv = dcf.pv_terminal_value / dcf.enterprise_value
upside = dcf.price_per_share / p.price - 1

print(f"  PV FCFFs:    R${dcf.pv_fcff:.1f}B  ({1-pct_tv:.0%})")
print(f"  PV Terminal: R${dcf.pv_terminal_value:.1f}B  ({pct_tv:.0%})")
print(f"  {'─'*36}")
print(f"  EV:          R${dcf.enterprise_value:.1f}B")
print(f"  Net debt:    R${p.net_debt:.1f}B")
print(f"  Equity:      R${dcf.equity_value:.1f}B")
print(f"  Shares:      {p.shares/1e9:.3f} billion")
print(f"\n  ► FAIR VALUE:    R${dcf.price_per_share:.2f}")
print(f"  ► MARKET PRICE:  R${p.price:.2f}")
print(f"  ► UPSIDE:        {upside:+.1%}")
print("=" * 62)

# ─────────────────────────────────────────────────────────────────────
# CHART
# ─────────────────────────────────────────────────────────────────────
years  = list(range(1, len(fcffs) + 1))
pvs    = [f * (1 + p.wacc) ** (-t) for f, t in zip(fcffs, years)]

fig, (ax1, ax2, ax3) = plt.subplots(1, 3, figsize=(14, 5))
fig.suptitle(f"{TICKER} — Damodaran Valuation\n"
             "Source: brapi.dev  |  CDI-based model",
             fontsize=13, fontweight="bold", y=1.02)

# Panel 1 — Cost of capital
components = [
    ("CDI × E/V",     p.ev * p.cdi),
    ("Premium × E/V", p.ev * (p.ke - p.cdi)),
    ("Kd(1-t) × D/V", p.dv * p.kd * (1 - p.tax_rate)),
]
colors_wacc = ["#1565C0", "#42A5F5", "#90CAF9"]
base = 0.0
for (name, val), color in zip(components, colors_wacc):
    ax1.bar(0, val, bottom=base, color=color, width=0.5,
            label=f"{name}  {val:.2%}", edgecolor="white")
    ax1.text(0.28, base + val / 2, f"{val:.2%}", va="center", fontsize=9)
    base += val
ax1.axhline(p.wacc, color="orange", lw=2, ls="--", label=f"WACC = {p.wacc:.2%}")
ax1.set_xlim(-0.5, 0.9)
ax1.set_ylim(0, p.wacc * 1.3)
ax1.set_xticks([])
ax1.yaxis.set_major_formatter(plt.FuncFormatter(lambda x, _: f"{x:.1%}"))
ax1.set_title("Cost of Capital", fontweight="bold")
ax1.legend(fontsize=7.5, loc="upper right")
ax1.spines[["top", "right", "bottom"]].set_visible(False)
ax1.grid(axis="y", alpha=0.4)

# Panel 2 — FCFFs
colors_phase = ["#1565C0"] * 3 + ["#1976D2"] * 3 + ["#42A5F5"] * 2
ax2.bar(years, fcffs, color=colors_phase, width=0.6, alpha=0.85, zorder=3)
ax2b = ax2.twinx()
ax2b.plot(years, pvs, color="orange", marker="o", lw=2, ms=6, zorder=4)
ax2.set_xlabel("Year")
ax2.set_ylabel("FCFF (R$ B)", color="#1565C0")
ax2b.set_ylabel("PV (R$ B)", color="orange")
ax2.set_title("Projected FCFF and Present Value", fontweight="bold")
ax2.set_xticks(years)
ax2.grid(axis="y", alpha=0.4, zorder=0)
ax2.spines[["top"]].set_visible(False)
ax2.legend(handles=[
    mpatches.Patch(color="#1565C0", alpha=0.85,
                   label=f"FCFF ({p.growth_high:.0%}/{p.growth_mid:.0%}/{p.growth_low:.0%})"),
    mpatches.Patch(color="orange", label="Present value"),
], fontsize=7.5)

# Panel 3 — Price
color_dcf = "#1B5E20" if upside >= 0 else "#b71c1c"
bars = ax3.bar(["DCF\n(ratvalue)", "Market\nprice"],
               [dcf.price_per_share, p.price],
               color=[color_dcf, "#e65100"], width=0.4, zorder=3,
               edgecolor="white", linewidth=1.2)
for bar, pr in zip(bars, [dcf.price_per_share, p.price]):
    ax3.text(bar.get_x() + bar.get_width() / 2,
             pr + max(dcf.price_per_share, p.price) * 0.02,
             f"R${pr:.2f}", ha="center", fontsize=11,
             fontweight="bold", color=bar.get_facecolor())
ax3.set_ylabel("R$ per share")
ax3.set_title("Fair Value vs Market Price", fontweight="bold")
ax3.set_ylim(0, max(dcf.price_per_share, p.price) * 1.3)
ax3.grid(axis="y", alpha=0.4, zorder=0)
ax3.spines[["top", "right"]].set_visible(False)
ax3.axhline(p.price, color="#e65100", lw=1.2, ls="--", alpha=0.5)
ax3.annotate(f"{upside:+.1%}", fontsize=11, fontweight="bold", color=color_dcf,
             xy=(0, dcf.price_per_share),
             xytext=(0.5, (dcf.price_per_share + p.price) / 2),
             ha="center",
             arrowprops=dict(arrowstyle="->", color="gray", lw=0.8))

plt.tight_layout()
plt.savefig(OUT_PNG, dpi=150, bbox_inches="tight")
print(f"\nChart saved to {OUT_PNG}")
