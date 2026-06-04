#include "bind_helpers.h"
#include "../dcf.h"
#include <nanobind/stl/vector.h>

namespace nb = nanobind;
using namespace py_helpers;

struct PyDCFResult {
    double enterprise_value;
    double equity_value;
    double price_per_share;
    double pv_fcff;
    double pv_terminal_value;
};

void bind_dcf(nb::module_& m) {
    nb::class_<PyDCFResult>(m, "DCFResult")
        .def_ro("enterprise_value",  &PyDCFResult::enterprise_value,
                "Enterprise value in BRL billions")
        .def_ro("equity_value",      &PyDCFResult::equity_value,
                "Equity value in BRL billions")
        .def_ro("price_per_share",   &PyDCFResult::price_per_share,
                "Intrinsic price per share in BRL")
        .def_ro("pv_fcff",           &PyDCFResult::pv_fcff,
                "PV of explicit FCFFs in BRL billions")
        .def_ro("pv_terminal_value", &PyDCFResult::pv_terminal_value,
                "PV of terminal value in BRL billions")
        .def("__repr__", [](const PyDCFResult& r) {
            return "DCFResult(ev=" + std::to_string(r.enterprise_value)
                 + "B, price=" + std::to_string(r.price_per_share) + ")";
        });

    m.def("compute_dcf",
        [](std::vector<double> fcffs_b, double wacc, double g,
           double net_debt_b, int64_t shares) -> PyDCFResult {
            std::vector<ratmoney::Currency> cflows;
            cflows.reserve(fcffs_b.size());
            for (double v : fcffs_b) cflows.push_back(b2c(v));
            auto r = unwrap(ratvalue::compute_dcf({
                .projected_fcff     = cflows,
                .wacc               = d2r(wacc),
                .terminal_growth    = d2r(g),
                .net_debt           = b2c(net_debt_b),
                .shares_outstanding = shares,
            }));
            return {c2b(r.enterprise_value), c2b(r.equity_value),
                    c2r(r.equity_value_per_share),
                    c2b(r.pv_fcff),          c2b(r.pv_terminal_value)};
        },
        nb::arg("projected_fcff_billions"),
        nb::arg("wacc"),
        nb::arg("terminal_growth"),
        nb::arg("net_debt_billions"),
        nb::arg("shares"),
        R"(
Two-stage DCF: explicit FCFFs + Gordon Growth terminal value.

  EV = sum(FCFF_t / (1+wacc)^t) + TV / (1+wacc)^N
  TV = FCFF_N * (1+g) / (wacc - g)
  Equity = EV - net_debt

Parameters
----------
projected_fcff_billions : list of projected FCFFs in BRL billions
wacc                    : weighted average cost of capital
terminal_growth         : stable perpetuity growth rate
net_debt_billions       : net debt in BRL billions
shares                  : shares outstanding

Returns
-------
DCFResult
)");
}
