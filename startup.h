#pragma once

#include "types.h"
#include <expected>
#include <string>
#include <vector>

namespace ratvalue {

// ── Scenario Valuation with Survival Probability ─────────────────────────────
//
// Damodaran, "The Dark Side of Valuation" (ch. 10-12):
// Young / high-growth firms have asymmetric outcome distributions.
// The scenario model captures this explicitly:
//
//   V = [Σ P_i × V_i] × P(survive)  +  V_failure × P(fail)
//
// Each scenario defines a path to stable state: terminal revenue, operating
// margin, and perpetual growth rate.  The value of each scenario is the TV
// discounted at the scenario WACC to today.

struct ValuationScenario {
    std::string        name;
    ratmoney::Rational probability;          // scenario weight ∈ (0,1)
    ratmoney::Currency terminal_fcff;        // FCFF in the terminal year (stable state)
    ratmoney::Rational terminal_growth;      // perpetual g after terminal year
    ratmoney::Rational wacc;                 // cost of capital for the scenario
    int                years_to_terminal;    // horizon to stable state
};

struct StartupValuationInputs {
    std::vector<ValuationScenario> scenarios;
    ratmoney::Rational             survival_probability;  // P(firm reaching terminal)
    ratmoney::Currency             failure_value;         // value in case of failure (≥ 0)
    ratmoney::Currency             net_debt;              // for equity = EV − debt
    int64_t                        shares_outstanding;
};

struct StartupValuationResult {
    ratmoney::Currency enterprise_value;        // E[V] × P(survive) + V_fail × P(fail)
    ratmoney::Currency equity_value;
    ratmoney::Currency equity_value_per_share;
    // per scenario: (name, scenario EV discounted to today)
    std::vector<std::pair<std::string, ratmoney::Currency>> scenario_evs;
};

// Validates: Σ probabilities ≈ 1 (tolerance 1e-6); P(survive) ∈ [0,1].
[[nodiscard]] std::expected<StartupValuationResult, ValuationError>
startup_valuation(const StartupValuationInputs& inputs);

} // namespace ratvalue
