#pragma once

#include "types.h"
#include <expected>

namespace ratvalue {

// ── ROIC (Return on Invested Capital) ────────────────────────────────────────

struct ROICInputs {
    ratmoney::Currency nopat;             // EBIT × (1-t) — na mesma moeda
    ratmoney::Currency invested_capital;  // patrimônio líquido contábil + dívida líquida
};

// ROIC = NOPAT / Invested Capital  (adimensional)
[[nodiscard]] std::expected<ratmoney::Rational, ValuationError>
compute_roic(const ROICInputs& inputs);

// ── Taxa de Reinvestimento ────────────────────────────────────────────────────

struct ReinvestmentRateInputs {
    ratmoney::Currency capex;
    ratmoney::Currency depreciation_amortization;
    ratmoney::Currency delta_nwc;
    ratmoney::Currency nopat;   // base para normalização
};

// RR = (CapEx - D&A + ΔCGN) / NOPAT
// Pode ser > 1 (reinvestimento acima do NOPAT) ou negativo (desinvestimento)
[[nodiscard]] std::expected<ratmoney::Rational, ValuationError>
compute_reinvestment_rate(const ReinvestmentRateInputs& inputs);

// ── Crescimento Fundamental da Firma ─────────────────────────────────────────

// g_firma = ROIC × Taxa de Reinvestimento
// Garante consistência entre crescimento assumido e capital alocado.
[[nodiscard]] std::expected<ratmoney::Rational, ValuationError>
fundamental_growth_firm(ratmoney::Rational roic,
                         ratmoney::Rational reinvestment_rate);

// ── ROE e Crescimento Fundamental do Equity ──────────────────────────────────

struct ROEInputs {
    ratmoney::Currency net_income;
    ratmoney::Currency book_equity;  // patrimônio líquido contábil
};

// ROE = Net Income / Book Equity  (adimensional)
[[nodiscard]] std::expected<ratmoney::Rational, ValuationError>
compute_roe(const ROEInputs& inputs);

// g_equity = ROE × retention_ratio
// retention_ratio = 1 - payout_ratio
[[nodiscard]] std::expected<ratmoney::Rational, ValuationError>
fundamental_growth_equity(ratmoney::Rational roe,
                           ratmoney::Rational retention_ratio);

} // namespace ratvalue
