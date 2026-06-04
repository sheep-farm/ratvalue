#include "bind_helpers.h"
#include "../kd.h"
#include <nanobind/stl/string.h>

namespace nb = nanobind;
using namespace py_helpers;

// Python-friendly result struct
struct PySyntheticKd {
    double      cost_of_debt;
    double      default_spread;
    std::string rating;
    double      interest_coverage;
};

void bind_kd(nb::module_& m) {
    nb::class_<PySyntheticKd>(m, "SyntheticKdResult")
        .def_ro("cost_of_debt",       &PySyntheticKd::cost_of_debt)
        .def_ro("default_spread",     &PySyntheticKd::default_spread)
        .def_ro("rating",             &PySyntheticKd::rating)
        .def_ro("interest_coverage",  &PySyntheticKd::interest_coverage)
        .def("__repr__", [](const PySyntheticKd& r) {
            return "SyntheticKdResult(rating=" + r.rating
                 + ", kd=" + std::to_string(r.cost_of_debt)
                 + ", icr=" + std::to_string(r.interest_coverage) + ")";
        });

    auto conv = [](const ratvalue::SyntheticKdResult& r) -> PySyntheticKd {
        return {r2d(r.cost_of_debt), r2d(r.default_spread),
                rating_str(r.rating), r.interest_coverage};
    };

    m.def("synthetic_cost_of_debt",
        [conv](double ebit_b, double interest_b, double rf, bool large_firm) {
            return conv(unwrap(ratvalue::synthetic_cost_of_debt({
                .ebit             = b2c(ebit_b),
                .interest_expense = b2c(interest_b),
                .risk_free_rate   = d2r(rf),
                .large_firm       = large_firm,
            })));
        },
        nb::arg("ebit_billions"), nb::arg("interest_billions"),
        nb::arg("rf"), nb::arg("large_firm") = true,
        R"(
Synthetic cost of debt via ICR → rating → spread (Damodaran 2023 tables).

  ICR = EBIT / Interest  →  credit rating  →  Kd = rf + spread

Parameters
----------
ebit_billions     : EBIT in BRL billions
interest_billions : interest expense in BRL billions
rf                : risk-free rate
large_firm        : use large-cap ICR thresholds (default True)
)");

    m.def("synthetic_cost_of_debt_from_icr",
        [conv](double icr, double rf, bool large_firm) {
            return conv(unwrap(ratvalue::synthetic_cost_of_debt_from_icr(
                icr, d2r(rf), large_firm)));
        },
        nb::arg("icr"), nb::arg("rf"), nb::arg("large_firm") = true,
        "Synthetic Kd from a pre-computed ICR value.");
}
