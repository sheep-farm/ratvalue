#pragma once

#include <ratmoney/currency.h>

namespace ratvalue {

enum class ValuationError {
    InvalidInput,      // negative or invalid input
    WACCLEGrowth,      // WACC ≤ terminal growth rate (Gordon denominator ≤ 0)
    Overflow,          // arithmetic overflow
    MoneyError,        // CurrencyError from ratmoney
    CurrencyMismatch,  // monetary inputs in different denominations
};

// Growth stage: rate and number of years
struct ProjectionStage {
    ratmoney::Rational growth_rate;
    int                periods;
};

} // namespace ratvalue
