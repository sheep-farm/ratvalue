#include "bind_helpers.h"
#include "../capm.h"

namespace nb = nanobind;
using namespace py_helpers;

void bind_capm(nb::module_& m) {
    m.def("cost_of_equity",
        [](double rf, double beta, double erp,
           double crp, double lam) -> double {
            return r2d(unwrap(ratvalue::cost_of_equity({
                .risk_free_rate       = d2r(rf),
                .beta                 = d2r(beta),
                .equity_risk_premium  = d2r(erp),
                .country_risk_premium = d2r(crp),
                .lambda               = d2r(lam),
            })));
        },
        nb::arg("rf"), nb::arg("beta"), nb::arg("erp"),
        nb::arg("crp") = 0.0, nb::arg("lam") = 1.0,
        R"(
Compute cost of equity via CAPM with optional country risk premium.

  Ke = rf + beta * erp + lam * crp

Parameters
----------
rf    : risk-free rate (e.g. 0.05 for 5%)
beta  : levered beta
erp   : equity risk premium of mature market
crp   : country risk premium (default 0)
lam   : firm's country exposure, 0-1 (default 1)

Returns
-------
float : cost of equity
)");
}
