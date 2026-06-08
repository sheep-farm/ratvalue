#include "bind_helpers.h"
#include "../fcff.h"
#include <nanobind/stl/vector.h>
#include <nanobind/stl/pair.h>

namespace nb = nanobind;
using namespace py_helpers;

void bind_fcff(nb::module_& m) {
    m.def("compute_fcff",
        [](nb::object ebit, double tax, nb::object da,
           nb::object capex, nb::object delta_nwc) -> double {
            return c2b(unwrap(ratvalue::compute_fcff({
                .ebit                       = obj2c(ebit),
                .tax_rate                   = d2r(tax),
                .depreciation_amortization  = obj2c(da),
                .capex                      = obj2c(capex),
                .delta_nwc                  = obj2c(delta_nwc),
            })));
        },
        nb::arg("ebit_b"), nb::arg("tax_rate"),
        nb::arg("da_b"),   nb::arg("capex_b"), nb::arg("delta_nwc_b") = 0.0,
        R"(
Compute FCFF.

  FCFF = EBIT*(1-t) + D&A - CapEx - delta_NWC

Monetary arguments accept float (major units, e.g. billions) or int (exact minor units).
)");

    m.def("project_fcff",
        [](nb::object base, std::vector<std::pair<double,int>> stages) {
            std::vector<ratvalue::ProjectionStage> ps;
            ps.reserve(stages.size());
            for (auto [g, n] : stages)
                ps.push_back({d2r(g), n});
            auto result = unwrap(ratvalue::project_fcff({
                .base_fcff = obj2c(base),
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
base_fcff_billions : FCFF in year 0 — float (billions) or int (exact minor units)
stages             : list of (growth_rate, periods) tuples

Returns
-------
list[float] : projected FCFFs in billions (years 1..N)

Example
-------
>>> project_fcff(53.2, [(0.03, 5)])
[54.8, 56.4, 58.1, 59.8, 61.6]
)");
}
