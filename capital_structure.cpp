#include "capital_structure.h"
#include "beta.h"
#include "capm.h"
#include "detail.h"
#include "wacc.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace ratvalue {

std::expected<OptimalCapitalStructureResult, ValuationError>
optimal_capital_structure(const OptimalCapitalStructureInputs& inputs) {
    if (inputs.steps < 2)
        return std::unexpected(ValuationError::InvalidInput);
    if (!detail::same_currency(inputs.ebit, inputs.firm_value))
        return std::unexpected(ValuationError::CurrencyMismatch);
    if (inputs.tax_rate.num < 0 || inputs.tax_rate.num > inputs.tax_rate.den)
        return std::unexpected(ValuationError::InvalidInput);
    if (inputs.firm_value.units() <= 0)
        return std::unexpected(ValuationError::InvalidInput);

    const double ebit_d = static_cast<double>(inputs.ebit.units());
    const double fv_d   = static_cast<double>(inputs.firm_value.units());
    const double rf_d   = static_cast<double>(inputs.risk_free_rate.num)
                        / inputs.risk_free_rate.den;
    const int    N      = inputs.steps;

    std::vector<CapitalStructurePoint> schedule;
    schedule.reserve(N);

    for (int i = 0; i < N; ++i) {
        // ── d = i/N as Rational ──────────────────────────────────────────────
        auto d_rat = detail::make_rational(
            static_cast<__int128>(i), static_cast<__int128>(N));
        if (!d_rat) continue;

        // D/E = i/(N-i);  for i=0 → D/E = 0
        auto de_rat = (i == 0)
            ? detail::make_rational(0, 1)
            : detail::make_rational(static_cast<__int128>(i),
                                    static_cast<__int128>(N - i));
        if (!de_rat) continue;

        // ── β_l via Hamada ────────────────────────────────────────────────────
        auto bl = lever_beta(inputs.unlevered_beta, *de_rat, inputs.tax_rate);
        if (!bl) continue;

        // ── Ke via CAPM ───────────────────────────────────────────────────────
        auto ke = cost_of_equity(CAPMInputs{
            .risk_free_rate       = inputs.risk_free_rate,
            .beta                 = *bl,
            .equity_risk_premium  = inputs.equity_risk_premium,
            .country_risk_premium = inputs.country_risk_premium,
            .lambda               = inputs.lambda,
        });
        if (!ke) continue;

        // ── Synthetic Kd with ICR iteration ──────────────────────────────────
        SyntheticKdResult kd_res{
            .cost_of_debt      = inputs.risk_free_rate,
            .default_spread    = {0, 1},
            .rating            = CreditRating::AAA,
            .interest_coverage = std::numeric_limits<double>::infinity(),
        };

        if (i > 0) {
            double D_units = fv_d * (static_cast<double>(i) / N);
            double kd_d    = rf_d;

            for (int iter = 0; iter < 10; ++iter) {
                double interest = D_units * kd_d;
                double icr      = (interest > 0.0) ? ebit_d / interest
                                                   : std::numeric_limits<double>::infinity();
                auto r = synthetic_cost_of_debt_from_icr(
                    icr, inputs.risk_free_rate, inputs.large_firm);
                if (!r) break;

                double new_kd = static_cast<double>(r->cost_of_debt.num)
                              / r->cost_of_debt.den;
                kd_res = *r;
                if (std::abs(new_kd - kd_d) < 1e-9) break;
                kd_d = new_kd;
            }
        }

        // ── WACC = E_w×Ke + D_w×Kd×(1-t) ────────────────────────────────────
        auto ew = detail::make_rational(static_cast<__int128>(N - i),
                                        static_cast<__int128>(N));
        auto dw = detail::make_rational(static_cast<__int128>(i),
                                        static_cast<__int128>(N));
        if (!ew || !dw) continue;

        auto wacc = compute_wacc(WACCInputs{
            .equity_weight  = *ew,
            .debt_weight    = *dw,
            .cost_of_equity = *ke,
            .cost_of_debt   = kd_res.cost_of_debt,
            .tax_rate       = inputs.tax_rate,
        });
        if (!wacc) continue;

        schedule.push_back(CapitalStructurePoint{
            .debt_ratio        = *d_rat,
            .levered_beta      = *bl,
            .cost_of_equity    = *ke,
            .cost_of_debt      = kd_res.cost_of_debt,
            .rating            = kd_res.rating,
            .interest_coverage = kd_res.interest_coverage,
            .wacc              = *wacc,
        });
    }

    if (schedule.empty())
        return std::unexpected(ValuationError::InvalidInput);

    // Minimum WACC (compared as double to avoid overflow in cross-Rational comparison)
    auto opt_it = std::min_element(
        schedule.begin(), schedule.end(),
        [](const CapitalStructurePoint& a, const CapitalStructurePoint& b) {
            return static_cast<double>(a.wacc.num) / a.wacc.den
                 < static_cast<double>(b.wacc.num) / b.wacc.den;
        });

    return OptimalCapitalStructureResult{
        .schedule = std::move(schedule),
        .optimal  = *opt_it,
    };
}

} // namespace ratvalue
