#include "bind_helpers.h"
#include "../normalize.h"
#include <nanobind/stl/vector.h>
#include <nanobind/stl/optional.h>
#include <optional>

namespace nb = nanobind;
using namespace py_helpers;

void bind_normalize(nb::module_& m) {
    m.def("normalize_ebit_avg",
        [](std::vector<double> historical_ebit_b) -> double {
            std::vector<ratmoney::Currency> hist;
            hist.reserve(historical_ebit_b.size());
            for (double v : historical_ebit_b) hist.push_back(b2c(v));
            return c2b(unwrap(ratvalue::normalize_ebit({
                .method          = ratvalue::NormalizationMethod::HistoricalAverage,
                .historical_ebit = hist,
                .current_revenue = b2c(0),
                .normalized_margin = {0, 1},
            })));
        },
        nb::arg("historical_ebit_billions"),
        R"(
Normalize EBIT using historical average (Damodaran ch. 8).

Parameters
----------
historical_ebit_billions : list of EBIT values in BRL billions (>= 2 years)

Returns
-------
float : normalized EBIT in BRL billions
)");

    m.def("normalize_ebit_margin",
        [](double current_revenue_b, double normalized_margin) -> double {
            return c2b(unwrap(ratvalue::normalize_ebit({
                .method            = ratvalue::NormalizationMethod::NormalizedMargin,
                .historical_ebit   = {},
                .current_revenue   = b2c(current_revenue_b),
                .normalized_margin = d2r(normalized_margin),
            })));
        },
        nb::arg("current_revenue_billions"), nb::arg("normalized_margin"),
        R"(
Normalize EBIT using current revenue * normalized operating margin.

Parameters
----------
current_revenue_billions : current revenue in BRL billions
normalized_margin        : normalized operating margin (e.g. 0.25 for 25%)

Returns
-------
float : normalized EBIT in BRL billions
)");
}
