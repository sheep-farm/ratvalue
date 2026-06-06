#pragma once

#include "types.h"
#include <expected>
#include <vector>

namespace ratvalue {

// ── Hamada Equation ───────────────────────────────────────────────────────────
//
// β_levered   = β_unlevered × [1 + (1-t) × D/E]
// β_unlevered = β_levered   / [1 + (1-t) × D/E]
//
// Rationale: financial leverage amplifies systematic risk.

// Lever: given sector β_u, compute β_l for a specific D/E structure
[[nodiscard]] std::expected<ratmoney::Rational, ValuationError>
lever_beta(ratmoney::Rational unlevered_beta,
           ratmoney::Rational debt_to_equity,
           ratmoney::Rational tax_rate);

// Unlever: given market-observed β_l, remove the effect of capital structure
[[nodiscard]] std::expected<ratmoney::Rational, ValuationError>
unlever_beta(ratmoney::Rational levered_beta,
             ratmoney::Rational debt_to_equity,
             ratmoney::Rational tax_rate);

// ── Bottom-Up Beta (Damodaran) ────────────────────────────────────────────────
//
// 1. Unlever betas of comparable firms → average β_u
// 2. Re-lever by the target firm's capital structure
//
// Advantage over historical beta: less noise, more stable, reflects current sector.

struct BottomUpBetaInputs {
    std::vector<ratmoney::Rational> comparable_levered_betas;  // market betas of comparables
    std::vector<ratmoney::Rational> comparable_debt_to_equity; // D/E of each comparable
    ratmoney::Rational              comparable_tax_rate;        // average sector tax rate
    ratmoney::Rational              target_debt_to_equity;      // D/E of the target firm
    ratmoney::Rational              target_tax_rate;            // target firm tax rate
};

// Returns β_levered for the target structure.
// Average of β_unlevered is computed in double (market data — 4 decimal places sufficient).
// Result is rounded to 4 decimal places as Rational.
[[nodiscard]] std::expected<ratmoney::Rational, ValuationError>
bottom_up_beta(const BottomUpBetaInputs& inputs);

} // namespace ratvalue
