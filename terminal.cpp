#include "terminal.h"
#include "detail.h"

namespace ratvalue {

TerminalConsistency check_terminal_consistency(
    ratmoney::Rational wacc,
    ratmoney::Rational terminal_growth,
    ratmoney::Rational terminal_roic)
{
    // implied RR = g / ROIC
    ratmoney::Rational implied_rr{0, 1};
    if (terminal_roic.num != 0) {
        auto rr = detail::make_rational(
            static_cast<__int128>(terminal_growth.num) * terminal_roic.den,
            static_cast<__int128>(terminal_growth.den) * terminal_roic.num);
        if (rr) implied_rr = *rr;
    }

    // spread = ROIC − WACC
    ratmoney::Rational spread{0, 1};
    auto s = detail::r_sub(terminal_roic, wacc);
    if (s) spread = *s;

    // RR ∈ [0, 1] ↔ 0 ≤ g/ROIC ≤ 1 ↔ g ≤ ROIC (signs assumed positive)
    bool rr_nonneg = implied_rr.num >= 0;
    bool rr_le_one = static_cast<__int128>(terminal_growth.num) * terminal_roic.den
                  <= static_cast<__int128>(terminal_roic.num) * terminal_growth.den;

    return TerminalConsistency{
        .implied_reinvestment_rate = implied_rr,
        .return_spread             = spread,
        .value_creating            = spread.num > 0,
        .reinvestment_feasible     = rr_nonneg && rr_le_one,
    };
}

std::expected<ratmoney::Currency, ValuationError>
consistent_terminal_value(const ConsistentTVInputs& inputs) {
    if (inputs.terminal_roic.num <= 0)
        return std::unexpected(ValuationError::InvalidInput);

    // WACC > g
    {
        __int128 lhs = static_cast<__int128>(inputs.wacc.num) * inputs.terminal_growth.den;
        __int128 rhs = static_cast<__int128>(inputs.terminal_growth.num) * inputs.wacc.den;
        if (lhs <= rhs) return std::unexpected(ValuationError::WACCLEGrowth);
    }

    // g / ROIC  →  implied reinvestment rate
    auto g_over_roic = detail::make_rational(
        static_cast<__int128>(inputs.terminal_growth.num) * inputs.terminal_roic.den,
        static_cast<__int128>(inputs.terminal_growth.den) * inputs.terminal_roic.num);
    if (!g_over_roic) return std::unexpected(g_over_roic.error());

    // 1 − g/ROIC
    auto one_minus_rr = detail::r_sub(ratmoney::Rational{1, 1}, *g_over_roic);
    if (!one_minus_rr) return std::unexpected(one_minus_rr.error());

    // WACC − g
    auto wacc_minus_g = detail::r_sub(inputs.wacc, inputs.terminal_growth);
    if (!wacc_minus_g || wacc_minus_g->num <= 0)
        return std::unexpected(ValuationError::WACCLEGrowth);

    // multiplier = (1 − g/ROIC) / (WACC − g)
    auto tv_mul = detail::make_rational(
        static_cast<__int128>(one_minus_rr->num) * wacc_minus_g->den,
        static_cast<__int128>(one_minus_rr->den) * wacc_minus_g->num);
    if (!tv_mul) return std::unexpected(tv_mul.error());

    auto tv = inputs.stable_nopat.scale(*tv_mul);
    if (!tv) return std::unexpected(ValuationError::MoneyError);
    return *tv;
}

std::expected<ratmoney::Currency, ValuationError>
terminal_value_by_multiple(ratmoney::Currency terminal_ebitda,
                            ratmoney::Rational ev_ebitda_multiple) {
    if (ev_ebitda_multiple.num <= 0)
        return std::unexpected(ValuationError::InvalidInput);
    auto tv = terminal_ebitda.scale(ev_ebitda_multiple);
    if (!tv) return std::unexpected(ValuationError::MoneyError);
    return *tv;
}

} // namespace ratvalue
