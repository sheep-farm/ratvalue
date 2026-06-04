#include "bind_helpers.h"
#include "../apv.h"
#include <nanobind/stl/vector.h>

namespace nb = nanobind;
using namespace py_helpers;

struct PyAPVResult {
    double unlevered_firm_value;
    double pv_tax_shield;
    double apv;
    double equity_value;
    double price_per_share;
};

void bind_apv(nb::module_& m) {
    nb::class_<PyAPVResult>(m, "APVResult")
        .def_ro("unlevered_firm_value", &PyAPVResult::unlevered_firm_value,
                "PV of FCFFs discounted at Ku (BRL billions)")
        .def_ro("pv_tax_shield",        &PyAPVResult::pv_tax_shield,
                "Present value of tax shields (BRL billions)")
        .def_ro("apv",                  &PyAPVResult::apv,
                "Adjusted present value = unlevered + tax shield (BRL billions)")
        .def_ro("equity_value",         &PyAPVResult::equity_value,
                "APV minus net debt (BRL billions)")
        .def_ro("price_per_share",      &PyAPVResult::price_per_share,
                "Intrinsic price per share in BRL")
        .def("__repr__", [](const PyAPVResult& r) {
            return "APVResult(apv=" + std::to_string(r.apv)
                 + "B, ts=" + std::to_string(r.pv_tax_shield)
                 + "B, price=" + std::to_string(r.price_per_share) + ")";
        });

    auto conv = [](const ratvalue::APVResult& r) -> PyAPVResult {
        return {c2b(r.unlevered_firm_value), c2b(r.pv_tax_shield),
                c2b(r.apv), c2b(r.equity_value),
                c2r(r.equity_value_per_share)};
    };

    m.def("compute_apv",
        [conv](std::vector<double> fcffs_b, double g, double ku,
               double debt_b, double tax, double net_debt_b,
               int64_t shares) -> PyAPVResult {
            std::vector<ratmoney::Currency> cflows;
            cflows.reserve(fcffs_b.size());
            for (double v : fcffs_b) cflows.push_back(b2c(v));
            return conv(unwrap(ratvalue::compute_apv({
                .projected_fcff            = cflows,
                .terminal_growth           = d2r(g),
                .unlevered_cost_of_equity  = d2r(ku),
                .debt_market_value         = b2c(debt_b),
                .tax_rate                  = d2r(tax),
                .net_debt                  = b2c(net_debt_b),
                .shares_outstanding        = shares,
            })));
        },
        nb::arg("projected_fcff_billions"), nb::arg("terminal_growth"),
        nb::arg("ku"), nb::arg("debt_billions"), nb::arg("tax_rate"),
        nb::arg("net_debt_billions"), nb::arg("shares"),
        R"(
APV using Modigliani-Miller tax shield: PV(TS) = t * D.
Assumes debt is permanent.
)");

    m.def("compute_apv_miles_ezzell",
        [conv](std::vector<double> fcffs_b, double g, double ku, double kd,
               double debt_b, double tax, double net_debt_b,
               int64_t shares) -> PyAPVResult {
            std::vector<ratmoney::Currency> cflows;
            cflows.reserve(fcffs_b.size());
            for (double v : fcffs_b) cflows.push_back(b2c(v));
            return conv(unwrap(ratvalue::compute_apv_miles_ezzell({
                .projected_fcff            = cflows,
                .terminal_growth           = d2r(g),
                .unlevered_cost_of_equity  = d2r(ku),
                .cost_of_debt              = d2r(kd),
                .debt_market_value         = b2c(debt_b),
                .tax_rate                  = d2r(tax),
                .net_debt                  = b2c(net_debt_b),
                .shares_outstanding        = shares,
            })));
        },
        nb::arg("projected_fcff_billions"), nb::arg("terminal_growth"),
        nb::arg("ku"), nb::arg("kd"),
        nb::arg("debt_billions"), nb::arg("tax_rate"),
        nb::arg("net_debt_billions"), nb::arg("shares"),
        R"(
APV using Miles-Ezzell tax shield: assumes firm rebalances D/V each period.

  PV(TS)_ME = t * Kd * D * (1+Ku) / [(1+Kd) * (Ku - g)]

More conservative than MM; recommended for stable leverage.
)");
}
