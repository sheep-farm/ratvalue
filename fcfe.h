#pragma once

#include "types.h"
#include <expected>
#include <vector>

namespace ratvalue {

// Inputs para FCFE (Free Cash Flow to Equity)
//
// FCFE = NI - (1-δ)(CapEx - D&A + ΔCGN) + Nova Dívida Líquida
//
//   δ = debt_ratio — fração do reinvestimento financiada por dívida nova
//   Nova Dívida Líquida = emissões - amortizações no período
struct FCFEInputs {
    ratmoney::Currency net_income;
    ratmoney::Rational debt_ratio;                    // δ = D/(D+E)
    ratmoney::Currency capex;
    ratmoney::Currency depreciation_amortization;
    ratmoney::Currency delta_nwc;
    ratmoney::Currency net_new_debt;                  // pode ser negativo (amortização líquida)
};

[[nodiscard]] std::expected<ratmoney::Currency, ValuationError>
compute_fcfe(const FCFEInputs& inputs);

// ── DCF por Equity (desconta FCFE ao custo do capital próprio Ke) ────────────

struct FCFEDCFInputs {
    std::vector<ratmoney::Currency> projected_fcfe;    // FCFEs projetados (anos 1..N)
    ratmoney::Rational              cost_of_equity;    // Ke — taxa de desconto
    ratmoney::Rational              terminal_growth;   // g terminal perpétuo
    int64_t                         shares_outstanding;
};

struct FCFEDCFResult {
    ratmoney::Currency equity_value;             // PV(FCFEs) + PV(TV)
    ratmoney::Currency equity_value_per_share;
    ratmoney::Currency pv_fcfe;
    ratmoney::Currency terminal_value;           // TV (sem descontar)
    ratmoney::Currency pv_terminal_value;
};

// Valor do equity diretamente — sem subtração de dívida líquida.
[[nodiscard]] std::expected<FCFEDCFResult, ValuationError>
compute_fcfe_dcf(const FCFEDCFInputs& inputs);

} // namespace ratvalue
