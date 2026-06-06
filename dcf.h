#pragma once

#include "types.h"
#include <expected>
#include <vector>

namespace ratvalue {

// Inputs for Damodaran's DCF (Discounted Cash Flow) model
struct DCFInputs {
    std::vector<ratmoney::Currency> projected_fcff;   // projected FCFFs (years 1..N)
    ratmoney::Rational              wacc;              // discount rate
    ratmoney::Rational              terminal_growth;   // stable perpetual growth
    ratmoney::Currency              net_debt;          // net debt (EV - net_debt = Equity)
    int64_t                         shares_outstanding; // shares outstanding
};

// DCF model result
struct DCFResult {
    ratmoney::Currency enterprise_value;        // EV = PV(FCFFs) + PV(TV)
    ratmoney::Currency equity_value;            // EV - net debt
    ratmoney::Currency equity_value_per_share;  // equity / shares
    ratmoney::Currency pv_fcff;                 // sum of PV(explicit FCFFs)
    ratmoney::Currency terminal_value;          // terminal value (undiscounted)
    ratmoney::Currency pv_terminal_value;       // PV of terminal value
};

// Computes DCF with two stages:
//   – explicit period: projected FCFFs discounted
//   – perpetuity: terminal value via Gordon Growth (FCFF_N × (1+g)/(WACC-g))
[[nodiscard]] std::expected<DCFResult, ValuationError>
compute_dcf(const DCFInputs& inputs);

} // namespace ratvalue
