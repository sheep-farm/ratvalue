#pragma once

#include "types.h"
#include <expected>
#include <vector>

namespace ratvalue {

// ── Equação de Hamada ─────────────────────────────────────────────────────────
//
// β_alavancado = β_desalavancado × [1 + (1-t) × D/E]
// β_desalavancado = β_alavancado / [1 + (1-t) × D/E]
//
// Fundamenta: a alavancagem financeira amplifica o risco sistemático.

// Alavancar: dado β_u do setor, calcular β_l para uma estrutura D/E específica
[[nodiscard]] std::expected<ratmoney::Rational, ValuationError>
lever_beta(ratmoney::Rational unlevered_beta,
           ratmoney::Rational debt_to_equity,
           ratmoney::Rational tax_rate);

// Desalavancar: dado β_l observado no mercado, remover o efeito da estrutura de capital
[[nodiscard]] std::expected<ratmoney::Rational, ValuationError>
unlever_beta(ratmoney::Rational levered_beta,
             ratmoney::Rational debt_to_equity,
             ratmoney::Rational tax_rate);

// ── Beta Bottom-Up (Damodaran) ────────────────────────────────────────────────
//
// 1. Desalavancar os betas de empresas comparáveis → média de β_u
// 2. Re-alavancar pela estrutura de capital da empresa-alvo
//
// Vantagem sobre beta histórico: menos ruído, mais estável, reflete o setor atual.

struct BottomUpBetaInputs {
    std::vector<ratmoney::Rational> comparable_levered_betas;  // betas de mercado dos comparáveis
    std::vector<ratmoney::Rational> comparable_debt_to_equity; // D/E de cada comparável
    ratmoney::Rational              comparable_tax_rate;        // alíquota média do setor
    ratmoney::Rational              target_debt_to_equity;      // D/E da empresa-alvo
    ratmoney::Rational              target_tax_rate;            // alíquota da empresa-alvo
};

// Retorna β_alavancado para a estrutura-alvo.
// A média dos β_desalavancados é computada em double (dados de mercado, sem precisão exata).
// O resultado é aproximado para 4 casas decimais como Rational.
[[nodiscard]] std::expected<ratmoney::Rational, ValuationError>
bottom_up_beta(const BottomUpBetaInputs& inputs);

} // namespace ratvalue
