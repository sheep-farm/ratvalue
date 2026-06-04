#pragma once

#include "types.h"
#include <expected>

namespace ratvalue {

// ── Múltiplos Justificados (Damodaran, Investment Valuation ch. 17-20) ────────
//
// Derivam do Gordon Growth Model.  Não são preços de mercado — são os
// múltiplos que seriam JUSTOS dado o crescimento, a rentabilidade e o custo
// de capital da empresa.  A diferença entre o múltiplo de mercado e o
// justificado revela se o ativo está caro, barato ou precificado corretamente.
//
// Hipóteses:
//   g_equity = ROE × (1 − payout)     — crescimento fundamental do equity
//   RR_firma = g_equity / ROIC        — reinvestimento necessário para sustentar g
//   g_firma  = ROIC × RR = g_equity   — firmas com ROE = ROIC (consistência)
//
// P/E    = payout / (Ke − g_equity)
// P/BV   = ROE × payout / (Ke − g_equity)   = ROE × P/E
// EV/EBITDA = (1−t) × (1−RR) / (WACC − g)

struct JustifiedMultiplesInputs {
    ratmoney::Rational cost_of_equity;  // Ke
    ratmoney::Rational wacc;
    ratmoney::Rational roe;             // ROE = NI / PL contábil
    ratmoney::Rational payout_ratio;    // dividendos / NI  ∈ [0,1]
    ratmoney::Rational roic;            // ROIC = NOPAT / Capital Investido
    ratmoney::Rational tax_rate;        // t
};

struct JustifiedMultiples {
    ratmoney::Rational pe_ratio;             // P/L justificado
    ratmoney::Rational pb_ratio;             // P/VP justificado
    ratmoney::Rational ev_ebitda;            // EV/EBITDA justificado
    ratmoney::Rational g_equity;             // crescimento fundamental implícito
    ratmoney::Rational implied_reinvestment; // RR = g / ROIC  (para EV/EBITDA)
};

// Verifica Ke > g_equity e WACC > g_equity.
[[nodiscard]] std::expected<JustifiedMultiples, ValuationError>
compute_justified_multiples(const JustifiedMultiplesInputs& inputs);

} // namespace ratvalue
