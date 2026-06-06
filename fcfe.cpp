#include "fcfe.h"
#include "detail.h"
#include <cmath>

namespace ratvalue {

std::expected<ratmoney::Currency, ValuationError>
compute_fcfe(const FCFEInputs& inputs) {
    if (!detail::same_currency(inputs.net_income, inputs.capex)
     || !detail::same_currency(inputs.net_income, inputs.depreciation_amortization)
     || !detail::same_currency(inputs.net_income, inputs.delta_nwc)
     || !detail::same_currency(inputs.net_income, inputs.net_new_debt))
        return std::unexpected(ValuationError::CurrencyMismatch);

    if (inputs.debt_ratio.num < 0 || inputs.debt_ratio.num > inputs.debt_ratio.den)
        return std::unexpected(ValuationError::InvalidInput);

    // (1 - δ)
    auto one_minus_delta = detail::make_rational(
        static_cast<__int128>(inputs.debt_ratio.den) - inputs.debt_ratio.num,
        static_cast<__int128>(inputs.debt_ratio.den));
    if (!one_minus_delta) return std::unexpected(one_minus_delta.error());

    // total reinvestment: CapEx - D&A + ΔNWC
    auto step1 = inputs.capex.subtract(inputs.depreciation_amortization);
    if (!step1) return std::unexpected(ValuationError::MoneyError);
    auto net_inv = step1->add(inputs.delta_nwc);
    if (!net_inv) return std::unexpected(ValuationError::MoneyError);

    // equity share of reinvestment: reinvestment × (1 - δ)
    auto equity_reinv = net_inv->scale(*one_minus_delta);
    if (!equity_reinv) return std::unexpected(ValuationError::MoneyError);

    // NI - equity reinvestment
    auto r1 = inputs.net_income.subtract(*equity_reinv);
    if (!r1) return std::unexpected(ValuationError::MoneyError);

    // + net new debt
    auto fcfe = r1->add(inputs.net_new_debt);
    if (!fcfe) return std::unexpected(ValuationError::MoneyError);

    return *fcfe;
}

std::expected<FCFEDCFResult, ValuationError>
compute_fcfe_dcf(const FCFEDCFInputs& inputs) {
    if (inputs.projected_fcfe.empty())
        return std::unexpected(ValuationError::InvalidInput);
    if (inputs.shares_outstanding <= 0)
        return std::unexpected(ValuationError::InvalidInput);
    if (inputs.cost_of_equity.num <= 0)
        return std::unexpected(ValuationError::InvalidInput);

    for (const auto& f : inputs.projected_fcfe)
        if (!detail::same_currency(f, inputs.projected_fcfe.front()))
            return std::unexpected(ValuationError::CurrencyMismatch);

    // Ke > g_terminal
    {
        __int128 lhs = static_cast<__int128>(inputs.cost_of_equity.num)
                     * inputs.terminal_growth.den;
        __int128 rhs = static_cast<__int128>(inputs.terminal_growth.num)
                     * inputs.cost_of_equity.den;
        if (lhs <= rhs) return std::unexpected(ValuationError::WACCLEGrowth);
    }

    const auto& fcfe = inputs.projected_fcfe;
    const auto& desc = fcfe.front().description();
    const auto& rate = fcfe.front().rate();
    const int   n    = static_cast<int>(fcfe.size());
    const double ke_d = static_cast<double>(inputs.cost_of_equity.num)
                      / inputs.cost_of_equity.den;

    double pv_sum = 0.0;
    for (int t = 0; t < n; ++t)
        pv_sum += detail::to_double(fcfe[t]) * std::pow(1.0 + ke_d, -(t + 1));

    // TV = FCFE_N × (1+g) / (Ke - g)
    const auto& g  = inputs.terminal_growth;
    const auto& ke = inputs.cost_of_equity;
    __int128 tv_mul_n = (static_cast<__int128>(g.den) + g.num) * ke.den;
    __int128 tv_mul_d = static_cast<__int128>(ke.num) * g.den
                      - static_cast<__int128>(g.num) * ke.den;

    auto tv_mul = detail::make_rational(tv_mul_n, tv_mul_d);
    if (!tv_mul) return std::unexpected(tv_mul.error());

    auto tv = fcfe.back().scale(*tv_mul);
    if (!tv) return std::unexpected(ValuationError::MoneyError);

    const double pv_tv_d = detail::to_double(*tv)
                         * std::pow(1.0 + ke_d, -static_cast<double>(n));
    const double eq_d    = pv_sum + pv_tv_d;

    auto mk = [&](double v) {
        return detail::make_currency_from_double(v, rate, desc);
    };

    auto pv_fcfe_cur = mk(pv_sum); if (!pv_fcfe_cur) return std::unexpected(pv_fcfe_cur.error());
    auto eq_cur      = mk(eq_d);   if (!eq_cur)      return std::unexpected(eq_cur.error());
    auto pv_tv_cur   = mk(pv_tv_d);if (!pv_tv_cur)   return std::unexpected(pv_tv_cur.error());

    auto per_share = eq_cur->scale(ratmoney::Rational{1, inputs.shares_outstanding});
    if (!per_share) return std::unexpected(ValuationError::MoneyError);

    return FCFEDCFResult{
        .equity_value            = *eq_cur,
        .equity_value_per_share  = *per_share,
        .pv_fcfe                 = *pv_fcfe_cur,
        .terminal_value          = *tv,
        .pv_terminal_value       = *pv_tv_cur,
    };
}

} // namespace ratvalue
