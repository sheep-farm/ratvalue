#pragma once

#include "types.h"
#include <expected>
#include <vector>

namespace ratvalue {

// Financial inputs for FCFF (Free Cash Flow to Firm) computation
struct FCFFInputs {
    ratmoney::Currency ebit;                         // EBIT
    ratmoney::Rational tax_rate;                     // tax rate (t)
    ratmoney::Currency depreciation_amortization;    // depreciation + amortization (D&A)
    ratmoney::Currency capex;                        // capital expenditure
    ratmoney::Currency delta_nwc;                    // change in net working capital
};

// FCFF = EBIT × (1 - t) + D&A - CapEx - ΔNWC
[[nodiscard]] std::expected<ratmoney::Currency, ValuationError>
compute_fcff(const FCFFInputs& inputs);

// Multi-stage FCFF projection
struct FCFFProjection {
    ratmoney::Currency           base_fcff;  // base FCFF (year 0)
    std::vector<ProjectionStage> stages;     // growth stages
};

// Projects FCFF from year 1 to N (base not included in result)
[[nodiscard]] std::expected<std::vector<ratmoney::Currency>, ValuationError>
project_fcff(const FCFFProjection& proj);

} // namespace ratvalue
