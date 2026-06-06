#pragma once

#include "types.h"
#include <expected>
#include <vector>

namespace ratvalue {

// ── Earnings Normalization (Damodaran, Ch. 8 and 18) ─────────────────────────
//
// Cyclical firms or firms with non-recurring items produce volatile EBIT that
// distorts the DCF.  Damodaran recommends normalizing before valuing:
//
//   Method 1 — Historical Average: arithmetic mean of EBIT over the last N years.
//     Captures "cycle-average EBIT".  Requires at least 2 years.
//
//   Method 2 — Normalized Margin: current revenue × "normal" sector or historical margin.
//     Useful when revenue is stable but margin oscillates.
//
// Both return a Currency that can be used directly as EBIT_0 in
// compute_fcff or as NOPAT_0 in excess_returns_value.

enum class NormalizationMethod {
    HistoricalAverage,  // historical EBIT average
    NormalizedMargin,   // current revenue × normalized margin
};

struct NormalizationInputs {
    NormalizationMethod             method;
    std::vector<ratmoney::Currency> historical_ebit;    // for HistoricalAverage (≥ 2)
    ratmoney::Currency              current_revenue;     // for NormalizedMargin
    ratmoney::Rational              normalized_margin;   // for NormalizedMargin
};

[[nodiscard]] std::expected<ratmoney::Currency, ValuationError>
normalize_ebit(const NormalizationInputs& inputs);

} // namespace ratvalue
