#pragma once

#include "types.h"
#include <expected>
#include <vector>

namespace ratvalue {

// APV = Valor da Firma Desalavancada  +  PV(Benefícios Fiscais)
//
// Abordagem Modigliani-Miller (1963) para dívida permanente:
//   PV(TS) = t × D
//
// Vantagem sobre WACC: os dois efeitos — operacional e financeiro — ficam
// separados e auditáveis.  Ideal quando a estrutura de capital muda ao longo
// do tempo (recapitalizações, LBOs), situação em que o WACC seria instável.
//
// O valor desalavancado é obtido descontando os FCFFs ao custo do capital
// próprio sem alavancagem (Ku), calculado via CAPM com β desalavancado.

struct APVInputs {
    std::vector<ratmoney::Currency> projected_fcff;
    ratmoney::Rational              terminal_growth;
    ratmoney::Rational              unlevered_cost_of_equity;  // Ku = Rf + β_u × ERP
    ratmoney::Currency              debt_market_value;          // D (valor de mercado da dívida)
    ratmoney::Rational              tax_rate;                   // t
    ratmoney::Currency              net_debt;                   // para equity = APV − net_debt
    int64_t                         shares_outstanding;
};

struct APVResult {
    ratmoney::Currency unlevered_firm_value;   // PV(FCFFs + TV @ Ku)
    ratmoney::Currency pv_tax_shield;          // t × D  (MM, dívida permanente)
    ratmoney::Currency apv;                    // unlevered + pv_tax_shield
    ratmoney::Currency equity_value;           // apv − net_debt
    ratmoney::Currency equity_value_per_share;
};

[[nodiscard]] std::expected<APVResult, ValuationError>
compute_apv(const APVInputs& inputs);

// ── APV Miles-Ezzell (1980) — dívida rebalanceada ────────────────────────────
//
// Quando a firma mantém um rácio D/V constante (rebalanceia a dívida a cada
// período), os tax shields têm o risco dos ativos → desconto a Ku, exceto o
// primeiro período (conhecido com certeza → desconto a Kd).
//
// Fórmula para perpetuidade com rebalanceamento:
//   PV(TS)_ME = t × Kd × D × (1 + Ku) / [(1 + Kd) × (Ku − g)]
//
// Resulta em menor benefício fiscal que MM, mais conservador e recomendado
// para firmas com leverage estável.

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
