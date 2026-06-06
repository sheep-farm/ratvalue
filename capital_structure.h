#pragma once

#include "kd.h"
#include "types.h"
#include <expected>
#include <vector>

namespace ratvalue {

// Point on the WACC × capital structure curve
struct CapitalStructurePoint {
    ratmoney::Rational debt_ratio;         // D/(D+E)
    ratmoney::Rational levered_beta;       // β_l = β_u × [1+(1-t)×D/E]
    ratmoney::Rational cost_of_equity;     // Ke (via CAPM)
    ratmoney::Rational cost_of_debt;       // synthetic Kd (via iterated ICR)
    CreditRating       rating;             // rating derived from iterated Kd
    double             interest_coverage;  // ICR = EBIT / (D × Kd)
    ratmoney::Rational wacc;               // WACC at this point
};

// ── Optimal Capital Structure (Damodaran) ─────────────────────────────────────
//
// Algorithm:
//   For each debt ratio d ∈ {0/N, 1/N, ..., (N-1)/N}:
//     1. β_l = β_u × [1 + (1-t) × d/(1-d)]          (Hamada)
//     2. Ke   = Rf + β_l × ERP + λ × CRP              (CAPM)
//     3. Kd   ← iterate ICR = EBIT/(d × FV × Kd) until convergence  (synthetic Kd)
//     4. WACC = (1-d) × Ke + d × Kd × (1-t)
//
//   The minimum WACC maximizes firm value.
//
// Synthetic Kd is iterated because of circularity:
//   Kd depends on ICR, which depends on interest expense, which depends on Kd.
//   Iteration converges in 2-5 steps for all Damodaran tables.

struct OptimalCapitalStructureInputs {
    ratmoney::Rational unlevered_beta;
    ratmoney::Rational risk_free_rate;
    ratmoney::Rational equity_risk_premium;
    ratmoney::Rational country_risk_premium{0, 1};  // CRP (default 0)
    ratmoney::Rational lambda{1, 1};                // country risk exposure
    ratmoney::Rational tax_rate;
    ratmoney::Currency ebit;        // EBIT to compute ICR at each debt level
    ratmoney::Currency firm_value;  // E+D — base for D = d × firm_value
    int                steps{10};   // evaluated points: d = 0/steps, 1/steps, ..., (steps-1)/steps
    bool               large_firm{true};
};

struct OptimalCapitalStructureResult {
    std::vector<CapitalStructurePoint> schedule;  // all points (size = steps)
    CapitalStructurePoint              optimal;   // minimum WACC point
};

[[nodiscard]] std::expected<OptimalCapitalStructureResult, ValuationError>
optimal_capital_structure(const OptimalCapitalStructureInputs& inputs);

} // namespace ratvalue
