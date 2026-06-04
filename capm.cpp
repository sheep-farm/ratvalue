#include "capm.h"
#include "detail.h"

namespace ratvalue {

std::expected<ratmoney::Rational, ValuationError>
cost_of_equity(const CAPMInputs& inputs) {
    if (inputs.risk_free_rate.num < 0
     || inputs.equity_risk_premium.num < 0
     || inputs.country_risk_premium.num < 0
     || inputs.lambda.num < 0)
        return std::unexpected(ValuationError::InvalidInput);

    // β × ERP_maduro
    auto beta_erp = detail::r_mul(inputs.beta, inputs.equity_risk_premium);
    if (!beta_erp) return std::unexpected(beta_erp.error());

    // λ × CRP
    auto lambda_crp = detail::r_mul(inputs.lambda, inputs.country_risk_premium);
    if (!lambda_crp) return std::unexpected(lambda_crp.error());

    // Rf + β×ERP
    auto partial = detail::r_add(inputs.risk_free_rate, *beta_erp);
    if (!partial) return std::unexpected(partial.error());

    // + λ×CRP
    return detail::r_add(*partial, *lambda_crp);
}

} // namespace ratvalue
