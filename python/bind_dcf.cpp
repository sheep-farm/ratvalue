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
                "Enterprise value in major units")
        .def_ro("equity_value",      &PyDCFResult::equity_value,
                "Equity value in major units")
        .def_ro("price_per_share",   &PyDCFResult::price_per_share,
                "Intrinsic price per share")
        .def_ro("pv_fcff",           &PyDCFResult::pv_fcff,
                "PV of explicit FCFFs in major units")
        .def_ro("pv_terminal_value", &PyDCFResult::pv_terminal_value,
                "PV of terminal value in major units")
        .def("__repr__", [](const PyDCFResult& r) {
            return "DCFResult(ev=" + std::to_string(r.enterprise_value)
                 + ", price=" + std::to_string(r.price_per_share) + ")";
        });

    m.def("compute_dcf",
        [](std::vector<nb::object> fcffs, double wacc, double g,
           nb::object net_debt, int64_t shares) -> PyDCFResult {
            std::vector<ratmoney::Currency> cflows;
            cflows.reserve(fcffs.size());
            for (auto& v : fcffs) cflows.push_back(obj2c(v));
            auto r = unwrap(ratvalue::compute_dcf({
                .projected_fcff     = cflows,
                .wacc               = d2r(wacc),
                .terminal_growth    = d2r(g),
                .net_debt           = obj2c(net_debt),
                .shares_outstanding = shares,
            }));
            return {c2b(r.enterprise_value), c2b(r.equity_value),
                    c2r(r.equity_value_per_share),
                    c2b(r.pv_fcff),          c2b(r.pv_terminal_value)};
        },
        nb::arg("projected_fcff"),
        nb::arg("wacc"),
        nb::arg("terminal_growth"),
        nb::arg("net_debt"),
        nb::arg("shares"),
        R"(
Two-stage DCF: explicit FCFFs + Gordon Growth terminal value.

  EV = sum(FCFF_t / (1+wacc)^t) + TV / (1+wacc)^N
  TV = FCFF_N * (1+g) / (wacc - g)
  Equity = EV - net_debt

Parameters
----------
projected_fcff : list of projected FCFFs — float (major units) or int (exact minor units)
wacc           : weighted average cost of capital
terminal_growth: stable perpetuity growth rate
net_debt       : net debt — float (major units) or int (exact minor units)
shares         : shares outstanding

Returns
-------
DCFResult
)");
}
