#pragma once

#include "types.h"
#include <expected>

namespace ratvalue {

// ── Terminal Value Consistency Audit ─────────────────────────────────────────
//
// The Gordon Growth implicit in TV requires the firm to reinvest exactly
// g/ROIC_stable of each NOPAT unit. Damodaran calls this the "consistent
// reinvestment constraint" — ignoring it produces an inflated or deflated TV.

struct TerminalConsistency {
    ratmoney::Rational implied_reinvestment_rate;  // g / ROIC_stable
    ratmoney::Rational return_spread;              // ROIC_stable − WACC
    bool               value_creating;             // ROIC > WACC
    bool               reinvestment_feasible;      // 0 ≤ g/ROIC ≤ 1
};

// Does not return expected — purely informational analysis, no division by zero
// as long as terminal_roic.num ≠ 0.
TerminalConsistency check_terminal_consistency(
    ratmoney::Rational wacc,
    ratmoney::Rational terminal_growth,
    ratmoney::Rational terminal_roic);

// ── TV with Consistent ROIC (Damodaran, Investment Valuation ch. 12) ─────────
//
// TV = NOPAT_stable × (1 − g/ROIC) / (WACC − g)
//
// Difference from standard Gordon: uses NOPAT (not FCFF) as base,
// deriving stable FCFF via implied RR.  Requires ROIC > g (otherwise
// the firm would need to reinvest more than it earns to grow).

struct ConsistentTVInputs {
    ratmoney::Currency stable_nopat;     // NOPAT of terminal period (= EBIT × (1-t))
    ratmoney::Rational wacc;
    ratmoney::Rational terminal_growth;  // stable g
    ratmoney::Rational terminal_roic;    // expected ROIC in perpetuity
};

[[nodiscard]] std::expected<ratmoney::Currency, ValuationError>
consistent_terminal_value(const ConsistentTVInputs& inputs);

// ── TV by Multiple ────────────────────────────────────────────────────────────
//
// Alternative to Gordon Growth: anchors TV to an observed market multiple.
// TV = EBITDA_terminal × EV/EBITDA multiple
//
// Advantage: does not depend on g and WACC assumptions in perpetuity.
// Disadvantage: imports market inconsistencies into the model.

[[nodiscard]] std::expected<ratmoney::Currency, ValuationError>
terminal_value_by_multiple(ratmoney::Currency terminal_ebitda,
                            ratmoney::Rational ev_ebitda_multiple);

} // namespace ratvalue
