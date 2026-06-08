#include "bind_helpers.h"
#include "../growth.h"

namespace nb = nanobind;
using namespace py_helpers;

void bind_growth(nb::module_& m) {
    m.def("compute_roic",
        [](nb::object nopat, nb::object ic) -> double {
            return r2d(unwrap(ratvalue::compute_roic({
                .nopat            = obj2c(nopat),
                .invested_capital = obj2c(ic),
            })));
        },
        nb::arg("nopat"), nb::arg("invested_capital"),
        "ROIC = NOPAT / Invested Capital");

    m.def("compute_roe",
        [](nb::object ni, nb::object book_equity) -> double {
            return r2d(unwrap(ratvalue::compute_roe({
                .net_income  = obj2c(ni),
                .book_equity = obj2c(book_equity),
            })));
        },
        nb::arg("net_income"), nb::arg("book_equity"),
        "ROE = Net Income / Book Equity");

    m.def("compute_reinvestment_rate",
        [](nb::object capex, nb::object da, nb::object delta_nwc, nb::object nopat) -> double {
            return r2d(unwrap(ratvalue::compute_reinvestment_rate({
                .capex                     = obj2c(capex),
                .depreciation_amortization = obj2c(da),
                .delta_nwc                 = obj2c(delta_nwc),
                .nopat                     = obj2c(nopat),
            })));
        },
        nb::arg("capex"), nb::arg("da"), nb::arg("delta_nwc"), nb::arg("nopat"),
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
