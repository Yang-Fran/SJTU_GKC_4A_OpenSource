 //*****************************************************************************
 //
 // receiver_baseband_main.c
 //
 // TM4C123G receiver baseband program for the IV-A receiver project.
 //
 // Signal chain:
 //   ADC PE1/AIN2
 //   -> median filter
 //   -> short edge filter + Schmitt edge detector + DPLL
 //   -> symbol synchronous integrate-and-dump decision
 //   -> differential decoder
 //   -> DAC7611 matched-filter monitor and GPIO debug outputs
 //
 // Pin map used by the baseband board:
 //   PE1 / AIN2 : ADC input from analog front-end
 //   PE2        : recovered bit clock, P11 / BCLK-OUT
 //   PA2        : sampled decision output, P12 / DECISION-OUT
 //   PA3        : differential decoded output, P13 / DECODER-OUT
 //   PD0        : DAC7611 /CS
 //   PD1        : DAC7611 CLK
 //   PD2        : DAC7611 SDI
 //   PD3        : DAC7611 /LD
 //   PF1        : ISR time probe, low while the ISR is running
 //
 //*****************************************************************************

 //#define PART_TM4C123GH6PM
 //#define TARGET_IS_TM4C123_RB1

 #include <stdint.h>
 #include <stdbool.h>

 #include "inc/hw_ints.h"
 #include "inc/hw_memmap.h"
 #include "inc/hw_types.h"

 #include "driverlib/adc.h"
 #include "driverlib/fpu.h"
 #include "driverlib/gpio.h"
 #include "driverlib/interrupt.h"
 #include "driverlib/rom.h"
 #include "driverlib/sysctl.h"
 #include "driverlib/timer.h"

 //*****************************************************************************
 // Tunable parameters
 //*****************************************************************************

 #define SYSCLK_HZ                   80000000UL
 #define SYMBOL_RATE_HZ              500UL
 #define OSR_N                       100UL
 #define SAMPLE_RATE_HZ              (SYMBOL_RATE_HZ * OSR_N)
 #define TIMER0_LOAD_VALUE           ((SYSCLK_HZ / SAMPLE_RATE_HZ) - 1UL)

 #define ADC_MAX_VALUE               4095UL
 #define ADC_MID_VALUE               2048UL

 // One-symbol matched filter (integrate-and-dump) value for DAC observation.
 // Output is the running mean of g_symbolSum within the current symbol; it
 // resets at every PLL boundary, producing one clean ramp per symbol.

 // Short filter only for transition detection.  It suppresses spikes without
 // adding the half-symbol delay of a full matched filter.
 #define EDGE_FILTER_LEN             (OSR_N / 4UL)

 // DAC bit-banging is relatively expensive, so the monitor waveform is decimated.
 #define DAC_DECIM                   5UL

 // Adaptive threshold tracking.  The first few symbols are used for fast
 // min/max acquisition, then a slow envelope tracker follows lab drift.
 #define LEVEL_LOW_INIT              (ADC_MAX_VALUE / 3UL)
 #define LEVEL_HIGH_INIT             ((ADC_MAX_VALUE * 2UL) / 3UL)
 #define LEVEL_MIN_SPAN              120UL
 #define LEVEL_ATTACK_SHIFT          6U
 #define LEVEL_RELEASE_SHIFT         10U
 #define LEVEL_CALIBRATION_TICKS     (OSR_N * 4UL)

 // Decision threshold sits slightly above the midpoint of the tracked envelope.
 #define DECISION_BIAS_NUM           1UL
 #define DECISION_BIAS_DEN           16UL

 // Schmitt thresholds are low + span * 3/8 and low + span * 5/8.
 #define EDGE_LOW_NUM                3UL
 #define EDGE_HIGH_NUM               5UL
 #define EDGE_DEN                    8UL

 // Ignore edge events that are too close together.  A valid NRZ stream cannot
 // have two different symbol boundaries closer than about one symbol.
 #define EDGE_HOLDOFF_TICKS          (OSR_N / 2UL)

 // DPLL deadband around the local boundary.  This reduces one-tick dithering.
 #define PLL_BOUNDARY_DEADBAND       1UL

 // Skip the first few samples of each symbol from the integrator: those samples
 // straddle the edge filter's settling ramp and would otherwise pull the I&D
 // mean toward the previous symbol level (visible as a transient on the DAC).
 #define INTEGRATOR_GUARD_TICKS      (EDGE_FILTER_LEN / 2UL)

 // CH3 monitor matched-filter window length.  Shorter than a full symbol so
 // each symbol contains a clear plateau after the boxcar settles, instead of
 // the V-shape that a full-symbol boxcar produces on alternating bits.
 #define CH3_MF_LEN                  (OSR_N / 2UL)

 //*****************************************************************************
 // Pin definitions
 //*****************************************************************************

 #define ADC_GPIO_PERIPH             SYSCTL_PERIPH_GPIOE
 #define ADC_GPIO_BASE               GPIO_PORTE_BASE
 #define ADC_GPIO_PIN                GPIO_PIN_1
 #define ADC_CHANNEL                 ADC_CTL_CH2

 #define DAC_GPIO_PERIPH             SYSCTL_PERIPH_GPIOD
 #define DAC_GPIO_BASE               GPIO_PORTD_BASE
 #define DAC_CS_PIN                  GPIO_PIN_0
 #define DAC_CLK_PIN                 GPIO_PIN_1
 #define DAC_SDI_PIN                 GPIO_PIN_2
 #define DAC_LD_PIN                  GPIO_PIN_3

 #define DAC_CS_L()                  GPIOPinWrite(DAC_GPIO_BASE, DAC_CS_PIN, 0)
 #define DAC_CS_H()                  GPIOPinWrite(DAC_GPIO_BASE, DAC_CS_PIN, DAC_CS_PIN)
 #define DAC_CLK_L()                 GPIOPinWrite(DAC_GPIO_BASE, DAC_CLK_PIN, 0)
 #define DAC_CLK_H()                 GPIOPinWrite(DAC_GPIO_BASE, DAC_CLK_PIN, DAC_CLK_PIN)
 #define DAC_SDI_L()                 GPIOPinWrite(DAC_GPIO_BASE, DAC_SDI_PIN, 0)
 #define DAC_SDI_H()                 GPIOPinWrite(DAC_GPIO_BASE, DAC_SDI_PIN, DAC_SDI_PIN)
 #define DAC_LD_L()                  GPIOPinWrite(DAC_GPIO_BASE, DAC_LD_PIN, 0)
 #define DAC_LD_H()                  GPIOPinWrite(DAC_GPIO_BASE, DAC_LD_PIN, DAC_LD_PIN)

 #define BCLK_GPIO_PERIPH            SYSCTL_PERIPH_GPIOE
 #define BCLK_GPIO_BASE              GPIO_PORTE_BASE
 #define BCLK_PIN                    GPIO_PIN_2

 #define DECISION_GPIO_PERIPH        SYSCTL_PERIPH_GPIOA
 #define DECISION_GPIO_BASE          GPIO_PORTA_BASE
 #define DECISION_PIN                GPIO_PIN_2

 #define DECODER_GPIO_PERIPH         SYSCTL_PERIPH_GPIOA
 #define DECODER_GPIO_BASE           GPIO_PORTA_BASE
 #define DECODER_PIN                 GPIO_PIN_3

 #define ISR_PROF_PERIPH             SYSCTL_PERIPH_GPIOF
 #define ISR_PROF_BASE               GPIO_PORTF_BASE
 #define ISR_PROF_PIN                GPIO_PIN_1

 //*****************************************************************************
 // Global state
 //*****************************************************************************

 static volatile uint32_t g_adcRaw = ADC_MID_VALUE;
 static volatile uint32_t g_adcMedian = ADC_MID_VALUE;
 static volatile uint32_t g_edgeFilterOut = ADC_MID_VALUE;
 static volatile uint32_t g_mfOut = ADC_MID_VALUE;
 static volatile uint32_t g_decisionThreshold = ADC_MID_VALUE;

 static uint32_t g_medX0 = ADC_MID_VALUE;
 static uint32_t g_medX1 = ADC_MID_VALUE;

 static uint32_t g_edgeBuf[EDGE_FILTER_LEN];
 static uint32_t g_edgeSum = 0;
 static uint32_t g_edgeIndex = 0;
 static uint8_t g_edgePrimed = 0;

 // Sliding-window matched filter: boxcar over CH3_MF_LEN samples (a fraction
 // of a symbol).  Bounded integrator on the raw ADC stream — used only as
 // the CH3 monitor signal, never feeds the decision path.
 static uint32_t g_mfBuf[CH3_MF_LEN];
 static uint32_t g_mfSum = 0;
 static uint32_t g_mfIndex = 0;
 static uint8_t g_mfPrimed = 0;

 static uint32_t g_levelLow = ADC_MAX_VALUE;
 static uint32_t g_levelHigh = 0;
 static uint32_t g_levelCalibCount = LEVEL_CALIBRATION_TICKS;
 static uint8_t g_levelsReady = 0;
 static uint8_t g_schmittLevel = 0;
 static uint8_t g_prevSchmittLevel = 0;
 static uint32_t g_edgeHoldoff = 0;

 static uint32_t g_pllPhase = 0;
 static uint8_t g_bitClock = 1;
 static uint8_t g_pllAcquired = 0;

 static uint32_t g_symbolSum = 0;
 static uint32_t g_symbolCount = 0;
 static uint32_t g_symbolGuardCnt = 0;
 static uint32_t g_thresholdLatched = ADC_MID_VALUE;
 static uint8_t g_decisionBit = 0;
 static uint8_t g_prevDecisionBit = 0;
 static uint8_t g_diffDecodedBit = 0;

 static uint32_t g_dacDecimCnt = 0;
 static volatile uint32_t g_dacPendingValue = ADC_MID_VALUE;
 static volatile uint8_t g_dacPendingFlag = 0;

 //*****************************************************************************
 // Function prototypes
 //*****************************************************************************

 static void SystemClock_Init(void);
 static void GPIO_Init(void);
 static void ADC0_Init(void);
 static void Timer0A_Init(void);

 static void ADC0_StartOnce(void);
 static bool ADC0_ReadIfReady(uint32_t *value);

 static uint32_t Median3(uint32_t a, uint32_t b, uint32_t c);
 static uint32_t ADC_Preprocess_Update(uint32_t x);
 static uint32_t EdgeFilter_Update(uint32_t x);
 static uint32_t MatchedFilter_Update(uint32_t x);
 static void LevelTracker_Update(uint32_t x);
 static uint8_t EdgeDetector_Update(uint32_t x);
 static uint8_t DPLL_Update(uint8_t edgeEvent);
 static void SymbolIntegrator_Update(uint32_t x, uint8_t symbolBoundary);
 static void OutputDebugSignals(void);
 static void DAC7611_Write(uint32_t dacData);

 //*****************************************************************************
 // Main
 //*****************************************************************************

 int main(void)
 {
     ROM_FPULazyStackingEnable();

     SystemClock_Init();
     GPIO_Init();
     ADC0_Init();
     Timer0A_Init();

     ADC0_StartOnce();

     ROM_IntMasterEnable();
     ROM_TimerEnable(TIMER0_BASE, TIMER_A);

     while(1)
     {
         if(g_dacPendingFlag)
         {
             uint32_t v = g_dacPendingValue;
             g_dacPendingFlag = 0;
             DAC7611_Write(v);
         }
     }
 }

 //*****************************************************************************
 // Initialization
 //*****************************************************************************

 static void SystemClock_Init(void)
 {
     ROM_SysCtlClockSet(SYSCTL_SYSDIV_2_5 |
                        SYSCTL_USE_PLL |
                        SYSCTL_OSC_MAIN |
                        SYSCTL_XTAL_16MHZ);
 }

 static void GPIO_Init(void)
 {
     ROM_SysCtlPeripheralEnable(DAC_GPIO_PERIPH);
     while(!ROM_SysCtlPeripheralReady(DAC_GPIO_PERIPH)) {}
     ROM_GPIOPinTypeGPIOOutput(DAC_GPIO_BASE,
                               DAC_CS_PIN | DAC_CLK_PIN | DAC_SDI_PIN | DAC_LD_PIN);
     DAC_CS_H();
     DAC_CLK_L();
     DAC_SDI_L();
     DAC_LD_H();

     ROM_SysCtlPeripheralEnable(BCLK_GPIO_PERIPH);
     while(!ROM_SysCtlPeripheralReady(BCLK_GPIO_PERIPH)) {}
     ROM_GPIOPinTypeGPIOOutput(BCLK_GPIO_BASE, BCLK_PIN);
     GPIOPinWrite(BCLK_GPIO_BASE, BCLK_PIN, 0);

     ROM_SysCtlPeripheralEnable(DECISION_GPIO_PERIPH);
     while(!ROM_SysCtlPeripheralReady(DECISION_GPIO_PERIPH)) {}
     ROM_GPIOPinTypeGPIOOutput(DECISION_GPIO_BASE, DECISION_PIN | DECODER_PIN);
     GPIOPinWrite(DECISION_GPIO_BASE, DECISION_PIN | DECODER_PIN, 0);

     ROM_SysCtlPeripheralEnable(ISR_PROF_PERIPH);
     while(!ROM_SysCtlPeripheralReady(ISR_PROF_PERIPH)) {}
     ROM_GPIOPinTypeGPIOOutput(ISR_PROF_BASE, ISR_PROF_PIN);
     GPIOPinWrite(ISR_PROF_BASE, ISR_PROF_PIN, ISR_PROF_PIN);
 }

 static void ADC0_Init(void)
 {
     uint32_t i;

     ROM_SysCtlPeripheralEnable(ADC_GPIO_PERIPH);
     ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0);
     while(!ROM_SysCtlPeripheralReady(ADC_GPIO_PERIPH)) {}
     while(!ROM_SysCtlPeripheralReady(SYSCTL_PERIPH_ADC0)) {}

     ROM_GPIOPinTypeADC(ADC_GPIO_BASE, ADC_GPIO_PIN);

     ROM_ADCSequenceDisable(ADC0_BASE, 3);
     ROM_ADCSequenceConfigure(ADC0_BASE, 3, ADC_TRIGGER_PROCESSOR, 0);
     ROM_ADCSequenceStepConfigure(ADC0_BASE,
                                  3,
                                  0,
                                  ADC_CHANNEL | ADC_CTL_IE | ADC_CTL_END);
     ROM_ADCSequenceEnable(ADC0_BASE, 3);
     ROM_ADCIntClear(ADC0_BASE, 3);

     for(i = 0; i < EDGE_FILTER_LEN; i++)
     {
         g_edgeBuf[i] = ADC_MID_VALUE;
         g_edgeSum += ADC_MID_VALUE;
     }
 }

 static void Timer0A_Init(void)
 {
     ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
     while(!ROM_SysCtlPeripheralReady(SYSCTL_PERIPH_TIMER0)) {}

     ROM_TimerDisable(TIMER0_BASE, TIMER_A);
     ROM_TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC);
     ROM_TimerLoadSet(TIMER0_BASE, TIMER_A, TIMER0_LOAD_VALUE);
     ROM_TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
     ROM_TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
     ROM_IntEnable(INT_TIMER0A);
 }

 //*****************************************************************************
 // ADC helpers
 //*****************************************************************************

 static void ADC0_StartOnce(void)
 {
     ROM_ADCProcessorTrigger(ADC0_BASE, 3);
 }

 static bool ADC0_ReadIfReady(uint32_t *value)
 {
     if(ROM_ADCIntStatus(ADC0_BASE, 3, false))
     {
         ROM_ADCSequenceDataGet(ADC0_BASE, 3, value);
         ROM_ADCIntClear(ADC0_BASE, 3);
         return true;
     }

     return false;
 }

 //*****************************************************************************
 // Filtering and adaptive thresholds
 //*****************************************************************************

 static uint32_t Median3(uint32_t a, uint32_t b, uint32_t c)
 {
     uint32_t t;

     if(a > b)
     {
         t = a;
         a = b;
         b = t;
     }

     if(b > c)
     {
         t = b;
         b = c;
         c = t;
     }

     if(a > b)
     {
         t = a;
         a = b;
         b = t;
     }

     return b;
 }

 static uint32_t ADC_Preprocess_Update(uint32_t x)
 {
     uint32_t y;

     y = Median3(g_medX0, g_medX1, x);
     g_medX0 = g_medX1;
     g_medX1 = x;

     return y;
 }

 static uint32_t EdgeFilter_Update(uint32_t x)
 {
     uint32_t i;

     if(!g_edgePrimed)
     {
         g_edgeSum = 0;
         for(i = 0; i < EDGE_FILTER_LEN; i++)
         {
             g_edgeBuf[i] = x;
             g_edgeSum += x;
         }
         g_edgePrimed = 1;
     }

     g_edgeSum -= g_edgeBuf[g_edgeIndex];
     g_edgeBuf[g_edgeIndex] = x;
     g_edgeSum += x;

     g_edgeIndex++;
     if(g_edgeIndex >= EDGE_FILTER_LEN)
     {
         g_edgeIndex = 0;
     }

     return g_edgeSum / EDGE_FILTER_LEN;
 }

 static uint32_t MatchedFilter_Update(uint32_t x)
 {
     uint32_t i;

     if(!g_mfPrimed)
     {
         g_mfSum = 0;
         for(i = 0; i < CH3_MF_LEN; i++)
         {
             g_mfBuf[i] = x;
             g_mfSum += x;
         }
         g_mfPrimed = 1;
     }

     g_mfSum -= g_mfBuf[g_mfIndex];
     g_mfBuf[g_mfIndex] = x;
     g_mfSum += x;

     g_mfIndex++;
     if(g_mfIndex >= CH3_MF_LEN)
     {
         g_mfIndex = 0;
     }

     return g_mfSum / CH3_MF_LEN;
 }

 static void LevelTracker_Update(uint32_t x)
 {
     uint32_t delta;
     uint32_t span;

     if(g_levelCalibCount > 0)
     {
         if(x < g_levelLow)
         {
             g_levelLow = x;
         }

         if(x > g_levelHigh)
         {
             g_levelHigh = x;
         }

         g_levelCalibCount--;

         if(g_levelCalibCount == 0)
         {
             if((g_levelHigh <= g_levelLow) ||
                ((g_levelHigh - g_levelLow) < LEVEL_MIN_SPAN))
             {
                 g_levelLow = LEVEL_LOW_INIT;
                 g_levelHigh = LEVEL_HIGH_INIT;
             }

             g_decisionThreshold = (g_levelLow + g_levelHigh) / 2UL;
             span = g_levelHigh - g_levelLow;
             g_decisionThreshold += (span * DECISION_BIAS_NUM) / DECISION_BIAS_DEN;
             g_levelsReady = 1;
         }

         return;
     }

     if(x < g_levelLow)
     {
         delta = g_levelLow - x;
         g_levelLow -= (delta >> LEVEL_ATTACK_SHIFT);
     }
     else
     {
         delta = x - g_levelLow;
         g_levelLow += (delta >> LEVEL_RELEASE_SHIFT);
     }

     if(x > g_levelHigh)
     {
         delta = x - g_levelHigh;
         g_levelHigh += (delta >> LEVEL_ATTACK_SHIFT);
     }
     else
     {
         delta = g_levelHigh - x;
         g_levelHigh -= (delta >> LEVEL_RELEASE_SHIFT);
     }

     if((g_levelHigh - g_levelLow) < LEVEL_MIN_SPAN)
     {
         g_levelLow = g_decisionThreshold - (LEVEL_MIN_SPAN / 2UL);
         g_levelHigh = g_decisionThreshold + (LEVEL_MIN_SPAN / 2UL);
     }

     g_decisionThreshold = (g_levelLow + g_levelHigh) / 2UL;
     span = g_levelHigh - g_levelLow;
     g_decisionThreshold += (span * DECISION_BIAS_NUM) / DECISION_BIAS_DEN;
 }

 static uint8_t EdgeDetector_Update(uint32_t x)
 {
     uint32_t span;
     uint32_t thLow;
     uint32_t thHigh;
     uint8_t edgeEvent = 0;

     LevelTracker_Update(x);

     if(!g_levelsReady)
     {
         return 0;
     }

     span = g_levelHigh - g_levelLow;
     thLow = g_levelLow + ((span * EDGE_LOW_NUM) / EDGE_DEN);
     thHigh = g_levelLow + ((span * EDGE_HIGH_NUM) / EDGE_DEN);

     g_prevSchmittLevel = g_schmittLevel;

     if(x >= thHigh)
     {
         g_schmittLevel = 1;
     }
     else if(x <= thLow)
     {
         g_schmittLevel = 0;
     }

     if(g_edgeHoldoff > 0)
     {
         g_edgeHoldoff--;
     }

     if((g_schmittLevel != g_prevSchmittLevel) && (g_edgeHoldoff == 0))
     {
         edgeEvent = 1;
         g_edgeHoldoff = EDGE_HOLDOFF_TICKS;
     }

     return edgeEvent;
 }

 //*****************************************************************************
 // DPLL and decision
 //*****************************************************************************

 static uint8_t DPLL_Update(uint8_t edgeEvent)
 {
     uint8_t boundary = 0;
     uint8_t stepCount = 1;
     uint8_t i;

     if(edgeEvent && !g_pllAcquired)
     {
         g_pllPhase = 0;
         g_pllAcquired = 1;
         g_bitClock = 1;
         return 1;
     }

     if(edgeEvent)
     {
         if(g_pllPhase <= PLL_BOUNDARY_DEADBAND)
         {
             stepCount = 1;
         }
         else if(g_pllPhase < (OSR_N / 2UL))
         {
             stepCount = 0;
         }
         else if(g_pllPhase >= (OSR_N - PLL_BOUNDARY_DEADBAND))
         {
             stepCount = 1;
         }
         else
         {
             stepCount = 2;
         }
     }

     for(i = 0; i < stepCount; i++)
     {
         g_pllPhase++;
         if(g_pllPhase >= OSR_N)
         {
             g_pllPhase = 0;
             boundary = 1;
         }
     }

     g_bitClock = (g_pllPhase < (OSR_N / 2UL)) ? 1U : 0U;

     return boundary;
 }

 static void SymbolIntegrator_Update(uint32_t x, uint8_t symbolBoundary)
 {
     uint32_t thresholdSum;

     if(symbolBoundary)
     {
         if(g_symbolCount >= (OSR_N / 4UL))
         {
             thresholdSum = g_thresholdLatched * g_symbolCount;

             g_prevDecisionBit = g_decisionBit;
             g_decisionBit = (g_symbolSum >= thresholdSum) ? 1U : 0U;
             g_diffDecodedBit = g_decisionBit ^ g_prevDecisionBit;
         }

         g_symbolSum = 0;
         g_symbolCount = 0;
         g_symbolGuardCnt = 0;
         g_thresholdLatched = g_decisionThreshold;
     }

     if(g_symbolGuardCnt < INTEGRATOR_GUARD_TICKS)
     {
         g_symbolGuardCnt++;
     }
     else
     {
         g_symbolSum += x;
         g_symbolCount++;
     }
 }

 static void OutputDebugSignals(void)
 {
     GPIOPinWrite(BCLK_GPIO_BASE,
                  BCLK_PIN,
                  g_bitClock ? BCLK_PIN : 0);

     GPIOPinWrite(DECISION_GPIO_BASE,
                  DECISION_PIN,
                  g_decisionBit ? DECISION_PIN : 0);

     GPIOPinWrite(DECODER_GPIO_BASE,
                  DECODER_PIN,
                  g_diffDecodedBit ? DECODER_PIN : 0);
 }

 //*****************************************************************************
 // DAC7611 monitor output
 //*****************************************************************************

 static void DAC7611_Write(uint32_t dacData)
 {
     uint32_t i;
     uint32_t mask;

     if(dacData > ADC_MAX_VALUE)
     {
         dacData = ADC_MAX_VALUE;
     }

     mask = 0x0800UL;

     DAC_CLK_L();
     DAC_LD_H();
     DAC_CS_L();

     for(i = 0; i < 12UL; i++)
     {
         if((dacData & mask) != 0)
         {
             DAC_SDI_H();
         }
         else
         {
             DAC_SDI_L();
         }

         DAC_CLK_H();
         DAC_CLK_L();
         mask >>= 1;
     }

     DAC_CS_H();
     DAC_LD_L();
     DAC_LD_H();
 }

 //*****************************************************************************
 // Timer0A ISR
 //*****************************************************************************

 void Timer0IntHandler(void)
 {
     uint32_t adcValue;
     uint8_t edgeEvent;
     uint8_t symbolBoundary;

     ROM_TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
     GPIOPinWrite(ISR_PROF_BASE, ISR_PROF_PIN, 0);

     if(ADC0_ReadIfReady(&adcValue))
     {
         g_adcRaw = adcValue;

         g_adcMedian = ADC_Preprocess_Update(g_adcRaw);
         g_edgeFilterOut = EdgeFilter_Update(g_adcMedian);

         edgeEvent = EdgeDetector_Update(g_edgeFilterOut);
         symbolBoundary = DPLL_Update(edgeEvent);
         SymbolIntegrator_Update(g_edgeFilterOut, symbolBoundary);

         // CH3 monitor: one-symbol sliding-window matched filter directly
         // on the raw ADC input (no median pre-processing, no edge filter).
         // Simple boxcar integrator over the last OSR_N samples, scaled by
         // 1/OSR_N so it reads in ADC units.
         g_mfOut = MatchedFilter_Update(g_adcRaw);

         OutputDebugSignals();

         g_dacDecimCnt++;
         if(g_dacDecimCnt >= DAC_DECIM)
         {
             g_dacDecimCnt = 0;
             g_dacPendingValue = g_mfOut;
             g_dacPendingFlag = 1;
         }
     }

     ADC0_StartOnce();
     GPIOPinWrite(ISR_PROF_BASE, ISR_PROF_PIN, ISR_PROF_PIN);
 }
