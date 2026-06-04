#pragma once

#include "types.h"
#include <expected>
#include <string>
#include <vector>

namespace ratvalue {

// ── Valuation por Cenários com Probabilidade de Sobrevivência ─────────────────
//
// Damodaran, "The Dark Side of Valuation" (cap. 10-12):
// Empresas jovens / alto crescimento têm distribuição de resultados assimétrica.
// O modelo de cenários captura isso explicitamente:
//
//   V = [Σ P_i × V_i] × P(sobrevive)  +  V_falência × P(falha)
//
// Cada cenário define um caminho ao estado estável: receita terminal, margem
// operacional e taxa de crescimento perpétuo.  O valor de cada cenário é o TV
// descontado ao WACC do cenário até hoje.

struct ValuationScenario {
    std::string        name;
    ratmoney::Rational probability;          // peso do cenário ∈ (0,1)
    ratmoney::Currency terminal_fcff;        // FCFF no ano terminal (estado estável)
    ratmoney::Rational terminal_growth;      // g perpétuo após o ano terminal
    ratmoney::Rational wacc;                 // custo de capital do cenário
    int                years_to_terminal;    // horizonte até o estado estável
};

struct StartupValuationInputs {
    std::vector<ValuationScenario> scenarios;
    ratmoney::Rational             survival_probability;  // P(firma chegar ao terminal)
    ratmoney::Currency             failure_value;         // valor em caso de falência (≥ 0)
    ratmoney::Currency             net_debt;              // para equity = EV − dívida
    int64_t                        shares_outstanding;
};

struct StartupValuationResult {
    ratmoney::Currency enterprise_value;        // E[V] × P(survive) + V_fail × P(fail)
    ratmoney::Currency equity_value;
    ratmoney::Currency equity_value_per_share;
    // por cenário: (nome, EV do cenário descontado a hoje)
    std::vector<std::pair<std::string, ratmoney::Currency>> scenario_evs;
};

// Valida: Σ probabilidades ≈ 1 (tolerância 1e-6); P(survive) ∈ [0,1].
[[nodiscard]] std::expected<StartupValuationResult, ValuationError>
startup_valuation(const StartupValuationInputs& inputs);

} // namespace ratvalue
