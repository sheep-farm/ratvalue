#pragma once

#include "types.h"
#include <expected>

namespace ratvalue {

// ── Justified Multiples (Damodaran, Investment Valuation ch. 17-20) ──────────
//
// Derived from the Gordon Growth Model.  These are not market prices — they are
// the multiples that would be FAIR given the firm's growth, profitability and
// cost of capital.  The gap between market and justified multiple reveals
// whether the asset is expensive, cheap, or correctly priced.
//
// Assumptions:
//   g_equity = ROE × (1 − payout)     — fundamental equity growth
//   RR_firm  = g_equity / ROIC        — reinvestment needed to sustain g
//   g_firm   = ROIC × RR = g_equity   — firms where ROE = ROIC (consistency)
//
// P/E    = payout / (Ke − g_equity)
// P/BV   = ROE × payout / (Ke − g_equity)   = ROE × P/E
// EV/EBITDA = (1−t) × (1−RR) / (WACC − g)

struct JustifiedMultiplesInputs {
    ratmoney::Rational cost_of_equity;  // Ke
    ratmoney::Rational wacc;
    ratmoney::Rational roe;             // ROE = NI / book equity
    ratmoney::Rational payout_ratio;    // dividends / NI  ∈ [0,1]
    ratmoney::Rational roic;            // ROIC = NOPAT / Invested Capital
    ratmoney::Rational tax_rate;        // t
};

struct JustifiedMultiples {
    ratmoney::Rational pe_ratio;             // justified P/E
    ratmoney::Rational pb_ratio;             // justified P/BV
    ratmoney::Rational ev_ebitda;            // justified EV/EBITDA
    ratmoney::Rational g_equity;             // implied fundamental growth
    ratmoney::Rational implied_reinvestment; // RR = g / ROIC  (for EV/EBITDA)
};

// Verifica Ke > g_equity e WACC > g_equity.
[[nodiscard]] std::expected<JustifiedMultiples, ValuationError>
compute_justified_multiples(const JustifiedMultiplesInputs& inputs);

} // namespace ratvalue
