#include "beta.h"
#include "detail.h"
#include <cmath>

namespace ratvalue {

// Leverage factor: [1 + (1-t) × D/E]
static std::expected<ratmoney::Rational, ValuationError>
leverage_factor(ratmoney::Rational debt_to_equity,
                ratmoney::Rational tax_rate) {
    if (debt_to_equity.num < 0)
        return std::unexpected(ValuationError::InvalidInput);
    if (tax_rate.num < 0 || tax_rate.num > tax_rate.den)
        return std::unexpected(ValuationError::InvalidInput);

    // (1-t) = (t.den - t.num) / t.den
    auto one_minus_t = detail::make_rational(
        static_cast<__int128>(tax_rate.den) - tax_rate.num,
        static_cast<__int128>(tax_rate.den));
    if (!one_minus_t) return std::unexpected(one_minus_t.error());

    // (1-t) × D/E
    auto term = detail::r_mul(*one_minus_t, debt_to_equity);
    if (!term) return std::unexpected(term.error());

    // 1 + (1-t) × D/E
    return detail::r_add(ratmoney::Rational{1, 1}, *term);
}

std::expected<ratmoney::Rational, ValuationError>
lever_beta(ratmoney::Rational unlevered_beta,
           ratmoney::Rational debt_to_equity,
           ratmoney::Rational tax_rate) {
    auto factor = leverage_factor(debt_to_equity, tax_rate);
    if (!factor) return std::unexpected(factor.error());
    return detail::r_mul(unlevered_beta, *factor);
}

std::expected<ratmoney::Rational, ValuationError>
unlever_beta(ratmoney::Rational levered_beta,
             ratmoney::Rational debt_to_equity,
             ratmoney::Rational tax_rate) {
    auto factor = leverage_factor(debt_to_equity, tax_rate);
    if (!factor) return std::unexpected(factor.error());
    // β_u = β_l / factor — equivalent to multiplying by (factor.den / factor.num)
    return detail::make_rational(
        static_cast<__int128>(levered_beta.num) * factor->den,
        static_cast<__int128>(levered_beta.den) * factor->num);
}

std::expected<ratmoney::Rational, ValuationError>
bottom_up_beta(const BottomUpBetaInputs& inputs) {
    if (inputs.comparable_levered_betas.empty())
        return std::unexpected(ValuationError::InvalidInput);
    if (inputs.comparable_levered_betas.size()
     != inputs.comparable_debt_to_equity.size())
        return std::unexpected(ValuationError::InvalidInput);

    const size_t n = inputs.comparable_levered_betas.size();

    // Unlever each comparable and accumulate average in double
    // (market data — 4 decimal places of precision is sufficient)
    double sum_bu = 0.0;
    for (size_t i = 0; i < n; ++i) {
        auto bu = unlever_beta(inputs.comparable_levered_betas[i],
                               inputs.comparable_debt_to_equity[i],
                               inputs.comparable_tax_rate);
        if (!bu) return std::unexpected(bu.error());
        sum_bu += static_cast<double>(bu->num) / bu->den;
    }
    double avg_bu = sum_bu / static_cast<double>(n);

    // Re-lever by the target structure
    double t_d  = static_cast<double>(inputs.target_tax_rate.num)
                / inputs.target_tax_rate.den;
    double de_d = static_cast<double>(inputs.target_debt_to_equity.num)
                / inputs.target_debt_to_equity.den;
    double bl_d = avg_bu * (1.0 + (1.0 - t_d) * de_d);

    // Represent as Rational with 4 decimal places
    auto num = static_cast<int64_t>(std::round(bl_d * 10000.0));
    return detail::make_rational(num, 10000);
}

} // namespace ratvalue
