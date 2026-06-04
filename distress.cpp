#include "distress.h"
#include <cmath>

namespace ratvalue {

// N(x) = P(Z ≤ x) para distribuição normal padrão
static double normal_cdf(double x) noexcept {
    // N(x) = 0.5 × erfc(−x / √2)
    return 0.5 * std::erfc(-x * 0.7071067811865475);  // 1/√2
}

EquityAsOptionResult equity_as_option(const EquityAsOptionInputs& in) noexcept {
    // Valores indefinidos/degenerar: retornar zeros em vez de NaN/crash
    if (in.firm_value <= 0.0 || in.debt_face_value <= 0.0
     || in.asset_volatility <= 0.0 || in.debt_maturity <= 0.0)
        return {};

    const double sigma_sqrt_T = in.asset_volatility * std::sqrt(in.debt_maturity);
    const double d1 = (std::log(in.firm_value / in.debt_face_value)
                    + (in.risk_free_rate + 0.5 * in.asset_volatility * in.asset_volatility)
                      * in.debt_maturity)
                    / sigma_sqrt_T;
    const double d2 = d1 - sigma_sqrt_T;

    const double Nd1 = normal_cdf(d1);
    const double Nd2 = normal_cdf(d2);
    const double pv_debt_rf = in.debt_face_value * std::exp(-in.risk_free_rate * in.debt_maturity);

    const double equity = in.firm_value * Nd1 - pv_debt_rf * Nd2;

    return EquityAsOptionResult{
        .equity_value         = equity,
        .debt_market_value    = in.firm_value - equity,
        .probability_default  = normal_cdf(-d2),  // N(−d2)
        .distance_to_default  = d2,
        .d1                   = d1,
    };
}

} // namespace ratvalue
