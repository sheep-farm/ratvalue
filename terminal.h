#pragma once

#include "types.h"
#include <expected>

namespace ratvalue {

// ── Auditoria de consistência do Valor Terminal ───────────────────────────────
//
// O Gordon Growth implícito no TV exige que a firma reinvista exatamente
// g/ROIC_estável de cada unidade de NOPAT. Damodaran chama isso de "restrição
// de reinvestimento consistente" — ignorá-la produz TV inflado ou deflado.

struct TerminalConsistency {
    ratmoney::Rational implied_reinvestment_rate;  // g / ROIC_estável
    ratmoney::Rational return_spread;              // ROIC_estável − WACC
    bool               value_creating;             // ROIC > WACC
    bool               reinvestment_feasible;      // 0 ≤ g/ROIC ≤ 1
};

// Não retorna expected — análise puramente informativa, sem divisão por zero
// desde que terminal_roic.num ≠ 0.
TerminalConsistency check_terminal_consistency(
    ratmoney::Rational wacc,
    ratmoney::Rational terminal_growth,
    ratmoney::Rational terminal_roic);

// ── TV com ROIC consistente (Damodaran, Investment Valuation ch. 12) ──────────
//
// TV = NOPAT_estável × (1 − g/ROIC) / (WACC − g)
//
// Diferença em relação ao Gordon padrão: usa NOPAT (não FCFF) como base,
// derivando o FCFF estável pela RR implícita.  Exige que ROIC > g (caso
// contrário a firma precisaria reinvestir mais do que ganha para crescer).

struct ConsistentTVInputs {
    ratmoney::Currency stable_nopat;     // NOPAT do período terminal (= EBIT × (1-t))
    ratmoney::Rational wacc;
    ratmoney::Rational terminal_growth;  // g estável
    ratmoney::Rational terminal_roic;    // ROIC esperado na perpetuidade
};

[[nodiscard]] std::expected<ratmoney::Currency, ValuationError>
consistent_terminal_value(const ConsistentTVInputs& inputs);

// ── TV por Múltiplo ───────────────────────────────────────────────────────────
//
// Alternativa ao Gordon Growth: ancora o TV em múltiplo de mercado observado.
// TV = EBITDA_terminal × múltiplo EV/EBITDA
//
// Vantagem: não depende de hipóteses sobre g e WACC na perpetuidade.
// Desvantagem: importa inconsistências de mercado para dentro do modelo.

[[nodiscard]] std::expected<ratmoney::Currency, ValuationError>
terminal_value_by_multiple(ratmoney::Currency terminal_ebitda,
                            ratmoney::Rational ev_ebitda_multiple);

} // namespace ratvalue
