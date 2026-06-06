#include "fcff.h"
#include "detail.h"

namespace ratvalue {

std::expected<ratmoney::Currency, ValuationError>
compute_fcff(const FCFFInputs& inputs) {
    if (inputs.tax_rate.num < 0 || inputs.tax_rate.num > inputs.tax_rate.den)
        return std::unexpected(ValuationError::InvalidInput);

    // All monetary inputs must share the same denomination
    if (!detail::same_currency(inputs.ebit, inputs.depreciation_amortization)
     || !detail::same_currency(inputs.ebit, inputs.capex)
     || !detail::same_currency(inputs.ebit, inputs.delta_nwc))
        return std::unexpected(ValuationError::CurrencyMismatch);

    // (1 - t) = (t.den - t.num) / t.den
    auto one_minus_t = detail::make_rational(
        static_cast<__int128>(inputs.tax_rate.den) - inputs.tax_rate.num,
        static_cast<__int128>(inputs.tax_rate.den));
    if (!one_minus_t) return std::unexpected(one_minus_t.error());

    // EBIT × (1 - t)
    auto ebit_after_tax = inputs.ebit.scale(*one_minus_t);
    if (!ebit_after_tax) return std::unexpected(ValuationError::MoneyError);

    // + D&A
    auto r1 = ebit_after_tax->add(inputs.depreciation_amortization);
    if (!r1) return std::unexpected(ValuationError::MoneyError);

    // - CapEx
    auto r2 = r1->subtract(inputs.capex);
    if (!r2) return std::unexpected(ValuationError::MoneyError);

    // - ΔNWC
    auto r3 = r2->subtract(inputs.delta_nwc);
    if (!r3) return std::unexpected(ValuationError::MoneyError);

    return *r3;
}

std::expected<std::vector<ratmoney::Currency>, ValuationError>
project_fcff(const FCFFProjection& proj) {
    if (proj.stages.empty())
        return std::unexpected(ValuationError::InvalidInput);

    std::vector<ratmoney::Currency> result;
    ratmoney::Currency current = proj.base_fcff;

    for (const auto& stage : proj.stages) {
        if (stage.periods <= 0)
            return std::unexpected(ValuationError::InvalidInput);

        // growth factor (1 + g) as exact Rational
        auto factor = detail::one_plus(stage.growth_rate);
        if (!factor) return std::unexpected(factor.error());

        for (int t = 0; t < stage.periods; ++t) {
            auto next = current.scale(*factor);
            if (!next) return std::unexpected(ValuationError::MoneyError);
            result.push_back(*next);
            current = *next;
        }
    }

    return result;
}

} // namespace ratvalue
