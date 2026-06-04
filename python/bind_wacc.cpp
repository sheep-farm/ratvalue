#include "bind_helpers.h"
#include "../wacc.h"

namespace nb = nanobind;
using namespace py_helpers;

void bind_wacc(nb::module_& m) {
    m.def("compute_wacc",
        [](double ew, double ke, double dw, double kd, double tax) -> double {
            return r2d(unwrap(ratvalue::compute_wacc({
                .equity_weight  = d2r(ew),
                .debt_weight    = d2r(dw),
                .cost_of_equity = d2r(ke),
                .cost_of_debt   = d2r(kd),
                .tax_rate       = d2r(tax),
            })));
        },
        nb::arg("equity_weight"), nb::arg("ke"),
        nb::arg("debt_weight"),   nb::arg("kd"),
        nb::arg("tax_rate"),
        R"(
Compute WACC.

  WACC = equity_weight * ke + debt_weight * kd * (1 - tax_rate)

Parameters
----------
equity_weight : E/V
ke            : cost of equity
debt_weight   : D/V
kd            : cost of debt
tax_rate      : corporate tax rate (e.g. 0.34)
)");
}
