#include "normalize.h"
#include "detail.h"

namespace ratvalue {

std::expected<ratmoney::Currency, ValuationError>
normalize_ebit(const NormalizationInputs& inputs) {
    switch (inputs.method) {

    case NormalizationMethod::HistoricalAverage: {
        if (inputs.historical_ebit.size() < 2)
            return std::unexpected(ValuationError::InvalidInput);

        // Valida denominação consistente
        for (const auto& e : inputs.historical_ebit)
            if (!detail::same_currency(e, inputs.historical_ebit.front()))
                return std::unexpected(ValuationError::CurrencyMismatch);

        // Acumula soma
        auto sum = inputs.historical_ebit.front();
        for (size_t i = 1; i < inputs.historical_ebit.size(); ++i) {
            auto r = sum.add(inputs.historical_ebit[i]);
            if (!r) return std::unexpected(ValuationError::MoneyError);
            sum = *r;
        }

        // Média = soma / n
        const int64_t n = static_cast<int64_t>(inputs.historical_ebit.size());
        auto avg = sum.scale(ratmoney::Rational{1, n});
        if (!avg) return std::unexpected(ValuationError::MoneyError);
        return *avg;
    }

    case NormalizationMethod::NormalizedMargin: {
        if (inputs.normalized_margin.num < 0)
            return std::unexpected(ValuationError::InvalidInput);
        // EBIT = receita × margem
        auto ebit = inputs.current_revenue.scale(inputs.normalized_margin);
        if (!ebit) return std::unexpected(ValuationError::MoneyError);
        return *ebit;
    }
    }

    return std::unexpected(ValuationError::InvalidInput);
}

} // namespace ratvalue
