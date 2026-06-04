#pragma once

#include "types.h"
#include <expected>

namespace ratvalue {

// ── FCFE para Instituições Financeiras (Damodaran, Ch. 22-23) ─────────────────
//
// Bancos e seguradoras diferem de firmas não-financeiras em dois aspectos:
//   1. Dívida é matéria-prima, não fonte de financiamento → sem WACC, só Ke
//   2. Reinvestimento = aumento de capital regulatório (Tier 1 / RWA)
//
// FCFE_banco = Lucro Líquido  −  ΔCapital_Regulatório
//            = NI × (1 − equity_reinvestment_rate)
//
// onde equity_reinvestment_rate = ΔCapital_Tier1 / NI
//
// Crescimento: g = ROE × equity_retention_rate  (igual ao modelo geral de equity)
// Valuation:   descontar FCFE_banco ao Ke (mesmo módulo fcfe_dcf já implementado)

// ── Abordagem 1: taxa de reinvestimento direta ────────────────────────────────

struct BankFCFEInputs {
    ratmoney::Currency net_income;
    ratmoney::Rational equity_reinvestment_rate;  // ΔCapital / NI  ∈ [0, ∞)
};

// FCFE = NI × (1 − equity_RR)
// Pode ser negativo (banco precisando de mais capital do que ganha)
[[nodiscard]] std::expected<ratmoney::Currency, ValuationError>
compute_bank_fcfe(const BankFCFEInputs& inputs);

// ── Abordagem 2: capital regulatório explícito ───────────────────────────────
//
// Deriva equity_RR da variação em RWA e do rácio de capital mínimo (Basileia III)

struct RegulatoryCapitalInputs {
    ratmoney::Currency net_income;
    ratmoney::Currency current_rwa;          // risk-weighted assets atual
    ratmoney::Currency projected_rwa;        // RWA projetado (próximo período)
    ratmoney::Rational target_tier1_ratio;   // rácio mínimo (ex: {8,100} = 8%)
};

// equity_RR = (projected_rwa − current_rwa) × target_tier1 / NI
[[nodiscard]] std::expected<ratmoney::Rational, ValuationError>
bank_equity_reinvestment_rate(const RegulatoryCapitalInputs& inputs);

} // namespace ratvalue
