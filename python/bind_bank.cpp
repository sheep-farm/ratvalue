#include "bind_helpers.h"
#include "../bank.h"

namespace nb = nanobind;
using namespace py_helpers;

void bind_bank(nb::module_& m) {
    m.def("bank_equity_reinvestment_rate",
        [](double ni_b, double current_rwa_b,
           double projected_rwa_b, double target_tier1) -> double {
            return r2d(unwrap(ratvalue::bank_equity_reinvestment_rate({
                .net_income      = b2c(ni_b),
                .current_rwa     = b2c(current_rwa_b),
                .projected_rwa   = b2c(projected_rwa_b),
                .target_tier1_ratio = d2r(target_tier1),
            })));
        },
        nb::arg("net_income_b"),
        nb::arg("current_rwa_b"),
        nb::arg("projected_rwa_b"),
        nb::arg("target_tier1_ratio"),
        R"(
Equity reinvestment rate for banks (Basel III).

  RR = (projected_rwa - current_rwa) * target_tier1 / net_income

All monetary arguments in BRL billions.
)");

    m.def("compute_bank_fcfe",
        [](double ni_b, double rr) -> double {
            return c2b(unwrap(ratvalue::compute_bank_fcfe({
                .net_income                = b2c(ni_b),
                .equity_reinvestment_rate  = d2r(rr),
            })));
        },
        nb::arg("net_income_billions"), nb::arg("equity_reinvestment_rate"),
        R"(
Bank FCFE = Net Income * (1 - equity_reinvestment_rate).

Returns float in BRL billions.
Use compute_fcfe_dcf() to discount the resulting cash flows.
)");
}
