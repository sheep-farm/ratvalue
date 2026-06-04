# ratvalue

C++23 implementation of Damodaran's complete valuation framework, built on
[ratmoney](https://github.com/sheep-farm/ratmoney) for exact rational monetary
arithmetic. Python bindings are provided via [nanobind](https://github.com/wjakob/nanobind).

## Modules

| File | Category | Contents |
|------|----------|----------|
| `normalize` | Data | EBIT normalization (historical average / margin) |
| `capm` | Cost of capital | CAPM with country risk premium (emerging markets) |
| `beta` | Cost of capital | Hamada equation; bottom-up beta |
| `kd` | Cost of capital | Synthetic cost of debt via ICR → rating |
| `wacc` | Cost of capital | WACC |
| `capital_structure` | Structure | Optimal capital structure |
| `fcff` | Cash flows | FCFF; multi-stage projection |
| `fcfe` | Cash flows | FCFE; equity DCF |
| `growth` | Growth | ROIC, reinvestment rate, fundamental growth |
| `dcf` | Valuation | DCF (FCFF → WACC → EV) |
| `ddm` | Valuation | Multi-stage DDM; H-Model |
| `terminal` | Valuation | Consistent terminal value; TV by multiple |
| `apv` | Valuation | APV (Modigliani-Miller and Miles-Ezzell) |
| `excess_returns` | Analysis | EVA / Excess Returns |
| `relative` | Analysis | Justified P/E, P/BV, EV/EBITDA |
| `startup` | Specialized | Scenario-based valuation with survival probability |
| `distress` | Specialized | Equity as a call option (Merton 1974) |
| `bank` | Specialized | FCFE for banks (Basel III regulatory capital) |

## Design

- **Exact rational arithmetic** for all cost of capital, cash flow, and
  terminal value calculations via `ratmoney::Rational` (`int64_t` numerator /
  denominator, GCD reduced over `__int128`).
- **`double` only for discount factors** — `pow(1 + r, -t)` is inherently
  irrational; everything else is exact.
- All functions return `std::expected<T, ValuationError>`, forcing explicit
  error handling at compile time.
- Currency denomination mismatches are caught at runtime via
  `CurrencyDescription` identity checks.

## Dependencies

### C++ library
- [ratmoney](https://github.com/sheep-farm/ratmoney) ≥ 0.1 (installed in `/usr/local`)
- GCC ≥ 13 or Clang ≥ 17 (C++23)
- CMake ≥ 3.20
- GTest (test suite)

### Python bindings (optional)
- Python ≥ 3.9
- nanobind ≥ 2.0
- scikit-build-core ≥ 0.9

## Build

### C++ library and tests

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
./build/main_bin   # full Petrobras + Itaú Unibanco example
```

Optional sanitizers:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSANITIZE=ON
```

### Python bindings

**Option 1 — pip install (recommended):**

```bash
pip install nanobind scikit-build-core
pip install .
```

**Option 2 — manual CMake build:**

```bash
NB_CMAKE=$(python3 -c "import nanobind; print(nanobind.cmake_dir())")
cmake -S . -B build_py \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_PYTHON=ON \
  -DCMAKE_PREFIX_PATH="${NB_CMAKE};/usr/local"
cmake --build build_py -j$(nproc)
# .so is in build_py/python/
```

## Python usage

All monetary arguments use **BRL billions** as the unit. Rates are plain
floats (`0.05` for 5%). Per-share values (dividends, prices) use BRL reais.

```python
import ratvalue as rv

# ── Cost of capital ───────────────────────────────────────────────────────────
ke   = rv.cost_of_equity(rf=0.05, beta=1.014, erp=0.05, crp=0.03, lam=0.5)
kd_r = rv.synthetic_cost_of_debt(ebit_billions=145, interest_billions=22, rf=0.05)
wacc = rv.compute_wacc(equity_weight=2/3, ke=ke,
                       debt_weight=1/3, kd=kd_r.cost_of_debt, tax_rate=0.34)
# Ke=11.57%  Kd=5.78% (AA)  WACC=8.99%

# ── FCFF projection ───────────────────────────────────────────────────────────
fcff0 = rv.compute_fcff(ebit_b=126, tax_rate=0.34, da_b=45, capex_b=70, delta_nwc_b=5)
fcffs = rv.project_fcff(fcff0, stages=[(0.03, 5)])  # 3% for 5 years

# ── DCF ───────────────────────────────────────────────────────────────────────
result = rv.compute_dcf(
    projected_fcff_billions=fcffs,
    wacc=wacc,
    terminal_growth=0.015,
    net_debt_billions=250,
    shares=13_000_000_000,
)
print(result.price_per_share)   # 39.89

# ── Scenario valuation ────────────────────────────────────────────────────────
scenarios = [
    {"name": "Bull", "probability": 0.35, "terminal_fcff_b": 90,
     "terminal_growth": 0.02, "wacc": 0.085, "years_to_terminal": 5},
    {"name": "Base", "probability": 0.45, "terminal_fcff_b": 60,
     "terminal_growth": 0.015, "wacc": 0.090, "years_to_terminal": 5},
    {"name": "Bear", "probability": 0.20, "terminal_fcff_b": 30,
     "terminal_growth": 0.010, "wacc": 0.100, "years_to_terminal": 5},
]
sc = rv.startup_valuation(scenarios, survival_probability=0.95,
                           failure_value_billions=100,
                           net_debt_billions=250, shares=13_000_000_000)
print(sc.price_per_share)       # 24.42

# ── Bank FCFE (Itaú Unibanco) ─────────────────────────────────────────────────
rr   = rv.bank_equity_reinvestment_rate(
           net_income_b=35, current_rwa_b=1800,
           projected_rwa_b=1944, target_tier1_ratio=0.135)
fcfe = rv.compute_bank_fcfe(net_income_billions=35, equity_reinvestment_rate=rr)
print(fcfe)                     # 15.56
```

## Example output (C++ binary excerpt)

```
PETROBRAS S.A. (PETR4) -- Full Valuation 2024

  EBIT normalized (5-year average)        BRL 126.0B
  WACC (exact rational 44933/500000)      8.99%
  Ke  (CAPM + CRP, exact rational)        11.57%
  Synthetic Kd  (ICR 6.6x -> AA)         5.78%

  DCF fair value  (FCFF, Gordon TV)       BRL 39.88/share
  Market price range (2024)               BRL 38-42/share

Model comparison:
  DCF (FCFF, Gordon TV)     BRL 39.88
  APV Miles-Ezzell          BRL 28.55
  H-Model (dividends)       BRL 54.11
  Scenarios (energy trans.) BRL 24.42
  EVA / Excess Returns      BRL 58.51
```

## Documentation

A full user manual (51 pages) covering theory, C++ and Python API reference,
and worked examples is available in two formats:

- **PDF:** [`docs/manual.pdf`](docs/manual.pdf)
- **Raw download:** `https://github.com/sheep-farm/ratvalue/raw/master/docs/manual.pdf`

## Test suite

82 tests covering all 18 modules (GTest).

```bash
ctest --test-dir build -V
```

## License

MIT
