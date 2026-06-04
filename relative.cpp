#include "relative.h"
#include "detail.h"

namespace ratvalue {

std::expected<JustifiedMultiples, ValuationError>
compute_justified_multiples(const JustifiedMultiplesInputs& inputs) {
    if (inputs.cost_of_equity.num <= 0 || inputs.wacc.num <= 0)
        return std::unexpected(ValuationError::InvalidInput);
    if (inputs.roic.num <= 0)
        return std::unexpected(ValuationError::InvalidInput);
    if (inputs.payout_ratio.num < 0 || inputs.payout_ratio.num > inputs.payout_ratio.den)
        return std::unexpected(ValuationError::InvalidInput);
    if (inputs.tax_rate.num < 0 || inputs.tax_rate.num > inputs.tax_rate.den)
        return std::unexpected(ValuationError::InvalidInput);

    // ── g_equity = ROE × (1 − payout) ────────────────────────────────────────
    auto retention = detail::make_rational(
        static_cast<__int128>(inputs.payout_ratio.den) - inputs.payout_ratio.num,
        static_cast<__int128>(inputs.payout_ratio.den));
    if (!retention) return std::unexpected(retention.error());

    auto g_equity = detail::r_mul(inputs.roe, *retention);
    if (!g_equity) return std::unexpected(g_equity.error());

    // Ke > g_equity  e  WACC > g_equity
    auto check_gt = [](ratmoney::Rational rate, ratmoney::Rational g) -> bool {
        return static_cast<__int128>(rate.num) * g.den
             > static_cast<__int128>(g.num)    * rate.den;
    };
    if (!check_gt(inputs.cost_of_equity, *g_equity))
        return std::unexpected(ValuationError::WACCLEGrowth);
    if (!check_gt(inputs.wacc, *g_equity))
        return std::unexpected(ValuationError::WACCLEGrowth);

    // ── Ke − g_equity ─────────────────────────────────────────────────────────
    auto ke_minus_g = detail::r_sub(inputs.cost_of_equity, *g_equity);
    if (!ke_minus_g || ke_minus_g->num <= 0)
        return std::unexpected(ValuationError::WACCLEGrowth);

    // ── P/E = payout / (Ke − g) ───────────────────────────────────────────────
    auto pe = detail::make_rational(
        static_cast<__int128>(inputs.payout_ratio.num) * ke_minus_g->den,
        static_cast<__int128>(inputs.payout_ratio.den) * ke_minus_g->num);
    if (!pe) return std::unexpected(pe.error());

    // ── P/BV = ROE × P/E ──────────────────────────────────────────────────────
    auto pb = detail::r_mul(inputs.roe, *pe);
    if (!pb) return std::unexpected(pb.error());

    // ── EV/EBITDA = (1−t)(1−RR) / (WACC−g) ──────────────────────────────────
    // RR = g / ROIC
    auto rr = detail::make_rational(
        static_cast<__int128>(g_equity->num) * inputs.roic.den,
        static_cast<__int128>(g_equity->den) * inputs.roic.num);
    if (!rr) return std::unexpected(rr.error());

    // (1 − t)
    auto one_minus_t = detail::make_rational(
        static_cast<__int128>(inputs.tax_rate.den) - inputs.tax_rate.num,
        static_cast<__int128>(inputs.tax_rate.den));
    if (!one_minus_t) return std::unexpected(one_minus_t.error());

    // (1 − RR)
    auto one_minus_rr = detail::r_sub(ratmoney::Rational{1, 1}, *rr);
    if (!one_minus_rr) return std::unexpected(one_minus_rr.error());

    // (WACC − g)
    auto wacc_minus_g = detail::r_sub(inputs.wacc, *g_equity);
    if (!wacc_minus_g || wacc_minus_g->num <= 0)
        return std::unexpected(ValuationError::WACCLEGrowth);

    // (1−t)(1−RR)
    auto numerator = detail::r_mul(*one_minus_t, *one_minus_rr);
    if (!numerator) return std::unexpected(numerator.error());

    // / (WACC − g)
    auto ev_ebitda = detail::make_rational(
        static_cast<__int128>(numerator->num) * wacc_minus_g->den,
        static_cast<__int128>(numerator->den) * wacc_minus_g->num);
    if (!ev_ebitda) return std::unexpected(ev_ebitda.error());

    return JustifiedMultiples{
        .pe_ratio             = *pe,
        .pb_ratio             = *pb,
        .ev_ebitda            = *ev_ebitda,
        .g_equity             = *g_equity,
        .implied_reinvestment = *rr,
    };
}

} // namespace ratvalue
