# Plano de Implementação — ratvalue (Damodaran completo)

## Fase 1 — Fluxo de Caixa para o Acionista
- [x] `fcfe.h/.cpp` — `compute_fcfe`, `FCFEDCFInputs/Result`, `compute_fcfe_dcf`

## Fase 2 — Crescimento Fundamental
- [x] `growth.h/.cpp` — ROIC, taxa de reinvestimento, `g = ROIC × RR`, `g = ROE × retention`

## Fase 3 — Beta e Custo de Capital
- [x] `beta.h/.cpp` — Hamada (`lever_beta`, `unlever_beta`), `bottom_up_beta`
- [x] `kd.h/.cpp` — Kd sintético (ICR → rating → spread, tabelas Damodaran large/small firm)
- [x] `capital_structure.h/.cpp` — estrutura ótima de capital (WACC mínimo via iteração ICR)

## Fase 4 — Modelos de Transição
- [x] H-Model (declínio linear de crescimento, fórmula fechada) — em `ddm.h/.cpp`

## Fase 5 — Valor Terminal Estendido
- [x] `terminal.h/.cpp` — `check_terminal_consistency`, `consistent_terminal_value`, `terminal_value_by_multiple`

## Fase 6 — APV (Adjusted Present Value)
- [x] `apv.h/.cpp` — Modigliani-Miller (`compute_apv`) + Miles-Ezzell (`compute_apv_miles_ezzell`)

## Fase 7 — Valuation Relativo
- [x] `relative.h/.cpp` — P/L, P/VP, EV/EBITDA justificados com crescimento fundamental

## Fase 8 — Modelos Especializados
- [x] `excess_returns.h/.cpp` — EVA / Excess Returns (single-stage exato + multi-stage)
- [x] `startup.h/.cpp` — valuation por cenários com probabilidade de sobrevivência
- [x] `distress.h/.cpp` — equity como opção de compra (Black-Scholes / Merton)
- [x] `bank.h/.cpp` — FCFE para instituições financeiras (capital regulatório / Tier 1)
- [x] `normalize.h/.cpp` — normalização de resultados (média histórica + margem normalizada)
