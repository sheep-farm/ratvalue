#include "apv.h"
#include "detail.h"
#include <cmath>

namespace ratvalue {

std::expected<APVResult, ValuationError>
compute_apv(const APVInputs& inputs) {
    if (inputs.projected_fcff.empty() || inputs.shares_outstanding <= 0)
        return std::unexpected(ValuationError::InvalidInput);
    if (inputs.unlevered_cost_of_equity.num <= 0)
        return std::unexpected(ValuationError::InvalidInput);
    if (inputs.tax_rate.num < 0 || inputs.tax_rate.num > inputs.tax_rate.den)
        return std::unexpected(ValuationError::InvalidInput);

    const auto& fcff = inputs.projected_fcff;

    for (const auto& f : fcff)
        if (!detail::same_currency(f, fcff.front()))
            return std::unexpected(ValuationError::CurrencyMismatch);
    if (!detail::same_currency(inputs.net_debt, fcff.front())
     || !detail::same_currency(inputs.debt_market_value, fcff.front()))
        return std::unexpected(ValuationError::CurrencyMismatch);

    // Ku > g
    {
        __int128 lhs = static_cast<__int128>(inputs.unlevered_cost_of_equity.num)
                     * inputs.terminal_growth.den;
        __int128 rhs = static_cast<__int128>(inputs.terminal_growth.num)
                     * inputs.unlevered_cost_of_equity.den;
        if (lhs <= rhs) return std::unexpected(ValuationError::WACCLEGrowth);
    }

    const auto& desc = fcff.front().description();
    const auto& rate = fcff.front().rate();
    const int   n    = static_cast<int>(fcff.size());
    const double ku_d = static_cast<double>(inputs.unlevered_cost_of_equity.num)
                      / inputs.unlevered_cost_of_equity.den;

    // Valor desalavancado: PV(FCFFs @ Ku) + PV(TV @ Ku)
    double pv_sum = 0.0;
    for (int t = 0; t < n; ++t)
        pv_sum += detail::to_double(fcff[t]) * std::pow(1.0 + ku_d, -(t + 1));

    // TV = FCFF_N × (1+g) / (Ku − g)   — multiplicador Rational exato
    const auto& g  = inputs.terminal_growth;
    const auto& ku = inputs.unlevered_cost_of_equity;

    __int128 tv_mul_n = (static_cast<__int128>(g.den) + g.num) * ku.den;
    __int128 tv_mul_d = static_cast<__int128>(ku.num) * g.den
                      - static_cast<__int128>(g.num) * ku.den;

    auto tv_mul = detail::make_rational(tv_mul_n, tv_mul_d);
    if (!tv_mul) return std::unexpected(tv_mul.error());

    auto tv = fcff.back().scale(*tv_mul);
    if (!tv) return std::unexpected(ValuationError::MoneyError);

    pv_sum += detail::to_double(*tv) * std::pow(1.0 + ku_d, -static_cast<double>(n));

    auto unlevered = detail::make_currency_from_double(pv_sum, rate, desc);
    if (!unlevered) return std::unexpected(unlevered.error());

    // PV(Tax Shield) = t × D   (Modigliani-Miller, dívida permanente)
    // Cálculo exato via Currency::scale — nenhuma conversão para double.
    auto pv_ts = inputs.debt_market_value.scale(inputs.tax_rate);
    if (!pv_ts) return std::unexpected(ValuationError::MoneyError);

    // APV = unlevered + PV(TS)
    auto apv_val = unlevered->add(*pv_ts);
    if (!apv_val) return std::unexpected(ValuationError::MoneyError);

    // Equity = APV − dívida líquida
    auto equity = apv_val->subtract(inputs.net_debt);
    if (!equity) return std::unexpected(ValuationError::MoneyError);

    auto per_share = equity->scale(ratmoney::Rational{1, inputs.shares_outstanding});
    if (!per_share) return std::unexpected(ValuationError::MoneyError);

    return APVResult{
        .unlevered_firm_value    = *unlevered,
        .pv_tax_shield           = *pv_ts,
        .apv                     = *apv_val,
        .equity_value            = *equity,
        .equity_value_per_share  = *per_share,
    };
}

// ── Miles-Ezzell: PV(TS) = t × Kd × D × (1+Ku) / [(1+Kd)(Ku−g)] ────────────

std::expected<APVResult, ValuationError>
compute_apv_miles_ezzell(const APVMilesEzzellInputs& inputs) {
    if (inputs.projected_fcff.empty() || inputs.shares_outstanding <= 0)
        return std::unexpected(ValuationError::InvalidInput);
    if (inputs.unlevered_cost_of_equity.num <= 0 || inputs.cost_of_debt.num <= 0)
        return std::unexpected(ValuationError::InvalidInput);
    if (inputs.tax_rate.num < 0 || inputs.tax_rate.num > inputs.tax_rate.den)
        return std::unexpected(ValuationError::InvalidInput);

    const auto& fcff = inputs.projected_fcff;
    for (const auto& f : fcff)
        if (!detail::same_currency(f, fcff.front()))
            return std::unexpected(ValuationError::CurrencyMismatch);
    if (!detail::same_currency(inputs.net_debt, fcff.front())
     || !detail::same_currency(inputs.debt_market_value, fcff.front()))
        return std::unexpected(ValuationError::CurrencyMismatch);

    // Ku > g
    {
        __int128 lhs = static_cast<__int128>(inputs.unlevered_cost_of_equity.num)
                     * inputs.terminal_growth.den;
        __int128 rhs = static_cast<__int128>(inputs.terminal_growth.num)
                     * inputs.unlevered_cost_of_equity.den;
        if (lhs <= rhs) return std::unexpected(ValuationError::WACCLEGrowth);
    }

    const auto& desc = fcff.front().description();
    const auto& rate = fcff.front().rate();
    const int   n    = static_cast<int>(fcff.size());
    const double ku_d = static_cast<double>(inputs.unlevered_cost_of_equity.num)
                      / inputs.unlevered_cost_of_equity.den;

    // Valor desalavancado: idêntico ao MM
    double pv_sum = 0.0;
    for (int t = 0; t < n; ++t)
        pv_sum += detail::to_double(fcff[t]) * std::pow(1.0 + ku_d, -(t + 1));

    const auto& g  = inputs.terminal_growth;
    const auto& ku = inputs.unlevered_cost_of_equity;
    __int128 tv_mul_n = (static_cast<__int128>(g.den) + g.num) * ku.den;
    __int128 tv_mul_d = static_cast<__int128>(ku.num) * g.den
                      - static_cast<__int128>(g.num) * ku.den;
    auto tv_mul = detail::make_rational(tv_mul_n, tv_mul_d);
    if (!tv_mul) return std::unexpected(tv_mul.error());
    auto tv = fcff.back().scale(*tv_mul);
    if (!tv) return std::unexpected(ValuationError::MoneyError);
    pv_sum += detail::to_double(*tv) * std::pow(1.0 + ku_d, -static_cast<double>(n));

    auto unlevered = detail::make_currency_from_double(pv_sum, rate, desc);
    if (!unlevered) return std::unexpected(unlevered.error());

    // PV(TS)_ME = t × Kd × D × (1+Ku) / [(1+Kd) × (Ku−g)]
    // Multiplicador como Rational exato:
    //   num = t.num × Kd.num × (Ku.den + Ku.num) × Kd.den  [= t × Kd × (1+Ku)]
    //   den = t.den × Kd.den × (Ku.den + Kd.num) × (Ku.num×g.den − g.num×Ku.den)
    //       = t.den × (1+Kd).den × (1+Kd).num_denom × (Ku−g)_denom
    // Mais claro passo a passo:

    const auto& kd  = inputs.cost_of_debt;

    // t × Kd
    auto t_kd = detail::r_mul(inputs.tax_rate, kd);
    if (!t_kd) return std::unexpected(t_kd.error());

    // (1 + Ku)
    auto one_plus_ku = detail::one_plus(ku);
    if (!one_plus_ku) return std::unexpected(one_plus_ku.error());

    // (1 + Kd)
    auto one_plus_kd = detail::one_plus(kd);
    if (!one_plus_kd) return std::unexpected(one_plus_kd.error());

    // (Ku − g)
    auto ku_minus_g = detail::r_sub(ku, g);
    if (!ku_minus_g || ku_minus_g->num <= 0)
        return std::unexpected(ValuationError::WACCLEGrowth);

    // multiplicador = t×Kd × (1+Ku) / [(1+Kd) × (Ku−g)]
    auto numer = detail::r_mul(*t_kd, *one_plus_ku);
    if (!numer) return std::unexpected(numer.error());

    auto denom = detail::r_mul(*one_plus_kd, *ku_minus_g);
    if (!denom) return std::unexpected(denom.error());

    auto ts_mul = detail::r_div(*numer, *denom);
    if (!ts_mul) return std::unexpected(ts_mul.error());

    auto pv_ts = inputs.debt_market_value.scale(*ts_mul);
    if (!pv_ts) return std::unexpected(ValuationError::MoneyError);

    auto apv_val = unlevered->add(*pv_ts);
    if (!apv_val) return std::unexpected(ValuationError::MoneyError);

    auto equity = apv_val->subtract(inputs.net_debt);
    if (!equity) return std::unexpected(ValuationError::MoneyError);

    auto per_share = equity->scale(ratmoney::Rational{1, inputs.shares_outstanding});
    if (!per_share) return std::unexpected(ValuationError::MoneyError);

    return APVResult{
        .unlevered_firm_value    = *unlevered,
        .pv_tax_shield           = *pv_ts,
        .apv                     = *apv_val,
        .equity_value            = *equity,
        .equity_value_per_share  = *per_share,
    };
}

} // namespace ratvalue
