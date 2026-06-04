#include "bind_helpers.h"
#include "../growth.h"

namespace nb = nanobind;
using namespace py_helpers;

void bind_growth(nb::module_& m) {
    m.def("compute_roic",
        [](double nopat_b, double ic_b) -> double {
            return r2d(unwrap(ratvalue::compute_roic({
                .nopat            = b2c(nopat_b),
                .invested_capital = b2c(ic_b),
            })));
        },
        nb::arg("nopat_billions"), nb::arg("invested_capital_billions"),
        "ROIC = NOPAT / Invested Capital");

    m.def("compute_roe",
        [](double ni_b, double book_equity_b) -> double {
            return r2d(unwrap(ratvalue::compute_roe({
                .net_income  = b2c(ni_b),
                .book_equity = b2c(book_equity_b),
            })));
        },
        nb::arg("net_income_billions"), nb::arg("book_equity_billions"),
        "ROE = Net Income / Book Equity");

    m.def("compute_reinvestment_rate",
        [](double capex_b, double da_b, double delta_nwc_b, double nopat_b) -> double {
            return r2d(unwrap(ratvalue::compute_reinvestment_rate({
                .capex                     = b2c(capex_b),
                .depreciation_amortization = b2c(da_b),
                .delta_nwc                 = b2c(delta_nwc_b),
                .nopat                     = b2c(nopat_b),
            })));
        },
        nb::arg("capex_b"), nb::arg("da_b"), nb::arg("delta_nwc_b"), nb::arg("nopat_b"),
        "Reinvestment rate = (CapEx - D&A + dNWC) / NOPAT");

    m.def("fundamental_growth_firm",
        [](double roic, double rr) -> double {
            return r2d(unwrap(ratvalue::fundamental_growth_firm(d2r(roic), d2r(rr))));
        },
        nb::arg("roic"), nb::arg("reinvestment_rate"),
        "g_firm = ROIC * reinvestment_rate");

    m.def("fundamental_growth_equity",
        [](double roe, double retention) -> double {
            return r2d(unwrap(ratvalue::fundamental_growth_equity(d2r(roe), d2r(retention))));
        },
        nb::arg("roe"), nb::arg("retention_ratio"),
        "g_equity = ROE * retention_ratio  (retention = 1 - payout)");
}
