#include "bind_helpers.h"
#include "../relative.h"

namespace nb = nanobind;
using namespace py_helpers;

struct PyJustifiedMultiples {
    double pe_ratio;
    double pb_ratio;
    double ev_ebitda;
    double g_equity;
    double implied_reinvestment;
};

void bind_relative(nb::module_& m) {
    nb::class_<PyJustifiedMultiples>(m, "JustifiedMultiples")
        .def_ro("pe_ratio",             &PyJustifiedMultiples::pe_ratio,
                "Justified P/E ratio")
        .def_ro("pb_ratio",             &PyJustifiedMultiples::pb_ratio,
                "Justified P/BV ratio")
        .def_ro("ev_ebitda",            &PyJustifiedMultiples::ev_ebitda,
                "Justified EV/EBITDA ratio")
        .def_ro("g_equity",             &PyJustifiedMultiples::g_equity,
                "Fundamental equity growth = ROE * retention")
        .def_ro("implied_reinvestment", &PyJustifiedMultiples::implied_reinvestment,
                "Implied firm reinvestment rate = g / ROIC")
        .def("__repr__", [](const PyJustifiedMultiples& r) {
            return "JustifiedMultiples(PE=" + std::to_string(r.pe_ratio)
                 + "x, PB=" + std::to_string(r.pb_ratio)
                 + "x, EV/EBITDA=" + std::to_string(r.ev_ebitda) + "x)";
        });

    m.def("compute_justified_multiples",
        [](double ke, double wacc, double roe,
           double payout, double roic, double tax) -> PyJustifiedMultiples {
            auto r = unwrap(ratvalue::compute_justified_multiples({
                .cost_of_equity = d2r(ke),
                .wacc           = d2r(wacc),
                .roe            = d2r(roe),
                .payout_ratio   = d2r(payout),
                .roic           = d2r(roic),
                .tax_rate       = d2r(tax),
            }));
            return {r2d(r.pe_ratio), r2d(r.pb_ratio), r2d(r.ev_ebitda),
                    r2d(r.g_equity), r2d(r.implied_reinvestment)};
        },
        nb::arg("ke"), nb::arg("wacc"),
        nb::arg("roe"), nb::arg("payout_ratio"),
        nb::arg("roic"), nb::arg("tax_rate"),
        R"(
Compute fundamentals-anchored justified multiples (Damodaran ch. 17-20).

  P/E    = payout / (Ke - g)
  P/BV   = ROE * payout / (Ke - g)
  EV/EBITDA = (1-t)*(1-RR) / (WACC - g)

where g = ROE * (1 - payout).
)");
}
