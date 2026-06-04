#pragma once
// Cabeçalho interno — não faz parte da API pública.

#include "types.h"
#include <cmath>
#include <expected>
#include <limits>

namespace ratvalue::detail {

// ── aritmética inteira ──────────────────────────────────────────────────────

inline bool fits_int64(__int128 v) noexcept {
    return v >= static_cast<__int128>(std::numeric_limits<int64_t>::min())
        && v <= static_cast<__int128>(std::numeric_limits<int64_t>::max());
}

inline __int128 gcd128(__int128 a, __int128 b) noexcept {
    if (a < 0) a = -a;
    if (b < 0) b = -b;
    while (b) { __int128 t = b; b = a % b; a = t; }
    return a == 0 ? 1 : a;
}

// Normaliza par (__int128 num, den) → Rational, verificando overflow.
inline std::expected<ratmoney::Rational, ValuationError>
make_rational(__int128 n, __int128 d) {
    if (d == 0) return std::unexpected(ValuationError::InvalidInput);
    __int128 g = gcd128(n < 0 ? -n : n, d < 0 ? -d : d);
    n /= g; d /= g;
    if (d < 0) { n = -n; d = -d; }
    if (!fits_int64(n) || !fits_int64(d))
        return std::unexpected(ValuationError::Overflow);
    try {
        return ratmoney::Rational{static_cast<int64_t>(n), static_cast<int64_t>(d)};
    } catch (...) {
        return std::unexpected(ValuationError::Overflow);
    }
}

// ── aritmética racional ─────────────────────────────────────────────────────

inline std::expected<ratmoney::Rational, ValuationError>
r_add(ratmoney::Rational a, ratmoney::Rational b) {
    return make_rational(
        static_cast<__int128>(a.num) * b.den + static_cast<__int128>(b.num) * a.den,
        static_cast<__int128>(a.den) * b.den);
}

inline std::expected<ratmoney::Rational, ValuationError>
r_sub(ratmoney::Rational a, ratmoney::Rational b) {
    return make_rational(
        static_cast<__int128>(a.num) * b.den - static_cast<__int128>(b.num) * a.den,
        static_cast<__int128>(a.den) * b.den);
}

inline std::expected<ratmoney::Rational, ValuationError>
r_mul(ratmoney::Rational a, ratmoney::Rational b) {
    return make_rational(
        static_cast<__int128>(a.num) * b.num,
        static_cast<__int128>(a.den) * b.den);
}

// a / b
inline std::expected<ratmoney::Rational, ValuationError>
r_div(ratmoney::Rational a, ratmoney::Rational b) {
    if (b.num == 0) return std::unexpected(ValuationError::InvalidInput);
    return make_rational(
        static_cast<__int128>(a.num) * b.den,
        static_cast<__int128>(a.den) * b.num);
}

// 1 + r = (den + num) / den
inline std::expected<ratmoney::Rational, ValuationError>
one_plus(ratmoney::Rational r) {
    return make_rational(
        static_cast<__int128>(r.num) + r.den,
        static_cast<__int128>(r.den));
}

// ── validação de moeda ─────────────────────────────────────────────────────

// Dois Currency compartilham a mesma denominação se têm o mesmo CurrencyDescription.
// O campo `rate` não faz parte da identidade da denominação — é só fator de conversão.
inline bool same_currency(const ratmoney::Currency& a,
                           const ratmoney::Currency& b) noexcept {
    return a.description() == b.description();
}

// ── utilitário monetário ────────────────────────────────────────────────────

// Converte Currency para double em unidades maiores (ex: 100 centavos → 1.00).
// Equivalente ao to_double() do ratmoney, implementado aqui para independência de versão.
inline double to_double(const ratmoney::Currency& c) noexcept {
    double divisor = 1.0;
    for (uint8_t i = 0; i < c.description().precision; ++i) divisor *= 10.0;
    return static_cast<double>(c.units()) / divisor;
}

// Converte valor em unidades maiores (ex: 42.00 BRL) para Currency.
inline std::expected<ratmoney::Currency, ValuationError>
make_currency_from_double(double value,
                           ratmoney::Rational rate,
                           const ratmoney::CurrencyDescription& desc) {
    double factor = 1.0;
    for (uint8_t i = 0; i < desc.precision; ++i) factor *= 10.0;
    double raw     = value * factor;
    double rounded = raw >= 0.0 ? std::floor(raw + 0.5) : std::ceil(raw - 0.5);
    constexpr double INT64_MAX_D = static_cast<double>(std::numeric_limits<int64_t>::max());
    constexpr double INT64_MIN_D = static_cast<double>(std::numeric_limits<int64_t>::min());
    if (rounded < INT64_MIN_D || rounded > INT64_MAX_D)
        return std::unexpected(ValuationError::Overflow);
    return ratmoney::Currency{static_cast<int64_t>(rounded), rate, desc};
}

} // namespace ratvalue::detail
