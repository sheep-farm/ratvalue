#include "bind_helpers.h"
#include "../startup.h"
#include <nanobind/stl/vector.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/pair.h>

namespace nb = nanobind;
using namespace py_helpers;

struct PyStartupResult {
    double enterprise_value;
    double equity_value;
    double price_per_share;
    std::vector<std::pair<std::string, double>> scenario_evs;
};

void bind_startup(nb::module_& m) {
    nb::class_<PyStartupResult>(m, "StartupValuationResult")
        .def_ro("enterprise_value", &PyStartupResult::enterprise_value,
                "Probability-weighted EV in BRL billions")
        .def_ro("equity_value",     &PyStartupResult::equity_value,
                "Equity value in BRL billions")
        .def_ro("price_per_share",  &PyStartupResult::price_per_share,
                "Price per share in BRL")
        .def_ro("scenario_evs",     &PyStartupResult::scenario_evs,
                "List of (scenario_name, ev_billions) per scenario")
        .def("__repr__", [](const PyStartupResult& r) {
            return "StartupValuationResult(ev=" + std::to_string(r.enterprise_value)
                 + "B, price=" + std::to_string(r.price_per_share) + ")";
        });

    // Each scenario: dict with keys name, probability, terminal_fcff_b,
    //                terminal_growth, wacc, years_to_terminal
    m.def("startup_valuation",
        [](std::vector<nb::dict> scenarios_py,
           double survival_prob, double failure_value_b,
           double net_debt_b, int64_t shares) -> PyStartupResult {
            std::vector<ratvalue::ValuationScenario> sc;
            sc.reserve(scenarios_py.size());
            for (auto& d : scenarios_py) {
                sc.push_back({
                    .name             = nb::cast<std::string>(d["name"]),
                    .probability      = d2r(nb::cast<double>(d["probability"])),
                    .terminal_fcff    = b2c(nb::cast<double>(d["terminal_fcff_b"])),
                    .terminal_growth  = d2r(nb::cast<double>(d["terminal_growth"])),
                    .wacc             = d2r(nb::cast<double>(d["wacc"])),
                    .years_to_terminal = nb::cast<int>(d["years_to_terminal"]),
                });
            }
            auto r = unwrap(ratvalue::startup_valuation({
                .scenarios           = sc,
                .survival_probability = d2r(survival_prob),
                .failure_value       = b2c(failure_value_b),
                .net_debt            = b2c(net_debt_b),
                .shares_outstanding  = shares,
            }));
            PyStartupResult out;
            out.enterprise_value = c2b(r.enterprise_value);
            out.equity_value     = c2b(r.equity_value);
            out.price_per_share  = c2r(r.equity_value_per_share);
            for (auto& [name, ev] : r.scenario_evs)
                out.scenario_evs.emplace_back(name, c2b(ev));
            return out;
        },
        nb::arg("scenarios"),
        nb::arg("survival_probability"),
        nb::arg("failure_value_billions"),
        nb::arg("net_debt_billions"),
        nb::arg("shares"),
        R"(
Scenario-based valuation with survival probability (Damodaran Dark Side ch. 10-12).

  EV = [sum(P_i * PV_i)] * P(survive) + V_fail * P(fail)

Parameters
----------
scenarios : list of dicts, each with keys:
    name              : str
    probability       : float (must sum to 1.0)
    terminal_fcff_b   : float, terminal FCFF in BRL billions
    terminal_growth   : float
    wacc              : float
    years_to_terminal : int
survival_probability    : P(firm reaches terminal stage)
failure_value_billions  : liquidation value in BRL billions
net_debt_billions       : to subtract from EV
shares                  : shares outstanding

Example
-------
>>> scenarios = [
...     {"name":"Bull","probability":0.35,"terminal_fcff_b":90,
...      "terminal_growth":0.02,"wacc":0.085,"years_to_terminal":5},
...     {"name":"Base","probability":0.45,"terminal_fcff_b":60,
...      "terminal_growth":0.015,"wacc":0.090,"years_to_terminal":5},
...     {"name":"Bear","probability":0.20,"terminal_fcff_b":30,
...      "terminal_growth":0.010,"wacc":0.100,"years_to_terminal":5},
... ]
>>> startup_valuation(scenarios, 0.95, 100, 250, 13_000_000_000)
)");
}
