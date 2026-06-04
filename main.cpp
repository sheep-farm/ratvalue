// main.cpp — Valuation completo: Petrobras S.A. (PETR4) + Itaú Unibanco (ITUB4)
// Dados aproximados 2023.  Fins educacionais — não constitui recomendação.


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

// ── utilitários de exibição ──────────────────────────────────────────────────

using ratmoney::Currency;
using ratmoney::Rational;
using namespace ratvalue;

static ratmoney::CurrencyDescription BRL{"BRL", "R$", 2};
static Rational UNIT_RATE{1, 1};

// R$ em bilhoes inteiros → Currency em centavos
static Currency B(int64_t bilhoes) {
    return Currency{bilhoes * 100'000'000'000LL, UNIT_RATE, BRL};
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

static void titulo(const std::string& s, char c = '=', int w = 66) {
    std::cout << "\n" << std::string(w, c) << "\n"
              << "  " << s << "\n"
              << std::string(w, c) << "\n";
}

static void sec(const std::string& s) {
    std::cout << "\n  -- " << s << " --\n";
}

static void linha(int w = 66) {
    std::cout << "  " << std::string(w - 2, '-') << "\n";
}

static void campo(const std::string& k, const std::string& v, int w = 40) {
    std::cout << "  " << std::left << std::setw(w) << k << v << "\n";
}

// ── main ─────────────────────────────────────────────────────────────────────

int main() {
    // =========================================================================
    titulo("PETROBRAS S.A. (PETR4) -- Valuation Completo 2024");
    // =========================================================================

    // ── Dados financeiros base ────────────────────────────────────────────────
    const Currency ebit_atual = B(145);   // EBIT 2023
    const Currency da         = B(45);    // Depreciacao & Amortizacao
    const Currency capex      = B(70);    // CapEx
    const Currency dnwc       = B(5);     // Variacao CGN
    const Currency ni         = B(124);   // Lucro liquido
    const Currency juros      = B(22);    // Despesa financeira
    const Currency net_debt   = B(250);   // Divida liquida
    const Currency ic         = B(500);   // Capital investido (PL + Div.liq.)
    const Currency firm_val   = B(750);   // EV estimado (para otim. estrutura)
    const int64_t  shares     = 13'000'000'000LL;
    const Rational tax        = {17, 50}; // 34%

    // =========================================================================
    titulo("I. NORMALIZACAO DO EBIT (ciclo 2019-2023)", '-');
    // =========================================================================
    // Petrobras e empresa ciclica de petroleo — usa media historica
    std::vector<Currency> hist = {B(80), B(95), B(145), B(165), B(145)};

    auto ebit_norm_r = normalize_ebit(NormalizationInputs{
        .method            = NormalizationMethod::HistoricalAverage,
        .historical_ebit   = hist,
        .current_revenue   = B(500),       // campo do outro metodo (nao usado aqui)
        .normalized_margin = {0, 1},
    });
    if (!ebit_norm_r) { std::cerr << "Erro: normalize\n"; return 1; }
    const Currency ebit_norm = *ebit_norm_r;

    campo("EBIT 2019", fmt_B(hist[0]));
    campo("EBIT 2020 (COVID)", fmt_B(hist[1]));
    campo("EBIT 2021", fmt_B(hist[2]));
    campo("EBIT 2022 (pico)", fmt_B(hist[3]));
    campo("EBIT 2023", fmt_B(hist[4]));
    linha();
    campo("EBIT NORMALIZADO (media)", fmt_B(ebit_norm));

    // =========================================================================
    titulo("II. CUSTO DE CAPITAL", '-');
    // =========================================================================

    // Beta bottom-up: comparaveis do setor O&G global
    sec("Beta Bottom-Up (Shell, ExxonMobil, BP, Chevron, Equinor)");
    auto bu_r = bottom_up_beta(BottomUpBetaInputs{
        .comparable_levered_betas  = {{19,20},{9,10},{11,10},{22,25},{3,4}},
        .comparable_debt_to_equity = {{1,4},  {1,5}, {2,5}, {3,20},{7,20}},
        .comparable_tax_rate       = {1, 4},   // 25% media setor
        .target_debt_to_equity     = {1, 2},   // D/E Petrobras = 250/500
        .target_tax_rate           = tax,
    });
    if (!bu_r) { std::cerr << "Erro: bottom_up_beta\n"; return 1; }
    const Rational beta_l = *bu_r;
    campo("Beta alavancado (bottom-up)", pct(beta_l, 3).substr(0, pct(beta_l,3).size()-1));
    // mostramos como numero, nao como pct
    campo("Beta levered (exato)", rat(beta_l));

    // CAPM com CRP Brasil
    sec("CAPM — Custo do Capital Proprio com Risco-Pais");
    const Rational rf  = {1, 20};   // 5%  (T-Bond 10Y stripped)
    const Rational erp = {1, 20};   // 5%  (ERP mercado maduro — EUA)
    const Rational crp = {3, 100};  // 3%  (EMBI+ ajustado por volatilidade)
    const Rational lam = {1, 2};    // 0.5 (Petrobras ~50% receita no exterior)

    auto ke_r = cost_of_equity(CAPMInputs{
        .risk_free_rate       = rf,
        .beta                 = beta_l,
        .equity_risk_premium  = erp,
        .country_risk_premium = crp,
        .lambda               = lam,
    });
    if (!ke_r) { std::cerr << "Erro: CAPM\n"; return 1; }
    const Rational ke = *ke_r;

    campo("Rf (T-Bond stripped)",       pct(rf));
    campo("ERP (mercado maduro)",        pct(erp));
    campo("CRP Brasil",                  pct(crp));
    campo("Lambda (exposicao pais)",     "0.50");
    campo("Ke = Rf + b*ERP + l*CRP",    pct(ke));
    campo("Ke (racional exato)",         rat(ke));

    // Kd sintetico via ICR
    sec("Kd Sintetico — ICR -> Rating -> Spread");
    auto kd_r = synthetic_cost_of_debt(SyntheticKdInputs{
        .ebit             = ebit_atual,
        .interest_expense = juros,
        .risk_free_rate   = rf,
        .large_firm       = true,
    });
    if (!kd_r) { std::cerr << "Erro: kd\n"; return 1; }
    const Rational kd = kd_r->cost_of_debt;

    campo("EBIT",                fmt_B(ebit_atual));
    campo("Despesa Financeira",  fmt_B(juros));
    campo("ICR (cobertura)",     [&]{ std::ostringstream s; s << std::fixed << std::setprecision(1) << kd_r->interest_coverage << "x"; return s.str(); }());
    campo("Rating Sintetico",    std::string(rating_name(kd_r->rating)));
    campo("Spread de Default",   pct(kd_r->default_spread));
    campo("Kd = Rf + spread",    pct(kd));

    // WACC
    sec("WACC");
    const Rational ew = {2, 3};  // 500/(500+250) = 2/3
    const Rational dw = {1, 3};  // 250/750 = 1/3

    auto wacc_r = compute_wacc(WACCInputs{
        .equity_weight  = ew,
        .debt_weight    = dw,
        .cost_of_equity = ke,
        .cost_of_debt   = kd,
        .tax_rate       = tax,
    });
    if (!wacc_r) { std::cerr << "Erro: WACC\n"; return 1; }
    const Rational wacc = *wacc_r;

    campo("Peso equity (E/V)",   pct(ew, 0));
    campo("Ke",                  pct(ke));
    campo("Peso divida (D/V)",   pct(dw, 0));
    campo("Kd",                  pct(kd));
    campo("Beneficio fiscal",    pct({66, 100}));
    campo("WACC",                pct(wacc));
    campo("WACC (racional)",     rat(wacc));

    // =========================================================================
    titulo("III. FCFF E CRESCIMENTO FUNDAMENTAL", '-');
    // =========================================================================

    auto fcff0_r = compute_fcff(FCFFInputs{
        .ebit                      = ebit_norm,
        .tax_rate                  = tax,
        .depreciation_amortization = da,
        .capex                     = capex,
        .delta_nwc                 = dnwc,
    });
    if (!fcff0_r) { std::cerr << "Erro: fcff\n"; return 1; }
    const Currency fcff0 = *fcff0_r;

    campo("EBIT normalizado",       fmt_B(ebit_norm));
    campo("EBIT * (1-34%)",         fmt_B(B(83)));   // aprox
    campo("+ D&A",                  fmt_B(da));
    campo("- CapEx",                fmt_B(capex));
    campo("- DCGN",                 fmt_B(dnwc));
    campo("FCFF base (normalizado)", fmt_B(fcff0));

    // ROIC e taxa de reinvestimento
    sec("Crescimento Fundamental: ROIC x RR");
    // NOPAT = EBIT_norm * (1-t) — calculamos via FCFFInputs
    // Para o ROIC usamos currency do mesmo tipo
    Currency nopat_cur{
        static_cast<int64_t>(B(126).units() * 66 / 100),
        UNIT_RATE, BRL};  // 126 * 66% = 83.16B aprox

    auto roic_r = compute_roic(ROICInputs{
        .nopat            = nopat_cur,
        .invested_capital = ic,
    });
    if (!roic_r) { std::cerr << "Erro: roic\n"; return 1; }

    auto rr_r = compute_reinvestment_rate(ReinvestmentRateInputs{
        .capex                     = capex,
        .depreciation_amortization = da,
        .delta_nwc                 = dnwc,
        .nopat                     = nopat_cur,
    });
    if (!rr_r) { std::cerr << "Erro: rr\n"; return 1; }

    auto g_fund_r = fundamental_growth_firm(*roic_r, *rr_r);
    if (!g_fund_r) { std::cerr << "Erro: g_fund\n"; return 1; }

    campo("NOPAT (EBIT_norm * 0.66)", fmt_B(nopat_cur));
    campo("Capital Investido",        fmt_B(ic));
    campo("ROIC = NOPAT/IC",          pct(*roic_r));
    campo("RR = (CapEx-D&A+DCGN)/NOPAT", pct(*rr_r));
    campo("g_fundamental = ROIC * RR",  pct(*g_fund_r));

    // =========================================================================
    titulo("IV. VALOR TERMINAL — CONSISTENCIA COM ROIC", '-');
    // =========================================================================
    const Rational g_high   = {3, 100};   // 3% crescimento explicito
    const Rational g_stable = {3, 200};   // 1.5% terminal
    const Rational roic_est = {3, 25};    // 12% ROIC na maturidade (setor petroleo)

    auto tc = check_terminal_consistency(wacc, g_stable, roic_est);
    campo("ROIC estavel assumido",        pct(roic_est));
    campo("WACC",                         pct(wacc));
    campo("g terminal",                   pct(g_stable));
    campo("RR implicita (g/ROIC)",        pct(tc.implied_reinvestment_rate));
    campo("Spread ROIC-WACC",             pct(tc.return_spread));
    campo("Cria valor (ROIC>WACC)?",      tc.value_creating ? "SIM" : "NAO");
    campo("RR viavel (g<=ROIC)?",         tc.reinvestment_feasible ? "SIM" : "NAO");

    // Projeta FCFFs e calcula TV consistente
    const Rational g_5anos = {3, 100};
    auto fcffs_r = project_fcff(FCFFProjection{
        .base_fcff = fcff0,
        .stages    = {{g_5anos, 5}},
    });
    if (!fcffs_r) { std::cerr << "Erro: project_fcff\n"; return 1; }

    // NOPAT no ano 5 (para TV consistente)
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
    if (!tv_cons_r) { std::cerr << "Erro: tv_consistente\n"; return 1; }

    campo("NOPAT estimado ano 5",      fmt_B(nopat_y5));
    campo("TV consistente (ROIC=12%)", fmt_B(*tv_cons_r));

    // =========================================================================
    titulo("V. DCF PRINCIPAL (FCFF -> WACC -> EV -> Equity)", '-');
    // =========================================================================

    std::cout << "\n  Projecao FCFF (crescimento " << pct(g_5anos) << " por 5 anos):\n\n";
    const double wacc_d = static_cast<double>(wacc.num) / wacc.den;
    for (int t = 0; t < 5; ++t) {
        double pv = detail::to_double((*fcffs_r)[t]) * std::pow(1.0 + wacc_d, -(t+1));
        Currency pv_cur{static_cast<int64_t>(pv * 100.0 + 0.5), UNIT_RATE, BRL};
        std::ostringstream row;
        row << std::left << std::setw(16) << fmt_B((*fcffs_r)[t])
            << "  PV: " << fmt_B(pv_cur);
        campo("    Ano " + std::to_string(t + 1), row.str(), 12);
    }

    auto dcf_r = compute_dcf(DCFInputs{
        .projected_fcff     = *fcffs_r,
        .wacc               = wacc,
        .terminal_growth    = g_stable,
        .net_debt           = net_debt,
        .shares_outstanding = shares,
    });
    if (!dcf_r) { std::cerr << "Erro: DCF\n"; return 1; }

    linha();
    campo("PV(FCFFs explicitos)",    fmt_B(dcf_r->pv_fcff));
    campo("Valor Terminal (Gordon)", fmt_B(dcf_r->terminal_value));
    campo("PV(Valor Terminal)",      fmt_B(dcf_r->pv_terminal_value));
    linha();
    campo("Enterprise Value (EV)",  fmt_B(dcf_r->enterprise_value));
    campo("(-) Divida liquida",      fmt_B(net_debt));
    campo("Equity Value",            fmt_B(dcf_r->equity_value));
    campo("Acoes em circulacao",     "13.0 bilhoes");
    linha();
    campo("PRECO JUSTO (DCF/FCFF)",  fmt_share(dcf_r->equity_value_per_share));

    // =========================================================================
    titulo("VI. FCFE — ABORDAGEM ALTERNATIVA POR EQUITY", '-');
    // =========================================================================
    // FCFE = NI - (1-delta)*(CapEx-D&A+dNWC) + nova_divida
    // Petrobras: delta = D_w = 1/3, nova_divida ~ 0 (divida estavel)
    auto fcfe0_r = compute_fcfe(FCFEInputs{
        .net_income                = ni,
        .debt_ratio                = dw,        // {1,3} = 33%
        .capex                     = capex,
        .depreciation_amortization = da,
        .delta_nwc                 = dnwc,
        .net_new_debt              = B(0),
    });
    if (!fcfe0_r) { std::cerr << "Erro: FCFE\n"; return 1; }

    auto fcfes_r = project_fcff(FCFFProjection{
        .base_fcff = *fcfe0_r,
        .stages    = {{g_5anos, 5}},
    });
    if (!fcfes_r) { std::cerr << "Erro: project_fcfe\n"; return 1; }

    auto fcfe_dcf_r = compute_fcfe_dcf(FCFEDCFInputs{
        .projected_fcfe     = *fcfes_r,
        .cost_of_equity     = ke,
        .terminal_growth    = g_stable,
        .shares_outstanding = shares,
    });
    if (!fcfe_dcf_r) { std::cerr << "Erro: FCFE_DCF\n"; return 1; }

    campo("FCFE base (NI - reinvestimento equity)", fmt_B(*fcfe0_r));
    campo("Equity Value (FCFE)",  fmt_B(fcfe_dcf_r->equity_value));
    campo("PRECO JUSTO (FCFE)",   fmt_share(fcfe_dcf_r->equity_value_per_share));

    // =========================================================================
    titulo("VII. APV — MM vs. MILES-EZZELL", '-');
    // =========================================================================
    auto apv_mm_r = compute_apv(APVInputs{
        .projected_fcff           = *fcffs_r,
        .terminal_growth          = g_stable,
        .unlevered_cost_of_equity = ke,   // aprox (deveria ser Ku sem alavancagem)
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
        campo("PV(Tax Shield) — MM (divida permanente)", fmt_B(apv_mm_r->pv_tax_shield));
        campo("PV(Tax Shield) — ME (divida rebalanceada)", fmt_B(apv_me_r->pv_tax_shield));
        campo("APV (MM)", fmt_B(apv_mm_r->apv));
        campo("APV (Miles-Ezzell)", fmt_B(apv_me_r->apv));
        campo("Preco MM",  fmt_share(apv_mm_r->equity_value_per_share));
        campo("Preco ME",  fmt_share(apv_me_r->equity_value_per_share));
    }

    // =========================================================================
    titulo("VIII. ESTRUTURA OTIMA DE CAPITAL", '-');
    // =========================================================================
    auto ocs_r = optimal_capital_structure(OptimalCapitalStructureInputs{
        .unlevered_beta       = {4, 5},    // beta_u setor O&G
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
        linha();
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
        linha();
        double opt_d = static_cast<double>(ocs_r->optimal.debt_ratio.num)
                     / ocs_r->optimal.debt_ratio.den * 100;
        campo("ESTRUTURA OTIMA  D/(D+E)",
              std::to_string(static_cast<int>(opt_d + 0.5)) + "%"
              + "   WACC = " + pct(ocs_r->optimal.wacc));
    }

    // =========================================================================
    titulo("IX. EXCESS RETURNS / EVA", '-');
    // =========================================================================
    auto er_r = excess_returns_value(SingleStageERInputs{
        .invested_capital = ic,
        .roic             = *roic_r,
        .wacc             = wacc,
        .terminal_growth  = g_stable,
    });
    if (er_r) {
        campo("Capital Investido (IC)",         fmt_B(er_r->asset_value));
        campo("PV(Retornos em Excesso, EVA)",   fmt_B(er_r->pv_excess_returns));
        campo("Valor da Firma (IC + PV_EVA)",   fmt_B(er_r->firm_value));
        double equity_er = detail::to_double(er_r->firm_value)
                         - detail::to_double(net_debt);
        std::ostringstream ep;
        ep << "R$" << std::fixed << std::setprecision(2)
           << equity_er / shares;
        campo("Preco/acao (EVA)", ep.str());
    }

    // =========================================================================
    titulo("X. MULTIPLOS JUSTIFICADOS", '-');
    // =========================================================================
    // ROE normalizado (ciclo completo, sem alavancagem extrema): ~15%
    // Payout historico Petrobras: ~60% (minimo 45% + especiais)
    // g = ROE_norm * retention = 15% * 40% = 6% < Ke (11.57%) → valido
    auto mult_r = compute_justified_multiples(JustifiedMultiplesInputs{
        .cost_of_equity = ke,
        .wacc           = wacc,
        .roe            = {3, 20},      // 15% (normalizado)
        .payout_ratio   = {3, 5},       // 60%
        .roic           = *roic_r,
        .tax_rate       = tax,
    });
    if (mult_r) {
        double pe  = static_cast<double>(mult_r->pe_ratio.num) / mult_r->pe_ratio.den;
        double pb  = static_cast<double>(mult_r->pb_ratio.num) / mult_r->pb_ratio.den;
        double ev  = static_cast<double>(mult_r->ev_ebitda.num) / mult_r->ev_ebitda.den;
        campo("g implicito (ROE * retention)", pct(mult_r->g_equity));
        campo("P/L justificado",   [&]{ std::ostringstream s; s << std::fixed << std::setprecision(1) << pe << "x"; return s.str(); }());
        campo("P/VP justificado",  [&]{ std::ostringstream s; s << std::fixed << std::setprecision(1) << pb << "x"; return s.str(); }());
        campo("EV/EBITDA justif.", [&]{ std::ostringstream s; s << std::fixed << std::setprecision(1) << ev << "x"; return s.str(); }());
    }

    // =========================================================================
    titulo("XI. H-MODEL (DIVIDENDOS)", '-');
    // =========================================================================
    // Petrobras: dividendo 2023 ~R$5/acao = 500 centavos
    // Transicao gradual de crescimento alto -> estavel
    Currency div0{500, UNIT_RATE, BRL};   // R$5.00/acao

    auto hm_r = compute_h_model(HModelInputs{
        .base_dividend  = div0,
        .high_growth    = g_5anos,          // 3% inicial
        .stable_growth  = g_stable,         // 1.5% final
        .half_life      = {5, 1},           // H=5 anos
        .cost_of_equity = ke,
    });
    if (hm_r) {
        campo("Dividendo base (D0)",     fmt_share(div0));
        campo("Crescimento inicial",     pct(g_5anos));
        campo("Crescimento terminal",    pct(g_stable));
        campo("Meia-vida transicao (H)", "5 anos");
        campo("Multiplicador exato",     rat(hm_r->tv_multiplier));
        campo("PRECO JUSTO (H-Model)",   fmt_share(hm_r->intrinsic_value));
    }

    // =========================================================================
    titulo("XII. CENARIOS — TRANSICAO ENERGETICA", '-');
    // =========================================================================
    // Petrobras sob pressao de descarbonizacao:
    //   Bull (35%): demanda petroleo resiliente ate 2040+
    //   Base (50%): transicao moderada, petroleo relevante ate 2035
    //   Bear (15%): transicao acelerada, FCFF cai apos 2030
    auto scen_r = startup_valuation(StartupValuationInputs{
        .scenarios = {
            {"Bull — Demanda resiliente",      {7,20},  B(120), g_stable, wacc, 8},
            {"Base — Transicao moderada",      {1,2},   B(80),  g_stable, wacc, 8},
            {"Bear — Descarbonizacao rapida",  {3,20},  B(40),  {0,1},    wacc, 8},
        },
        .survival_probability = {19, 20},   // 95%
        .failure_value        = B(100),     // valor residual de ativos
        .net_debt             = net_debt,
        .shares_outstanding   = shares,
    });
    if (scen_r) {
        for (const auto& [nome, ev_cen] : scen_r->scenario_evs)
            campo("  EV cenario: " + nome, fmt_B(ev_cen));
        linha();
        campo("EV ponderado por probabilidade", fmt_B(scen_r->enterprise_value));
        campo("Equity Value (cenarios)",        fmt_B(scen_r->equity_value));
        campo("PRECO JUSTO (cenarios)",         fmt_share(scen_r->equity_value_per_share));
    }

    // =========================================================================
    titulo("XIII. EQUITY COMO OPCAO (Petrobras na crise 2015)", '-');
    // =========================================================================
    // Em 2015 (Lava Jato + queda do petroleo):
    //   Valor dos ativos: ~R$350B; Divida face: ~R$420B; sigma_V ~= 0.45
    //   Modelo de Merton mostra equity com valor positivo pela opcionalidade
    auto opt_r = equity_as_option({
        .firm_value       = 350.0e9,   // R$350B em reais
        .debt_face_value  = 420.0e9,   // R$420B
        .asset_volatility = 0.45,
        .risk_free_rate   = 0.12,      // Selic 2015 ~12%
        .debt_maturity    = 3.0,
    });
    campo("Valor dos ativos (V)",  "R$350B");
    campo("Valor de face divida",  "R$420B");
    campo("Volatilidade ativos",   "45%");
    campo("Selic (2015)",          "12%");
    campo("Maturidade media",      "3 anos");
    linha();
    std::ostringstream ov;
    ov << "R$" << std::fixed << std::setprecision(1)
       << opt_r.equity_value / 1e9 << "B  (valor pela opcionalidade)";
    campo("Equity (Black-Scholes)", ov.str());

    std::ostringstream dv;
    dv << std::fixed << std::setprecision(1)
       << opt_r.probability_default * 100 << "%";
    campo("P(default) risk-neutral", dv.str());

    std::ostringstream dd;
    dd << std::fixed << std::setprecision(2) << opt_r.distance_to_default;
    campo("Distancia ao default (d2)", dd.str());

    // =========================================================================
    titulo("PAINEL II — BANCO ITAU UNIBANCO (ITUB4)", '=');
    titulo("XIV. FCFE PARA BANCO (Capital Regulatorio Tier 1)", '-');
    // =========================================================================
    // Dados aproximados 2023
    const Currency ni_itau  = B(35);          // Lucro liquido R$35B
    const Currency rwa_atual = B(1800);        // RWA atual R$1.8T
    const Currency rwa_proj  = B(1944);        // RWA projetado (+8%)
    const Rational tier1_target = {27, 200};  // 13.5%

    auto eq_rr_r = bank_equity_reinvestment_rate(RegulatoryCapitalInputs{
        .net_income         = ni_itau,
        .current_rwa        = rwa_atual,
        .projected_rwa      = rwa_proj,
        .target_tier1_ratio = tier1_target,
    });
    if (!eq_rr_r) { std::cerr << "Erro: bank_rr\n"; return 1; }

    auto fcfe_banco_r = compute_bank_fcfe(BankFCFEInputs{
        .net_income               = ni_itau,
        .equity_reinvestment_rate = *eq_rr_r,
    });
    if (!fcfe_banco_r) { std::cerr << "Erro: bank_fcfe\n"; return 1; }

    campo("Lucro Liquido", fmt_B(ni_itau));
    campo("RWA atual",     fmt_B(rwa_atual));
    campo("RWA projetado", fmt_B(rwa_proj));
    campo("Target Tier1",  pct(tier1_target));
    campo("Equity RR = DRWA*Tier1 / NI",
          pct(*eq_rr_r));
    campo("FCFE banco = NI*(1-RR)", fmt_B(*fcfe_banco_r));

    // ROE Itau ~= 20%, Ke banco ~= 13% (menos risco que petroleo)
    const Rational ke_banco = {13, 100};
    const Rational g_banco  = {6, 100};  // crescimento bancario Brasil
    const Rational tv_mul_banco_n{
        static_cast<int64_t>((100 + 6) * 100),
        static_cast<int64_t>((13 - 6) * 10000 / 100)
    };

    // Usando compute_fcfe_dcf: DDM equivalente para banco
    auto fcfe_b_proj = project_fcff(FCFFProjection{
        .base_fcff = *fcfe_banco_r,
        .stages    = {{g_banco, 5}},
    });
    if (fcfe_b_proj) {
        auto banco_dcf = compute_fcfe_dcf(FCFEDCFInputs{
            .projected_fcfe     = *fcfe_b_proj,
            .cost_of_equity     = ke_banco,
            .terminal_growth    = {3, 100},
            .shares_outstanding = 8'000'000'000LL,  // 8B acoes
        });
        if (banco_dcf) {
            campo("Equity Value (banco)", fmt_B(banco_dcf->equity_value));
            campo("PRECO JUSTO (Itau)",   fmt_share(banco_dcf->equity_value_per_share));
        }
    }

    // =========================================================================
    titulo("RESUMO COMPARATIVO — PETR4", '=');
    // =========================================================================
    std::cout << "\n  Modelo                        Preco Justo\n";
    linha();
    if (dcf_r)
        campo("  DCF (FCFF, Gordon TV)",   fmt_share(dcf_r->equity_value_per_share));
    if (fcfe_dcf_r)
        campo("  FCFE (equity direto)",    fmt_share(fcfe_dcf_r->equity_value_per_share));
    if (apv_mm_r)
        campo("  APV Modigliani-Miller",   fmt_share(apv_mm_r->equity_value_per_share));
    if (apv_me_r)
        campo("  APV Miles-Ezzell",        fmt_share(apv_me_r->equity_value_per_share));
    if (hm_r)
        campo("  H-Model (dividendos)",    fmt_share(hm_r->intrinsic_value));
    if (scen_r)
        campo("  Cenarios (transicao)",    fmt_share(scen_r->equity_value_per_share));
    linha();
    campo("  Cotacao de mercado (ref.)",  "R$38-42/acao (2024)");
    std::cout << "\n  Nota: todos os valores sao estimativas educacionais.\n"
                 "  Dados reais requerem DRE/Balanco auditados e hipoteses proprias.\n\n";

    return 0;
}
