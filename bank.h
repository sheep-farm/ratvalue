#pragma once

#include "types.h"
#include <expected>

namespace ratvalue {

// ── FCFE for Financial Institutions (Damodaran, Ch. 22-23) ───────────────────
//
// Banks and insurers differ from non-financial firms in two aspects:
//   1. Debt is a raw material, not a funding source → no WACC, only Ke
//   2. Reinvestment = increase in regulatory capital (Tier 1 / RWA)
//
// FCFE_bank = Net Income  −  ΔRegulatory_Capital
//           = NI × (1 − equity_reinvestment_rate)
//
// where equity_reinvestment_rate = ΔCapital_Tier1 / NI
//
// Growth:    g = ROE × equity_retention_rate  (same as general equity model)
// Valuation: discount FCFE_bank at Ke (same fcfe_dcf module already implemented)

// ── Approach 1: direct reinvestment rate ─────────────────────────────────────

struct BankFCFEInputs {
    ratmoney::Currency net_income;
    ratmoney::Rational equity_reinvestment_rate;  // ΔCapital / NI  ∈ [0, ∞)
};

// FCFE = NI × (1 − equity_RR)
// Can be negative (bank needing more capital than it earns)
[[nodiscard]] std::expected<ratmoney::Currency, ValuationError>
compute_bank_fcfe(const BankFCFEInputs& inputs);

// ── Approach 2: explicit regulatory capital ──────────────────────────────────
//
// Derives equity_RR from the change in RWA and minimum capital ratio (Basel III)

struct RegulatoryCapitalInputs {
    ratmoney::Currency net_income;
    ratmoney::Currency current_rwa;          // current risk-weighted assets
    ratmoney::Currency projected_rwa;        // projected RWA (next period)
    ratmoney::Rational target_tier1_ratio;   // minimum ratio (e.g. {8,100} = 8%)
};

// equity_RR = (projected_rwa − current_rwa) × target_tier1 / NI
[[nodiscard]] std::expected<ratmoney::Rational, ValuationError>
bank_equity_reinvestment_rate(const RegulatoryCapitalInputs& inputs);

} // namespace ratvalue
