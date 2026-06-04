#pragma once

// Não usa expected — Black-Scholes é bem definido para qualquer input positivo.

namespace ratvalue {

// ── Equity como Opção de Compra — Modelo de Merton (1974) ────────────────────
//
// Em firmas com alto grau de alavancagem, o equity age como uma call option
// sobre o valor dos ativos, com strike = valor de face da dívida.
//
//   E = V × N(d1) − D × e^(−rT) × N(d2)
//   P(default) = N(−d2)   (probabilidade risk-neutral de insolvência)
//
//   d1 = [ln(V/D) + (r + σ²/2)T] / (σ√T)
//   d2 = d1 − σ√T
//
// Inputs: V e σ_V são os ATIVOS (não observáveis diretamente).
// Para derivá-los de mercado (E observável + σ_E):
//   resolver o sistema {E=BS(V,σ_V); σ_E×E = N(d1)×σ_V×V} iterativamente.
// Essa segunda etapa não é implementada aqui — o usuário fornece V e σ_V.

struct EquityAsOptionInputs {
    double firm_value;         // V — valor de mercado dos ativos (estimado)
    double debt_face_value;    // D — valor de face total da dívida
    double asset_volatility;   // σ_V — volatilidade anual dos ativos
    double risk_free_rate;     // r — taxa livre de risco contínua
    double debt_maturity;      // T — vencimento médio da dívida (anos)
};

struct EquityAsOptionResult {
    double equity_value;          // E = call value
    double debt_market_value;     // D_mkt = V − E
    double probability_default;   // N(−d2) — probabilidade risk-neutral de default
    double distance_to_default;   // d2 (distância ao default, maior = mais seguro)
    double d1;
};

EquityAsOptionResult equity_as_option(const EquityAsOptionInputs& inputs) noexcept;

} // namespace ratvalue
