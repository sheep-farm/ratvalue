#pragma once

#include "types.h"
#include <expected>

namespace ratvalue {

// ── ROIC (Return on Invested Capital) ────────────────────────────────────────

struct ROICInputs {
    ratmoney::Currency nopat;             // EBIT × (1-t) — in the same currency
    ratmoney::Currency invested_capital;  // book equity + net debt
};

// ROIC = NOPAT / Invested Capital  (dimensionless)
[[nodiscard]] std::expected<ratmoney::Rational, ValuationError>
compute_roic(const ROICInputs& inputs);

// ── Reinvestment Rate ─────────────────────────────────────────────────────────

struct ReinvestmentRateInputs {
    ratmoney::Currency capex;
    ratmoney::Currency depreciation_amortization;
    ratmoney::Currency delta_nwc;
    ratmoney::Currency nopat;   // normalization base
};

// RR = (CapEx - D&A + ΔNWC) / NOPAT
// Can be > 1 (reinvestment above NOPAT) or negative (disinvestment)
[[nodiscard]] std::expected<ratmoney::Rational, ValuationError>
compute_reinvestment_rate(const ReinvestmentRateInputs& inputs);

// ── Fundamental Growth of the Firm ───────────────────────────────────────────

// g_firm = ROIC × Reinvestment Rate
// Ensures consistency between assumed growth and allocated capital.
[[nodiscard]] std::expected<ratmoney::Rational, ValuationError>
fundamental_growth_firm(ratmoney::Rational roic,
                         ratmoney::Rational reinvestment_rate);

// ── ROE and Fundamental Growth of Equity ─────────────────────────────────────

struct ROEInputs {
    ratmoney::Currency net_income;
    ratmoney::Currency book_equity;  // book equity
};

// ROE = Net Income / Book Equity  (adimensional)
[[nodiscard]] std::expected<ratmoney::Rational, ValuationError>
compute_roe(const ROEInputs& inputs);

// g_equity = ROE × retention_ratio
// retention_ratio = 1 - payout_ratio
[[nodiscard]] std::expected<ratmoney::Rational, ValuationError>
fundamental_growth_equity(ratmoney::Rational roe,
                           ratmoney::Rational retention_ratio);

} // namespace ratvalue
