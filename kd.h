#pragma once

#include "types.h"
#include <expected>
#include <string_view>

namespace ratvalue {

// Synthetic credit rating (simplified Moody's/S&P scale, 15 levels)
enum class CreditRating : uint8_t {
    AAA, AA, A_plus, A, A_minus, BBB, BB_plus, BB, B_plus, B, B_minus, CCC, CC, C, D
};

std::string_view rating_name(CreditRating r) noexcept;

// Default spread in basis points for each rating (Damodaran 2023 table)
int32_t rating_spread_bps(CreditRating r) noexcept;

// ── Synthetic Kd (Damodaran) ──────────────────────────────────────────────────
//
// Method: ICR = EBIT / Interest Expense  →  synthetic rating  →  Kd = Rf + spread
//
// Eliminates dependence on traded bonds — useful for private firms,
// emerging-market firms without local ratings, or any capital structure analysis.
//
// Two tables available (Damodaran, "Ratings, Interest Coverage and Default Spread"):
//   large_firm = true  → thresholds for large firms
//   large_firm = false → stricter thresholds for small firms

struct SyntheticKdInputs {
    ratmoney::Currency ebit;
    ratmoney::Currency interest_expense;  // net interest expense
    ratmoney::Rational risk_free_rate;
    bool               large_firm{true};
};

struct SyntheticKdResult {
    ratmoney::Rational cost_of_debt;      // Kd = Rf + spread  (as exact Rational)
    ratmoney::Rational default_spread;    // spread in isolation
    CreditRating       rating;
    double             interest_coverage; // computed ICR
};

[[nodiscard]] std::expected<SyntheticKdResult, ValuationError>
synthetic_cost_of_debt(const SyntheticKdInputs& inputs);

// Direct version: ICR supplied as double (for internal use in optimizer)
[[nodiscard]] std::expected<SyntheticKdResult, ValuationError>
synthetic_cost_of_debt_from_icr(double icr,
                                  ratmoney::Rational risk_free_rate,
                                  bool large_firm = true);

} // namespace ratvalue
