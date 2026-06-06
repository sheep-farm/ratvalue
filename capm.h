#pragma once

#include "types.h"
#include <expected>

namespace ratvalue {

// Inputs for the extended CAPM model for emerging markets (Damodaran)
//
// Formula: Ke = Rf + β × ERP_mature + λ × CRP
//
//   ERP_mature  — equity risk premium of the reference market (e.g. USA)
//   CRP         — country risk premium (default 0 = developed market)
//   lambda (λ)  — company's exposure to country risk (default 1 = full)
//
// For developed markets: set country_risk_premium = {0,1}.
// For emerging markets (Brazil): CRP ≈ sovereign spread × (σ_equity / σ_bond).
struct CAPMInputs {
    ratmoney::Rational risk_free_rate;
    ratmoney::Rational beta;
    ratmoney::Rational equity_risk_premium;                  // ERP of the mature market
    ratmoney::Rational country_risk_premium{0, 1};           // CRP (default: no country risk)
    ratmoney::Rational lambda{1, 1};                         // country risk exposure
};

// Ke = Rf + β × ERP_mature + λ × CRP
[[nodiscard]] std::expected<ratmoney::Rational, ValuationError>
cost_of_equity(const CAPMInputs& inputs);

} // namespace ratvalue
