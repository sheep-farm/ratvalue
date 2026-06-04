# ratvalue

C++23 implementation of Damodaran's complete valuation framework, built on
[ratmoney](https://github.com/sheep-farm/ratmoney) for exact rational monetary
arithmetic.

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

- [ratmoney](https://github.com/sheep-farm/ratmoney) ≥ 0.1 (installed in `/usr/local`)
- GCC ≥ 13 or Clang ≥ 17 (C++23)
- CMake ≥ 3.20
- GTest (test suite)

## Build

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

## Example output (excerpt)

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

A full user manual covering theory, API reference, and worked examples is
available in [`docs/manual.pdf`](docs/manual.pdf) (42 pages).

## Test suite

82 tests covering all 18 modules (GTest).

```bash
ctest --test-dir build -V
```

## License

MIT
