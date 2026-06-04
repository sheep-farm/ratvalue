#include "growth.h"
#include "detail.h"

namespace ratvalue {

std::expected<ratmoney::Rational, ValuationError>
compute_roic(const ROICInputs& inputs) {
    if (!detail::same_currency(inputs.nopat, inputs.invested_capital))
        return std::unexpected(ValuationError::CurrencyMismatch);
    if (inputs.invested_capital.units() == 0)
        return std::unexpected(ValuationError::InvalidInput);

    return detail::make_rational(
        static_cast<__int128>(inputs.nopat.units()),
        static_cast<__int128>(inputs.invested_capital.units()));
}

std::expected<ratmoney::Rational, ValuationError>
compute_reinvestment_rate(const ReinvestmentRateInputs& inputs) {
    if (!detail::same_currency(inputs.capex, inputs.depreciation_amortization)
     || !detail::same_currency(inputs.capex, inputs.delta_nwc)
     || !detail::same_currency(inputs.capex, inputs.nopat))
        return std::unexpected(ValuationError::CurrencyMismatch);
    if (inputs.nopat.units() == 0)
        return std::unexpected(ValuationError::InvalidInput);

    // reinvestimento = CapEx - D&A + ΔCGN
    auto step1 = inputs.capex.subtract(inputs.depreciation_amortization);
    if (!step1) return std::unexpected(ValuationError::MoneyError);
    auto reinv = step1->add(inputs.delta_nwc);
    if (!reinv) return std::unexpected(ValuationError::MoneyError);

    return detail::make_rational(
        static_cast<__int128>(reinv->units()),
        static_cast<__int128>(inputs.nopat.units()));
}

std::expected<ratmoney::Rational, ValuationError>
fundamental_growth_firm(ratmoney::Rational roic,
                         ratmoney::Rational reinvestment_rate) {
    return detail::r_mul(roic, reinvestment_rate);
}

std::expected<ratmoney::Rational, ValuationError>
compute_roe(const ROEInputs& inputs) {
    if (!detail::same_currency(inputs.net_income, inputs.book_equity))
        return std::unexpected(ValuationError::CurrencyMismatch);
    if (inputs.book_equity.units() == 0)
        return std::unexpected(ValuationError::InvalidInput);

    return detail::make_rational(
        static_cast<__int128>(inputs.net_income.units()),
        static_cast<__int128>(inputs.book_equity.units()));
}

std::expected<ratmoney::Rational, ValuationError>
fundamental_growth_equity(ratmoney::Rational roe,
                           ratmoney::Rational retention_ratio) {
    if (retention_ratio.num < 0 || retention_ratio.num > retention_ratio.den)
        return std::unexpected(ValuationError::InvalidInput);
    return detail::r_mul(roe, retention_ratio);
}

} // namespace ratvalue
