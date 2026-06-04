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
                "Equity value in BRL billions")
        .def_ro("price_per_share",   &PyFCFEDCFResult::price_per_share,
                "Intrinsic price per share in BRL")
        .def_ro("pv_fcfe",           &PyFCFEDCFResult::pv_fcfe,
                "PV of explicit FCFEs in BRL billions")
        .def_ro("pv_terminal_value", &PyFCFEDCFResult::pv_terminal_value,
                "PV of terminal value in BRL billions")
        .def("__repr__", [](const PyFCFEDCFResult& r) {
            return "FCFEDCFResult(equity=" + std::to_string(r.equity_value)
                 + "B, price=" + std::to_string(r.price_per_share) + ")";
        });

    m.def("compute_fcfe",
        [](double ni_b, double debt_ratio, double capex_b,
           double da_b, double delta_nwc_b, double new_debt_b) -> double {
            return c2b(unwrap(ratvalue::compute_fcfe({
                .net_income                = b2c(ni_b),
                .debt_ratio                = d2r(debt_ratio),
                .capex                     = b2c(capex_b),
                .depreciation_amortization = b2c(da_b),
                .delta_nwc                 = b2c(delta_nwc_b),
                .net_new_debt              = b2c(new_debt_b),
            })));
        },
        nb::arg("net_income_b"), nb::arg("debt_ratio"),
        nb::arg("capex_b"), nb::arg("da_b"),
        nb::arg("delta_nwc_b") = 0.0, nb::arg("net_new_debt_b") = 0.0,
        R"(
Compute FCFE.

  FCFE = NI - (1-delta)*(CapEx - D&A + dNWC) + net_new_debt

All monetary arguments in BRL billions.
)");

    m.def("compute_fcfe_dcf",
        [](std::vector<double> fcfes_b, double ke,
           double g, int64_t shares) -> PyFCFEDCFResult {
            std::vector<ratmoney::Currency> cflows;
            cflows.reserve(fcfes_b.size());
            for (double v : fcfes_b) cflows.push_back(b2c(v));
            auto r = unwrap(ratvalue::compute_fcfe_dcf({
                .projected_fcfe     = cflows,
                .cost_of_equity     = d2r(ke),
                .terminal_growth    = d2r(g),
                .shares_outstanding = shares,
            }));
            return {c2b(r.equity_value), c2r(r.equity_value_per_share),
                    c2b(r.pv_fcfe),      c2b(r.pv_terminal_value)};
        },
        nb::arg("projected_fcfe_billions"), nb::arg("cost_of_equity"),
        nb::arg("terminal_growth"), nb::arg("shares"),
        "DCF from equity perspective: discount FCFEs at cost of equity.");
}
