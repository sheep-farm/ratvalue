#pragma once

#include "types.h"
#include <expected>
#include <vector>

namespace ratvalue {

// Inputs para o Dividend Discount Model (DDM) multi-estágio de Damodaran
struct DDMInputs {
    ratmoney::Currency           base_dividend_per_share;  // dividendo por ação (D₀)
    std::vector<ProjectionStage> stages;                   // estágios de crescimento explícito
    ratmoney::Rational           cost_of_equity;           // Ke, taxa de desconto
    ratmoney::Rational           stable_growth;            // g terminal (perpétuo)
};

// Resultado do DDM
struct DDMResult {
    ratmoney::Currency              intrinsic_value_per_share;  // valor intrínseco
    std::vector<ratmoney::Currency> projected_dividends;        // dividendos projetados
    ratmoney::Currency              pv_dividends;               // PV dos dividendos explícitos
    ratmoney::Currency              terminal_value_per_share;   // TV (sem descontar)
    ratmoney::Currency              pv_terminal_value;          // PV do TV
};

// Gordon Growth Model multi-estágio: dividendos explícitos + TV por perpetuidade
[[nodiscard]] std::expected<DDMResult, ValuationError>
compute_ddm(const DDMInputs& inputs);

// ── H-Model (Fuller & Hsia, 1984) ────────────────────────────────────────────
//
// Fórmula fechada para crescimento que declina linearmente de g_alto → g_estável
// em 2H anos, com H = meia-vida do período de alto crescimento.
//
// P = D₀ × [(1 + g_estável) + H × (g_alto - g_estável)] / (Ke - g_estável)
//
// Resultado é exato via aritmética racional (sem loop de desconto).
struct HModelInputs {
    ratmoney::Currency base_dividend;   // D₀ — dividendo atual
    ratmoney::Rational high_growth;     // g_alto — taxa inicial (maior)
    ratmoney::Rational stable_growth;   // g_estável — taxa terminal
    ratmoney::Rational half_life;       // H — meia-vida em anos (ex: {5,1} = 5 anos)
    ratmoney::Rational cost_of_equity;  // Ke
};

struct HModelResult {
    ratmoney::Currency intrinsic_value;   // preço intrínseco
    ratmoney::Rational tv_multiplier;     // multiplicador sobre D₀ (para auditoria)
};

[[nodiscard]] std::expected<HModelResult, ValuationError>
compute_h_model(const HModelInputs& inputs);

} // namespace ratvalue
