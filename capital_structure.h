#pragma once

#include "kd.h"
#include "types.h"
#include <expected>
#include <vector>

namespace ratvalue {

// Ponto na curva WACC × estrutura de capital
struct CapitalStructurePoint {
    ratmoney::Rational debt_ratio;         // D/(D+E)
    ratmoney::Rational levered_beta;       // β_l = β_u × [1+(1-t)×D/E]
    ratmoney::Rational cost_of_equity;     // Ke (via CAPM)
    ratmoney::Rational cost_of_debt;       // Kd sintético (via ICR iterado)
    CreditRating       rating;             // rating derivado do Kd iterado
    double             interest_coverage;  // ICR = EBIT / (D × Kd)
    ratmoney::Rational wacc;               // WACC neste ponto
};

// ── Estrutura de Capital Ótima (Damodaran) ────────────────────────────────────
//
// Algoritmo:
//   Para cada razão de endividamento d ∈ {0/N, 1/N, ..., (N-1)/N}:
//     1. β_l = β_u × [1 + (1-t) × d/(1-d)]          (Hamada)
//     2. Ke   = Rf + β_l × ERP + λ × CRP              (CAPM)
//     3. Kd   ← iterar ICR = EBIT/(d × FV × Kd) até convergência  (Kd sintético)
//     4. WACC = (1-d) × Ke + d × Kd × (1-t)
//
//   O mínimo de WACC maximiza o valor da firma.
//
// Kd sintético é iterado porque há circularidade:
//   Kd depende do ICR, que depende da despesa de juros, que depende de Kd.
//   A iteração converge em 2-5 passos para todas as tabelas Damodaran.

struct OptimalCapitalStructureInputs {
    ratmoney::Rational unlevered_beta;
    ratmoney::Rational risk_free_rate;
    ratmoney::Rational equity_risk_premium;
    ratmoney::Rational country_risk_premium{0, 1};  // CRP (default 0)
    ratmoney::Rational lambda{1, 1};                // exposição ao risco-país
    ratmoney::Rational tax_rate;
    ratmoney::Currency ebit;        // EBIT para calcular ICR a cada nível de dívida
    ratmoney::Currency firm_value;  // E+D — base para D = d × firm_value
    int                steps{10};   // pontos avaliados: d = 0/steps, 1/steps, ..., (steps-1)/steps
    bool               large_firm{true};
};

struct OptimalCapitalStructureResult {
    std::vector<CapitalStructurePoint> schedule;  // todos os pontos (tamanho = steps)
    CapitalStructurePoint              optimal;   // ponto de WACC mínimo
};

[[nodiscard]] std::expected<OptimalCapitalStructureResult, ValuationError>
optimal_capital_structure(const OptimalCapitalStructureInputs& inputs);

} // namespace ratvalue
