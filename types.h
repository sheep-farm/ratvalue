#pragma once

#include <ratmoney/currency.h>

namespace ratvalue {

enum class ValuationError {
    InvalidInput,      // entrada negativa ou inválida
    WACCLEGrowth,      // WACC ≤ taxa de crescimento terminal (denominador Gordon ≤ 0)
    Overflow,          // overflow aritmético
    MoneyError,        // CurrencyError do ratmoney
    CurrencyMismatch,  // inputs monetários em denominações diferentes
};

// Estágio de crescimento: taxa e número de anos
struct ProjectionStage {
    ratmoney::Rational growth_rate;
    int                periods;
};

} // namespace ratvalue
