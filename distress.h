#pragma once

// Does not use expected — Black-Scholes is well-defined for any positive input.

namespace ratvalue {

// ── Equity as a Call Option — Merton Model (1974) ────────────────────────────
//
// In highly leveraged firms, equity acts as a call option
// on asset value, with strike = face value of debt.
//
//   E = V × N(d1) − D × e^(−rT) × N(d2)
//   P(default) = N(−d2)   (risk-neutral probability of insolvency)
//
//   d1 = [ln(V/D) + (r + σ²/2)T] / (σ√T)
//   d2 = d1 − σ√T
//
// Inputs: V and σ_V are ASSET values (not directly observable).
// To derive them from market data (observable E + σ_E):
//   solve the system {E=BS(V,σ_V); σ_E×E = N(d1)×σ_V×V} iteratively.
// This second step is not implemented here — the caller supplies V and σ_V.

struct EquityAsOptionInputs {
    double firm_value;         // V — estimated market value of assets
    double debt_face_value;    // D — total face value of debt
    double asset_volatility;   // σ_V — annual asset volatility
    double risk_free_rate;     // r — continuous risk-free rate
    double debt_maturity;      // T — average debt maturity (years)
};

struct EquityAsOptionResult {
    double equity_value;          // E = call value
    double debt_market_value;     // D_mkt = V − E
    double probability_default;   // N(−d2) — risk-neutral probability of default
    double distance_to_default;   // d2 (distance to default, larger = safer)
    double d1;
};

EquityAsOptionResult equity_as_option(const EquityAsOptionInputs& inputs) noexcept;

} // namespace ratvalue
