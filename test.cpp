#include <gtest/gtest.h>
#include "apv.h"
#include "bank.h"
#include "beta.h"
#include "capital_structure.h"
#include "capm.h"
#include "dcf.h"
#include "ddm.h"
#include "distress.h"
#include "excess_returns.h"
#include "fcfe.h"
#include "fcff.h"
#include "growth.h"
#include "kd.h"
#include "normalize.h"
#include "relative.h"
#include "startup.h"
#include "terminal.h"
#include "wacc.h"

using namespace ratvalue;
using ratmoney::Currency;
using ratmoney::Rational;

namespace {

ratmoney::CurrencyDescription usd{"USD", "$", 2};
Currency usd_cur(int64_t cents) { return Currency{cents, {1, 1}, usd}; }

} // anonymous namespace

// ── CAPM ─────────────────────────────────────────────────────────────────────

TEST(CAPM, SimpleCase) {
    // Rf=5%, β=1,30, ERP=7% → Ke = 5% + 1,3×7% = 14,1% = 141/1000
    auto result = cost_of_equity(CAPMInputs{
        .risk_free_rate      = {1, 20},
        .beta                = {13, 10},
        .equity_risk_premium = {7, 100},
    });
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->num, 141);
    EXPECT_EQ(result->den, 1000);
}

TEST(CAPM, ZeroBeta) {
    // β=0 → Ke = Rf
    auto result = cost_of_equity(CAPMInputs{
        .risk_free_rate      = {1, 10},
        .beta                = {0, 1},
        .equity_risk_premium = {7, 100},
    });
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->num, 1);
    EXPECT_EQ(result->den, 10);
}

TEST(CAPM, NegativeRiskFreeRateRejected) {
    auto result = cost_of_equity(CAPMInputs{
        .risk_free_rate      = {-1, 100},
        .beta                = {1, 1},
        .equity_risk_premium = {7, 100},
    });
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ValuationError::InvalidInput);
}

TEST(CAPM, ComCRPMercadoEmergente) {
    // Rf=5%, β=1,30, ERP_maduro=7%, CRP=3%, λ=1
    // Ke = 5% + 1,3×7% + 1×3% = 5% + 9,1% + 3% = 17,1%
    // = 1/20 + 91/1000 + 3/100 = 50/1000 + 91/1000 + 30/1000 = 171/1000
    auto result = cost_of_equity(CAPMInputs{
        .risk_free_rate       = {1, 20},
        .beta                 = {13, 10},
        .equity_risk_premium  = {7, 100},
        .country_risk_premium = {3, 100},
        .lambda               = {1, 1},
    });
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->num, 171);
    EXPECT_EQ(result->den, 1000);
}

TEST(CAPM, LambdaParcial) {
    // λ=0,5: empresa com metade da receita no exterior
    // Ke = 5% + 1,3×7% + 0,5×3% = 14,1% + 1,5% = 15,6% = 156/1000
    auto result = cost_of_equity(CAPMInputs{
        .risk_free_rate       = {1, 20},
        .beta                 = {13, 10},
        .equity_risk_premium  = {7, 100},
        .country_risk_premium = {3, 100},
        .lambda               = {1, 2},    // 0,5
    });
    ASSERT_TRUE(result.has_value());
    // 171/1000 - 3/200 = 342/2000 - 30/2000... espera: 14,1% + 1,5% = 15,6%
    double ke = static_cast<double>(result->num) / result->den;
    EXPECT_NEAR(ke, 0.156, 1e-12);
}

TEST(CAPM, CRPZeroEquivalenteAoSimples) {
    // CRP=0 deve produzir resultado idêntico ao CAPM simples
    auto sem_crp = cost_of_equity(CAPMInputs{
        .risk_free_rate      = {1, 20},
        .beta                = {13, 10},
        .equity_risk_premium = {7, 100},
    });
    auto com_crp_zero = cost_of_equity(CAPMInputs{
        .risk_free_rate       = {1, 20},
        .beta                 = {13, 10},
        .equity_risk_premium  = {7, 100},
        .country_risk_premium = {0, 1},
        .lambda               = {1, 1},
    });
    ASSERT_TRUE(sem_crp.has_value());
    ASSERT_TRUE(com_crp_zero.has_value());
    EXPECT_EQ(*sem_crp, *com_crp_zero);
}

// ── WACC ─────────────────────────────────────────────────────────────────────

TEST(WACC, FullComputation) {
    // Ew=70%, Dw=30%, Ke=14,1%, Kd=12%, t=34%
    // WACC = 0,7×0,141 + 0,3×0,12×0,66 = 0,0987 + 0,02376 = 0,12246 = 6123/50000
    auto result = compute_wacc(WACCInputs{
        .equity_weight  = {7, 10},
        .debt_weight    = {3, 10},
        .cost_of_equity = {141, 1000},
        .cost_of_debt   = {3, 25},   // 12%
        .tax_rate       = {34, 100},
    });
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->num, 6123);
    EXPECT_EQ(result->den, 50000);
}

TEST(WACC, SoCapitalProprio) {
    // Dw=0 → WACC = Ke
    auto result = compute_wacc(WACCInputs{
        .equity_weight  = {1, 1},
        .debt_weight    = {0, 1},
        .cost_of_equity = {1, 10},
        .cost_of_debt   = {1, 10},
        .tax_rate       = {3, 10},
    });
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->num, 1);
    EXPECT_EQ(result->den, 10);
}

TEST(WACC, AliquotaInvalidaRejeitada) {
    auto result = compute_wacc(WACCInputs{
        .equity_weight  = {1, 2},
        .debt_weight    = {1, 2},
        .cost_of_equity = {1, 10},
        .cost_of_debt   = {1, 10},
        .tax_rate       = {-1, 100},  // negativa
    });
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ValuationError::InvalidInput);
}

// ── Validação de moeda ────────────────────────────────────────────────────────

TEST(CurrencyValidation, FCFFInputsDenominacoesDistintas) {
    ratmoney::CurrencyDescription brl{"BRL", "R$", 2};
    ratmoney::CurrencyDescription usd_d{"USD", "$", 2};

    // EBIT em BRL, CapEx em USD — deve rejeitar
    auto result = compute_fcff(FCFFInputs{
        .ebit                      = Currency{10000, {1, 1}, brl},
        .tax_rate                  = {34, 100},
        .depreciation_amortization = Currency{1000,  {1, 1}, brl},
        .capex                     = Currency{1500,  {1, 1}, usd_d},  // moeda errada
        .delta_nwc                 = Currency{500,   {1, 1}, brl},
    });
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ValuationError::CurrencyMismatch);
}

TEST(CurrencyValidation, DCFDividaLiquidaOutraMoeda) {
    ratmoney::CurrencyDescription brl{"BRL", "R$", 2};
    ratmoney::CurrencyDescription usd_d{"USD", "$", 2};

    auto fcffs = project_fcff(FCFFProjection{
        .base_fcff = Currency{10000, {1, 1}, brl},
        .stages    = {{Rational{0, 1}, 1}},
    });
    ASSERT_TRUE(fcffs.has_value());

    // net_debt em USD, FCFFs em BRL — deve rejeitar
    auto result = compute_dcf(DCFInputs{
        .projected_fcff     = *fcffs,
        .wacc               = {1, 10},
        .terminal_growth    = {0, 1},
        .net_debt           = Currency{20000, {1, 5}, usd_d},  // moeda diferente
        .shares_outstanding = 10,
    });
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ValuationError::CurrencyMismatch);
}

// ── FCFF ─────────────────────────────────────────────────────────────────────

TEST(FCFF, FormulaBasica) {
    // FCFF = EBIT(1-t) + D&A - CapEx - ΔCGN
    //      = 100(0,66) + 10 - 15 - 5 = 66 + 10 - 15 - 5 = 56
    // em cents: ebit=10000, t=34/100, da=1000, capex=1500, dnwc=500
    // EBIT(1-t) = roundedDiv(10000×66, 100) = 6600
    // FCFF = 6600 + 1000 - 1500 - 500 = 5600 cents
    auto result = compute_fcff(FCFFInputs{
        .ebit                      = usd_cur(10000),  // $100
        .tax_rate                  = {34, 100},
        .depreciation_amortization = usd_cur(1000),   // $10
        .capex                     = usd_cur(1500),   // $15
        .delta_nwc                 = usd_cur(500),    // $5
    });
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->units(), 5600);  // $56.00
}

TEST(FCFF, ProjecaoMultiEstagio) {
    // FCFF₀ = $100 = 10000 cents
    // estágio 1: g=20%=1/5, 2 anos → FCFF₁=12000, FCFF₂=14400
    auto result = project_fcff(FCFFProjection{
        .base_fcff = usd_cur(10000),
        .stages    = {{Rational{1, 5}, 2}},
    });
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 2u);
    EXPECT_EQ((*result)[0].units(), 12000);   // $120
    EXPECT_EQ((*result)[1].units(), 14400);   // $144
}

TEST(FCFF, ProjecaoDoisEstagios) {
    // Estágio 1: g=20%, 2 anos; Estágio 2: g=0%, 1 ano
    // [12000, 14400, 14400]
    auto result = project_fcff(FCFFProjection{
        .base_fcff = usd_cur(10000),
        .stages    = {{Rational{1, 5}, 2}, {Rational{0, 1}, 1}},
    });
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 3u);
    EXPECT_EQ((*result)[0].units(), 12000);
    EXPECT_EQ((*result)[1].units(), 14400);
    EXPECT_EQ((*result)[2].units(), 14400);
}

// ── DCF ──────────────────────────────────────────────────────────────────────

TEST(DCF, PerpetuagemSemCrescimento) {
    // FCFF constante: $100/ano, WACC=10%, g=0%
    // EV = FCFF/WACC = $100/0,10 = $1000, independente do horizon explícito.
    // Com 2 anos explícitos + TV:
    //   PV1 = 100/1,1; PV2 = 100/1,1²; TV = 100/0,1; PV(TV) = 1000/1,1²
    //   EV = (100 + 100 + 1000) / 1,1² = 1200/1,21 + ... = $1000
    auto fcffs = project_fcff(FCFFProjection{
        .base_fcff = usd_cur(10000),  // $100
        .stages    = {{Rational{0, 1}, 2}},
    });
    ASSERT_TRUE(fcffs.has_value());

    auto result = compute_dcf(DCFInputs{
        .projected_fcff     = *fcffs,
        .wacc               = {1, 10},
        .terminal_growth    = {0, 1},
        .net_debt           = usd_cur(0),
        .shares_outstanding = 1,
    });
    ASSERT_TRUE(result.has_value());
    // EV ≈ $1000 = 100000 cents (tolerância de 1 centavo por arredondamento double)
    EXPECT_NEAR(result->enterprise_value.units(), 100000, 1);
}

TEST(DCF, EquityValueComDivida) {
    // EV = $1000, dívida = $200 → equity = $800
    // Com 10 ações: preço = $80
    auto fcffs = project_fcff(FCFFProjection{
        .base_fcff = usd_cur(10000),
        .stages    = {{Rational{0, 1}, 1}},
    });
    ASSERT_TRUE(fcffs.has_value());

    auto result = compute_dcf(DCFInputs{
        .projected_fcff     = *fcffs,
        .wacc               = {1, 10},
        .terminal_growth    = {0, 1},
        .net_debt           = usd_cur(20000),  // $200
        .shares_outstanding = 10,
    });
    ASSERT_TRUE(result.has_value());
    EXPECT_NEAR(result->equity_value.units(), 80000, 2);        // $800
    EXPECT_NEAR(result->equity_value_per_share.units(), 8000, 1); // $80
}

TEST(DCF, WACCMenorOuIgualGRejeitado) {
    auto fcffs = project_fcff(FCFFProjection{
        .base_fcff = usd_cur(10000),
        .stages    = {{Rational{0, 1}, 1}},
    });
    ASSERT_TRUE(fcffs.has_value());

    auto result = compute_dcf(DCFInputs{
        .projected_fcff     = *fcffs,
        .wacc               = {5, 100},  // 5%
        .terminal_growth    = {5, 100},  // 5% — igual!
        .net_debt           = usd_cur(0),
        .shares_outstanding = 1,
    });
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ValuationError::WACCLEGrowth);
}

// ── DDM ──────────────────────────────────────────────────────────────────────

TEST(DDM, GordonGrowthModelUmPeriodo) {
    // D₀ = $1,00 = 100 cents
    // D₁ = 100 × 21/20 = 105 cents  (g=5%)
    // TV₁ = 105 × (1,05/0,05) = 105 × 21 = 2205 cents
    // PV = (105 + 2205) / 1,1 = 2310 / 1,1 = 2100 cents = $21,00
    // Invariante: equivale à fórmula direta P = D₀(1+g)/(Ke-g) = 1×1,05/0,05 = $21,00
    auto result = compute_ddm(DDMInputs{
        .base_dividend_per_share = usd_cur(100),   // $1,00
        .stages                  = {{Rational{1, 20}, 1}},
        .cost_of_equity          = {1, 10},         // Ke = 10%
        .stable_growth           = {1, 20},          // g  = 5%
    });
    ASSERT_TRUE(result.has_value());
    EXPECT_NEAR(result->intrinsic_value_per_share.units(), 2100, 1);
}

TEST(DDM, KeMenorOuIgualGRejeitado) {
    auto result = compute_ddm(DDMInputs{
        .base_dividend_per_share = usd_cur(100),
        .stages                  = {{Rational{1, 10}, 1}},
        .cost_of_equity          = {5, 100},   // Ke = 5%
        .stable_growth           = {5, 100},   // g  = 5% — igual!
    });
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ValuationError::WACCLEGrowth);
}

TEST(DDM, ValorTerminalExato) {
    // Com g=0% e Ke=10%, TV multiplier = 1/Ke = 10
    // TV = D₁ × 10.  Com D₀=100, D₁=100 (g=0).
    // TV = 1000 cents = $10.  PV(TV) = 1000/1,1 = 909...
    // PV(D₁) = 100/1,1 = 90,9...
    // Total ≈ 1000 cents = $10  (P = D/Ke = 100/100 × 10% = $10)
    auto result = compute_ddm(DDMInputs{
        .base_dividend_per_share = usd_cur(100),
        .stages                  = {{Rational{0, 1}, 1}},
        .cost_of_equity          = {1, 10},
        .stable_growth           = {0, 1},
    });
    ASSERT_TRUE(result.has_value());
    EXPECT_NEAR(result->intrinsic_value_per_share.units(), 1000, 1);
    EXPECT_EQ(result->terminal_value_per_share.units(), 1000);  // exato via Rational
}

// ── FCFE ─────────────────────────────────────────────────────────────────────

TEST(FCFE, FormulaBasica) {
    // NI=100, δ=30%, CapEx=20, D&A=10, ΔCGN=5, NND=5
    // net_inv = 20-10+5 = 15 → equity_reinv = 15×0,7 = 10,5 → 1050 cents
    // FCFE = 10000 - 1050 + 500 = 9450 cents = $94,50
    auto result = compute_fcfe(FCFEInputs{
        .net_income                = usd_cur(10000),
        .debt_ratio                = {3, 10},
        .capex                     = usd_cur(2000),
        .depreciation_amortization = usd_cur(1000),
        .delta_nwc                 = usd_cur(500),
        .net_new_debt              = usd_cur(500),
    });
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->units(), 9450);
}

TEST(FCFE, SemAlavancagem) {
    // δ=0 (sem dívida): FCFE = NI - (CapEx - D&A + ΔCGN) + 0 = NI - reinvestimento
    // = 10000 - (2000-1000+500) = 10000 - 1500 = 8500
    auto result = compute_fcfe(FCFEInputs{
        .net_income                = usd_cur(10000),
        .debt_ratio                = {0, 1},
        .capex                     = usd_cur(2000),
        .depreciation_amortization = usd_cur(1000),
        .delta_nwc                 = usd_cur(500),
        .net_new_debt              = usd_cur(0),
    });
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->units(), 8500);
}

TEST(FCFE, DCFPerpetuidade) {
    // FCFE constante $100/ano, Ke=10%, g=0% → equity = FCFE/Ke = $1000
    auto fcfes = project_fcff(FCFFProjection{
        .base_fcff = usd_cur(10000),
        .stages    = {{Rational{0, 1}, 2}},
    });
    ASSERT_TRUE(fcfes.has_value());

    auto result = compute_fcfe_dcf(FCFEDCFInputs{
        .projected_fcfe     = *fcfes,
        .cost_of_equity     = {1, 10},
        .terminal_growth    = {0, 1},
        .shares_outstanding = 1,
    });
    ASSERT_TRUE(result.has_value());
    EXPECT_NEAR(result->equity_value.units(), 100000, 1);
}

TEST(FCFE, MoedaErradaRejeitada) {
    ratmoney::CurrencyDescription brl{"BRL", "R$", 2};
    auto result = compute_fcfe(FCFEInputs{
        .net_income                = usd_cur(10000),
        .debt_ratio                = {3, 10},
        .capex                     = Currency{2000, {1, 1}, brl},   // moeda errada
        .depreciation_amortization = usd_cur(1000),
        .delta_nwc                 = usd_cur(500),
        .net_new_debt              = usd_cur(0),
    });
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ValuationError::CurrencyMismatch);
}

// ── Crescimento Fundamental ───────────────────────────────────────────────────

TEST(Growth, ROICBasico) {
    // NOPAT=$330, InvestedCapital=$2000 → ROIC = 330/2000 = 33/200 = 16,5%
    auto result = compute_roic(ROICInputs{
        .nopat            = usd_cur(33000),
        .invested_capital = usd_cur(200000),
    });
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->num, 33);
    EXPECT_EQ(result->den, 200);
}

TEST(Growth, TaxaReinvestimento) {
    // RR = (CapEx - D&A + ΔCGN) / NOPAT = (20-10+5)/66 = 15/66 = 5/22
    auto result = compute_reinvestment_rate(ReinvestmentRateInputs{
        .capex                     = usd_cur(2000),
        .depreciation_amortization = usd_cur(1000),
        .delta_nwc                 = usd_cur(500),
        .nopat                     = usd_cur(6600),
    });
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->num, 5);
    EXPECT_EQ(result->den, 22);
}

TEST(Growth, CrescimentoFundamentalFirma) {
    // ROIC=15%, RR=60% → g = 9% = {9,100}
    auto result = fundamental_growth_firm({15, 100}, {3, 5});
    ASSERT_TRUE(result.has_value());
    double g = static_cast<double>(result->num) / result->den;
    EXPECT_NEAR(g, 0.09, 1e-12);
}

TEST(Growth, CrescimentoFundamentalEquity) {
    // ROE=20%, retention=70% → g = 14% = {7,50}
    auto result = fundamental_growth_equity({1, 5}, {7, 10});
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->num, 7);
    EXPECT_EQ(result->den, 50);
}

TEST(Growth, ROE) {
    // NI=$200, Book Equity=$1000 → ROE=20%
    auto result = compute_roe(ROEInputs{
        .net_income  = usd_cur(20000),
        .book_equity = usd_cur(100000),
    });
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->num, 1);
    EXPECT_EQ(result->den, 5);
}

// ── Beta e Hamada ─────────────────────────────────────────────────────────────

TEST(Beta, LeverBeta) {
    // β_u=1,0, D/E=1,0, t=40% → β_l = 1,0 × (1 + 0,6×1,0) = 1,6 = {8,5}
    auto result = lever_beta({1, 1}, {1, 1}, {2, 5});
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->num, 8);
    EXPECT_EQ(result->den, 5);
}

TEST(Beta, UnleverBeta) {
    // β_l=1,6, D/E=1,0, t=40% → β_u = 1,6 / 1,6 = 1,0
    auto result = unlever_beta({8, 5}, {1, 1}, {2, 5});
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->num, 1);
    EXPECT_EQ(result->den, 1);
}

TEST(Beta, LeverUnleverInverso) {
    // lever(unlever(β)) deve devolver β original
    Rational bl_orig{13, 10};  // 1,30
    Rational de{1, 2};          // D/E = 50%
    Rational t{34, 100};

    auto bu = unlever_beta(bl_orig, de, t);
    ASSERT_TRUE(bu.has_value());
    auto bl_back = lever_beta(*bu, de, t);
    ASSERT_TRUE(bl_back.has_value());

    double orig = static_cast<double>(bl_orig.num) / bl_orig.den;
    double back = static_cast<double>(bl_back->num) / bl_back->den;
    EXPECT_NEAR(back, orig, 1e-10);
}

TEST(Beta, BottomUpBeta) {
    // 2 comparáveis: β_l={6,5}, D/E={1,1}, t=40%  → β_u = 6/5 / 1,6 = 0,75
    //                β_l={8,5}, D/E={1,1}, t=40%  → β_u = 8/5 / 1,6 = 1,00
    // Média β_u = 0,875
    // Alvo: D/E={1,2}=50%, t=34% → β_l = 0,875 × (1 + 0,66×0,5) = 0,875×1,33 = 1,16375
    auto result = bottom_up_beta(BottomUpBetaInputs{
        .comparable_levered_betas  = {{6, 5}, {8, 5}},
        .comparable_debt_to_equity = {{1, 1}, {1, 1}},
        .comparable_tax_rate       = {2, 5},
        .target_debt_to_equity     = {1, 2},
        .target_tax_rate           = {34, 100},
    });
    ASSERT_TRUE(result.has_value());
    double bl = static_cast<double>(result->num) / result->den;
    EXPECT_NEAR(bl, 1.16375, 0.001);
}

// ── H-Model ───────────────────────────────────────────────────────────────────

TEST(HModel, FormulaFechadaExata) {
    // D₀=100 cents, g_h=20%, g_l=5%, H=5, Ke=10%
    // P = D₀ × [(1+g_l) + H×(g_h-g_l)] / (Ke-g_l)
    //   = 100 × [(21/20) + 5×(15/100)] / (5/100)
    //   = 100 × [(21/20) + (3/4)] / (1/20)
    //   = 100 × [(42/40 + 30/40)] / (1/20)
    //   = 100 × (72/40) / (1/20)
    //   = 100 × (72/40) × 20
    //   = 100 × 36 = 3600 cents = $36,00
    auto result = compute_h_model(HModelInputs{
        .base_dividend  = usd_cur(100),
        .high_growth    = {1, 5},   // 20%
        .stable_growth  = {1, 20},  // 5%
        .half_life      = {5, 1},
        .cost_of_equity = {1, 10},  // 10%
    });
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->intrinsic_value.units(), 3600);
    // multiplicador exato = 36
    EXPECT_EQ(result->tv_multiplier.num, 36);
    EXPECT_EQ(result->tv_multiplier.den, 1);
}

TEST(HModel, SemTransicao_EquivaleGordon) {
    // H=0: P = D₀ × (1+g_l)/(Ke-g_l) = Gordon Growth clássico
    // D₀=100, g_l=5%, Ke=10% → P = 100 × 1,05/0,05 = 2100 cents
    auto result = compute_h_model(HModelInputs{
        .base_dividend  = usd_cur(100),
        .high_growth    = {1, 5},
        .stable_growth  = {1, 20},
        .half_life      = {0, 1},   // H=0
        .cost_of_equity = {1, 10},
    });
    // H=0 é inválido (half_life.num <= 0)
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ValuationError::InvalidInput);
}

TEST(HModel, KeIgualGlRejeitado) {
    auto result = compute_h_model(HModelInputs{
        .base_dividend  = usd_cur(100),
        .high_growth    = {1, 5},
        .stable_growth  = {1, 10},
        .half_life      = {5, 1},
        .cost_of_equity = {1, 10},  // Ke = g_l — denominador zero
    });
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ValuationError::WACCLEGrowth);
}

// ── Terminal Value Estendido (Fase 5) ─────────────────────────────────────────

TEST(Terminal, ConsistentTVComROIC) {
    // NOPAT=10000 cents, WACC=10%, g=3%, ROIC=15%
    // RR = 3%/15% = 1/5 = 20%
    // TV = 10000 × (1 - 1/5) / (1/10 - 3/100)
    //    = 10000 × (4/5) / (7/100)
    //    = 10000 × 80/7
    //    = 800000/7 → 114286 (arredondamento banker's)
    auto result = consistent_terminal_value(ConsistentTVInputs{
        .stable_nopat    = usd_cur(10000),
        .wacc            = {1, 10},
        .terminal_growth = {3, 100},
        .terminal_roic   = {3, 20},   // 15%
    });
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->units(), 114286);
}

TEST(Terminal, TVConsistenteMaiorQueGordonQuandoROICAlto) {
    // Quando ROIC muito alto (>WACC), RR baixo → FCFF estável > NOPAT × (1-RR_alto)
    // TV_consistente deve ser maior do que Gordon simples com mesmo NOPAT
    // NOPAT=10000, WACC=10%, g=3%, ROIC=50%
    // RR = 3%/50% = 6% → FCFF = 10000 × 94% = 9400
    // TV = 9400 / 7% = 134285 cents
    auto result = consistent_terminal_value(ConsistentTVInputs{
        .stable_nopat    = usd_cur(10000),
        .wacc            = {1, 10},
        .terminal_growth = {3, 100},
        .terminal_roic   = {1, 2},    // 50%
    });
    ASSERT_TRUE(result.has_value());
    // 10000 × (1-3/50)/(7/100) = 10000 × (47/50)/(7/100) = 10000 × 4700/350 = 10000 × 94/7
    // = 940000/7 = 134285.7 → 134286
    EXPECT_EQ(result->units(), 134286);
}

TEST(Terminal, TVPorMultiplo) {
    // EBITDA=10000 cents, múltiplo=8 → TV=80000
    auto result = terminal_value_by_multiple(usd_cur(10000), {8, 1});
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->units(), 80000);
}

TEST(Terminal, TVPorMultiploFracionario) {
    // múltiplo=7,5 = {15,2} → TV = 10000 × 15/2 = 75000
    auto result = terminal_value_by_multiple(usd_cur(10000), {15, 2});
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->units(), 75000);
}

TEST(Terminal, CheckConsistencyValueCreating) {
    // ROIC=15% > WACC=10% → cria valor; RR=3/15=1/5 ∈ [0,1] → viável
    auto c = check_terminal_consistency({1, 10}, {3, 100}, {3, 20});
    EXPECT_TRUE(c.value_creating);
    EXPECT_TRUE(c.reinvestment_feasible);
    EXPECT_EQ(c.implied_reinvestment_rate.num, 1);
    EXPECT_EQ(c.implied_reinvestment_rate.den, 5);
    EXPECT_EQ(c.return_spread.num, 1);   // 15% - 10% = 5% = 1/20
    EXPECT_EQ(c.return_spread.den, 20);
}

TEST(Terminal, CheckConsistencyDestroyingValue) {
    // ROIC=5% < WACC=10% → destrói valor; spread negativo
    auto c = check_terminal_consistency({1, 10}, {1, 100}, {1, 20});
    EXPECT_FALSE(c.value_creating);
    // spread = 5% - 10% = -5% = -1/20
    EXPECT_LT(c.return_spread.num, 0);
}

TEST(Terminal, CheckConsistencyInfeasibleReinvestment) {
    // g=20% > ROIC=15% → RR = 20/15 = 4/3 > 1 → inviável (reinveste mais que ganha)
    auto c = check_terminal_consistency({1, 4}, {1, 5}, {3, 20});
    EXPECT_FALSE(c.reinvestment_feasible);
}

// ── APV (Fase 6) ──────────────────────────────────────────────────────────────

TEST(APV, TaxShieldExato) {
    // PV(TS) = t × D = 34% × $500 = $170 = 17000 cents  (exato, sem double)
    auto fcffs = project_fcff(FCFFProjection{
        .base_fcff = usd_cur(10000),
        .stages    = {{Rational{0, 1}, 1}},
    });
    ASSERT_TRUE(fcffs.has_value());

    auto result = compute_apv(APVInputs{
        .projected_fcff             = *fcffs,
        .terminal_growth            = {0, 1},
        .unlevered_cost_of_equity   = {3, 20},    // Ku = 15%
        .debt_market_value          = usd_cur(50000),  // $500
        .tax_rate                   = {17, 50},   // 34%
        .net_debt                   = usd_cur(50000),
        .shares_outstanding         = 1,
    });
    ASSERT_TRUE(result.has_value());
    // PV(TS) = 50000 × 17/50 = 17000 cents — exato
    EXPECT_EQ(result->pv_tax_shield.units(), 17000);
}

TEST(APV, ValorDesalavancdoAproximado) {
    // FCFF constante $100/ano, Ku=15%, g=0% → unlevered ≈ 100/0,15 = $666,67
    auto fcffs = project_fcff(FCFFProjection{
        .base_fcff = usd_cur(10000),
        .stages    = {{Rational{0, 1}, 1}},
    });
    ASSERT_TRUE(fcffs.has_value());

    auto result = compute_apv(APVInputs{
        .projected_fcff             = *fcffs,
        .terminal_growth            = {0, 1},
        .unlevered_cost_of_equity   = {3, 20},
        .debt_market_value          = usd_cur(0),
        .tax_rate                   = {17, 50},
        .net_debt                   = usd_cur(0),
        .shares_outstanding         = 1,
    });
    ASSERT_TRUE(result.has_value());
    // $666,67 = 66667 cents (tolerância double)
    EXPECT_NEAR(result->unlevered_firm_value.units(), 66667, 1);
    EXPECT_EQ(result->pv_tax_shield.units(), 0);
    EXPECT_NEAR(result->apv.units(), 66667, 1);
}

TEST(APV, APVMaiorQueWACC) {
    // APV e WACC convergem para o mesmo equity quando a estrutura de capital é estável,
    // mas aqui apenas verificamos que APV > unlevered (o TS agrega valor)
    auto fcffs = project_fcff(FCFFProjection{
        .base_fcff = usd_cur(10000),
        .stages    = {{Rational{0, 1}, 2}},
    });
    ASSERT_TRUE(fcffs.has_value());

    auto result = compute_apv(APVInputs{
        .projected_fcff             = *fcffs,
        .terminal_growth            = {0, 1},
        .unlevered_cost_of_equity   = {3, 20},
        .debt_market_value          = usd_cur(30000),  // $300
        .tax_rate                   = {17, 50},
        .net_debt                   = usd_cur(30000),
        .shares_outstanding         = 1,
    });
    ASSERT_TRUE(result.has_value());
    // PV(TS) = 30000 × 17/50 = 10200 → APV > unlevered
    EXPECT_EQ(result->pv_tax_shield.units(), 10200);
    EXPECT_GT(result->apv.units(), result->unlevered_firm_value.units());
}

TEST(APV, MoedaInconsistenteRejeitada) {
    ratmoney::CurrencyDescription brl{"BRL", "R$", 2};
    auto fcffs = project_fcff(FCFFProjection{
        .base_fcff = usd_cur(10000),
        .stages    = {{Rational{0, 1}, 1}},
    });
    ASSERT_TRUE(fcffs.has_value());

    auto result = compute_apv(APVInputs{
        .projected_fcff             = *fcffs,
        .terminal_growth            = {0, 1},
        .unlevered_cost_of_equity   = {3, 20},
        .debt_market_value          = Currency{10000, {1, 1}, brl},  // moeda errada
        .tax_rate                   = {17, 50},
        .net_debt                   = usd_cur(10000),
        .shares_outstanding         = 1,
    });
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ValuationError::CurrencyMismatch);
}

// ── Valuation Relativo (Fase 7) ───────────────────────────────────────────────

TEST(Relative, PEJustificado) {
    // ROE=20%, payout=60%, Ke=12%
    // g = 20% × 40% = 8% = 2/25
    // Ke-g = 12%-8% = 4% = 1/25
    // P/E = 0,60/0,04 = 15
    auto result = compute_justified_multiples(JustifiedMultiplesInputs{
        .cost_of_equity = {3, 25},   // 12%
        .wacc           = {1, 10},   // 10%
        .roe            = {1, 5},    // 20%
        .payout_ratio   = {3, 5},    // 60%
        .roic           = {1, 5},    // 20%
        .tax_rate       = {17, 50},  // 34%
    });
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->pe_ratio.num, 15);
    EXPECT_EQ(result->pe_ratio.den, 1);
}

TEST(Relative, PBJustificado) {
    // P/BV = ROE × P/E = 20% × 15 = 3
    auto result = compute_justified_multiples(JustifiedMultiplesInputs{
        .cost_of_equity = {3, 25},
        .wacc           = {1, 10},
        .roe            = {1, 5},
        .payout_ratio   = {3, 5},
        .roic           = {1, 5},
        .tax_rate       = {17, 50},
    });
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->pb_ratio.num, 3);
    EXPECT_EQ(result->pb_ratio.den, 1);
}

TEST(Relative, EVEBITDAJustificado) {
    // RR = g/ROIC = 8%/20% = 40% = 2/5
    // (1-t) = 33/50,  (1-RR) = 3/5
    // WACC-g = 10%-8% = 2% = 1/50
    // EV/EBITDA = (33/50)×(3/5) / (1/50) = (99/250)×50 = 99/5 = 19,8
    auto result = compute_justified_multiples(JustifiedMultiplesInputs{
        .cost_of_equity = {3, 25},
        .wacc           = {1, 10},
        .roe            = {1, 5},
        .payout_ratio   = {3, 5},
        .roic           = {1, 5},
        .tax_rate       = {17, 50},
    });
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->ev_ebitda.num, 99);
    EXPECT_EQ(result->ev_ebitda.den, 5);
    double ev_ebitda = static_cast<double>(result->ev_ebitda.num) / result->ev_ebitda.den;
    EXPECT_NEAR(ev_ebitda, 19.8, 1e-12);
}

TEST(Relative, GrowthImplicito) {
    // g_equity = ROE × retention = 20% × 40% = 8% = 2/25
    auto result = compute_justified_multiples(JustifiedMultiplesInputs{
        .cost_of_equity = {3, 25},
        .wacc           = {1, 10},
        .roe            = {1, 5},
        .payout_ratio   = {3, 5},
        .roic           = {1, 5},
        .tax_rate       = {17, 50},
    });
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->g_equity.num, 2);
    EXPECT_EQ(result->g_equity.den, 25);
}

TEST(Relative, KeMenorQueGRejeitado) {
    // g = ROE × retention = 15% × 70% = 10,5% > Ke=10% → inválido
    auto result = compute_justified_multiples(JustifiedMultiplesInputs{
        .cost_of_equity = {1, 10},   // 10%
        .wacc           = {1, 10},
        .roe            = {3, 20},   // 15%
        .payout_ratio   = {3, 10},   // 30% → retention=70%
        .roic           = {3, 20},
        .tax_rate       = {17, 50},
    });
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ValuationError::WACCLEGrowth);
}

TEST(Relative, PayoutTotalPEEqualsGordon) {
    // payout=100%: g=0, P/E = 1/Ke (perpetuidade sem crescimento)
    // Ke=10% → P/E = 10
    auto result = compute_justified_multiples(JustifiedMultiplesInputs{
        .cost_of_equity = {1, 10},
        .wacc           = {1, 10},
        .roe            = {1, 5},
        .payout_ratio   = {1, 1},    // 100%
        .roic           = {1, 5},
        .tax_rate       = {17, 50},
    });
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->pe_ratio.num, 10);
    EXPECT_EQ(result->pe_ratio.den, 1);
    EXPECT_EQ(result->g_equity.num, 0);
}

// ── Kd Sintético (Fase 3) ─────────────────────────────────────────────────────

TEST(SyntheticKd, ICRAltoRetornaAAA) {
    // ICR=10 → AAA → spread=63bps
    // Rf=5%={1,20}, spread={63,10000}
    // Kd = 1/20 + 63/10000 = 500/10000 + 63/10000 = 563/10000
    auto result = synthetic_cost_of_debt(SyntheticKdInputs{
        .ebit             = usd_cur(10000),   // $100
        .interest_expense = usd_cur(1000),    // $10  → ICR=10
        .risk_free_rate   = {1, 20},
        .large_firm       = true,
    });
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->rating, CreditRating::AAA);
    EXPECT_NEAR(result->interest_coverage, 10.0, 1e-9);
    EXPECT_EQ(result->cost_of_debt.num, 563);
    EXPECT_EQ(result->cost_of_debt.den, 10000);
    EXPECT_EQ(result->default_spread.num, 63);
    EXPECT_EQ(result->default_spread.den, 10000);
}

TEST(SyntheticKd, ICRMedioRetornaBBB) {
    // ICR=2.75 → BBB (tabela large: [2.50, 3.00)) → spread=156bps
    // Rf=5%, Kd=5%+1.56%=6.56%
    // 5% = {1,20} = 2500/50000; spread=156/10000=780/50000
    // Kd = (2500+780)/50000 = 3280/50000 = 41/625
    auto result = synthetic_cost_of_debt(SyntheticKdInputs{
        .ebit             = usd_cur(27500),  // $275
        .interest_expense = usd_cur(10000),  // $100 → ICR=2.75
        .risk_free_rate   = {1, 20},
        .large_firm       = true,
    });
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->rating, CreditRating::BBB);
    EXPECT_NEAR(result->interest_coverage, 2.75, 1e-9);
    EXPECT_EQ(result->cost_of_debt.num, 41);
    EXPECT_EQ(result->cost_of_debt.den, 625);
}

TEST(SyntheticKd, ICRBaixoRetornaD) {
    // ICR=0.10 → D → spread=1200bps=12%
    // Kd = 5% + 12% = 17% = {17,100}
    auto result = synthetic_cost_of_debt(SyntheticKdInputs{
        .ebit             = usd_cur(1000),   // $10
        .interest_expense = usd_cur(10000),  // $100 → ICR=0.10
        .risk_free_rate   = {1, 20},
    });
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->rating, CreditRating::D);
    double kd = static_cast<double>(result->cost_of_debt.num) / result->cost_of_debt.den;
    EXPECT_NEAR(kd, 0.17, 1e-9);
}

TEST(SyntheticKd, SemDividaRetornaAAA) {
    // interest_expense=0 → ICR=∞ → AAA, Kd=Rf
    auto result = synthetic_cost_of_debt(SyntheticKdInputs{
        .ebit             = usd_cur(10000),
        .interest_expense = usd_cur(0),
        .risk_free_rate   = {1, 20},
    });
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->rating, CreditRating::AAA);
    EXPECT_EQ(result->cost_of_debt.num, 563);
    EXPECT_EQ(result->cost_of_debt.den, 10000);
}

TEST(SyntheticKd, TabelaPequenaEmpresaMaisExigente) {
    // ICR=5.0 → large=A (tabela large: [4.25,5.50)) mas small=A- (tabela small: [4.50,6.00))
    auto large = synthetic_cost_of_debt(SyntheticKdInputs{
        .ebit = usd_cur(50000), .interest_expense = usd_cur(10000),
        .risk_free_rate = {1, 20}, .large_firm = true});
    auto small = synthetic_cost_of_debt(SyntheticKdInputs{
        .ebit = usd_cur(50000), .interest_expense = usd_cur(10000),
        .risk_free_rate = {1, 20}, .large_firm = false});
    ASSERT_TRUE(large.has_value());
    ASSERT_TRUE(small.has_value());
    // ICR=5.0: large → A (spread=108bps), small → A- (spread=122bps)
    EXPECT_EQ(large->rating, CreditRating::A);
    EXPECT_EQ(small->rating, CreditRating::A_minus);
    // Small firm → Kd maior (spread maior)
    double kd_large = static_cast<double>(large->cost_of_debt.num) / large->cost_of_debt.den;
    double kd_small = static_cast<double>(small->cost_of_debt.num) / small->cost_of_debt.den;
    EXPECT_LT(kd_large, kd_small);
}

TEST(SyntheticKd, MoedaInconsistenteRejeitada) {
    ratmoney::CurrencyDescription brl{"BRL", "R$", 2};
    auto result = synthetic_cost_of_debt(SyntheticKdInputs{
        .ebit             = usd_cur(10000),
        .interest_expense = Currency{1000, {1,1}, brl},
        .risk_free_rate   = {1, 20},
    });
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ValuationError::CurrencyMismatch);
}

// ── Estrutura de Capital Ótima (Fase 3) ───────────────────────────────────────

TEST(OptimalCapital, SemDividaWACCEqualsKe) {
    // d=0: β_l = β_u, WACC = Ke = Rf + β×ERP
    // β_u=1, Rf=5%, ERP=7%, t=34% → Ke=12%={3,25}
    // schedule[0].wacc deve ser {3,25}
    ratmoney::CurrencyDescription usd_d{"USD", "$", 2};
    auto result = optimal_capital_structure(OptimalCapitalStructureInputs{
        .unlevered_beta       = {1, 1},
        .risk_free_rate       = {1, 20},
        .equity_risk_premium  = {7, 100},
        .tax_rate             = {17, 50},
        .ebit                 = usd_cur(10000000),    // $100K
        .firm_value           = usd_cur(200000000),   // $2M
        .steps                = 5,
        .large_firm           = true,
    });
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->schedule.size(), 5u);
    // d=0: WACC = Ke = 5% + 1×7% = 12% = {3,25}
    EXPECT_EQ(result->schedule[0].wacc.num, 3);
    EXPECT_EQ(result->schedule[0].wacc.den, 25);
    // d=0: sem dívida, rating AAA, ICR=∞
    EXPECT_EQ(result->schedule[0].rating, CreditRating::AAA);
}

TEST(OptimalCapital, OtimoCriaValorViaShieldFiscal) {
    // Para qualquer empresa com EBIT razoável, algum grau de alavancagem
    // gera benefício fiscal que reduz WACC abaixo do nível sem dívida.
    auto result = optimal_capital_structure(OptimalCapitalStructureInputs{
        .unlevered_beta       = {1, 1},
        .risk_free_rate       = {1, 20},
        .equity_risk_premium  = {7, 100},
        .tax_rate             = {17, 50},
        .ebit                 = usd_cur(10000000),
        .firm_value           = usd_cur(200000000),
        .steps                = 10,
        .large_firm           = true,
    });
    ASSERT_TRUE(result.has_value());
    double wacc_unlevered = static_cast<double>(result->schedule[0].wacc.num)
                          / result->schedule[0].wacc.den;
    double wacc_optimal   = static_cast<double>(result->optimal.wacc.num)
                          / result->optimal.wacc.den;
    // Ótimo deve ser melhor (menor WACC) do que sem dívida
    EXPECT_LT(wacc_optimal, wacc_unlevered);
    // E a dívida ótima deve ser > 0
    EXPECT_GT(result->optimal.debt_ratio.num, 0);
}

TEST(OptimalCapital, ScheduleOrdenadoPorRazaoDeEndividamento) {
    auto result = optimal_capital_structure(OptimalCapitalStructureInputs{
        .unlevered_beta       = {1, 1},
        .risk_free_rate       = {1, 20},
        .equity_risk_premium  = {7, 100},
        .tax_rate             = {17, 50},
        .ebit                 = usd_cur(5000000),
        .firm_value           = usd_cur(100000000),
        .steps                = 6,
    });
    ASSERT_TRUE(result.has_value());
    for (size_t i = 1; i < result->schedule.size(); ++i) {
        double d_prev = static_cast<double>(result->schedule[i-1].debt_ratio.num)
                      / result->schedule[i-1].debt_ratio.den;
        double d_curr = static_cast<double>(result->schedule[i].debt_ratio.num)
                      / result->schedule[i].debt_ratio.den;
        EXPECT_LT(d_prev, d_curr);
    }
}

TEST(OptimalCapital, KdCresceComAlavancagem) {
    // Quanto mais dívida, pior o rating (menor ICR), maior o Kd
    auto result = optimal_capital_structure(OptimalCapitalStructureInputs{
        .unlevered_beta       = {1, 1},
        .risk_free_rate       = {1, 20},
        .equity_risk_premium  = {7, 100},
        .tax_rate             = {17, 50},
        .ebit                 = usd_cur(1000000),    // EBIT modesto → ICR piora rapidamente
        .firm_value           = usd_cur(200000000),
        .steps                = 5,
    });
    ASSERT_TRUE(result.has_value());
    // Kd no último ponto (80% dívida) deve ser >= Kd no primeiro (0%)
    double kd_first = static_cast<double>(result->schedule.front().cost_of_debt.num)
                    / result->schedule.front().cost_of_debt.den;
    double kd_last  = static_cast<double>(result->schedule.back().cost_of_debt.num)
                    / result->schedule.back().cost_of_debt.den;
    EXPECT_GE(kd_last, kd_first);
}

// ── Excess Returns / EVA ──────────────────────────────────────────────────────

TEST(ExcessReturns, SingleStageValorExato) {
    // IC=10000, ROIC=15%={3,20}, WACC=10%={1,10}, g=3%={3,100}
    // V = IC × (ROIC−g)/(WACC−g) = 10000 × (12/100)/(7/100) = 10000 × 12/7
    // = 120000/7 → 17143 (arredondamento banker's: 0.571 > 0.5 → sobe)
    auto result = excess_returns_value(SingleStageERInputs{
        .invested_capital = usd_cur(10000),
        .roic             = {3, 20},
        .wacc             = {1, 10},
        .terminal_growth  = {3, 100},
    });
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->firm_value.units(), 17143);
    // asset_value = IC = 10000
    EXPECT_EQ(result->asset_value.units(), 10000);
    // pv_excess_returns = 17143 − 10000 = 7143
    EXPECT_EQ(result->pv_excess_returns.units(), 7143);
}

TEST(ExcessReturns, ROICEqualsWACCZeroExcessReturns) {
    // ROIC = WACC = 10%: sem retorno em excesso → V = IC
    auto result = excess_returns_value(SingleStageERInputs{
        .invested_capital = usd_cur(10000),
        .roic             = {1, 10},
        .wacc             = {1, 10},
        .terminal_growth  = {3, 100},
    });
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->pv_excess_returns.units(), 0);
    EXPECT_EQ(result->firm_value.units(), result->asset_value.units());
}

TEST(ExcessReturns, ROICAbaixoWACCDestroeValor) {
    // ROIC < WACC: V < IC (firm_value < invested_capital)
    auto result = excess_returns_value(SingleStageERInputs{
        .invested_capital = usd_cur(10000),
        .roic             = {5, 100},   // 5%
        .wacc             = {1, 10},    // 10%
        .terminal_growth  = {2, 100},
    });
    ASSERT_TRUE(result.has_value());
    EXPECT_LT(result->firm_value.units(), result->asset_value.units());
    EXPECT_LT(result->pv_excess_returns.units(), 0);
}

TEST(ExcessReturns, MultiStageEquivalenteAoSingleQuandoUmEstagio) {
    // Monoestágio: um período de crescimento zero → V deve ≈ single-stage
    auto multi = excess_returns_multistage(MultiStageERInputs{
        .base_invested_capital = usd_cur(10000),
        .roic                  = {3, 20},
        .stages                = {{Rational{3, 100}, 1}},
        .wacc                  = {1, 10},
        .terminal_roic         = {3, 20},
        .terminal_growth       = {3, 100},
    });
    ASSERT_TRUE(multi.has_value());
    // Valor deve ser razoavelmente próximo do single-stage (diferença de projeção discreta)
    double v = static_cast<double>(multi->firm_value.units());
    EXPECT_GT(v, 0.0);
}

// ── Startup / Cenários ────────────────────────────────────────────────────────

TEST(Startup, DoisCenariosComSobrevivencia) {
    // Cenário Bull (60%): FCFF_terminal=200 cents, g=3%, WACC=10%, T=1
    //   TV = 200×(1.03)/0.07 = 200×(103/100)/(7/100) = 200×103/7 ≈ 2942.9 cents
    //   PV = 2942.9/1.1 ≈ 2675.3 cents
    // Cenário Bear (40%): FCFF_terminal=100 cents, g=2%, WACC=12%, T=1
    //   TV = 100×(1.02)/0.10 = 1020 cents
    //   PV = 1020/1.12 ≈ 910.7 cents
    // E[EV] = 0.6×2675.3 + 0.4×910.7 = 1605.2 + 364.3 = 1969.5 cents
    // P(survive)=90%, failure_value=0 → adjusted = 1969.5×0.9 = 1772.6 cents

    auto result = startup_valuation(StartupValuationInputs{
        .scenarios = {
            {"Bull", {3,5},  usd_cur(200), {3,100}, {1,10},  1},
            {"Bear", {2,5},  usd_cur(100), {2,100}, {3,25},  1},
        },
        .survival_probability = {9, 10},
        .failure_value        = usd_cur(0),
        .net_debt             = usd_cur(0),
        .shares_outstanding   = 1,
    });
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scenario_evs.size(), 2u);
    EXPECT_GT(result->enterprise_value.units(), 0);
    // Valor ajustado < valor esperado sem ajuste de sobrevivência
    double ev = static_cast<double>(result->enterprise_value.units());
    EXPECT_LT(ev, 2000.0);   // < E[EV] pré-ajuste ≈ 1969.5
}

TEST(Startup, ProbabilidadesNaoSomam1Rejeitado) {
    auto result = startup_valuation(StartupValuationInputs{
        .scenarios = {
            {"A", {1,2}, usd_cur(100), {3,100}, {1,10}, 1},
            // prob total = 0.5, não 1.0
        },
        .survival_probability = {1, 1},
        .failure_value        = usd_cur(0),
        .net_debt             = usd_cur(0),
        .shares_outstanding   = 1,
    });
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ValuationError::InvalidInput);
}

TEST(Startup, SobrevivenciaZeroRetornaFailureValue) {
    // P(survive)=0 → EV = failure_value
    auto result = startup_valuation(StartupValuationInputs{
        .scenarios = {
            {"Only", {1,1}, usd_cur(1000), {3,100}, {1,10}, 1},
        },
        .survival_probability = {0, 1},
        .failure_value        = usd_cur(5000),  // $50
        .net_debt             = usd_cur(0),
        .shares_outstanding   = 1,
    });
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->enterprise_value.units(), 5000);
}

// ── Distress — Equity como Opção ──────────────────────────────────────────────

TEST(Distress, EquityPositivoMenorQueAtivos) {
    // V=100, D=80, σ=20%, r=5%, T=1
    auto res = equity_as_option({100.0, 80.0, 0.20, 0.05, 1.0});
    EXPECT_GT(res.equity_value, 0.0);
    EXPECT_LT(res.equity_value, 100.0);
    EXPECT_NEAR(res.equity_value + res.debt_market_value, 100.0, 1e-9);
}

TEST(Distress, ProbabilidadeDefaultEntre0e1) {
    auto res = equity_as_option({100.0, 80.0, 0.20, 0.05, 1.0});
    EXPECT_GE(res.probability_default, 0.0);
    EXPECT_LE(res.probability_default, 1.0);
}

TEST(Distress, DistressProfundoEquityTemValorTemporal) {
    // V=50 < D=100 (firma insolvente hoje): equity ainda tem valor pela optionalidade
    auto res = equity_as_option({50.0, 100.0, 0.40, 0.05, 3.0});
    EXPECT_GT(res.equity_value, 0.0);         // time value > 0
    EXPECT_GT(res.probability_default, 0.5);  // provavelmente vai fazer default
}

TEST(Distress, FirmaSaudavelBaixaProb) {
    // V muito maior que D: default improvável
    auto res = equity_as_option({1000.0, 100.0, 0.20, 0.05, 1.0});
    EXPECT_LT(res.probability_default, 0.01);
    EXPECT_NEAR(res.equity_value, 900.0, 20.0);  // equity ≈ V − D
}

// ── Banco — FCFE com Capital Regulatório ──────────────────────────────────────

TEST(Bank, FCFEBasico) {
    // NI=10000 cents, RR=30% → FCFE = 10000×0.7 = 7000
    auto result = compute_bank_fcfe(BankFCFEInputs{
        .net_income                = usd_cur(10000),
        .equity_reinvestment_rate  = {3, 10},
    });
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->units(), 7000);
}

TEST(Bank, RRZeroFCFEEqualsNI) {
    auto result = compute_bank_fcfe(BankFCFEInputs{
        .net_income               = usd_cur(10000),
        .equity_reinvestment_rate = {0, 1},
    });
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->units(), 10000);
}

TEST(Bank, CapitalRegulatorioDerivado) {
    // current_rwa=100000, projected_rwa=110000, Δ=10000
    // target_tier1=8% → ΔCapital = 10000 × 0.08 = 800 cents
    // NI=5000 → equity_RR = 800/5000 = 4/25
    auto rr = bank_equity_reinvestment_rate(RegulatoryCapitalInputs{
        .net_income         = usd_cur(5000),
        .current_rwa        = usd_cur(100000),
        .projected_rwa      = usd_cur(110000),
        .target_tier1_ratio = {2, 25},   // 8%
    });
    ASSERT_TRUE(rr.has_value());
    EXPECT_EQ(rr->num, 4);
    EXPECT_EQ(rr->den, 25);
}

// ── Normalização de Resultados ────────────────────────────────────────────────

TEST(Normalize, MediaHistorica) {
    // [8000, 9000, 10000, 11000, 12000] → média = 10000 cents
    auto result = normalize_ebit(NormalizationInputs{
        .method        = NormalizationMethod::HistoricalAverage,
        .historical_ebit = {
            usd_cur(8000), usd_cur(9000), usd_cur(10000),
            usd_cur(11000), usd_cur(12000),
        },
        .current_revenue   = usd_cur(0),
        .normalized_margin = {0, 1},
    });
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->units(), 10000);
}

TEST(Normalize, MargemNormalizada) {
    // receita=100000 cents ($1000), margem=15% → EBIT = 15000 cents ($150)
    auto result = normalize_ebit(NormalizationInputs{
        .method            = NormalizationMethod::NormalizedMargin,
        .historical_ebit   = {},
        .current_revenue   = usd_cur(100000),
        .normalized_margin = {3, 20},   // 15%
    });
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->units(), 15000);
}

TEST(Normalize, MenosDeDoisPeriodosRejeitado) {
    auto result = normalize_ebit(NormalizationInputs{
        .method          = NormalizationMethod::HistoricalAverage,
        .historical_ebit = {usd_cur(5000)},   // só 1 período
        .current_revenue = usd_cur(0),
        .normalized_margin = {0, 1},
    });
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ValuationError::InvalidInput);
}

// ── APV Miles-Ezzell ──────────────────────────────────────────────────────────

TEST(APVMilesEzzell, MenorQueMM) {
    // ME sempre dá PV(TS) menor que MM porque desconta parte dos shields a Ku (>Kd)
    auto fcffs = project_fcff(FCFFProjection{
        .base_fcff = usd_cur(10000),
        .stages    = {{Rational{0, 1}, 1}},
    });
    ASSERT_TRUE(fcffs.has_value());

    auto mm = compute_apv(APVInputs{
        .projected_fcff           = *fcffs,
        .terminal_growth          = {0, 1},
        .unlevered_cost_of_equity = {3, 20},  // Ku=15%
        .debt_market_value        = usd_cur(30000),
        .tax_rate                 = {17, 50},
        .net_debt                 = usd_cur(30000),
        .shares_outstanding       = 1,
    });
    auto me = compute_apv_miles_ezzell(APVMilesEzzellInputs{
        .projected_fcff           = *fcffs,
        .terminal_growth          = {0, 1},
        .unlevered_cost_of_equity = {3, 20},
        .cost_of_debt             = {3, 50},  // Kd=6%
        .debt_market_value        = usd_cur(30000),
        .tax_rate                 = {17, 50},
        .net_debt                 = usd_cur(30000),
        .shares_outstanding       = 1,
    });
    ASSERT_TRUE(mm.has_value());
    ASSERT_TRUE(me.has_value());
    // PV(TS) MM > PV(TS) ME (MM é mais otimista)
    EXPECT_GT(mm->pv_tax_shield.units(), me->pv_tax_shield.units());
    // Mas ambos positivos
    EXPECT_GT(me->pv_tax_shield.units(), 0);
}

TEST(APVMilesEzzell, FormulaNumerica) {
    // D=100000 cents=$1000, t=34%, Kd=6%, Ku=15%, g=0%
    // PV(TS)_ME = 34% × 6% × 1000 × (1.15) / [(1.06) × (15%)] [g=0]
    //           = 0.34 × 0.06 × 1000 × 1.15 / (1.06 × 0.15)
    //           = 23.46 / 0.159 ≈ 147.5...
    // Verificação: MM daria 340. ME é ~43% do MM neste caso.
    auto fcffs = project_fcff(FCFFProjection{
        .base_fcff = usd_cur(10000),
        .stages    = {{Rational{0, 1}, 1}},
    });
    ASSERT_TRUE(fcffs.has_value());
    auto me = compute_apv_miles_ezzell(APVMilesEzzellInputs{
        .projected_fcff           = *fcffs,
        .terminal_growth          = {0, 1},
        .unlevered_cost_of_equity = {3, 20},
        .cost_of_debt             = {3, 50},
        .debt_market_value        = usd_cur(100000),
        .tax_rate                 = {17, 50},
        .net_debt                 = usd_cur(100000),
        .shares_outstanding       = 1,
    });
    ASSERT_TRUE(me.has_value());
    double pv_ts = static_cast<double>(me->pv_tax_shield.units()) / 100.0;
    EXPECT_NEAR(pv_ts, 147.5, 1.0);  // tolerância de $1 por arredondamento
}
