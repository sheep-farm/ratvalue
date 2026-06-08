#include "bind_helpers.h"
#include "../fcfe.h"
#include <nanobind/stl/vector.h>

namespace nb = nanobind;
using namespace py_helpers;

struct PyFCFEDCFResult {
    double equity_value;
    double price_per_share;
    double pv_fcfe;
    double pv_terminal_value;
};

void bind_fcfe(nb::module_& m) {
    nb::class_<PyFCFEDCFResult>(m, "FCFEDCFResult")
        .def_ro("equity_value",      &PyFCFEDCFResult::equity_value,
                "Equity value in major units")
        .def_ro("price_per_share",   &PyFCFEDCFResult::price_per_share,
                "Intrinsic price per share")
        .def_ro("pv_fcfe",           &PyFCFEDCFResult::pv_fcfe,
                "PV of explicit FCFEs in major units")
        .def_ro("pv_terminal_value", &PyFCFEDCFResult::pv_terminal_value,
                "PV of terminal value in major units")
        .def("__repr__", [](const PyFCFEDCFResult& r) {
            return "FCFEDCFResult(equity=" + std::to_string(r.equity_value)
                 + ", price=" + std::to_string(r.price_per_share) + ")";
        });

    m.def("compute_fcfe",
        [](nb::object ni, double debt_ratio, nb::object capex,
           nb::object da, nb::object delta_nwc, nb::object new_debt) -> double {
            return c2b(unwrap(ratvalue::compute_fcfe({
                .net_income                = obj2c(ni),
                .debt_ratio                = d2r(debt_ratio),
                .capex                     = obj2c(capex),
                .depreciation_amortization = obj2c(da),
                .delta_nwc                 = obj2c(delta_nwc),
                .net_new_debt              = obj2c(new_debt),
            })));
        },
        nb::arg("net_income_b"), nb::arg("debt_ratio"),
        nb::arg("capex_b"), nb::arg("da_b"),
        nb::arg("delta_nwc_b") = 0.0, nb::arg("net_new_debt_b") = 0.0,
        R"(
Compute FCFE.

  FCFE = NI - (1-delta)*(CapEx - D&A + dNWC) + net_new_debt

Monetary arguments accept float (major units) or int (exact minor units).
)");

    m.def("compute_fcfe_dcf",
        [](std::vector<nb::object> fcfes, double ke,
           double g, int64_t shares) -> PyFCFEDCFResult {
            std::vector<ratmoney::Currency> cflows;
            cflows.reserve(fcfes.size());
            for (auto& v : fcfes) cflows.push_back(obj2c(v));
            auto r = unwrap(ratvalue::compute_fcfe_dcf({
                .projected_fcfe     = cflows,
                .cost_of_equity     = d2r(ke),
                .terminal_growth    = d2r(g),
                .shares_outstanding = shares,
            }));
            return {c2b(r.equity_value), c2r(r.equity_value_per_share),
                    c2b(r.pv_fcfe),      c2b(r.pv_terminal_value)};
        },
        nb::arg("projected_fcfe"), nb::arg("cost_of_equity"),
        nb::arg("terminal_growth"), nb::arg("shares"),
        "DCF from equity perspective: discount FCFEs at cost of equity.");
}
