#include "bind_helpers.h"
#include "../normalize.h"
#include <nanobind/stl/vector.h>
#include <nanobind/stl/optional.h>
#include <optional>

namespace nb = nanobind;
using namespace py_helpers;

void bind_normalize(nb::module_& m) {
    m.def("normalize_ebit_avg",
        [](std::vector<nb::object> historical_ebit) -> double {
            std::vector<ratmoney::Currency> hist;
            hist.reserve(historical_ebit.size());
            for (auto& v : historical_ebit) hist.push_back(obj2c(v));
            return c2b(unwrap(ratvalue::normalize_ebit({
                .method          = ratvalue::NormalizationMethod::HistoricalAverage,
                .historical_ebit = hist,
                .current_revenue = b2c(0),
                .normalized_margin = {0, 1},
            })));
        },
        nb::arg("historical_ebit"),
        R"(
Normalize EBIT using historical average (Damodaran ch. 8).

Parameters
----------
historical_ebit : list of EBIT values — float (major units) or int (exact minor units)
                  minimum 2 years

Returns
-------
float : normalized EBIT in major units
)");

    m.def("normalize_ebit_margin",
        [](nb::object current_revenue, double normalized_margin) -> double {
            return c2b(unwrap(ratvalue::normalize_ebit({
                .method            = ratvalue::NormalizationMethod::NormalizedMargin,
                .historical_ebit   = {},
                .current_revenue   = obj2c(current_revenue),
                .normalized_margin = d2r(normalized_margin),
            })));
        },
        nb::arg("current_revenue"), nb::arg("normalized_margin"),
        R"(
Normalize EBIT using current revenue * normalized operating margin.

Parameters
----------
current_revenue   : float (major units) or int (exact minor units)
normalized_margin : normalized operating margin (e.g. 0.25 for 25%)

Returns
-------
float : normalized EBIT in major units
)");
}
