#pragma once

#include "types.h"
#include <expected>
#include <vector>

namespace ratvalue {

// Inputs for Damodaran's multi-stage Dividend Discount Model (DDM)
struct DDMInputs {
    ratmoney::Currency           base_dividend_per_share;  // dividend per share (D₀)
    std::vector<ProjectionStage> stages;                   // explicit growth stages
    ratmoney::Rational           cost_of_equity;           // Ke, discount rate
    ratmoney::Rational           stable_growth;            // terminal g (perpetual)
};

// DDM result
struct DDMResult {
    ratmoney::Currency              intrinsic_value_per_share;  // intrinsic value
    std::vector<ratmoney::Currency> projected_dividends;        // projected dividends
    ratmoney::Currency              pv_dividends;               // PV of explicit dividends
    ratmoney::Currency              terminal_value_per_share;   // TV (undiscounted)
    ratmoney::Currency              pv_terminal_value;          // PV of TV
};

// Multi-stage Gordon Growth Model: explicit dividends + TV by perpetuity
[[nodiscard]] std::expected<DDMResult, ValuationError>
compute_ddm(const DDMInputs& inputs);

// ── H-Model (Fuller & Hsia, 1984) ────────────────────────────────────────────
//
// Closed-form formula for growth declining linearly from g_high → g_stable
// over 2H years, where H = half-life of the high-growth period.
//
// P = D₀ × [(1 + g_stable) + H × (g_high - g_stable)] / (Ke - g_stable)
//
// Result is exact via rational arithmetic (no discounting loop).
struct HModelInputs {
    ratmoney::Currency base_dividend;   // D₀ — current dividend
    ratmoney::Rational high_growth;     // g_high — initial (higher) rate
    ratmoney::Rational stable_growth;   // g_stable — terminal rate
    ratmoney::Rational half_life;       // H — half-life in years (e.g. {5,1} = 5 years)
    ratmoney::Rational cost_of_equity;  // Ke
};

struct HModelResult {
    ratmoney::Currency intrinsic_value;   // intrinsic price
    ratmoney::Rational tv_multiplier;     // multiplier on D₀ (for auditing)
};

[[nodiscard]] std::expected<HModelResult, ValuationError>
compute_h_model(const HModelInputs& inputs);

} // namespace ratvalue
