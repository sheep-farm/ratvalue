#include "bind_helpers.h"
#include "../ddm.h"
#include <nanobind/stl/vector.h>
#include <nanobind/stl/pair.h>

namespace nb = nanobind;
using namespace py_helpers;

struct PyDDMResult {
    double intrinsic_value;
    double pv_dividends;
    double pv_terminal_value;
};

struct PyHModelResult {
    double intrinsic_value;
    double tv_multiplier;
};

void bind_ddm(nb::module_& m) {
    nb::class_<PyDDMResult>(m, "DDMResult")
        .def_ro("intrinsic_value",   &PyDDMResult::intrinsic_value,
                "Intrinsic value per share in BRL")
        .def_ro("pv_dividends",      &PyDDMResult::pv_dividends,
                "PV of explicit dividends in BRL")
        .def_ro("pv_terminal_value", &PyDDMResult::pv_terminal_value,
                "PV of terminal value in BRL")
        .def("__repr__", [](const PyDDMResult& r) {
            return "DDMResult(value=" + std::to_string(r.intrinsic_value) + ")";
        });

    nb::class_<PyHModelResult>(m, "HModelResult")
        .def_ro("intrinsic_value", &PyHModelResult::intrinsic_value,
                "Intrinsic value per share in BRL")
        .def_ro("tv_multiplier",   &PyHModelResult::tv_multiplier,
                "Exact rational multiplier applied to D0")
        .def("__repr__", [](const PyHModelResult& r) {
            return "HModelResult(value=" + std::to_string(r.intrinsic_value) + ")";
        });

    // stages: list of (growth_rate, periods)
    m.def("compute_ddm",
        [](double base_div, std::vector<std::pair<double,int>> stages,
           double ke, double g_stable) -> PyDDMResult {
            std::vector<ratvalue::ProjectionStage> ps;
            ps.reserve(stages.size());
            for (auto [g, n] : stages) ps.push_back({d2r(g), n});
            auto r = unwrap(ratvalue::compute_ddm({
                .base_dividend_per_share = r2c(base_div),
                .stages                  = ps,
                .cost_of_equity          = d2r(ke),
                .stable_growth           = d2r(g_stable),
            }));
            return {c2r(r.intrinsic_value_per_share),
                    c2r(r.pv_dividends),
                    c2r(r.pv_terminal_value)};
        },
        nb::arg("base_dividend"), nb::arg("stages"),
        nb::arg("cost_of_equity"), nb::arg("stable_growth"),
        R"(
Multi-stage Dividend Discount Model.

Parameters
----------
base_dividend   : D0, current dividend per share in BRL
stages          : list of (growth_rate, periods) tuples
cost_of_equity  : Ke
stable_growth   : terminal perpetuity growth rate
)");

    m.def("compute_h_model",
        [](double base_div, double g_high, double g_stable,
           double half_life, double ke) -> PyHModelResult {
            auto r = unwrap(ratvalue::compute_h_model({
                .base_dividend  = r2c(base_div),
                .high_growth    = d2r(g_high),
                .stable_growth  = d2r(g_stable),
                .half_life      = d2r(half_life),
                .cost_of_equity = d2r(ke),
            }));
            return {c2r(r.intrinsic_value), r2d(r.tv_multiplier)};
        },
        nb::arg("base_dividend"), nb::arg("high_growth"),
        nb::arg("stable_growth"), nb::arg("half_life"),
        nb::arg("cost_of_equity"),
        R"(
H-Model (Fuller & Hsia 1984): growth declines linearly from high to stable over 2H years.

  P = D0 * [(1+g_stable) + H*(g_high - g_stable)] / (Ke - g_stable)

Parameters
----------
base_dividend  : D0 in BRL per share
high_growth    : initial high growth rate
stable_growth  : terminal stable growth rate
half_life      : H in years (half the transition period)
cost_of_equity : Ke
)");
}
