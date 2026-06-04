#include "dcf.h"
#include "detail.h"
#include <cmath>

namespace ratvalue {

std::expected<DCFResult, ValuationError>
compute_dcf(const DCFInputs& inputs) {
    if (inputs.projected_fcff.empty())
        return std::unexpected(ValuationError::InvalidInput);
    if (inputs.shares_outstanding <= 0)
        return std::unexpected(ValuationError::InvalidInput);
    if (inputs.wacc.num <= 0)
        return std::unexpected(ValuationError::InvalidInput);

    // Verifica WACC > g_terminal:  wacc.num × g.den > g.num × wacc.den
    {
        __int128 lhs = static_cast<__int128>(inputs.wacc.num)
                     * inputs.terminal_growth.den;
        __int128 rhs = static_cast<__int128>(inputs.terminal_growth.num)
                     * inputs.wacc.den;
        if (lhs <= rhs)
            return std::unexpected(ValuationError::WACCLEGrowth);
    }

    const auto& fcff = inputs.projected_fcff;

    // Todos os FCFFs e dívida líquida devem estar na mesma denominação
    for (const auto& f : fcff)
        if (!detail::same_currency(f, fcff.front()))
            return std::unexpected(ValuationError::CurrencyMismatch);
    if (!detail::same_currency(inputs.net_debt, fcff.front()))
        return std::unexpected(ValuationError::CurrencyMismatch);

    const auto& desc = fcff.front().description();
    const auto& rate = fcff.front().rate();
    const int   n    = static_cast<int>(fcff.size());

    const double wacc_d = static_cast<double>(inputs.wacc.num)
                        / inputs.wacc.den;

    // ── PV dos FCFFs explícitos (usando double para desconto) ──────────────
    double pv_sum = 0.0;
    for (int t = 0; t < n; ++t) {
        double discount = std::pow(1.0 + wacc_d, -(t + 1));
        pv_sum += detail::to_double(fcff[t]) * discount;
    }

    // ── Valor Terminal: TV = FCFF_N × (1+g) / (WACC - g) ──────────────────
    // Multiplicador como Rational exato: (g.den+g.num)×wacc.den / (wacc.num×g.den - g.num×wacc.den)
    const auto& g = inputs.terminal_growth;
    const auto& w = inputs.wacc;

    __int128 tv_mul_n = (static_cast<__int128>(g.den) + g.num)
                      * w.den;
    __int128 tv_mul_d = static_cast<__int128>(w.num) * g.den
                      - static_cast<__int128>(g.num) * w.den;

    auto tv_mul = detail::make_rational(tv_mul_n, tv_mul_d);
    if (!tv_mul) return std::unexpected(tv_mul.error());

    auto tv = fcff.back().scale(*tv_mul);
    if (!tv) return std::unexpected(ValuationError::MoneyError);

    const double pv_tv_d = detail::to_double(*tv)
                         * std::pow(1.0 + wacc_d, -static_cast<double>(n));

    // ── Enterprise Value ────────────────────────────────────────────────────
    const double ev_d = pv_sum + pv_tv_d;

    auto mk = [&](double v) {
        return detail::make_currency_from_double(v, rate, desc);
    };

    auto pv_fcff_cur = mk(pv_sum);  if (!pv_fcff_cur) return std::unexpected(pv_fcff_cur.error());
    auto ev_cur      = mk(ev_d);    if (!ev_cur)      return std::unexpected(ev_cur.error());
    auto pv_tv_cur   = mk(pv_tv_d); if (!pv_tv_cur)   return std::unexpected(pv_tv_cur.error());

    // ── Equity Value = EV - dívida líquida ─────────────────────────────────
    auto equity = ev_cur->subtract(inputs.net_debt);
    if (!equity) return std::unexpected(ValuationError::MoneyError);

    // ── Valor por ação ──────────────────────────────────────────────────────
    auto per_share = equity->scale(
        ratmoney::Rational{1, inputs.shares_outstanding});
    if (!per_share) return std::unexpected(ValuationError::MoneyError);

    return DCFResult{
        .enterprise_value       = *ev_cur,
        .equity_value           = *equity,
        .equity_value_per_share = *per_share,
        .pv_fcff                = *pv_fcff_cur,
        .terminal_value         = *tv,
        .pv_terminal_value      = *pv_tv_cur,
    };
}

} // namespace ratvalue
