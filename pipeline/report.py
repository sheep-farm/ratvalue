#!/usr/bin/env python3
"""
report.py — Full Damodaran Valuation Report (PDF)
==================================================
Usage: ./report.py TICKER [--inputs inputs/]
       ./report.py WEGE3
"""

import argparse, pathlib, sys, csv, json
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
import matplotlib.ticker as mticker
from matplotlib.backends.backend_pdf import PdfPages
from datetime import date

import ratvalue as rv

ap = argparse.ArgumentParser()
ap.add_argument("ticker")
ap.add_argument("--inputs", default=str(pathlib.Path(__file__).parent / "inputs"))
args = ap.parse_args()

TICKER = args.ticker.upper()
INPUTS = pathlib.Path(args.inputs)

sys.path.insert(0, str(pathlib.Path(__file__).parent))
from prepare_inputs import load

p = load(INPUTS, TICKER)

# ── company info from raw JSON ────────────────────────────────────────
COMPANY = TICKER
SECTOR  = ""
raw_path = INPUTS / f"{TICKER}_raw.json"
if raw_path.exists():
    try:
        r0 = json.loads(raw_path.read_bytes()).get("results", [{}])[0]
        COMPANY = r0.get("longName") or r0.get("shortName") or TICKER
        SECTOR  = r0.get("sector", "")
    except Exception:
        pass

REPORT_DATE = date.today().strftime("%Y-%m-%d")
REPORTS     = pathlib.Path(__file__).parent / "reports"
REPORTS.mkdir(exist_ok=True)
OUT_PDF     = REPORTS / f"{TICKER}_report.pdf"

# ── DCF ───────────────────────────────────────────────────────────────
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

years  = list(range(1, len(fcffs) + 1))
pvs    = [f * (1 + p.wacc) ** (-t) for f, t in zip(fcffs, years)]
pct_tv = dcf.pv_terminal_value / dcf.enterprise_value
upside = dcf.price_per_share / p.price - 1
icr    = p._ebit_raw / p._fin_expenses if p._fin_expenses else float("inf")

if upside >= 0.15:
    rec, rec_col = "BUY",  "#1d8348"
elif upside >= -0.15:
    rec, rec_col = "HOLD", "#b7770d"
else:
    rec, rec_col = "SELL", "#922b21"

# ── sensitivity grid: WACC × terminal growth ─────────────────────────
WACC_STEPS = [p.wacc + d for d in (-0.04, -0.03, -0.02, -0.01, 0, 0.01, 0.02, 0.03, 0.04)]
GTRM_STEPS = [p.growth_terminal + d for d in (-0.02, -0.01, 0, 0.01, 0.02)]
NW, NG = len(WACC_STEPS), len(GTRM_STEPS)

sens = np.full((NW, NG), np.nan)
for i, w in enumerate(WACC_STEPS):
    for j, g in enumerate(GTRM_STEPS):
        if g >= w or w <= 0:
            continue
        try:
            d = rv.compute_dcf(fcffs, wacc=w, terminal_growth=g,
                               net_debt_billions=p.net_debt, shares=p.shares)
            sens[i, j] = d.price_per_share
        except Exception:
            pass

# ── design ────────────────────────────────────────────────────────────
BG    = "#ffffff"
HDR   = "#0d2137"
ACC   = "#1a6faf"
GOLD  = "#c97d10"
LIGHT = "#f4f7fa"
FG    = "#1a1a1a"
MUTED = "#6b7280"

plt.rcParams.update({
    "figure.facecolor": BG, "axes.facecolor": BG,
    "axes.edgecolor": "#d0d7de", "axes.labelcolor": FG,
    "xtick.color": MUTED, "ytick.color": MUTED,
    "text.color": FG, "grid.color": "#e5e9ed",
    "grid.linewidth": 0.5, "font.family": "DejaVu Sans", "font.size": 9,
})

def band(fig, y0, h, color, zorder=0):
    fig.patches.append(mpatches.Rectangle(
        (0, y0), 1, h, transform=fig.transFigure,
        facecolor=color, edgecolor="none", zorder=zorder))

def hdr_footer(fig, title, page):
    band(fig, 0.958, 0.042, HDR, zorder=2)
    band(fig, 0.000, 0.030, HDR, zorder=2)
    fig.text(0.015, 0.979, f"{COMPANY}  ·  {TICKER}",
             fontsize=9, color="white", fontweight="bold", va="center", zorder=3)
    fig.text(0.500, 0.979, title,
             fontsize=10, color="white", fontweight="bold",
             va="center", ha="center", zorder=3)
    fig.text(0.985, 0.979, REPORT_DATE,
             fontsize=8, color="#aac8e0", va="center", ha="right", zorder=3)
    fig.text(0.015, 0.015,
             "ratvalue · MIT · For educational purposes only.",
             fontsize=7, color="#aac8e0", va="center", zorder=3)
    fig.text(0.985, 0.015, f"Page {page} of {TOTAL}",
             fontsize=7, color="#aac8e0", va="center", ha="right", zorder=3)

def kpi(fig, x, y, w, h, label, value, color=ACC):
    fig.patches.append(mpatches.FancyBboxPatch(
        (x, y), w, h, boxstyle="round,pad=0.005",
        transform=fig.transFigure,
        facecolor=LIGHT, edgecolor=color, linewidth=1.2, zorder=3))
    fig.text(x + w/2, y + h*0.72, label,
             ha="center", va="center", fontsize=7.5,
             color=MUTED, transform=fig.transFigure, zorder=4)
    fig.text(x + w/2, y + h*0.28, value,
             ha="center", va="center", fontsize=12,
             color=color, fontweight="bold",
             transform=fig.transFigure, zorder=4)

def draw_table(ax, rows, col_w=None, highlight_last=True):
    nr = len(rows); nc = len(rows[0])
    cw = col_w or [1/nc] * nc
    xs = []
    x0 = 0.0
    for w in cw:
        xs.append(x0); x0 += w
    ax.set_xlim(0, 1); ax.set_ylim(0, 1); ax.axis("off")
    for r, row in enumerate(rows):
        is_hdr  = r == 0
        is_last = r == nr - 1 and highlight_last
        bg = HDR if is_hdr else ("#e8f4fd" if is_last else (LIGHT if r % 2 == 0 else BG))
        for c, cell in enumerate(row):
            ax.add_patch(plt.Rectangle(
                (xs[c], 1 - (r+1)/nr), cw[c], 1/nr,
                facecolor=bg, edgecolor="#d0d7de", linewidth=0.4, zorder=1))
            fc = "white" if is_hdr else FG
            fw = "bold"  if is_hdr or is_last or c == 0 else "normal"
            ax.text(xs[c] + cw[c]/2, 1 - (r + 0.5)/nr,
                    str(cell), ha="center", va="center",
                    fontsize=8.5, color=fc, fontweight=fw, zorder=2)

def read_csv_kv(tag):
    """Read a multi-year CSV and return the first year column as a dict."""
    vals = {}
    path = INPUTS / f"{TICKER}_{tag}.csv"
    if not path.exists():
        return vals
    with open(path, newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        year_cols = [k for k in reader.fieldnames if k != "field"]
        if not year_cols:
            return vals
        col = year_cols[0]  # most recent year
        for row in reader:
            try:
                vals[row["field"].strip()] = float(row[col])
            except (ValueError, KeyError):
                pass
    return vals

def read_csv_bs(tag):
    """Read a multi-year balance-sheet CSV; return (year0, year1) dicts."""
    cur, pri = {}, {}
    path = INPUTS / f"{TICKER}_{tag}.csv"
    if not path.exists():
        return cur, pri
    with open(path, newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        year_cols = [k for k in reader.fieldnames if k != "field"]
        if not year_cols:
            return cur, pri
        col_cur = year_cols[0]
        col_pri = year_cols[1] if len(year_cols) > 1 else year_cols[0]
        for row in reader:
            try:
                cur[row["field"].strip()] = float(row[col_cur])
                pri[row["field"].strip()] = float(row[col_pri])
            except (ValueError, KeyError):
                pass
    return cur, pri

# ════════════════════════════════════════════════════════════════════
TOTAL = 5
with PdfPages(OUT_PDF) as pdf:

    # ── PAGE 1: COVER ────────────────────────────────────────────────
    fig = plt.figure(figsize=(11, 8.5)); fig.patch.set_facecolor(BG)
    band(fig, 0.56, 0.44, HDR)
    band(fig, 0.00, 0.03, HDR)

    sector_line = f"{TICKER}  ·  {SECTOR}" if SECTOR else TICKER
    fig.text(0.06, 0.90, COMPANY, fontsize=32, color="white",
             fontweight="bold", va="center")
    fig.text(0.06, 0.81, sector_line, fontsize=13, color="#aac8e0", va="center")
    fig.text(0.06, 0.73,
             "Valuation Report  —  Damodaran Multi-Stage DCF",
             fontsize=11, color="#cce0f0", va="center")

    fig.patches.append(mpatches.FancyBboxPatch(
        (0.71, 0.60), 0.22, 0.30,
        boxstyle="round,pad=0.01", transform=fig.transFigure,
        facecolor=rec_col, edgecolor="none", zorder=2))
    fig.text(0.82, 0.80, rec,   ha="center", va="center",
             fontsize=32, color="white", fontweight="bold", zorder=3)
    fig.text(0.82, 0.70, f"Target  R${dcf.price_per_share:.2f}",
             ha="center", va="center", fontsize=11, color="white", zorder=3)
    fig.text(0.82, 0.63, f"Upside  {upside:+.1%}",
             ha="center", va="center", fontsize=9.5, color="white", zorder=3)

    kpis_data = [
        ("Market price",  f"R${p.price:.2f}",                FG),
        ("DCF target",    f"R${dcf.price_per_share:.2f}",    ACC),
        ("EV",            f"R${dcf.enterprise_value:.1f}B",   ACC),
        ("WACC",          f"{p.wacc:.2%}",                    MUTED),
        ("Equity",        f"R${dcf.equity_value:.1f}B",       ACC),
        ("DY",            f"{p.dy:.2%}",                      MUTED),
    ]
    for i, (lbl, val, col) in enumerate(kpis_data):
        kpi(fig, 0.04 + i*0.158, 0.43, 0.138, 0.11, lbl, val, col)

    fig.text(0.06, 0.375,
        f"Independent valuation of {COMPANY} ({TICKER}) using Damodaran's multi-stage DCF "
        f"framework, implemented in ratvalue (C++23). CDI-based model. "
        f"Monetary values in R$ billions. Data: brapi.dev Pro.",
        fontsize=9, color=MUTED, va="top", linespacing=1.8)
    fig.text(0.06, 0.27,
        f"CDI = {p.cdi:.2%}   ·   Ku = {p.ku:.2%}   ·   Ke = {p.ke:.2%}   ·   "
        f"Kd = {p.kd:.2%}   ·   WACC = {p.wacc:.2%}   ·   D/V = {p.dv:.1%}   ·   "
        f"ICR = {'∞' if np.isinf(icr) else f'{icr:.1f}×'}",
        fontsize=9, color=MUTED, va="top")

    fig.text(0.985, 0.015, "Page 1 of 5",
             fontsize=7, color="#aac8e0", va="center", ha="right")
    fig.text(0.015, 0.015, "ratvalue · MIT · For educational purposes only.",
             fontsize=7, color="#aac8e0", va="center")
    pdf.savefig(fig, bbox_inches="tight"); plt.close(fig)

    # ── PAGE 2: VALUATION SUMMARY ─────────────────────────────────────
    fig = plt.figure(figsize=(11, 8.5)); fig.patch.set_facecolor(BG)
    hdr_footer(fig, "Valuation Summary", 2)

    # WACC build-up
    ax_w = fig.add_axes([0.05, 0.53, 0.24, 0.38])
    comps_wacc = [
        ("CDI × E/V",     p.ev * p.cdi),
        ("Premium × E/V", p.ev * (p.ke - p.cdi)),
        ("Kd(1-t) × D/V", p.dv * p.kd * (1 - p.tax_rate)),
    ]
    cols_wacc = ["#1565C0", "#42A5F5", "#90CAF9"]
    base = 0.0
    for (name, val), color in zip(comps_wacc, cols_wacc):
        ax_w.bar(0, val, bottom=base, color=color, width=0.5,
                 label=f"{name}  {val:.2%}", edgecolor="white", zorder=3)
        ax_w.text(0.30, base + val/2, f"{val:.2%}", va="center", fontsize=8)
        base += val
    ax_w.axhline(p.wacc, color=GOLD, lw=2, ls="--", label=f"WACC = {p.wacc:.2%}")
    ax_w.set_xlim(-0.4, 0.8); ax_w.set_ylim(0, p.wacc * 1.35)
    ax_w.set_xticks([])
    ax_w.yaxis.set_major_formatter(mticker.PercentFormatter(1.0))
    ax_w.set_title("WACC Build-up (CDI-based)", fontsize=10, color=FG, pad=6)
    ax_w.legend(fontsize=7, facecolor=BG, edgecolor="#d0d7de", loc="upper right")
    ax_w.spines[["top", "right", "bottom"]].set_visible(False)
    ax_w.grid(axis="y", zorder=0)

    # Price comparison
    ax_pr = fig.add_axes([0.35, 0.53, 0.26, 0.38])
    bars_pr = ax_pr.bar(
        ["DCF target\n", "Market\nprice"],
        [dcf.price_per_share, p.price],
        color=[rec_col, "#e65100"], width=0.45,
        zorder=3, edgecolor="white", linewidth=1.2)
    for bar, pr in zip(bars_pr, [dcf.price_per_share, p.price]):
        ax_pr.text(bar.get_x() + bar.get_width()/2,
                   pr + max(dcf.price_per_share, p.price)*0.02,
                   f"R${pr:.2f}", ha="center", fontsize=11,
                   fontweight="bold", color=bar.get_facecolor())
    ax_pr.set_ylim(0, max(dcf.price_per_share, p.price) * 1.35)
    ax_pr.set_title("DCF Target vs Market Price", fontsize=10, color=FG, pad=6)
    ax_pr.set_ylabel("R$ per share")
    ax_pr.axhline(p.price, color="#e65100", lw=1.2, ls="--", alpha=0.5)
    ax_pr.annotate(f"{upside:+.1%}", fontsize=11, fontweight="bold", color=rec_col,
                   xy=(0, dcf.price_per_share),
                   xytext=(0.5, (dcf.price_per_share + p.price)/2),
                   ha="center",
                   arrowprops=dict(arrowstyle="->", color="gray", lw=0.8))
    ax_pr.grid(axis="y", alpha=0.4, zorder=0)
    ax_pr.spines[["top", "right"]].set_visible(False)

    # EV composition pie
    ax_pie = fig.add_axes([0.66, 0.53, 0.30, 0.38])
    wedges, _, auts = ax_pie.pie(
        [dcf.pv_fcff, dcf.pv_terminal_value],
        labels=["Explicit\nFCFFs", "Terminal\nValue"],
        colors=[ACC, GOLD], autopct="%1.1f%%", startangle=90,
        wedgeprops={"edgecolor": "white", "linewidth": 2})
    for a in auts: a.set_fontsize(9)
    ax_pie.set_title("EV Composition", fontsize=10, color=FG)

    # Key metrics table
    B = 1e9
    icr_str = "∞" if np.isinf(icr) else f"{icr:.1f}×"
    metrics = [
        ["Metric",           "Value",                             "Metric",      "Value"],
        ["EBIT",             f"R${p.ebit:.2f}B",                  "CDI",         f"{p.cdi:.2%}"],
        ["NOPAT",            f"R${p.ebit*(1-p.tax_rate):.2f}B",   "WACC",        f"{p.wacc:.2%}"],
        ["D&A (proxy)",      f"R${p.da:.2f}B",                    "Ku",          f"{p.ku:.2%}"],
        ["CapEx",            f"R${p.capex:.2f}B",                 "Ke",          f"{p.ke:.2%}"],
        ["Δ NWC",            f"R${p.delta_nwc:.2f}B",             "Kd",          f"{p.kd:.2%}"],
        ["Base FCFF",        f"R${fcff0:.2f}B",                   "D/V",         f"{p.dv:.1%}"],
        ["PV FCFFs",         f"R${dcf.pv_fcff:.1f}B ({1-pct_tv:.0%})",  "E/V",  f"{p.ev:.1%}"],
        ["PV Terminal",      f"R${dcf.pv_terminal_value:.1f}B ({pct_tv:.0%})", "DY", f"{p.dy:.2%}"],
        ["EV",               f"R${dcf.enterprise_value:.1f}B",    "ICR",         icr_str],
        ["Net debt",         f"R${p.net_debt:.2f}B",              "Shares",      f"{p.shares/B:.3f}bn"],
        ["Equity Value",     f"R${dcf.equity_value:.1f}B",        "Market price",f"R${p.price:.2f}"],
        ["DCF target",       f"R${dcf.price_per_share:.2f}",      "Upside",      f"{upside:+.1%}"],
    ]
    ax_t = fig.add_axes([0.03, 0.07, 0.93, 0.40])
    ax_t.set_title("Key Metrics", fontsize=10, color=FG, pad=4, loc="left")
    draw_table(ax_t, metrics, col_w=[0.30, 0.20, 0.30, 0.20])
    pdf.savefig(fig, bbox_inches="tight"); plt.close(fig)

    # ── PAGE 3: HISTORY + DCF ─────────────────────────────────────────
    fig = plt.figure(figsize=(11, 8.5)); fig.patch.set_facecolor(BG)
    hdr_footer(fig, "History + DCF Analysis", 3)

    h = p.history

    # Left chart: historical EBIT and FCFF (long series)
    ax_hist = fig.add_axes([0.04, 0.55, 0.44, 0.36])
    if h.years:
        hy = list(range(len(h.years)))
        xlabels = [yr[:4] for yr in reversed(h.years)]
        ax_hist.bar(hy, list(reversed(h.ebit)),    color=ACC,      alpha=0.75,
                    width=0.6, label="EBIT", zorder=3)
        ax_hist.bar(hy, list(reversed(h.fcff)),    color="#1d8348", alpha=0.85,
                    width=0.6, label="FCFF", zorder=4)
        ax_h2 = ax_hist.twinx()
        ax_h2.plot(hy, list(reversed(h.revenue)), color=GOLD,
                   marker=".", lw=1.8, ms=4, zorder=5, label="Revenue")
        ax_hist.set_xticks(hy[::2])
        ax_hist.set_xticklabels(xlabels[::2], fontsize=7.5, rotation=45)
        ax_hist.set_ylabel("R$ B  (EBIT / FCFF)", color=ACC)
        ax_h2.set_ylabel("R$ B  (Revenue)", color=GOLD)
        ax_hist.grid(axis="y", zorder=0, alpha=0.5)
        ax_hist.spines[["top"]].set_visible(False)
        n_h = len(h.years)
        cagr_txt = ""
        if h.revenue_cagr: cagr_txt += f"Revenue {h.revenue_cagr:.1%}  "
        if h.ebit_cagr:    cagr_txt += f"EBIT {h.ebit_cagr:.1%}  "
        if h.fcff_cagr:    cagr_txt += f"FCFF {h.fcff_cagr:.1%}"
        ax_hist.set_title(
            f"History {h.years[-1][:4]}–{h.years[0][:4]}  ({n_h} years)\n"
            f"CAGR  {cagr_txt}",
            fontsize=10, color=FG, pad=6)
        handles = [mpatches.Patch(color=ACC, alpha=0.75, label="EBIT"),
                   mpatches.Patch(color="#1d8348", alpha=0.85, label="FCFF"),
                   mpatches.Patch(color=GOLD, label="Revenue")]
        ax_hist.legend(handles=handles, fontsize=7.5, facecolor=BG,
                       edgecolor="#d0d7de", loc="upper left")
    else:
        ax_hist.axis("off")
        ax_hist.text(0.5, 0.5, "No history available",
                     ha="center", va="center", color=MUTED)

    # Right chart: FCFF projection + PV
    ax_f = fig.add_axes([0.54, 0.55, 0.43, 0.36])
    colors_phase = [ACC]*3 + ["#2e86c1"]*3 + ["#5dade2"]*2
    ax_f.bar(years, fcffs, color=colors_phase, alpha=0.85, width=0.6, zorder=3)
    ax_r2 = ax_f.twinx()
    ax_r2.plot(years, pvs, color=GOLD, marker="o", lw=2, ms=5, zorder=4)
    ax_f.set_xlabel("Projected year")
    ax_f.set_ylabel("FCFF (R$ B)", color=ACC)
    ax_r2.set_ylabel("PV (R$ B)", color=GOLD)
    ax_f.set_title(
        f"FCFF Projection  ({p.growth_high:.0%} / {p.growth_mid:.0%} / {p.growth_low:.0%})\n"
        f"Base FCFF: R${fcff0:.2f}B  |  WACC: {p.wacc:.2%}",
        fontsize=10, color=FG, pad=6)
    ax_f.set_xticks(years); ax_f.grid(axis="y", zorder=0)
    ax_f.spines[["top"]].set_visible(False)
    ax_f.legend(handles=[
        mpatches.Patch(color=ACC, alpha=0.85, label="Projected FCFF"),
        mpatches.Patch(color=GOLD, label="PV of FCFF"),
    ], fontsize=7.5, facecolor=BG, edgecolor="#d0d7de")

    # FCFF detail table
    fcff_rows = [["Year", "FCFF (R$B)", "Discount factor", "PV (R$B)"]]
    for i, (f, pv) in enumerate(zip(fcffs, pvs), 1):
        fac = (1 + p.wacc) ** (-i)
        fcff_rows.append([str(i), f"{f:.2f}", f"{fac:.4f}", f"{pv:.2f}"])
    fcff_rows.append(["Σ FCFFs",  f"{sum(fcffs):.2f}", "—", f"{dcf.pv_fcff:.2f}"])
    fcff_rows.append(["PV(TV)",   "—",                  "—", f"{dcf.pv_terminal_value:.2f}"])
    fcff_rows.append(["Total EV", "—",                  "—", f"{dcf.enterprise_value:.2f}"])

    ax_ft = fig.add_axes([0.04, 0.37, 0.38, 0.14])
    ax_ft.set_title("Projected FCFFs", fontsize=9, color=FG, pad=3, loc="left")
    draw_table(ax_ft, fcff_rows, col_w=[0.10, 0.28, 0.28, 0.34])

    # Waterfall: PV(FCFFs) → PV(TV) → EV → net_debt → Equity
    wf_labels = [f"Year {t}" for t in years] + ["PV\n(TV)", "EV", "Net\nDebt", "Equity"]
    cum = 0.0
    wf_v, wf_b, wf_c = [], [], []
    for lbl in wf_labels:
        if lbl == "EV":
            wf_v.append(dcf.enterprise_value); wf_b.append(0); wf_c.append(HDR)
        elif lbl == "Equity":
            wf_v.append(dcf.equity_value); wf_b.append(0); wf_c.append(rec_col)
        elif "Debt" in lbl:
            dl = abs(p.net_debt)
            if p.net_debt <= 0:
                wf_v.append(dl); wf_b.append(dcf.enterprise_value); wf_c.append("#1d8348")
            else:
                wf_v.append(dl); wf_b.append(dcf.enterprise_value - dl); wf_c.append("#922b21")
        elif "TV" in lbl:
            v = dcf.pv_terminal_value
            wf_v.append(v); wf_b.append(cum); wf_c.append(GOLD); cum += v
        else:
            idx = int(lbl.split()[1]) - 1
            v = pvs[idx]
            wf_v.append(v); wf_b.append(cum); wf_c.append(ACC); cum += v

    ax_wf = fig.add_axes([0.04, 0.06, 0.92, 0.28])
    xw = np.arange(len(wf_labels))
    ax_wf.bar(xw, wf_v, bottom=wf_b, color=wf_c, width=0.6,
              zorder=3, edgecolor="white", lw=0.8)
    ax_wf.set_xticks(xw); ax_wf.set_xticklabels(wf_labels, fontsize=8)
    ax_wf.set_ylabel("R$ B")
    ax_wf.set_title(
        "DCF Bridge: FCFF → EV → Equity"
        f"  (green = net cash  |  red = net debt)",
        fontsize=11, color=FG, pad=8)
    ax_wf.grid(axis="y", zorder=0)
    ax_wf.spines[["top", "right"]].set_visible(False)
    for xi, (v, b) in enumerate(zip(wf_v, wf_b)):
        if v > 0.5:
            ax_wf.text(xi, b + v + max(wf_v)*0.01,
                       f"{v:.0f}", ha="center", fontsize=7.5, color=FG)

    pdf.savefig(fig, bbox_inches="tight"); plt.close(fig)

    # ── PAGE 4: SENSITIVITY ───────────────────────────────────────────
    fig = plt.figure(figsize=(11, 8.5)); fig.patch.set_facecolor(BG)
    hdr_footer(fig, "Sensitivity Analysis", 4)

    ax_s = fig.add_axes([0.10, 0.10, 0.85, 0.80])

    cell_colors = {
        "strong_buy":  ("#1d8348", "white"),   # > +15%
        "buy":         ("#52be80", FG),         # 0% to +15%
        "hold":        ("#f0b27a", FG),         # -15% to 0%
        "sell":        ("#e74c3c", "white"),    # -30% to -15%
        "strong_sell": ("#922b21", "white"),    # < -30%
        "nan":         ("#dddddd", "#888888"),
    }

    for i, w in enumerate(WACC_STEPS):
        for j, g in enumerate(GTRM_STEPS):
            v = sens[i, j]
            row_y = NW - 1 - i
            if np.isnan(v):
                bg, fc = cell_colors["nan"]
                label  = "n/a"
            else:
                ratio = v / p.price - 1
                if ratio >= 0.15:
                    bg, fc = cell_colors["strong_buy"]
                elif ratio >= 0.0:
                    bg, fc = cell_colors["buy"]
                elif ratio >= -0.15:
                    bg, fc = cell_colors["hold"]
                elif ratio >= -0.30:
                    bg, fc = cell_colors["sell"]
                else:
                    bg, fc = cell_colors["strong_sell"]
                label = f"R${v:.2f}"

            is_base = (abs(w - p.wacc) < 1e-9 and abs(g - p.growth_terminal) < 1e-9)
            ax_s.add_patch(plt.Rectangle(
                (j, row_y), 1, 1,
                facecolor=bg, edgecolor="white", linewidth=2, zorder=1))
            prefix = "★ " if is_base else ""
            ax_s.text(j + 0.5, row_y + 0.5, prefix + label,
                      ha="center", va="center", fontsize=9.5,
                      color=fc, fontweight="bold" if is_base else "normal", zorder=2)

    ax_s.set_xlim(0, NG); ax_s.set_ylim(0, NW)
    ax_s.set_xticks([j + 0.5 for j in range(NG)])
    ax_s.set_xticklabels([f"g = {g:.1%}" for g in GTRM_STEPS], fontsize=9.5)
    ax_s.set_yticks([i + 0.5 for i in range(NW)])
    ax_s.set_yticklabels([f"WACC = {w:.1%}" for w in reversed(WACC_STEPS)], fontsize=9.5)
    ax_s.set_xlabel("Terminal Growth (g)", fontsize=10, labelpad=8)
    ax_s.set_ylabel("WACC", fontsize=10, labelpad=8)
    ax_s.set_title(
        f"Fair Value per Share (R$)  —  WACC × Terminal Growth\n"
        f"Market price: R${p.price:.2f}  |  ★ = base case  |  "
        f"■ Dark green: >+15%   ■ Green: 0% to +15%   "
        f"■ Orange: -15% to 0%   ■ Red: <-15%",
        fontsize=10, color=FG, pad=10)
    ax_s.spines[:].set_visible(False)
    ax_s.tick_params(length=0)

    pdf.savefig(fig, bbox_inches="tight"); plt.close(fig)

    # ── PAGE 5: FINANCIAL DATA ─────────────────────────────────────────
    fig = plt.figure(figsize=(11, 8.5)); fig.patch.set_facecolor(BG)
    hdr_footer(fig, "Financial Data", 5)

    B = 1e9
    inc_vals      = read_csv_kv("income")
    bs_cur, bs_pri = read_csv_bs("balance")

    # Income statement table
    revenue = inc_vals.get("net_revenue", 1)
    ebit_calc = (inc_vals.get("net_revenue", 0) + inc_vals.get("cogs", 0) +
                 inc_vals.get("selling_expenses", 0) + inc_vals.get("ga_expenses", 0) +
                 inc_vals.get("other_op_income", 0))

    inc_items = [
        ("net_revenue",      "Net Revenue"),
        ("cogs",             "COGS"),
        ("selling_expenses", "Selling Expenses"),
        ("ga_expenses",      "G&A Expenses"),
        ("other_op_income",  "Other Op. Income"),
        (None,               "  EBIT"),
        ("da",               "  D&A (proxy)"),
        ("fin_expenses",     "Financial Expenses"),
        ("income_tax",       "Income Tax"),
        ("net_income",       "Net Income"),
    ]
    inc_rows = [["Income Statement", "R$ (bn)", "% Revenue"]]
    for key, label in inc_items:
        if key is None:
            v = ebit_calc
        else:
            v = inc_vals.get(key, 0)
        pct_val = v / revenue if revenue else 0
        inc_rows.append([label, f"{v/B:.2f}", f"{pct_val:.1%}"])

    ax_inc = fig.add_axes([0.03, 0.08, 0.44, 0.82])
    ax_inc.set_title(f"Income Statement — {TICKER}",
                     fontsize=10, color=FG, pad=4, loc="left")
    draw_table(ax_inc, inc_rows, col_w=[0.54, 0.23, 0.23], highlight_last=False)

    # Balance sheet table
    cash_cur  = bs_cur.get("cash_equiv", 0)  + bs_cur.get("short_term_investments", 0)
    cash_pri  = bs_pri.get("cash_equiv", 0) + bs_pri.get("short_term_investments", 0)
    debt_cur  = (bs_cur.get("short_term_loans", 0) + bs_cur.get("short_term_leasing", 0) +
                 bs_cur.get("long_term_loans", 0) + bs_cur.get("long_term_leasing", 0))
    debt_pri  = (bs_pri.get("short_term_loans", 0) + bs_pri.get("short_term_leasing", 0) +
                 bs_pri.get("long_term_loans", 0) + bs_pri.get("long_term_leasing", 0))

    bs_items = [
        ("cash_equiv",             "Cash & Equiv."),
        ("short_term_investments", "Short-term Invest."),
        ("accounts_receivable",    "Accounts Receivable"),
        ("inventories",            "Inventories"),
        ("other_current_assets",   "Other Current Assets"),
        (None,                     "── Total Cash"),
        ("short_term_loans",       "Short-term Loans"),
        ("short_term_leasing",     "Short-term Leasing"),
        ("trade_payables",         "Trade Payables"),
        ("tax_liabilities",        "Tax Liabilities"),
        ("other_current_liabilities", "Other Current Liab."),
        ("long_term_loans",        "Long-term Loans"),
        ("long_term_leasing",      "Long-term Leasing"),
        ("net_ppe",                "Net PP&E"),
        (None,                     "── Financial Debt"),
        (None,                     "── Net Debt"),
    ]
    bs_rows = [["Balance Sheet (highlights)", "Curr. (R$bn)", "Prior (R$bn)"]]
    for key, label in bs_items:
        if key is None:
            if "Cash" in label:
                c_, p_ = cash_cur, cash_pri
            elif "Net Debt" in label:
                c_, p_ = debt_cur - cash_cur, debt_pri - cash_pri
            else:
                c_, p_ = debt_cur, debt_pri
        else:
            c_  = bs_cur.get(key, 0)
            p_  = bs_pri.get(key, 0)
        bs_rows.append([label, f"{c_/B:.2f}", f"{p_/B:.2f}"])

    ax_bs = fig.add_axes([0.53, 0.08, 0.44, 0.82])
    ax_bs.set_title(f"Balance Sheet — {TICKER}",
                    fontsize=10, color=FG, pad=4, loc="left")
    draw_table(ax_bs, bs_rows, col_w=[0.52, 0.24, 0.24], highlight_last=False)

    pdf.savefig(fig, bbox_inches="tight"); plt.close(fig)

print(f"Report saved → {OUT_PDF}  ({TOTAL} pages)")
