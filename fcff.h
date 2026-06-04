#pragma once

#include "types.h"
#include <expected>
#include <vector>

namespace ratvalue {

// Dados financeiros para cálculo do FCFF (Free Cash Flow to Firm)
struct FCFFInputs {
    ratmoney::Currency ebit;                         // LAJIR
    ratmoney::Rational tax_rate;                     // alíquota de imposto (t)
    ratmoney::Currency depreciation_amortization;    // depreciação + amortização (D&A)
    ratmoney::Currency capex;                        // investimento em ativo fixo
    ratmoney::Currency delta_nwc;                    // variação no capital de giro
};

// FCFF = EBIT × (1 - t) + D&A - CapEx - ΔCGN
[[nodiscard]] std::expected<ratmoney::Currency, ValuationError>
compute_fcff(const FCFFInputs& inputs);

// Projeção multi-estágio de FCFF
struct FCFFProjection {
    ratmoney::Currency           base_fcff;  // FCFF do período base (ano 0)
    std::vector<ProjectionStage> stages;     // estágios de crescimento
};

// Projeta FCFF do ano 1 ao ano N (base não está incluído no resultado)
[[nodiscard]] std::expected<std::vector<ratmoney::Currency>, ValuationError>
project_fcff(const FCFFProjection& proj);

} // namespace ratvalue
