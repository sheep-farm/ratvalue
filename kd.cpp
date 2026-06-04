#include "kd.h"
#include "detail.h"
#include <array>
#include <limits>

namespace ratvalue {

// ── Tabelas ICR → rating (Damodaran, Jan 2023) ────────────────────────────────
// Cada entrada define o ICR mínimo para obter o rating correspondente.
// Ordenadas de melhor para pior (ICR decrescente).

struct RatingEntry {
    double       icr_min;
    CreditRating rating;
    int32_t      spread_bps;
};

static constexpr std::array<RatingEntry, 15> LARGE_FIRM_TABLE = {{
    { 8.50, CreditRating::AAA,    63 },
    { 6.50, CreditRating::AA,     78 },
    { 5.50, CreditRating::A_plus, 98 },
    { 4.25, CreditRating::A,     108 },
    { 3.00, CreditRating::A_minus,122 },
    { 2.50, CreditRating::BBB,   156 },
    { 2.25, CreditRating::BB_plus,200 },
    { 2.00, CreditRating::BB,    240 },
    { 1.75, CreditRating::B_plus,350 },
    { 1.50, CreditRating::B,     375 },
    { 1.25, CreditRating::B_minus,500 },
    { 0.80, CreditRating::CCC,   600 },
    { 0.65, CreditRating::CC,    700 },
    { 0.20, CreditRating::C,     800 },
    {-1e15, CreditRating::D,    1200 },   // catch-all (ICR negativo ou muito baixo)
}};

static constexpr std::array<RatingEntry, 15> SMALL_FIRM_TABLE = {{
    {12.50, CreditRating::AAA,    63 },
    { 9.50, CreditRating::AA,     78 },
    { 7.50, CreditRating::A_plus, 98 },
    { 6.00, CreditRating::A,     108 },
    { 4.50, CreditRating::A_minus,122 },
    { 4.00, CreditRating::BBB,   156 },
    { 3.50, CreditRating::BB_plus,200 },
    { 3.00, CreditRating::BB,    240 },
    { 2.50, CreditRating::B_plus,350 },
    { 2.00, CreditRating::B,     375 },
    { 1.50, CreditRating::B_minus,500 },
    { 1.25, CreditRating::CCC,   600 },
    { 0.80, CreditRating::CC,    700 },
    { 0.50, CreditRating::C,     800 },
    {-1e15, CreditRating::D,    1200 },
}};

// Lookup: retorna o primeiro entry onde icr >= icr_min (tabela já ordenada de melhor para pior)
static const RatingEntry& lookup_icr(double icr, bool large_firm) noexcept {
    const auto& table = large_firm ? LARGE_FIRM_TABLE : SMALL_FIRM_TABLE;
    for (const auto& entry : table)
        if (icr >= entry.icr_min) return entry;
    return table.back();   // D rating
}

// ── API pública ───────────────────────────────────────────────────────────────

std::string_view rating_name(CreditRating r) noexcept {
    static constexpr std::array<std::string_view, 15> names = {
        "AAA", "AA", "A+", "A", "A-", "BBB", "BB+", "BB",
        "B+", "B", "B-", "CCC", "CC", "C", "D"
    };
    return names[static_cast<uint8_t>(r)];
}

int32_t rating_spread_bps(CreditRating r) noexcept {
    return LARGE_FIRM_TABLE[static_cast<uint8_t>(r)].spread_bps;
}

std::expected<SyntheticKdResult, ValuationError>
synthetic_cost_of_debt_from_icr(double icr,
                                  ratmoney::Rational risk_free_rate,
                                  bool large_firm) {
    const auto& entry = lookup_icr(icr, large_firm);

    // spread como Rational exato: spread_bps / 10000
    auto spread = detail::make_rational(
        static_cast<__int128>(entry.spread_bps), 10000);
    if (!spread) return std::unexpected(spread.error());

    // Kd = Rf + spread
    auto kd = detail::r_add(risk_free_rate, *spread);
    if (!kd) return std::unexpected(kd.error());

    return SyntheticKdResult{
        .cost_of_debt      = *kd,
        .default_spread    = *spread,
        .rating            = entry.rating,
        .interest_coverage = icr,
    };
}

std::expected<SyntheticKdResult, ValuationError>
synthetic_cost_of_debt(const SyntheticKdInputs& inputs) {
    if (!detail::same_currency(inputs.ebit, inputs.interest_expense))
        return std::unexpected(ValuationError::CurrencyMismatch);

    double icr;
    const int64_t interest_units = inputs.interest_expense.units();

    if (interest_units <= 0) {
        // Sem despesa financeira (ou receita > despesa): sem risco de default
        icr = std::numeric_limits<double>::infinity();
    } else {
        icr = static_cast<double>(inputs.ebit.units())
            / static_cast<double>(interest_units);
    }

    return synthetic_cost_of_debt_from_icr(icr, inputs.risk_free_rate, inputs.large_firm);
}

} // namespace ratvalue
