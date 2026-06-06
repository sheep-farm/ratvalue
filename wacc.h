#pragma once

#include "types.h"
#include <expected>

namespace ratvalue {

// WACC computation inputs
struct WACCInputs {
    ratmoney::Rational equity_weight;   // E / (E + D), equity weight
    ratmoney::Rational debt_weight;     // D / (E + D), debt weight
    ratmoney::Rational cost_of_equity;  // Ke, cost of equity
    ratmoney::Rational cost_of_debt;    // Kd, pre-tax cost of debt
    ratmoney::Rational tax_rate;        // t, marginal tax rate
};

// WACC = E_w × Ke + D_w × Kd × (1 - t)
[[nodiscard]] std::expected<ratmoney::Rational, ValuationError>
compute_wacc(const WACCInputs& inputs);

} // namespace ratvalue
