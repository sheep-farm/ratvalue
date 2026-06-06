#pragma once

#include "types.h"
#include <expected>
#include <vector>

namespace ratvalue {

// ── Excess Returns / EVA Model (Damodaran) ───────────────────────────────────
//
// Decomposes firm value into:
//   1. Existing invested capital (value of assets in operation)
//   2. PV of returns in excess of WACC (where value growth comes from)
//
// V = IC + PV(EVA_t)   where   EVA_t = (ROIC_t − WACC) × IC_t
//
// Advantage over pure DCF: makes explicit WHEN and BY HOW MUCH the firm creates
// (ROIC > WACC) or destroys (ROIC < WACC) value.  Mathematically equivalent
// to DCF when inputs are consistent.

struct ExcessReturnsResult {
    ratmoney::Currency firm_value;         // IC + PV(EVA)
    ratmoney::Currency asset_value;        // IC (value of current assets = "floor" of value)
    ratmoney::Currency pv_excess_returns;  // PV(EVA) — positive if ROIC > WACC
};

// ── Single-stage version (closed form, exact rational arithmetic) ─────────────
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

// ── Multi-stage version ───────────────────────────────────────────────────────
//
// Projects IC_t = IC_0 × ∏(1+g_stage) and computes EVA_t = (ROIC−WACC)×IC_t.
// Discounting via double (powers — same rationale as DCF).

struct MultiStageERInputs {
    ratmoney::Currency           base_invested_capital;
    ratmoney::Rational           roic;             // ROIC during the explicit period
    std::vector<ProjectionStage> stages;           // IC growth by stage
    ratmoney::Rational           wacc;
    ratmoney::Rational           terminal_roic;    // ROIC in perpetuity
    ratmoney::Rational           terminal_growth;
};

[[nodiscard]] std::expected<ExcessReturnsResult, ValuationError>
excess_returns_multistage(const MultiStageERInputs& inputs);

} // namespace ratvalue
