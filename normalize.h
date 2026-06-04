#pragma once

#include "types.h"
#include <expected>
#include <vector>

namespace ratvalue {

// ── Normalização de Resultados (Damodaran, Ch. 8 e 18) ───────────────────────
//
// Firmas cíclicas ou com itens não-recorrentes produzem EBIT volátil que
// distorce o DCF.  Damodaran recomenda normalizar antes de valuar:
//
//   Método 1 — Média Histórica: média aritmética do EBIT dos últimos N anos.
//     Captura o "EBIT médio do ciclo".  Requer pelo menos 2 anos.
//
//   Método 2 — Margem Normalizada: receita atual × margem "normal" do setor
//     ou histórica.  Útil quando a receita é estável mas a margem oscila.
//
// Ambos retornam um Currency que pode ser usado diretamente como EBIT_0 no
// compute_fcff ou como NOPAT_0 no excess_returns_value.

enum class NormalizationMethod {
    HistoricalAverage,  // média do EBIT histórico
    NormalizedMargin,   // receita atual × margem normalizada
};

struct NormalizationInputs {
    NormalizationMethod             method;
    std::vector<ratmoney::Currency> historical_ebit;    // para HistoricalAverage (≥ 2)
    ratmoney::Currency              current_revenue;     // para NormalizedMargin
    ratmoney::Rational              normalized_margin;   // para NormalizedMargin
};

[[nodiscard]] std::expected<ratmoney::Currency, ValuationError>
normalize_ebit(const NormalizationInputs& inputs);

} // namespace ratvalue
