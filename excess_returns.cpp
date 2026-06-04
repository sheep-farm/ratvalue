#include "excess_returns.h"
#include "detail.h"
#include "fcff.h"
#include <cmath>

namespace ratvalue {

std::expected<ExcessReturnsResult, ValuationError>
excess_returns_value(const SingleStageERInputs& inputs) {
    // WACC > g
    {
        __int128 lhs = static_cast<__int128>(inputs.wacc.num) * inputs.terminal_growth.den;
        __int128 rhs = static_cast<__int128>(inputs.terminal_growth.num) * inputs.wacc.den;
        if (lhs <= rhs) return std::unexpected(ValuationError::WACCLEGrowth);
    }

    // (ROIC − g) / (WACC − g)  →  V = IC × this
    auto roic_minus_g  = detail::r_sub(inputs.roic, inputs.terminal_growth);
    auto wacc_minus_g  = detail::r_sub(inputs.wacc, inputs.terminal_growth);
    if (!roic_minus_g || !wacc_minus_g || wacc_minus_g->num <= 0)
        return std::unexpected(ValuationError::WACCLEGrowth);

    auto v_mul = detail::r_div(*roic_minus_g, *wacc_minus_g);
    if (!v_mul) return std::unexpected(v_mul.error());

    auto firm_value = inputs.invested_capital.scale(*v_mul);
    if (!firm_value) return std::unexpected(ValuationError::MoneyError);

    // PV(EVA) = IC × (ROIC − WACC) / (WACC − g)
    auto roic_minus_wacc = detail::r_sub(inputs.roic, inputs.wacc);
    if (!roic_minus_wacc) return std::unexpected(roic_minus_wacc.error());

    auto er_mul = detail::r_div(*roic_minus_wacc, *wacc_minus_g);
    if (!er_mul) return std::unexpected(er_mul.error());

    auto pv_er = inputs.invested_capital.scale(*er_mul);
    if (!pv_er) return std::unexpected(ValuationError::MoneyError);

    return ExcessReturnsResult{
        .firm_value        = *firm_value,
        .asset_value       = inputs.invested_capital,
        .pv_excess_returns = *pv_er,
    };
}

std::expected<ExcessReturnsResult, ValuationError>
excess_returns_multistage(const MultiStageERInputs& inputs) {
    if (inputs.stages.empty())
        return std::unexpected(ValuationError::InvalidInput);
    {
        __int128 lhs = static_cast<__int128>(inputs.wacc.num) * inputs.terminal_growth.den;
        __int128 rhs = static_cast<__int128>(inputs.terminal_growth.num) * inputs.wacc.den;
        if (lhs <= rhs) return std::unexpected(ValuationError::WACCLEGrowth);
    }

    // (ROIC − WACC) como Rational para escalar IC_t em cada período
    auto roic_minus_wacc = detail::r_sub(inputs.roic, inputs.wacc);
    if (!roic_minus_wacc) return std::unexpected(roic_minus_wacc.error());

    const double wacc_d = static_cast<double>(inputs.wacc.num) / inputs.wacc.den;
    const auto& desc    = inputs.base_invested_capital.description();
    const auto& rate    = inputs.base_invested_capital.rate();

    // Projeta IC_t e acumula PV(EVA_t)
    auto ic_proj = project_fcff(FCFFProjection{
        .base_fcff = inputs.base_invested_capital,
        .stages    = inputs.stages,
    });
    if (!ic_proj) return std::unexpected(ic_proj.error());

    double pv_eva = 0.0;
    for (int t = 0; t < static_cast<int>(ic_proj->size()); ++t) {
        auto eva = (*ic_proj)[t].scale(*roic_minus_wacc);
        if (!eva) return std::unexpected(ValuationError::MoneyError);
        pv_eva += detail::to_double(*eva) * std::pow(1.0 + wacc_d, -(t + 1));
    }

    // Valor terminal dos excess returns: (ROIC_stable − WACC) × IC_N / (WACC − g_stable)
    const ratmoney::Currency& ic_n = ic_proj->back();

    auto terminal_roic_minus_wacc = detail::r_sub(inputs.terminal_roic, inputs.wacc);
    auto wacc_minus_g             = detail::r_sub(inputs.wacc, inputs.terminal_growth);
    if (!terminal_roic_minus_wacc || !wacc_minus_g || wacc_minus_g->num <= 0)
        return std::unexpected(ValuationError::WACCLEGrowth);

    auto er_terminal_mul = detail::r_div(*terminal_roic_minus_wacc, *wacc_minus_g);
    if (!er_terminal_mul) return std::unexpected(er_terminal_mul.error());

    auto eva_terminal = ic_n.scale(*er_terminal_mul);
    if (!eva_terminal) return std::unexpected(ValuationError::MoneyError);

    const int n = static_cast<int>(ic_proj->size());
    pv_eva += detail::to_double(*eva_terminal) * std::pow(1.0 + wacc_d, -n);

    auto pv_er_cur = detail::make_currency_from_double(pv_eva, rate, desc);
    if (!pv_er_cur) return std::unexpected(pv_er_cur.error());

    auto firm_value = inputs.base_invested_capital.add(*pv_er_cur);
    if (!firm_value) return std::unexpected(ValuationError::MoneyError);

    return ExcessReturnsResult{
        .firm_value        = *firm_value,
        .asset_value       = inputs.base_invested_capital,
        .pv_excess_returns = *pv_er_cur,
    };
}

} // namespace ratvalue
