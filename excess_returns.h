#pragma once

#include "types.h"
#include <expected>
#include <vector>

namespace ratvalue {

// ── Modelo de Excess Returns / EVA (Damodaran) ────────────────────────────────
//
// Decompõe o valor da firma em:
//   1. Capital investido existente (valor dos ativos em operação)
//   2. PV dos retornos em excesso sobre o WACC (onde vem o crescimento de valor)
//
// V = IC + PV(EVA_t)   onde   EVA_t = (ROIC_t − WACC) × IC_t
//
// Vantagem sobre DCF puro: torna explícito QUANDO e POR QUANTO a firma cria
// (ROIC > WACC) ou destrói (ROIC < WACC) valor.  Matematicamente equivalente
// ao DCF quando os inputs são consistentes.

struct ExcessReturnsResult {
    ratmoney::Currency firm_value;         // IC + PV(EVA)
    ratmoney::Currency asset_value;        // IC (valor dos ativos atuais = "chão" do valor)
    ratmoney::Currency pv_excess_returns;  // PV(EVA) — positivo se ROIC > WACC
};

// ── Versão monoestágio (fórmula fechada, aritmética racional exata) ───────────
//
// V = IC × (ROIC − g) / (WACC − g)
// PV(EVA) = IC × (ROIC − WACC) / (WACC − g)

struct SingleStageERInputs {
    ratmoney::Currency invested_capital;
    ratmoney::Rational roic;
    ratmoney::Rational wacc;
    ratmoney::Rational terminal_growth;
};

[[nodiscard]] std::expected<ExcessReturnsResult, ValuationError>
excess_returns_value(const SingleStageERInputs& inputs);

// ── Versão multiestágio ───────────────────────────────────────────────────────
//
// Projeta IC_t = IC_0 × ∏(1+g_estágio) e computa EVA_t = (ROIC−WACC)×IC_t.
// Desconto via double (potências — mesma razão do DCF).

struct MultiStageERInputs {
    ratmoney::Currency           base_invested_capital;
    ratmoney::Rational           roic;             // ROIC durante o período explícito
    std::vector<ProjectionStage> stages;           // crescimento do IC por estágio
    ratmoney::Rational           wacc;
    ratmoney::Rational           terminal_roic;    // ROIC na perpetuidade
    ratmoney::Rational           terminal_growth;
};

[[nodiscard]] std::expected<ExcessReturnsResult, ValuationError>
excess_returns_multistage(const MultiStageERInputs& inputs);

} // namespace ratvalue
