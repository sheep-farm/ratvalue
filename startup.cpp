#include "startup.h"
#include "detail.h"
#include <cmath>

namespace ratvalue {

std::expected<StartupValuationResult, ValuationError>
startup_valuation(const StartupValuationInputs& inputs) {
    if (inputs.scenarios.empty() || inputs.shares_outstanding <= 0)
        return std::unexpected(ValuationError::InvalidInput);
    if (inputs.survival_probability.num < 0
     || inputs.survival_probability.num > inputs.survival_probability.den)
        return std::unexpected(ValuationError::InvalidInput);

    // Valida soma das probabilidades ≈ 1
    double prob_sum = 0.0;
    for (const auto& s : inputs.scenarios) {
        if (s.wacc.num <= 0 || s.years_to_terminal <= 0
         || s.probability.num < 0)
            return std::unexpected(ValuationError::InvalidInput);
        if (static_cast<__int128>(s.wacc.num) * s.terminal_growth.den
         <= static_cast<__int128>(s.terminal_growth.num) * s.wacc.den)
            return std::unexpected(ValuationError::WACCLEGrowth);
        prob_sum += static_cast<double>(s.probability.num) / s.probability.den;
    }
    if (std::abs(prob_sum - 1.0) > 1e-6)
        return std::unexpected(ValuationError::InvalidInput);

    // Moeda de referência: primeiro FCFF terminal
    const auto& ref = inputs.scenarios.front().terminal_fcff;
    for (const auto& s : inputs.scenarios)
        if (!detail::same_currency(s.terminal_fcff, ref))
            return std::unexpected(ValuationError::CurrencyMismatch);
    if (!detail::same_currency(inputs.failure_value, ref)
     || !detail::same_currency(inputs.net_debt, ref))
        return std::unexpected(ValuationError::CurrencyMismatch);

    const auto& desc = ref.description();
    const auto& rate = ref.rate();

    // Valor de cada cenário descontado a hoje
    std::vector<std::pair<std::string, ratmoney::Currency>> scenario_evs;
    double ev_expected = 0.0;

    for (const auto& s : inputs.scenarios) {
        const double wacc_d = static_cast<double>(s.wacc.num) / s.wacc.den;

        // TV = FCFF_terminal × (1+g) / (WACC−g)
        __int128 tv_mul_n = (static_cast<__int128>(s.terminal_growth.den) + s.terminal_growth.num)
                          * s.wacc.den;
        __int128 tv_mul_d = static_cast<__int128>(s.wacc.num) * s.terminal_growth.den
                          - static_cast<__int128>(s.terminal_growth.num) * s.wacc.den;
        auto tv_mul = detail::make_rational(tv_mul_n, tv_mul_d);
        if (!tv_mul) return std::unexpected(tv_mul.error());

        auto tv = s.terminal_fcff.scale(*tv_mul);
        if (!tv) return std::unexpected(ValuationError::MoneyError);

        // PV = TV / (1+WACC)^T
        double pv_d = detail::to_double(*tv)
                    * std::pow(1.0 + wacc_d, -static_cast<double>(s.years_to_terminal));

        auto pv_cur = detail::make_currency_from_double(pv_d, rate, desc);
        if (!pv_cur) return std::unexpected(pv_cur.error());

        scenario_evs.emplace_back(s.name, *pv_cur);

        double prob = static_cast<double>(s.probability.num) / s.probability.den;
        ev_expected += prob * pv_d;
    }

    // E(EV) × P(survive) + V_fail × P(fail)
    const double p_surv = static_cast<double>(inputs.survival_probability.num)
                        / inputs.survival_probability.den;
    const double p_fail = 1.0 - p_surv;
    const double vfail  = detail::to_double(inputs.failure_value);

    const double ev_d = ev_expected * p_surv + vfail * p_fail;

    auto ev_cur = detail::make_currency_from_double(ev_d, rate, desc);
    if (!ev_cur) return std::unexpected(ev_cur.error());

    auto equity = ev_cur->subtract(inputs.net_debt);
    if (!equity) return std::unexpected(ValuationError::MoneyError);

    auto per_share = equity->scale(ratmoney::Rational{1, inputs.shares_outstanding});
    if (!per_share) return std::unexpected(ValuationError::MoneyError);

    return StartupValuationResult{
        .enterprise_value       = *ev_cur,
        .equity_value           = *equity,
        .equity_value_per_share = *per_share,
        .scenario_evs           = std::move(scenario_evs),
    };
}

} // namespace ratvalue
