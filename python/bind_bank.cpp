#include "bind_helpers.h"
#include "../bank.h"

namespace nb = nanobind;
using namespace py_helpers;

void bind_bank(nb::module_& m) {
    m.def("bank_equity_reinvestment_rate",
        [](nb::object ni, nb::object current_rwa,
           nb::object projected_rwa, double target_tier1) -> double {
            return r2d(unwrap(ratvalue::bank_equity_reinvestment_rate({
                .net_income         = obj2c(ni),
                .current_rwa        = obj2c(current_rwa),
                .projected_rwa      = obj2c(projected_rwa),
                .target_tier1_ratio = d2r(target_tier1),
            })));
        },
        nb::arg("net_income"),
        nb::arg("current_rwa"),
        nb::arg("projected_rwa"),
        nb::arg("target_tier1_ratio"),
        R"(
Equity reinvestment rate for banks (Basel III).

  RR = (projected_rwa - current_rwa) * target_tier1 / net_income

Monetary arguments accept float (major units) or int (exact minor units).
)");

    m.def("compute_bank_fcfe",
        [](nb::object ni, double rr) -> double {
            return c2b(unwrap(ratvalue::compute_bank_fcfe({
                .net_income               = obj2c(ni),
                .equity_reinvestment_rate = d2r(rr),
            })));
        },
        nb::arg("net_income"), nb::arg("equity_reinvestment_rate"),
        R"(
Bank FCFE = Net Income * (1 - equity_reinvestment_rate).

Returns float in major units.
Use compute_fcfe_dcf() to discount the resulting cash flows.
)");
}
