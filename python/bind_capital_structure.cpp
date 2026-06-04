#include "bind_helpers.h"
#include "../capital_structure.h"
#include <nanobind/stl/vector.h>
#include <nanobind/stl/string.h>

namespace nb = nanobind;
using namespace py_helpers;

struct PyCSPoint {
    double      debt_ratio;
    double      cost_of_equity;
    double      cost_of_debt;
    std::string rating;
    double      interest_coverage;
    double      wacc;
};

struct PyCSResult {
    std::vector<PyCSPoint> schedule;
    PyCSPoint              optimal;
};

static PyCSPoint conv_point(const ratvalue::CapitalStructurePoint& p) {
    return {r2d(p.debt_ratio), r2d(p.cost_of_equity), r2d(p.cost_of_debt),
            rating_str(p.rating), p.interest_coverage, r2d(p.wacc)};
}

void bind_capital_structure(nb::module_& m) {
    nb::class_<PyCSPoint>(m, "CapitalStructurePoint")
        .def_ro("debt_ratio",        &PyCSPoint::debt_ratio)
        .def_ro("cost_of_equity",    &PyCSPoint::cost_of_equity)
        .def_ro("cost_of_debt",      &PyCSPoint::cost_of_debt)
        .def_ro("rating",            &PyCSPoint::rating)
        .def_ro("interest_coverage", &PyCSPoint::interest_coverage)
        .def_ro("wacc",              &PyCSPoint::wacc)
        .def("__repr__", [](const PyCSPoint& p) {
            return "CapitalStructurePoint(d=" + std::to_string(p.debt_ratio)
                 + ", rating=" + p.rating
                 + ", wacc=" + std::to_string(p.wacc) + ")";
        });

    nb::class_<PyCSResult>(m, "OptimalCapitalStructureResult")
        .def_ro("schedule", &PyCSResult::schedule,
                "Full WACC schedule across all debt ratios")
        .def_ro("optimal",  &PyCSResult::optimal,
                "Point with minimum WACC")
        .def("__repr__", [](const PyCSResult& r) {
            return "OptimalCapitalStructureResult(optimal_d="
                 + std::to_string(r.optimal.debt_ratio)
                 + ", min_wacc=" + std::to_string(r.optimal.wacc) + ")";
        });

    m.def("optimal_capital_structure",
        [](double bu, double rf, double erp, double tax,
           double ebit_b, double fv_b,
           double crp, double lam, int steps, bool large_firm) -> PyCSResult {
            auto r = unwrap(ratvalue::optimal_capital_structure({
                .unlevered_beta       = d2r(bu),
                .risk_free_rate       = d2r(rf),
                .equity_risk_premium  = d2r(erp),
                .country_risk_premium = d2r(crp),
                .lambda               = d2r(lam),
                .tax_rate             = d2r(tax),
                .ebit                 = b2c(ebit_b),
                .firm_value           = b2c(fv_b),
                .steps                = steps,
                .large_firm           = large_firm,
            }));
            PyCSResult out;
            out.schedule.reserve(r.schedule.size());
            for (auto& p : r.schedule) out.schedule.push_back(conv_point(p));
            out.optimal = conv_point(r.optimal);
            return out;
        },
        nb::arg("unlevered_beta"),
        nb::arg("rf"), nb::arg("erp"), nb::arg("tax_rate"),
        nb::arg("ebit_billions"), nb::arg("firm_value_billions"),
        nb::arg("crp") = 0.0, nb::arg("lam") = 1.0,
        nb::arg("steps") = 10, nb::arg("large_firm") = true,
        R"(
Sweep debt ratios to find the capital structure minimizing WACC.

For each d in {0/steps, 1/steps, ..., (steps-1)/steps}:
  1. Lever beta via Hamada
  2. Compute Ke via CAPM
  3. Iterate Kd via synthetic ICR until convergence
  4. Compute exact WACC

Returns the full schedule and the optimal (minimum WACC) point.
)");
}
