#pragma once

#include <nanobind/nanobind.h>
#include <cmath>
#include <cstdint>

#include "../detail.h"
#include "../types.h"
#include "../kd.h"

namespace nb = nanobind;

namespace py_helpers {

// ── Currency description (BRL, precision=2) ──────────────────────────────────
inline const ratmoney::CurrencyDescription& BRL_DESC() {
    static ratmoney::CurrencyDescription d{"BRL", "R$", 2};
    return d;
}
inline const ratmoney::Rational& UNIT_RATE() {
    static ratmoney::Rational r{1, 1};
    return r;
}

// ── Currency conversions ──────────────────────────────────────────────────────

// double (BRL billions) → Currency
inline ratmoney::Currency b2c(double billions) {
    int64_t units = static_cast<int64_t>(std::round(billions * 1e11));
    return {units, UNIT_RATE(), BRL_DESC()};
}

// double (BRL reais) → Currency  (for per-share values like dividends)
inline ratmoney::Currency r2c(double reais) {
    int64_t units = static_cast<int64_t>(std::round(reais * 100.0));
    return {units, UNIT_RATE(), BRL_DESC()};
}

// Currency → double (BRL billions)
inline double c2b(const ratmoney::Currency& c) {
    return ratvalue::detail::to_double(c) / 1e9;
}

// Currency → double (BRL reais)  (for per-share values)
inline double c2r(const ratmoney::Currency& c) {
    return ratvalue::detail::to_double(c);
}

// ── Rational conversions ──────────────────────────────────────────────────────

// double (e.g. 0.05 for 5%) → Rational
inline ratmoney::Rational d2r(double v, int64_t den = 100'000) {
    return {static_cast<int64_t>(std::round(v * den)), den};
}

// Rational → double
inline double r2d(ratmoney::Rational r) {
    return static_cast<double>(r.num) / static_cast<double>(r.den);
}

// ── Unwrap expected or raise Python exception ─────────────────────────────────

inline const char* err_msg(ratvalue::ValuationError e) {
    using E = ratvalue::ValuationError;
    switch (e) {
        case E::InvalidInput:     return "invalid input";
        case E::WACCLEGrowth:     return "WACC must be greater than terminal growth rate";
        case E::Overflow:         return "rational arithmetic overflow";
        case E::MoneyError:       return "currency arithmetic error";
        case E::CurrencyMismatch: return "currency denomination mismatch";
        default:                  return "unknown valuation error";
    }
}

template<typename T>
T unwrap(std::expected<T, ratvalue::ValuationError> r) {
    if (!r) throw nb::value_error(err_msg(r.error()));
    return std::move(*r);
}

// ── CreditRating → string ─────────────────────────────────────────────────────
inline std::string rating_str(ratvalue::CreditRating r) {
    return std::string(ratvalue::rating_name(r));
}

} // namespace py_helpers
