#include "bind_helpers.h"
#include "../distress.h"

namespace nb = nanobind;
using namespace py_helpers;

void bind_distress(nb::module_& m) {
    nb::class_<ratvalue::EquityAsOptionResult>(m, "EquityAsOptionResult")
        .def_ro("equity_value",         &ratvalue::EquityAsOptionResult::equity_value,
                "Call option value = equity value (same units as inputs)")
        .def_ro("debt_market_value",    &ratvalue::EquityAsOptionResult::debt_market_value,
                "V - E")
        .def_ro("probability_default",  &ratvalue::EquityAsOptionResult::probability_default,
                "Risk-neutral default probability N(-d2)")
        .def_ro("distance_to_default",  &ratvalue::EquityAsOptionResult::distance_to_default,
                "d2: higher is safer")
        .def_ro("d1",                   &ratvalue::EquityAsOptionResult::d1)
        .def("__repr__", [](const ratvalue::EquityAsOptionResult& r) {
            return "EquityAsOptionResult(equity=" + std::to_string(r.equity_value)
                 + ", p_default=" + std::to_string(r.probability_default) + ")";
        });

    m.def("equity_as_option",
        [](double firm_value, double debt_face, double sigma,
           double rf, double maturity) -> ratvalue::EquityAsOptionResult {
            return ratvalue::equity_as_option({
                .firm_value       = firm_value,
                .debt_face_value  = debt_face,
                .asset_volatility = sigma,
                .risk_free_rate   = rf,
                .debt_maturity    = maturity,
            });
        },
        nb::arg("firm_value"), nb::arg("debt_face_value"),
        nb::arg("asset_volatility"), nb::arg("risk_free_rate"),
        nb::arg("debt_maturity_years"),
        R"(
Merton (1974) model: equity as a call option on firm assets.

  E = V*N(d1) - D*exp(-rT)*N(d2)
  P(default) = N(-d2)

Note: firm_value and debt_face_value must be in the same units.
Typically pass billions directly (no conversion needed as this model
uses pure doubles throughout).

Parameters
----------
firm_value           : V, estimated firm asset value
debt_face_value      : D, face value of total debt
asset_volatility     : sigma_V, annual asset volatility
risk_free_rate       : r, continuously compounded risk-free rate
debt_maturity_years  : T, average debt maturity in years
)");
}
