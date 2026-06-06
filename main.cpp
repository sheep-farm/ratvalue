// main.cpp — Complete valuation: Petrobras S.A. (PETR4) + Itau Unibanco (ITUB4)
// Approximate 2023 data.  For educational purposes — not investment advice.


#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "apv.h"
#include "bank.h"
#include "beta.h"
#include "capital_structure.h"
#include "capm.h"
#include "dcf.h"
#include "ddm.h"
#include "detail.h"
#include "distress.h"
#include "excess_returns.h"
#include "fcfe.h"
#include "fcff.h"
#include "growth.h"
#include "kd.h"
#include "normalize.h"
#include "relative.h"
#include "startup.h"
#include "terminal.h"
#include "wacc.h"

// ── display utilities ────────────────────────────────────────────────────────

using ratmoney::Currency;
using ratmoney::Rational;
using namespace ratvalue;

static ratmoney::CurrencyDescription BRL{"BRL", "R$", 2};
static Rational UNIT_RATE{1, 1};

// R$ in whole billions → Currency in cents
static Currency B(int64_t billions) {
    return Currency{billions * 100'000'000'000LL, UNIT_RATE, BRL};
}

static std::string fmt_B(const Currency& c) {
    double v = detail::to_double(c) / 1e9;
    std::ostringstream os;
    bool neg = v < 0;
    if (neg) v = -v;
    os << (neg ? "-" : "") << "R$"
       << std::fixed << std::setprecision(1) << v << "B";
    return os.str();
}

static std::string fmt_share(const Currency& c) {
    std::ostringstream os;
    os << "R$" << std::fixed << std::setprecision(2)
       << detail::to_double(c);
    return os.str();
}

static std::string pct(Rational r, int d = 2) {
    std::ostringstream os;
    os << std::fixed << std::setprecision(d)
       << static_cast<double>(r.num) / r.den * 100.0 << "%";
    return os.str();
}

static std::string rat(Rational r) {
    std::ostringstream os;
    os << r.num << "/" << r.den;
    return os.str();
}

static void header(const std::string& s, char c = '=', int w = 66) {
    std::cout << "\n" << std::string(w, c) << "\n"
              << "  " << s << "\n"
              << std::string(w, c) << "\n";
}

static void section(const std::string& s) {
    std::cout << "\n  -- " << s << " --\n";
}

static void line(int w = 66) {
    std::cout << "  " << std::string(w - 2, '-') << "\n";
}

static void field(const std::string& k, const std::string& v, int w = 40) {
    std::cout << "  " << std::left << std::setw(w) << k << v << "\n";
}

// ── main ─────────────────────────────────────────────────────────────────────

int main() {
    // =========================================================================
    header("PETROBRAS S.A. (PETR4) -- Complete Valuation 2024");
    // =========================================================================

    // ── Base financial data ───────────────────────────────────────────────────
    const Currency ebit_current = B(145);   // EBIT 2023
    const Currency da           = B(45);    // Depreciation & Amortization
    const Currency capex        = B(70);    // CapEx
    const Currency dnwc         = B(5);     // Change in NWC
    const Currency ni           = B(124);   // Net income
    const Currency interest     = B(22);    // Interest expense
    const Currency net_debt     = B(250);   // Net debt
    const Currency ic           = B(500);   // Invested capital (equity + net debt)
    const Currency firm_val     = B(750);   // Estimated EV (for capital structure opt.)
    const int64_t  shares       = 13'000'000'000LL;
    const Rational tax          = {17, 50}; // 34%

    // =========================================================================
    header("I. EBIT NORMALIZATION (2019-2023 cycle)", '-');
    // =========================================================================
    // Petrobras is a cyclical oil company — use historical average
    std::vector<Currency> hist = {B(80), B(95), B(145), B(165), B(145)};

    auto ebit_norm_r = normalize_ebit(NormalizationInputs{
        .method            = NormalizationMethod::HistoricalAverage,
        .historical_ebit   = hist,
        .current_revenue   = B(500),       // field for the other method (not used here)
        .normalized_margin = {0, 1},
    });
    if (!ebit_norm_r) { std::cerr << "Error: normalize\n"; return 1; }
    const Currency ebit_norm = *ebit_norm_r;

    field("EBIT 2019", fmt_B(hist[0]));
    field("EBIT 2020 (COVID)", fmt_B(hist[1]));
    field("EBIT 2021", fmt_B(hist[2]));
    field("EBIT 2022 (peak)", fmt_B(hist[3]));
    field("EBIT 2023", fmt_B(hist[4]));
    line();
    field("NORMALIZED EBIT (average)", fmt_B(ebit_norm));

    // =========================================================================
    header("II. COST OF CAPITAL", '-');
    // =========================================================================

    // Bottom-up beta: global O&G sector comparables
    section("Bottom-Up Beta (Shell, ExxonMobil, BP, Chevron, Equinor)");
    auto bu_r = bottom_up_beta(BottomUpBetaInputs{
        .comparable_levered_betas  = {{19,20},{9,10},{11,10},{22,25},{3,4}},
        .comparable_debt_to_equity = {{1,4},  {1,5}, {2,5}, {3,20},{7,20}},
        .comparable_tax_rate       = {1, 4},   // 25% sector average
        .target_debt_to_equity     = {1, 2},   // D/E Petrobras = 250/500
        .target_tax_rate           = tax,
    });
    if (!bu_r) { std::cerr << "Error: bottom_up_beta\n"; return 1; }
    const Rational beta_l = *bu_r;
    field("Levered beta (bottom-up)", pct(beta_l, 3).substr(0, pct(beta_l,3).size()-1));
    // displayed as number, not as pct
    field("Levered beta (exact)", rat(beta_l));

    // CAPM with Brazil CRP
    section("CAPM — Cost of Equity with Country Risk");
    const Rational rf  = {1, 20};   // 5%  (10Y T-Bond stripped)
    const Rational erp = {1, 20};   // 5%  (mature market ERP — USA)
    const Rational crp = {3, 100};  // 3%  (EMBI+ adjusted for volatility)
    const Rational lam = {1, 2};    // 0.5 (Petrobras ~50% revenue abroad)

    auto ke_r = cost_of_equity(CAPMInputs{
        .risk_free_rate       = rf,
        .beta                 = beta_l,
        .equity_risk_premium  = erp,
        .country_risk_premium = crp,
        .lambda               = lam,
    });
    if (!ke_r) { std::cerr << "Error: CAPM\n"; return 1; }
    const Rational ke = *ke_r;

    field("Rf (stripped T-Bond)",         pct(rf));
    field("ERP (mature market)",           pct(erp));
    field("CRP Brazil",                    pct(crp));
    field("Lambda (country exposure)",     "0.50");
    field("Ke = Rf + b*ERP + l*CRP",      pct(ke));
    field("Ke (exact rational)",           rat(ke));

    // Synthetic Kd via ICR
    section("Synthetic Kd — ICR -> Rating -> Spread");
    auto kd_r = synthetic_cost_of_debt(SyntheticKdInputs{
        .ebit             = ebit_current,
        .interest_expense = interest,
        .risk_free_rate   = rf,
        .large_firm       = true,
    });
    if (!kd_r) { std::cerr << "Error: kd\n"; return 1; }
    const Rational kd = kd_r->cost_of_debt;

    field("EBIT",                 fmt_B(ebit_current));
    field("Interest Expense",     fmt_B(interest));
    field("ICR (coverage)",       [&]{ std::ostringstream s; s << std::fixed << std::setprecision(1) << kd_r->interest_coverage << "x"; return s.str(); }());
    field("Synthetic Rating",     std::string(rating_name(kd_r->rating)));
    field("Default Spread",       pct(kd_r->default_spread));
    field("Kd = Rf + spread",     pct(kd));

    // WACC
    section("WACC");
    const Rational ew = {2, 3};  // 500/(500+250) = 2/3
    const Rational dw = {1, 3};  // 250/750 = 1/3

    auto wacc_r = compute_wacc(WACCInputs{
        .equity_weight  = ew,
        .debt_weight    = dw,
        .cost_of_equity = ke,
        .cost_of_debt   = kd,
        .tax_rate       = tax,
    });
    if (!wacc_r) { std::cerr << "Error: WACC\n"; return 1; }
    const Rational wacc = *wacc_r;

    field("Equity weight (E/V)",  pct(ew, 0));
    field("Ke",                   pct(ke));
    field("Debt weight (D/V)",    pct(dw, 0));
    field("Kd",                   pct(kd));
    field("Tax shield",           pct({66, 100}));
    field("WACC",                 pct(wacc));
    field("WACC (rational)",      rat(wacc));

    // =========================================================================
    header("III. FCFF AND FUNDAMENTAL GROWTH", '-');
    // =========================================================================

    auto fcff0_r = compute_fcff(FCFFInputs{
        .ebit                      = ebit_norm,
        .tax_rate                  = tax,
        .depreciation_amortization = da,
        .capex                     = capex,
        .delta_nwc                 = dnwc,
    });
    if (!fcff0_r) { std::cerr << "Error: fcff\n"; return 1; }
    const Currency fcff0 = *fcff0_r;

    field("Normalized EBIT",       fmt_B(ebit_norm));
    field("EBIT * (1-34%)",        fmt_B(B(83)));   // approx
    field("+ D&A",                 fmt_B(da));
    field("- CapEx",               fmt_B(capex));
    field("- DNWC",                fmt_B(dnwc));
    field("Base FCFF (normalized)", fmt_B(fcff0));

    // ROIC and reinvestment rate
    section("Fundamental Growth: ROIC x RR");
    // NOPAT = EBIT_norm * (1-t) — computed via FCFFInputs
    // Currency of the same type for ROIC
    Currency nopat_cur{
        static_cast<int64_t>(B(126).units() * 66 / 100),
        UNIT_RATE, BRL};  // 126 * 66% = 83.16B approx

    auto roic_r = compute_roic(ROICInputs{
        .nopat            = nopat_cur,
        .invested_capital = ic,
    });
    if (!roic_r) { std::cerr << "Error: roic\n"; return 1; }

    auto rr_r = compute_reinvestment_rate(ReinvestmentRateInputs{
        .capex                     = capex,
        .depreciation_amortization = da,
        .delta_nwc                 = dnwc,
        .nopat                     = nopat_cur,
    });
    if (!rr_r) { std::cerr << "Error: rr\n"; return 1; }

    auto g_fund_r = fundamental_growth_firm(*roic_r, *rr_r);
    if (!g_fund_r) { std::cerr << "Error: g_fund\n"; return 1; }

    field("NOPAT (EBIT_norm * 0.66)",      fmt_B(nopat_cur));
    field("Invested Capital",              fmt_B(ic));
    field("ROIC = NOPAT/IC",              pct(*roic_r));
    field("RR = (CapEx-D&A+DNWC)/NOPAT", pct(*rr_r));
    field("g_fundamental = ROIC * RR",   pct(*g_fund_r));

    // =========================================================================
    header("IV. TERMINAL VALUE — CONSISTENCY WITH ROIC", '-');
    // =========================================================================
    const Rational g_high   = {3, 100};   // 3% explicit growth
    const Rational g_stable = {3, 200};   // 1.5% terminal
    const Rational roic_est = {3, 25};    // 12% ROIC at maturity (oil sector)

    auto tc = check_terminal_consistency(wacc, g_stable, roic_est);
    field("Assumed stable ROIC",           pct(roic_est));
    field("WACC",                          pct(wacc));
    field("Terminal g",                    pct(g_stable));
    field("Implied RR (g/ROIC)",           pct(tc.implied_reinvestment_rate));
    field("ROIC-WACC spread",              pct(tc.return_spread));
    field("Value-creating (ROIC>WACC)?",   tc.value_creating ? "YES" : "NO");
    field("RR feasible (g<=ROIC)?",        tc.reinvestment_feasible ? "YES" : "NO");

    // Project FCFFs and compute consistent TV
    const Rational g_5yr = {3, 100};
    auto fcffs_r = project_fcff(FCFFProjection{
        .base_fcff = fcff0,
        .stages    = {{g_5yr, 5}},
    });
    if (!fcffs_r) { std::cerr << "Error: project_fcff\n"; return 1; }

    // NOPAT in year 5 (for consistent TV)
    Currency nopat_y5{
        static_cast<int64_t>(nopat_cur.units()
            * static_cast<int64_t>(std::round(std::pow(1.03, 5) * 1000)) / 1000),
        UNIT_RATE, BRL};

    auto tv_cons_r = consistent_terminal_value(ConsistentTVInputs{
        .stable_nopat    = nopat_y5,
        .wacc            = wacc,
        .terminal_growth = g_stable,
        .terminal_roic   = roic_est,
    });
    if (!tv_cons_r) { std::cerr << "Error: consistent_tv\n"; return 1; }

    field("Estimated NOPAT year 5",      fmt_B(nopat_y5));
    field("Consistent TV (ROIC=12%)",    fmt_B(*tv_cons_r));

    // =========================================================================
    header("V. MAIN DCF (FCFF -> WACC -> EV -> Equity)", '-');
    // =========================================================================

    std::cout << "\n  FCFF Projection (growth " << pct(g_5yr) << " for 5 years):\n\n";
    const double wacc_d = static_cast<double>(wacc.num) / wacc.den;
    for (int t = 0; t < 5; ++t) {
        double pv = detail::to_double((*fcffs_r)[t]) * std::pow(1.0 + wacc_d, -(t+1));
        Currency pv_cur{static_cast<int64_t>(pv * 100.0 + 0.5), UNIT_RATE, BRL};
        std::ostringstream row;
        row << std::left << std::setw(16) << fmt_B((*fcffs_r)[t])
            << "  PV: " << fmt_B(pv_cur);
        field("    Year " + std::to_string(t + 1), row.str(), 12);
    }

    auto dcf_r = compute_dcf(DCFInputs{
        .projected_fcff     = *fcffs_r,
        .wacc               = wacc,
        .terminal_growth    = g_stable,
        .net_debt           = net_debt,
        .shares_outstanding = shares,
    });
    if (!dcf_r) { std::cerr << "Error: DCF\n"; return 1; }

    line();
    field("PV(explicit FCFFs)",      fmt_B(dcf_r->pv_fcff));
    field("Terminal Value (Gordon)", fmt_B(dcf_r->terminal_value));
    field("PV(Terminal Value)",      fmt_B(dcf_r->pv_terminal_value));
    line();
    field("Enterprise Value (EV)",   fmt_B(dcf_r->enterprise_value));
    field("(-) Net debt",            fmt_B(net_debt));
    field("Equity Value",            fmt_B(dcf_r->equity_value));
    field("Shares outstanding",      "13.0 billion");
    line();
    field("FAIR VALUE (DCF/FCFF)",   fmt_share(dcf_r->equity_value_per_share));

    // =========================================================================
    header("VI. FCFE — ALTERNATIVE EQUITY APPROACH", '-');
    // =========================================================================
    // FCFE = NI - (1-delta)*(CapEx-D&A+dNWC) + net_new_debt
    // Petrobras: delta = D_w = 1/3, net_new_debt ~ 0 (stable debt)
    auto fcfe0_r = compute_fcfe(FCFEInputs{
        .net_income                = ni,
        .debt_ratio                = dw,        // {1,3} = 33%
        .capex                     = capex,
        .depreciation_amortization = da,
        .delta_nwc                 = dnwc,
        .net_new_debt              = B(0),
    });
    if (!fcfe0_r) { std::cerr << "Error: FCFE\n"; return 1; }

    auto fcfes_r = project_fcff(FCFFProjection{
        .base_fcff = *fcfe0_r,
        .stages    = {{g_5yr, 5}},
    });
    if (!fcfes_r) { std::cerr << "Error: project_fcfe\n"; return 1; }

    auto fcfe_dcf_r = compute_fcfe_dcf(FCFEDCFInputs{
        .projected_fcfe     = *fcfes_r,
        .cost_of_equity     = ke,
        .terminal_growth    = g_stable,
        .shares_outstanding = shares,
    });
    if (!fcfe_dcf_r) { std::cerr << "Error: FCFE_DCF\n"; return 1; }

    field("Base FCFE (NI - equity reinvestment)", fmt_B(*fcfe0_r));
    field("Equity Value (FCFE)",  fmt_B(fcfe_dcf_r->equity_value));
    field("FAIR VALUE (FCFE)",    fmt_share(fcfe_dcf_r->equity_value_per_share));

    // =========================================================================
    header("VII. APV — MM vs. MILES-EZZELL", '-');
    // =========================================================================
    auto apv_mm_r = compute_apv(APVInputs{
        .projected_fcff           = *fcffs_r,
        .terminal_growth          = g_stable,
        .unlevered_cost_of_equity = ke,   // approx (should be Ku without leverage)
        .debt_market_value        = net_debt,
        .tax_rate                 = tax,
        .net_debt                 = net_debt,
        .shares_outstanding       = shares,
    });

    auto apv_me_r = compute_apv_miles_ezzell(APVMilesEzzellInputs{
        .projected_fcff           = *fcffs_r,
        .terminal_growth          = g_stable,
        .unlevered_cost_of_equity = ke,
        .cost_of_debt             = kd,
        .debt_market_value        = net_debt,
        .tax_rate                 = tax,
        .net_debt                 = net_debt,
        .shares_outstanding       = shares,
    });

    if (apv_mm_r && apv_me_r) {
        field("PV(Tax Shield) — MM (permanent debt)",    fmt_B(apv_mm_r->pv_tax_shield));
        field("PV(Tax Shield) — ME (rebalanced debt)",   fmt_B(apv_me_r->pv_tax_shield));
        field("APV (MM)", fmt_B(apv_mm_r->apv));
        field("APV (Miles-Ezzell)", fmt_B(apv_me_r->apv));
        field("Fair price MM",  fmt_share(apv_mm_r->equity_value_per_share));
        field("Fair price ME",  fmt_share(apv_me_r->equity_value_per_share));
    }

    // =========================================================================
    header("VIII. OPTIMAL CAPITAL STRUCTURE", '-');
    // =========================================================================
    auto ocs_r = optimal_capital_structure(OptimalCapitalStructureInputs{
        .unlevered_beta       = {4, 5},    // beta_u O&G sector
        .risk_free_rate       = rf,
        .equity_risk_premium  = erp,
        .country_risk_premium = crp,
        .lambda               = lam,
        .tax_rate             = tax,
        .ebit                 = ebit_norm,
        .firm_value           = firm_val,
        .steps                = 10,
        .large_firm           = true,
    });
    if (ocs_r) {
        std::cout << "\n  D/(D+E)  Rating     Kd        Ke        WACC\n";
        line();
        for (const auto& p : ocs_r->schedule) {
            double d  = static_cast<double>(p.debt_ratio.num) / p.debt_ratio.den;
            std::ostringstream row;
            row << std::left
                << std::setw(9)  << static_cast<int>(d * 100 + 0.5)
                << std::setw(11) << rating_name(p.rating)
                << std::setw(10) << pct(p.cost_of_debt)
                << std::setw(10) << pct(p.cost_of_equity)
                << pct(p.wacc);
            std::cout << "  " << row.str() << "\n";
        }
        line();
        double opt_d = static_cast<double>(ocs_r->optimal.debt_ratio.num)
                     / ocs_r->optimal.debt_ratio.den * 100;
        field("OPTIMAL STRUCTURE  D/(D+E)",
              std::to_string(static_cast<int>(opt_d + 0.5)) + "%"
              + "   WACC = " + pct(ocs_r->optimal.wacc));
    }

    // =========================================================================
    header("IX. EXCESS RETURNS / EVA", '-');
    // =========================================================================
    auto er_r = excess_returns_value(SingleStageERInputs{
        .invested_capital = ic,
        .roic             = *roic_r,
        .wacc             = wacc,
        .terminal_growth  = g_stable,
    });
    if (er_r) {
        field("Invested Capital (IC)",          fmt_B(er_r->asset_value));
        field("PV(Excess Returns, EVA)",        fmt_B(er_r->pv_excess_returns));
        field("Firm Value (IC + PV_EVA)",       fmt_B(er_r->firm_value));
        double equity_er = detail::to_double(er_r->firm_value)
                         - detail::to_double(net_debt);
        std::ostringstream ep;
        ep << "R$" << std::fixed << std::setprecision(2)
           << equity_er / shares;
        field("Price/share (EVA)", ep.str());
    }

    // =========================================================================
    header("X. JUSTIFIED MULTIPLES", '-');
    // =========================================================================
    // Normalized ROE (full cycle, without extreme leverage): ~15%
    // Historical Petrobras payout: ~60% (minimum 45% + specials)
    // g = ROE_norm * retention = 15% * 40% = 6% < Ke (11.57%) → valid
    auto mult_r = compute_justified_multiples(JustifiedMultiplesInputs{
        .cost_of_equity = ke,
        .wacc           = wacc,
        .roe            = {3, 20},      // 15% (normalized)
        .payout_ratio   = {3, 5},       // 60%
        .roic           = *roic_r,
        .tax_rate       = tax,
    });
    if (mult_r) {
        double pe  = static_cast<double>(mult_r->pe_ratio.num) / mult_r->pe_ratio.den;
        double pb  = static_cast<double>(mult_r->pb_ratio.num) / mult_r->pb_ratio.den;
        double ev  = static_cast<double>(mult_r->ev_ebitda.num) / mult_r->ev_ebitda.den;
        field("Implied g (ROE * retention)", pct(mult_r->g_equity));
        field("Justified P/E",   [&]{ std::ostringstream s; s << std::fixed << std::setprecision(1) << pe << "x"; return s.str(); }());
        field("Justified P/BV",  [&]{ std::ostringstream s; s << std::fixed << std::setprecision(1) << pb << "x"; return s.str(); }());
        field("EV/EBITDA justif.", [&]{ std::ostringstream s; s << std::fixed << std::setprecision(1) << ev << "x"; return s.str(); }());
    }

    // =========================================================================
    header("XI. H-MODEL (DIVIDENDS)", '-');
    // =========================================================================
    // Petrobras: 2023 dividend ~R$5/share = 500 cents
    // Gradual transition from high growth -> stable
    Currency div0{500, UNIT_RATE, BRL};   // R$5.00/share

    auto hm_r = compute_h_model(HModelInputs{
        .base_dividend  = div0,
        .high_growth    = g_5yr,            // 3% initial
        .stable_growth  = g_stable,         // 1.5% final
        .half_life      = {5, 1},           // H=5 years
        .cost_of_equity = ke,
    });
    if (hm_r) {
        field("Base dividend (D0)",       fmt_share(div0));
        field("Initial growth",           pct(g_5yr));
        field("Terminal growth",          pct(g_stable));
        field("Transition half-life (H)", "5 years");
        field("Exact multiplier",         rat(hm_r->tv_multiplier));
        field("FAIR VALUE (H-Model)",     fmt_share(hm_r->intrinsic_value));
    }

    // =========================================================================
    header("XII. SCENARIOS — ENERGY TRANSITION", '-');
    // =========================================================================
    // Petrobras under decarbonization pressure:
    //   Bull (35%): resilient oil demand through 2040+
    //   Base (50%): moderate transition, oil relevant through 2035
    //   Bear (15%): accelerated transition, FCFF falls after 2030
    auto scen_r = startup_valuation(StartupValuationInputs{
        .scenarios = {
            {"Bull — Resilient demand",         {7,20},  B(120), g_stable, wacc, 8},
            {"Base — Moderate transition",       {1,2},   B(80),  g_stable, wacc, 8},
            {"Bear — Rapid decarbonization",     {3,20},  B(40),  {0,1},    wacc, 8},
        },
        .survival_probability = {19, 20},   // 95%
        .failure_value        = B(100),     // residual asset value
        .net_debt             = net_debt,
        .shares_outstanding   = shares,
    });
    if (scen_r) {
        for (const auto& [name, ev_scen] : scen_r->scenario_evs)
            field("  EV scenario: " + name, fmt_B(ev_scen));
        line();
        field("Probability-weighted EV",  fmt_B(scen_r->enterprise_value));
        field("Equity Value (scenarios)", fmt_B(scen_r->equity_value));
        field("FAIR VALUE (scenarios)",   fmt_share(scen_r->equity_value_per_share));
    }

    // =========================================================================
    header("XIII. EQUITY AS OPTION (Petrobras 2015 crisis)", '-');
    // =========================================================================
    // In 2015 (Carwash + oil price collapse):
    //   Asset value: ~R$350B; Debt face value: ~R$420B; sigma_V ~= 0.45
    //   Merton model shows positive equity value through optionality
    auto opt_r = equity_as_option({
        .firm_value       = 350.0e9,   // R$350B in reais
        .debt_face_value  = 420.0e9,   // R$420B
        .asset_volatility = 0.45,
        .risk_free_rate   = 0.12,      // Selic 2015 ~12%
        .debt_maturity    = 3.0,
    });
    field("Asset value (V)",        "R$350B");
    field("Debt face value",        "R$420B");
    field("Asset volatility",       "45%");
    field("Selic (2015)",           "12%");
    field("Average maturity",       "3 years");
    line();
    std::ostringstream ov;
    ov << "R$" << std::fixed << std::setprecision(1)
       << opt_r.equity_value / 1e9 << "B  (value through optionality)";
    field("Equity (Black-Scholes)", ov.str());

    std::ostringstream dv;
    dv << std::fixed << std::setprecision(1)
       << opt_r.probability_default * 100 << "%";
    field("P(default) risk-neutral", dv.str());

    std::ostringstream dd;
    dd << std::fixed << std::setprecision(2) << opt_r.distance_to_default;
    field("Distance to default (d2)", dd.str());

    // =========================================================================
    header("PANEL II — ITAU UNIBANCO (ITUB4)", '=');
    header("XIV. BANK FCFE (Regulatory Capital Tier 1)", '-');
    // =========================================================================
    // Approximate 2023 data
    const Currency ni_itau   = B(35);          // Net income R$35B
    const Currency rwa_current = B(1800);       // Current RWA R$1.8T
    const Currency rwa_proj    = B(1944);       // Projected RWA (+8%)
    const Rational tier1_target = {27, 200};   // 13.5%

    auto eq_rr_r = bank_equity_reinvestment_rate(RegulatoryCapitalInputs{
        .net_income         = ni_itau,
        .current_rwa        = rwa_current,
        .projected_rwa      = rwa_proj,
        .target_tier1_ratio = tier1_target,
    });
    if (!eq_rr_r) { std::cerr << "Error: bank_rr\n"; return 1; }

    auto fcfe_bank_r = compute_bank_fcfe(BankFCFEInputs{
        .net_income               = ni_itau,
        .equity_reinvestment_rate = *eq_rr_r,
    });
    if (!fcfe_bank_r) { std::cerr << "Error: bank_fcfe\n"; return 1; }

    field("Net Income",                      fmt_B(ni_itau));
    field("Current RWA",                     fmt_B(rwa_current));
    field("Projected RWA",                   fmt_B(rwa_proj));
    field("Target Tier1",                    pct(tier1_target));
    field("Equity RR = DRWA*Tier1 / NI",    pct(*eq_rr_r));
    field("Bank FCFE = NI*(1-RR)",          fmt_B(*fcfe_bank_r));

    // Itau ROE ~= 20%, bank Ke ~= 13% (less risk than oil)
    const Rational ke_bank = {13, 100};
    const Rational g_bank  = {6, 100};  // Brazilian banking growth
    const Rational tv_mul_bank_n{
        static_cast<int64_t>((100 + 6) * 100),
        static_cast<int64_t>((13 - 6) * 10000 / 100)
    };

    // Using compute_fcfe_dcf: DDM equivalent for bank
    auto fcfe_b_proj = project_fcff(FCFFProjection{
        .base_fcff = *fcfe_bank_r,
        .stages    = {{g_bank, 5}},
    });
    if (fcfe_b_proj) {
        auto bank_dcf = compute_fcfe_dcf(FCFEDCFInputs{
            .projected_fcfe     = *fcfe_b_proj,
            .cost_of_equity     = ke_bank,
            .terminal_growth    = {3, 100},
            .shares_outstanding = 8'000'000'000LL,  // 8B shares
        });
        if (bank_dcf) {
            field("Equity Value (bank)", fmt_B(bank_dcf->equity_value));
            field("FAIR VALUE (Itau)",   fmt_share(bank_dcf->equity_value_per_share));
        }
    }

    // =========================================================================
    header("COMPARATIVE SUMMARY — PETR4", '=');
    // =========================================================================
    std::cout << "\n  Model                         Fair Value\n";
    line();
    if (dcf_r)
        field("  DCF (FCFF, Gordon TV)",    fmt_share(dcf_r->equity_value_per_share));
    if (fcfe_dcf_r)
        field("  FCFE (direct equity)",     fmt_share(fcfe_dcf_r->equity_value_per_share));
    if (apv_mm_r)
        field("  APV Modigliani-Miller",    fmt_share(apv_mm_r->equity_value_per_share));
    if (apv_me_r)
        field("  APV Miles-Ezzell",         fmt_share(apv_me_r->equity_value_per_share));
    if (hm_r)
        field("  H-Model (dividends)",      fmt_share(hm_r->intrinsic_value));
    if (scen_r)
        field("  Scenarios (transition)",   fmt_share(scen_r->equity_value_per_share));
    line();
    field("  Market price (ref.)",      "R$38-42/share (2024)");
    std::cout << "\n  Note: all values are educational estimates.\n"
                 "  Real data requires audited financial statements and own assumptions.\n\n";

    return 0;
}
