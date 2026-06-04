#include "ddm.h"
#include "detail.h"
#include "fcff.h"
#include <cmath>

namespace ratvalue {

std::expected<DDMResult, ValuationError>
compute_ddm(const DDMInputs& inputs) {
    if (inputs.base_dividend_per_share.units() < 0)
        return std::unexpected(ValuationError::InvalidInput);
    if (inputs.cost_of_equity.num <= 0)
        return std::unexpected(ValuationError::InvalidInput);

    // Verifica Ke > g_terminal
    {
        __int128 lhs = static_cast<__int128>(inputs.cost_of_equity.num)
                     * inputs.stable_growth.den;
        __int128 rhs = static_cast<__int128>(inputs.stable_growth.num)
                     * inputs.cost_of_equity.den;
        if (lhs <= rhs)
            return std::unexpected(ValuationError::WACCLEGrowth);
    }

    // Projeta dividendos explícitos reutilizando project_fcff
    auto div_result = project_fcff(
        FCFFProjection{inputs.base_dividend_per_share, inputs.stages});
    if (!div_result) return std::unexpected(div_result.error());

    const auto& divs = *div_result;
    const auto& desc = divs.front().description();
    const auto& rate = divs.front().rate();
    const int   n    = static_cast<int>(divs.size());

    const double ke_d = static_cast<double>(inputs.cost_of_equity.num)
                      / inputs.cost_of_equity.den;

    // PV dos dividendos explícitos
    double pv_sum = 0.0;
    for (int t = 0; t < n; ++t) {
        double discount = std::pow(1.0 + ke_d, -(t + 1));
        pv_sum += detail::to_double(divs[t]) * discount;
    }

    // TV = D_N × (1+g) / (Ke - g)  →  multiplicador como Rational exato
    const auto& g  = inputs.stable_growth;
    const auto& ke = inputs.cost_of_equity;

    __int128 tv_mul_n = (static_cast<__int128>(g.den) + g.num) * ke.den;
    __int128 tv_mul_d = static_cast<__int128>(ke.num) * g.den
                      - static_cast<__int128>(g.num) * ke.den;

    auto tv_mul = detail::make_rational(tv_mul_n, tv_mul_d);
    if (!tv_mul) return std::unexpected(tv_mul.error());

    auto tv = divs.back().scale(*tv_mul);
    if (!tv) return std::unexpected(ValuationError::MoneyError);

    const double pv_tv_d = detail::to_double(*tv)
                         * std::pow(1.0 + ke_d, -static_cast<double>(n));

    const double total = pv_sum + pv_tv_d;

    auto mk = [&](double v) {
        return detail::make_currency_from_double(v, rate, desc);
    };

    auto pv_div_cur = mk(pv_sum);  if (!pv_div_cur) return std::unexpected(pv_div_cur.error());
    auto total_cur  = mk(total);   if (!total_cur)  return std::unexpected(total_cur.error());
    auto pv_tv_cur  = mk(pv_tv_d); if (!pv_tv_cur)  return std::unexpected(pv_tv_cur.error());

    return DDMResult{
        .intrinsic_value_per_share = *total_cur,
        .projected_dividends       = divs,
        .pv_dividends              = *pv_div_cur,
        .terminal_value_per_share  = *tv,
        .pv_terminal_value         = *pv_tv_cur,
    };
}

std::expected<HModelResult, ValuationError>
compute_h_model(const HModelInputs& inputs) {
    if (inputs.cost_of_equity.num <= 0)
        return std::unexpected(ValuationError::InvalidInput);
    if (inputs.half_life.num <= 0)
        return std::unexpected(ValuationError::InvalidInput);

    // Ke > g_estável
    {
        __int128 lhs = static_cast<__int128>(inputs.cost_of_equity.num)
                     * inputs.stable_growth.den;
        __int128 rhs = static_cast<__int128>(inputs.stable_growth.num)
                     * inputs.cost_of_equity.den;
        if (lhs <= rhs) return std::unexpected(ValuationError::WACCLEGrowth);
    }

    // P = D₀ × [(1+g_l) + H×(g_h - g_l)] / (Ke - g_l)
    // Tudo como aritmética racional exata via detail::*

    const auto& g_h = inputs.high_growth;
    const auto& g_l = inputs.stable_growth;
    const auto& ke  = inputs.cost_of_equity;
    const auto& H   = inputs.half_life;

    // (1 + g_l)
    auto one_plus_gl = detail::one_plus(g_l);
    if (!one_plus_gl) return std::unexpected(one_plus_gl.error());

    // (g_h - g_l)
    auto diff_g = detail::r_sub(g_h, g_l);    // pode ser negativo se g_h < g_l
    if (!diff_g) return std::unexpected(diff_g.error());

    // H × (g_h - g_l)
    auto h_times_diff = detail::r_mul(H, *diff_g);
    if (!h_times_diff) return std::unexpected(h_times_diff.error());

    // (1+g_l) + H×(g_h-g_l)  — numerador do multiplicador
    auto numerator = detail::r_add(*one_plus_gl, *h_times_diff);
    if (!numerator) return std::unexpected(numerator.error());

    // (Ke - g_l)  — denominador do multiplicador
    auto denominator = detail::r_sub(ke, g_l);
    if (!denominator) return std::unexpected(denominator.error());
    if (denominator->num <= 0) return std::unexpected(ValuationError::WACCLEGrowth);

    // multiplicador = numerator / denominator
    auto multiplier = detail::make_rational(
        static_cast<__int128>(numerator->num) * denominator->den,
        static_cast<__int128>(numerator->den) * denominator->num);
    if (!multiplier) return std::unexpected(multiplier.error());

    // P = D₀ × multiplier
    auto price = inputs.base_dividend.scale(*multiplier);
    if (!price) return std::unexpected(ValuationError::MoneyError);

    return HModelResult{
        .intrinsic_value = *price,
        .tv_multiplier   = *multiplier,
    };
}

} // namespace ratvalue
