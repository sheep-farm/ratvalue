#pragma once

#include "types.h"
#include <expected>
#include <string_view>

namespace ratvalue {

// Rating de crédito sintético (escala Moody's/S&P simplificada, 15 níveis)
enum class CreditRating : uint8_t {
    AAA, AA, A_plus, A, A_minus, BBB, BB_plus, BB, B_plus, B, B_minus, CCC, CC, C, D
};

std::string_view rating_name(CreditRating r) noexcept;

// Spread de default em basis points para cada rating (tabela Damodaran 2023)
int32_t rating_spread_bps(CreditRating r) noexcept;

// ── Kd Sintético (Damodaran) ──────────────────────────────────────────────────
//
// Método: ICR = EBIT / Despesa Financeira  →  rating sintético  →  Kd = Rf + spread
//
// Elimina a dependência de bonds negociados — útil para empresas fechadas,
// emergentes sem rating local ou qualquer análise de estrutura de capital.
//
// Duas tabelas disponíveis (Damodaran, "Ratings, Interest Coverage and Default Spread"):
//   large_firm = true  → thresholds para empresas de grande porte
//   large_firm = false → thresholds mais exigentes para pequenas empresas

struct SyntheticKdInputs {
    ratmoney::Currency ebit;
    ratmoney::Currency interest_expense;  // despesa financeira líquida
    ratmoney::Rational risk_free_rate;
    bool               large_firm{true};
};

struct SyntheticKdResult {
    ratmoney::Rational cost_of_debt;      // Kd = Rf + spread  (como Rational exato)
    ratmoney::Rational default_spread;    // spread isolado
    CreditRating       rating;
    double             interest_coverage; // ICR calculado
};

[[nodiscard]] std::expected<SyntheticKdResult, ValuationError>
synthetic_cost_of_debt(const SyntheticKdInputs& inputs);

// Versão direta: ICR fornecido como double (para uso interno no otimizador)
[[nodiscard]] std::expected<SyntheticKdResult, ValuationError>
synthetic_cost_of_debt_from_icr(double icr,
                                  ratmoney::Rational risk_free_rate,
                                  bool large_firm = true);

} // namespace ratvalue
