#include "bind_helpers.h"
#include "../excess_returns.h"
#include <nanobind/stl/vector.h>
#include <nanobind/stl/pair.h>

namespace nb = nanobind;
using namespace py_helpers;

struct PyERResult {
    double firm_value;
    double asset_value;
    double pv_excess_returns;
};

void bind_excess_returns(nb::module_& m) {
    nb::class_<PyERResult>(m, "ExcessReturnsResult")
        .def_ro("firm_value",         &PyERResult::firm_value,
                "IC + PV(EVA) in BRL billions")
        .def_ro("asset_value",        &PyERResult::asset_value,
                "Invested capital (floor value) in BRL billions")
        .def_ro("pv_excess_returns",  &PyERResult::pv_excess_returns,
                "PV(EVA) in BRL billions; positive when ROIC > WACC")
        .def("__repr__", [](const PyERResult& r) {
            return "ExcessReturnsResult(firm=" + std::to_string(r.firm_value)
                 + "B, pv_eva=" + std::to_string(r.pv_excess_returns) + "B)";
        });

    auto conv = [](const ratvalue::ExcessReturnsResult& r) -> PyERResult {
        return {c2b(r.firm_value), c2b(r.asset_value), c2b(r.pv_excess_returns)};
    };

    m.def("excess_returns_value",
        [conv](double ic_b, double roic, double wacc, double g) -> PyERResult {
            return conv(unwrap(ratvalue::excess_returns_value({
                .invested_capital = b2c(ic_b),
                .roic             = d2r(roic),
                .wacc             = d2r(wacc),
                .terminal_growth  = d2r(g),
            })));
        },
        nb::arg("invested_capital_billions"),
        nb::arg("roic"), nb::arg("wacc"), nb::arg("terminal_growth"),
        R"(
Single-stage EVA model: V = IC * (ROIC - g) / (WACC - g)

Equivalent to DCF when inputs are consistent.
)");

    m.def("excess_returns_multistage",
        [conv](double ic_b, double roic,
               std::vector<std::pair<double,int>> stages,
               double wacc, double terminal_roic, double g) -> PyERResult {
            std::vector<ratvalue::ProjectionStage> ps;
            ps.reserve(stages.size());
            for (auto [gr, n] : stages) ps.push_back({d2r(gr), n});
            return conv(unwrap(ratvalue::excess_returns_multistage({
                .base_invested_capital = b2c(ic_b),
                .roic                  = d2r(roic),
                .stages                = ps,
                .wacc                  = d2r(wacc),
                .terminal_roic         = d2r(terminal_roic),
                .terminal_growth       = d2r(g),
            })));
        },
        nb::arg("invested_capital_billions"), nb::arg("roic"),
        nb::arg("stages"),
        nb::arg("wacc"), nb::arg("terminal_roic"), nb::arg("terminal_growth"),
        "Multi-stage EVA model with explicit IC growth and ROIC transition.");
}
