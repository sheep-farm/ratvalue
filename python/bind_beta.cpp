#include "bind_helpers.h"
#include "../beta.h"
#include <nanobind/stl/vector.h>

namespace nb = nanobind;
using namespace py_helpers;

void bind_beta(nb::module_& m) {
    m.def("lever_beta",
        [](double bu, double de, double tax) -> double {
            return r2d(unwrap(ratvalue::lever_beta(d2r(bu), d2r(de), d2r(tax))));
        },
        nb::arg("unlevered_beta"), nb::arg("debt_to_equity"), nb::arg("tax_rate"),
        "Apply Hamada equation: beta_L = beta_U * [1 + (1-t) * D/E]");

    m.def("unlever_beta",
        [](double bl, double de, double tax) -> double {
            return r2d(unwrap(ratvalue::unlever_beta(d2r(bl), d2r(de), d2r(tax))));
        },
        nb::arg("levered_beta"), nb::arg("debt_to_equity"), nb::arg("tax_rate"),
        "Reverse Hamada equation: beta_U = beta_L / [1 + (1-t) * D/E]");

    m.def("bottom_up_beta",
        [](std::vector<double> bl, std::vector<double> de,
           double comp_tax, double target_de, double target_tax) -> double {
            std::vector<ratmoney::Rational> rbl, rde;
            rbl.reserve(bl.size());
            rde.reserve(de.size());
            for (double v : bl)  rbl.push_back(d2r(v));
            for (double v : de)  rde.push_back(d2r(v));
            return r2d(unwrap(ratvalue::bottom_up_beta({
                .comparable_levered_betas  = rbl,
                .comparable_debt_to_equity = rde,
                .comparable_tax_rate       = d2r(comp_tax),
                .target_debt_to_equity     = d2r(target_de),
                .target_tax_rate           = d2r(target_tax),
            })));
        },
        nb::arg("comparable_levered_betas"),
        nb::arg("comparable_debt_to_equity"),
        nb::arg("comparable_tax_rate"),
        nb::arg("target_debt_to_equity"),
        nb::arg("target_tax_rate"),
        R"(
Bottom-up beta: unlever comparables, average, re-lever to target structure.

Parameters
----------
comparable_levered_betas   : list of market betas of comparable firms
comparable_debt_to_equity  : list of D/E ratios of comparables
comparable_tax_rate        : average sector tax rate
target_debt_to_equity      : D/E of the firm being valued
target_tax_rate            : tax rate of the firm being valued
)");
}
