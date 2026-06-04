#include "bind_helpers.h"
#include "../terminal.h"

namespace nb = nanobind;
using namespace py_helpers;

struct PyTerminalConsistency {
    double implied_reinvestment_rate;
    double return_spread;
    bool   value_creating;
    bool   reinvestment_feasible;
};

void bind_terminal(nb::module_& m) {
    nb::class_<PyTerminalConsistency>(m, "TerminalConsistency")
        .def_ro("implied_reinvestment_rate", &PyTerminalConsistency::implied_reinvestment_rate,
                "g / ROIC_stable")
        .def_ro("return_spread",             &PyTerminalConsistency::return_spread,
                "ROIC_stable - WACC")
        .def_ro("value_creating",            &PyTerminalConsistency::value_creating,
                "True if ROIC > WACC")
        .def_ro("reinvestment_feasible",     &PyTerminalConsistency::reinvestment_feasible,
                "True if 0 <= g/ROIC <= 1")
        .def("__repr__", [](const PyTerminalConsistency& r) {
            return std::string("TerminalConsistency(value_creating=")
                 + (r.value_creating ? "True" : "False")
                 + ", rr=" + std::to_string(r.implied_reinvestment_rate) + ")";
        });

    m.def("check_terminal_consistency",
        [](double wacc, double g, double roic) -> PyTerminalConsistency {
            auto r = ratvalue::check_terminal_consistency(d2r(wacc), d2r(g), d2r(roic));
            return {r2d(r.implied_reinvestment_rate), r2d(r.return_spread),
                    r.value_creating, r.reinvestment_feasible};
        },
        nb::arg("wacc"), nb::arg("terminal_growth"), nb::arg("terminal_roic"),
        R"(
Audit terminal value assumptions for internal consistency.

Returns
-------
TerminalConsistency with implied reinvestment rate, ROIC-WACC spread,
and boolean flags for value creation and feasibility.
)");

    m.def("consistent_terminal_value",
        [](double stable_nopat_b, double wacc, double g, double roic) -> double {
            return c2b(unwrap(ratvalue::consistent_terminal_value({
                .stable_nopat    = b2c(stable_nopat_b),
                .wacc            = d2r(wacc),
                .terminal_growth = d2r(g),
                .terminal_roic   = d2r(roic),
            })));
        },
        nb::arg("stable_nopat_billions"), nb::arg("wacc"),
        nb::arg("terminal_growth"), nb::arg("terminal_roic"),
        R"(
ROIC-consistent terminal value (Damodaran ch. 12).

  TV = NOPAT * (1 - g/ROIC) / (WACC - g)

Returns
-------
float : terminal value in BRL billions
)");

    m.def("terminal_value_by_multiple",
        [](double ebitda_b, double multiple) -> double {
            return c2b(unwrap(ratvalue::terminal_value_by_multiple(
                b2c(ebitda_b), d2r(multiple, 10))));
        },
        nb::arg("terminal_ebitda_billions"), nb::arg("ev_ebitda_multiple"),
        "TV = terminal_EBITDA * EV/EBITDA_multiple  (in BRL billions)");
}
