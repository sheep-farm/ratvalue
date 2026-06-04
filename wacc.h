#pragma once

#include "types.h"
#include <expected>

namespace ratvalue {

// Inputs para cálculo do WACC
struct WACCInputs {
    ratmoney::Rational equity_weight;   // E / (E + D), peso do capital próprio
    ratmoney::Rational debt_weight;     // D / (E + D), peso da dívida
    ratmoney::Rational cost_of_equity;  // Ke, custo do capital próprio
    ratmoney::Rational cost_of_debt;    // Kd, custo da dívida (pré-imposto)
    ratmoney::Rational tax_rate;        // t, alíquota marginal de imposto
};

// WACC = E_w × Ke + D_w × Kd × (1 - t)
[[nodiscard]] std::expected<ratmoney::Rational, ValuationError>
compute_wacc(const WACCInputs& inputs);

} // namespace ratvalue
