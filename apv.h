#pragma once

#include "types.h"
#include <expected>
#include <vector>

namespace ratvalue {

// APV = Unlevered Firm Value  +  PV(Tax Shields)
//
// Modigliani-Miller (1963) approach for permanent debt:
//   PV(TS) = t × D
//
// Advantage over WACC: the two effects — operational and financial — are
// separated and auditable.  Ideal when the capital structure changes over
// time (recapitalizations, LBOs), where WACC would be unstable.
//
// Unlevered value is obtained by discounting FCFFs at the unlevered cost of
// equity (Ku), computed via CAPM with unlevered beta.

struct APVInputs {
    std::vector<ratmoney::Currency> projected_fcff;
    ratmoney::Rational              terminal_growth;
    ratmoney::Rational              unlevered_cost_of_equity;  // Ku = Rf + β_u × ERP
    ratmoney::Currency              debt_market_value;          // D (market value of debt)
    ratmoney::Rational              tax_rate;                   // t
    ratmoney::Currency              net_debt;                   // for equity = APV − net_debt
    int64_t                         shares_outstanding;
};

struct APVResult {
    ratmoney::Currency unlevered_firm_value;   // PV(FCFFs + TV @ Ku)
    ratmoney::Currency pv_tax_shield;          // t × D  (MM, permanent debt)
    ratmoney::Currency apv;                    // unlevered + pv_tax_shield
    ratmoney::Currency equity_value;           // apv − net_debt
    ratmoney::Currency equity_value_per_share;
};

[[nodiscard]] std::expected<APVResult, ValuationError>
compute_apv(const APVInputs& inputs);

// ── APV Miles-Ezzell (1980) — rebalanced debt ────────────────────────────────
//
// When the firm maintains a constant D/V ratio (rebalances debt each period),
// tax shields have asset risk → discounted at Ku, except the first period
// (known with certainty → discounted at Kd).
//
// Formula for perpetuity with rebalancing:
//   PV(TS)_ME = t × Kd × D × (1 + Ku) / [(1 + Kd) × (Ku − g)]
//
// Results in lower tax benefit than MM, more conservative and recommended
// for firms with stable leverage.

struct APVMilesEzzellInputs {
    std::vector<ratmoney::Currency> projected_fcff;
    ratmoney::Rational              terminal_growth;
    ratmoney::Rational              unlevered_cost_of_equity;  // Ku
    ratmoney::Rational              cost_of_debt;              // Kd
    ratmoney::Currency              debt_market_value;
    ratmoney::Rational              tax_rate;
    ratmoney::Currency              net_debt;
    int64_t                         shares_outstanding;
};

[[nodiscard]] std::expected<APVResult, ValuationError>
compute_apv_miles_ezzell(const APVMilesEzzellInputs& inputs);

} // namespace ratvalue
