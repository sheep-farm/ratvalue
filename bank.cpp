#include "bank.h"
#include "detail.h"

namespace ratvalue {

std::expected<ratmoney::Currency, ValuationError>
compute_bank_fcfe(const BankFCFEInputs& inputs) {
    if (inputs.equity_reinvestment_rate.num < 0)
        return std::unexpected(ValuationError::InvalidInput);

    // (1 − equity_RR)
    auto one_minus_rr = detail::make_rational(
        static_cast<__int128>(inputs.equity_reinvestment_rate.den)
            - inputs.equity_reinvestment_rate.num,
        static_cast<__int128>(inputs.equity_reinvestment_rate.den));
    if (!one_minus_rr) return std::unexpected(one_minus_rr.error());

    // FCFE = NI × (1 − RR)
    auto fcfe = inputs.net_income.scale(*one_minus_rr);
    if (!fcfe) return std::unexpected(ValuationError::MoneyError);
    return *fcfe;
}

std::expected<ratmoney::Rational, ValuationError>
bank_equity_reinvestment_rate(const RegulatoryCapitalInputs& inputs) {
    if (!detail::same_currency(inputs.net_income, inputs.current_rwa)
     || !detail::same_currency(inputs.net_income, inputs.projected_rwa))
        return std::unexpected(ValuationError::CurrencyMismatch);
    if (inputs.net_income.units() == 0)
        return std::unexpected(ValuationError::InvalidInput);
    if (inputs.target_tier1_ratio.num < 0
     || inputs.target_tier1_ratio.num > inputs.target_tier1_ratio.den)
        return std::unexpected(ValuationError::InvalidInput);

    // ΔRWA = projected − current
    auto delta_rwa = inputs.projected_rwa.subtract(inputs.current_rwa);
    if (!delta_rwa) return std::unexpected(ValuationError::MoneyError);

    // ΔCapital = ΔRWA × target_tier1_ratio  →  em unidades monetárias
    auto delta_cap = delta_rwa->scale(inputs.target_tier1_ratio);
    if (!delta_cap) return std::unexpected(ValuationError::MoneyError);

    // equity_RR = ΔCapital / NI
    return detail::make_rational(
        static_cast<__int128>(delta_cap->units()),
        static_cast<__int128>(inputs.net_income.units()));
}

} // namespace ratvalue
