#pragma once

#include "types.h"
#include <expected>
#include <vector>

namespace ratvalue {

// Inputs for FCFE (Free Cash Flow to Equity)
//
// FCFE = NI - (1-δ)(CapEx - D&A + ΔNWC) + Net New Debt
//
//   δ = debt_ratio — fraction of reinvestment financed by new debt
//   Net New Debt = issuances - repayments in the period
struct FCFEInputs {
    ratmoney::Currency net_income;
    ratmoney::Rational debt_ratio;                    // δ = D/(D+E)
    ratmoney::Currency capex;
    ratmoney::Currency depreciation_amortization;
    ratmoney::Currency delta_nwc;
    ratmoney::Currency net_new_debt;                  // can be negative (net repayment)
};

[[nodiscard]] std::expected<ratmoney::Currency, ValuationError>
compute_fcfe(const FCFEInputs& inputs);

// ── Equity DCF (discounts FCFE at cost of equity Ke) ─────────────────────────

struct FCFEDCFInputs {
    std::vector<ratmoney::Currency> projected_fcfe;    // projected FCFEs (years 1..N)
    ratmoney::Rational              cost_of_equity;    // Ke — discount rate
    ratmoney::Rational              terminal_growth;   // perpetual terminal growth
    int64_t                         shares_outstanding;
};

struct FCFEDCFResult {
    ratmoney::Currency equity_value;             // PV(FCFEs) + PV(TV)
    ratmoney::Currency equity_value_per_share;
    ratmoney::Currency pv_fcfe;
    ratmoney::Currency terminal_value;           // TV (undiscounted)
    ratmoney::Currency pv_terminal_value;
};

// Equity value directly — no net debt subtraction.
[[nodiscard]] std::expected<FCFEDCFResult, ValuationError>
compute_fcfe_dcf(const FCFEDCFInputs& inputs);

} // namespace ratvalue
