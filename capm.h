#pragma once

#include "types.h"
#include <expected>

namespace ratvalue {

// Inputs do modelo CAPM estendido para mercados emergentes (Damodaran)
//
// Fórmula: Ke = Rf + β × ERP_maduro + λ × CRP
//
//   ERP_maduro  — prêmio de risco do mercado de referência (ex: EUA)
//   CRP         — prêmio de risco-país (default 0 = mercado desenvolvido)
//   lambda (λ)  — exposição da empresa ao risco-país (default 1 = integral)
//
// Para mercado desenvolvido: deixar country_risk_premium = {0,1}.
// Para mercado emergente (Brasil): CRP ≈ spread soberano × (σ_ações / σ_bônus).
struct CAPMInputs {
    ratmoney::Rational risk_free_rate;
    ratmoney::Rational beta;
    ratmoney::Rational equity_risk_premium;                  // ERP do mercado maduro
    ratmoney::Rational country_risk_premium{0, 1};           // CRP (padrão: sem risco-país)
    ratmoney::Rational lambda{1, 1};                         // exposição ao risco-país
};

// Ke = Rf + β × ERP_maduro + λ × CRP
[[nodiscard]] std::expected<ratmoney::Rational, ValuationError>
cost_of_equity(const CAPMInputs& inputs);

} // namespace ratvalue
