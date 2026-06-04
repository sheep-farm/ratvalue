#include <nanobind/nanobind.h>

namespace nb = nanobind;

// Forward declarations
void bind_capm(nb::module_&);
void bind_beta(nb::module_&);
void bind_kd(nb::module_&);
void bind_wacc(nb::module_&);
void bind_normalize(nb::module_&);
void bind_fcff(nb::module_&);
void bind_fcfe(nb::module_&);
void bind_growth(nb::module_&);
void bind_dcf(nb::module_&);
void bind_ddm(nb::module_&);
void bind_terminal(nb::module_&);
void bind_apv(nb::module_&);
void bind_excess_returns(nb::module_&);
void bind_relative(nb::module_&);
void bind_capital_structure(nb::module_&);
void bind_startup(nb::module_&);
void bind_distress(nb::module_&);
void bind_bank(nb::module_&);

NB_MODULE(ratvalue, m) {
    m.doc() = R"(
ratvalue — Damodaran valuation framework in C++23, Python bindings via nanobind.

All monetary inputs/outputs are in BRL billions unless noted as per-share
(which are in BRL reais). Rates are plain floats (e.g. 0.05 for 5%).

Modules
-------
Cost of capital : cost_of_equity, lever_beta, unlever_beta, bottom_up_beta,
                  synthetic_cost_of_debt, compute_wacc
Data            : normalize_ebit_avg, normalize_ebit_margin
Cash flows      : compute_fcff, project_fcff, compute_fcfe, compute_fcfe_dcf
Growth          : compute_roic, compute_roe, compute_reinvestment_rate,
                  fundamental_growth_firm, fundamental_growth_equity
Valuation       : compute_dcf, compute_ddm, compute_h_model,
                  check_terminal_consistency, consistent_terminal_value,
                  terminal_value_by_multiple,
                  compute_apv, compute_apv_miles_ezzell,
                  excess_returns_value, excess_returns_multistage,
                  compute_justified_multiples,
                  optimal_capital_structure,
                  startup_valuation, equity_as_option
Banks           : bank_equity_reinvestment_rate, compute_bank_fcfe
)";

    bind_capm(m);
    bind_beta(m);
    bind_kd(m);
    bind_wacc(m);
    bind_normalize(m);
    bind_fcff(m);
    bind_fcfe(m);
    bind_growth(m);
    bind_dcf(m);
    bind_ddm(m);
    bind_terminal(m);
    bind_apv(m);
    bind_excess_returns(m);
    bind_relative(m);
    bind_capital_structure(m);
    bind_startup(m);
    bind_distress(m);
    bind_bank(m);
}
