#include "bind_helpers.h"
#include "../fcff.h"
#include <nanobind/stl/vector.h>
#include <nanobind/stl/pair.h>

namespace nb = nanobind;
using namespace py_helpers;

void bind_fcff(nb::module_& m) {
    m.def("compute_fcff",
        [](double ebit_b, double tax, double da_b,
           double capex_b, double delta_nwc_b) -> double {
            return c2b(unwrap(ratvalue::compute_fcff({
                .ebit                       = b2c(ebit_b),
                .tax_rate                   = d2r(tax),
                .depreciation_amortization  = b2c(da_b),
                .capex                      = b2c(capex_b),
                .delta_nwc                  = b2c(delta_nwc_b),
            })));
        },
        nb::arg("ebit_b"), nb::arg("tax_rate"),
        nb::arg("da_b"),   nb::arg("capex_b"), nb::arg("delta_nwc_b") = 0.0,
        R"(
Compute FCFF.

  FCFF = EBIT*(1-t) + D&A - CapEx - delta_NWC

All monetary arguments in BRL billions.
)");

    // stages: list of (growth_rate, periods) pairs
    m.def("project_fcff",
        [](double base_b, std::vector<std::pair<double,int>> stages) {
            std::vector<ratvalue::ProjectionStage> ps;
            ps.reserve(stages.size());
            for (auto [g, n] : stages)
                ps.push_back({d2r(g), n});
            auto result = unwrap(ratvalue::project_fcff({
                .base_fcff = b2c(base_b),
                .stages    = ps,
            }));
            std::vector<double> out;
            out.reserve(result.size());
            for (auto& c : result) out.push_back(c2b(c));
            return out;
        },
        nb::arg("base_fcff_billions"),
        nb::arg("stages"),
        R"(
Project FCFF across one or more growth stages.

Parameters
----------
base_fcff_billions : FCFF in year 0 (BRL billions)
stages             : list of (growth_rate, periods) tuples
                     e.g. [(0.03, 5)] for 3% growth over 5 years

Returns
-------
list[float] : projected FCFFs in BRL billions (years 1..N)

Example
-------
>>> project_fcff(53.2, [(0.03, 5)])
[54.8, 56.4, 58.1, 59.8, 61.6]
)");
}
