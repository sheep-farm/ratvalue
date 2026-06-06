#include "wacc.h"
#include "detail.h"

namespace ratvalue {

std::expected<ratmoney::Rational, ValuationError>
compute_wacc(const WACCInputs& inputs) {
    if (inputs.equity_weight.num < 0 || inputs.debt_weight.num < 0
     || inputs.cost_of_equity.num < 0 || inputs.cost_of_debt.num < 0)
        return std::unexpected(ValuationError::InvalidInput);
    if (inputs.tax_rate.num < 0
     || inputs.tax_rate.num > inputs.tax_rate.den)
        return std::unexpected(ValuationError::InvalidInput);

    // (1 - t) = (t.den - t.num) / t.den
    auto one_minus_t = detail::make_rational(
        static_cast<__int128>(inputs.tax_rate.den) - inputs.tax_rate.num,
        static_cast<__int128>(inputs.tax_rate.den));
    if (!one_minus_t) return std::unexpected(one_minus_t.error());

    // D_w × Kd
    auto dw_kd = detail::r_mul(inputs.debt_weight, inputs.cost_of_debt);
    if (!dw_kd) return std::unexpected(dw_kd.error());

    // D_w × Kd × (1 - t)  →  debt term with tax shield
    auto debt_term = detail::r_mul(*dw_kd, *one_minus_t);
    if (!debt_term) return std::unexpected(debt_term.error());

    // E_w × Ke  →  equity term
    auto equity_term = detail::r_mul(inputs.equity_weight, inputs.cost_of_equity);
    if (!equity_term) return std::unexpected(equity_term.error());

    // WACC = equity_term + debt_term
    return detail::r_add(*equity_term, *debt_term);
}

} // namespace ratvalue
