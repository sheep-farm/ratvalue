#pragma once

#include "types.h"
#include <expected>
#include <vector>

namespace ratvalue {

// Inputs para o modelo DCF (Discounted Cash Flow) de Damodaran
struct DCFInputs {
    std::vector<ratmoney::Currency> projected_fcff;   // FCFFs projetados (anos 1..N)
    ratmoney::Rational              wacc;              // taxa de desconto
    ratmoney::Rational              terminal_growth;   // crescimento estável perpétuo
    ratmoney::Currency              net_debt;          // dívida líquida (EV - dívida = Equity)
    int64_t                         shares_outstanding; // ações em circulação
};

// Resultado do modelo DCF
struct DCFResult {
    ratmoney::Currency enterprise_value;        // EV = PV(FCFFs) + PV(TV)
    ratmoney::Currency equity_value;            // EV - dívida líquida
    ratmoney::Currency equity_value_per_share;  // equity / ações
    ratmoney::Currency pv_fcff;                 // soma dos PV(FCFFs) explícitos
    ratmoney::Currency terminal_value;          // valor terminal (sem descontar)
    ratmoney::Currency pv_terminal_value;       // PV do valor terminal
};

// Calcula DCF com crescimento em dois estágios:
//   – período explícito: FCFFs projetados descontados
//   – perpetuidade: valor terminal via Gordon Growth (FCFF_N × (1+g)/(WACC-g))
[[nodiscard]] std::expected<DCFResult, ValuationError>
compute_dcf(const DCFInputs& inputs);

} // namespace ratvalue
