/*
 * TimingTest_HwPWM — timing accuracy sweep across multiple PWM configurations
 *
 * Runs the same two-channel measurement for each entry in the `configs[]` table:
 *
 *   CH0 — single TOTAL_FADE_MS hardware fade (0% → 100%).
 *          Measures duty-step quantisation error: LEDC always rounds the fade
 *          duration down to an integer number of timer cycles, so CH0 is
 *          typically a few hundred µs short of the requested duration.
 *
 *   CH1 — TOTAL_STEPS × MICROFADE_MS micro-fades chained via the FIFO queue.
 *          Measures per-step reload latency: after each step the LEDC ISR
 *          fires, Sming posts a task-queue message, the main task wakes and
 *          calls ledc_set_fade_time_and_start.  LEDC then waits for the next
 *          timer edge before starting.  This accumulates over all steps.
 *
 * After all configs complete a summary table is printed:
 *
 *   Config        | Quant err (CH0)         | Reload overhead (CH1)
 *                 | deviation µs   % total  | total dev ms   per step µs
 */

#include <SmingCore.h>
#include <Esp32HardwarePwm.h>
#include <esp_timer.h>

namespace
{
// ---------------------------------------------------------------------------
// Test parameters — shared across all configs
// ---------------------------------------------------------------------------
constexpr uint32_t TOTAL_FADE_MS         = 10000; ///< Total fade duration (ms) per run
constexpr uint32_t MICROFADE_MS          = 20;    ///< Each micro-step duration (ms) — 50 Hz
constexpr uint32_t TOTAL_STEPS           = TOTAL_FADE_MS / MICROFADE_MS; ///< 1000
constexpr uint8_t  CH_SINGLE             = 0;     ///< Single long fade channel
constexpr uint8_t  CH_MICRO              = 1;     ///< Micro-fade queue channel
constexpr uint8_t  MICROFADE_QUEUE_DEPTH = 20;    ///< FIFO depth for CH1

// ---------------------------------------------------------------------------
// PWM configurations to benchmark
// ---------------------------------------------------------------------------
struct TestConfig {
    ledc_timer_bit_t resolution;
    uint32_t         frequency;
    const char*      label;
};

// clang-format off
// Reload overhead correction is computed automatically by the library:
//   overheadUs = (1_000_000 / frequency) + DISPATCH_LATENCY_US
// 8kHz/8-bit omitted: cycles_per_duty_step ≈ 1882 exceeds ESP32 LEDC step_num max (1023)
static const TestConfig configs[] = {
    { LEDC_TIMER_8_BIT,   1000, " 1kHz/ 8-bit" },
    { LEDC_TIMER_8_BIT,   4000, " 4kHz/ 8-bit" },
    { LEDC_TIMER_10_BIT,  1000, " 1kHz/10-bit" },
    { LEDC_TIMER_10_BIT,  2000, " 2kHz/10-bit" },
    { LEDC_TIMER_10_BIT,  3000, " 3kHz/10-bit" },
    { LEDC_TIMER_10_BIT,  4000, " 4kHz/10-bit" },
    { LEDC_TIMER_10_BIT,  5000, " 5kHz/10-bit" },
    { LEDC_TIMER_10_BIT,  6000, " 6kHz/10-bit" },
    { LEDC_TIMER_10_BIT,  7000, " 7kHz/10-bit" },
    { LEDC_TIMER_10_BIT,  8000, " 8kHz/10-bit" },
    { LEDC_TIMER_11_BIT,  1000, " 1kHz/11-bit" },
    { LEDC_TIMER_11_BIT,  2000, " 2kHz/11-bit" },
    { LEDC_TIMER_11_BIT,  3000, " 3kHz/11-bit" },
    { LEDC_TIMER_11_BIT,  4000, " 4kHz/11-bit" },
    { LEDC_TIMER_11_BIT,  5000, " 5kHz/11-bit" },
    { LEDC_TIMER_11_BIT,  6000, " 6kHz/11-bit" },
    { LEDC_TIMER_11_BIT,  7000, " 7kHz/11-bit" },
    { LEDC_TIMER_11_BIT,  8000, " 8kHz/11-bit" },
    { LEDC_TIMER_12_BIT,  1000, " 1kHz/12-bit" },
    { LEDC_TIMER_12_BIT,  2000, " 2kHz/12-bit" },
    { LEDC_TIMER_12_BIT,  3000, " 3kHz/12-bit" },
    { LEDC_TIMER_12_BIT,  4000, " 4kHz/12-bit" },
    { LEDC_TIMER_12_BIT,  5000, " 5kHz/12-bit" },
    { LEDC_TIMER_12_BIT,  6000, " 6kHz/12-bit" },
    { LEDC_TIMER_12_BIT,  7000, " 7kHz/12-bit" },
    { LEDC_TIMER_12_BIT,  8000, " 8kHz/12-bit" },
};
// clang-format on
constexpr size_t NUM_CONFIGS = sizeof(configs) / sizeof(configs[0]);

// ---------------------------------------------------------------------------
// Per-run result storage
// ---------------------------------------------------------------------------
struct TestResult {
    bool    valid;
    int64_t devCh0_us;     ///< Quantisation error: elapsedCh0 − expected (µs)
    int64_t devCh1_us;     ///< Total CH1 deviation from expected (µs)
    int64_t latPerStep_us; ///< devCh1 / TOTAL_STEPS (µs)
};
static TestResult results[NUM_CONFIGS];
static size_t     currentConfig = 0;

// ---------------------------------------------------------------------------
// Per-run mutable state
// ---------------------------------------------------------------------------
static std::vector<uint8_t> pinList{13, 12};
static Esp32HardwarePwm*    pwm         = nullptr;
static int64_t              tStart      = 0;
static int64_t              tEndCh0     = 0;
static int64_t              tEndCh1     = 0;
static uint32_t             stepsPushed = 0;
static bool                 ch0Done     = false;
static bool                 ch1Done     = false;


// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Triangle wave: alternate 0%↔100% so every step is a real duty transition.
float stepDuty(uint32_t step)
{
    return (step % 2 == 0) ? 0.0f : 100.0f;
}

void pushMicroSteps()
{
    // Reload overhead correction is applied automatically by the library
    // (1 PWM period + DISPATCH_LATENCY_US per step, via carry accumulator).
    while(stepsPushed < TOTAL_STEPS) {
        if(!pwm->queueFadePercentChan(CH_MICRO, stepDuty(stepsPushed), MICROFADE_MS))
            break;
        ++stepsPushed;
    }
}

// Forward declarations
void runConfig(size_t idx);
void printTable();

// Called from the Sming main task after dispatchFadeCallbacks fully returns.
// Safe to delete the old pwm object and create the next one here.
static void teardownAndRunNext()
{
    delete pwm;
    pwm = nullptr;

    if(currentConfig < NUM_CONFIGS)
        runConfig(currentConfig);
    else
        printTable();
}

// Called from within dispatchFadeCallbacks via onQueueEmpty — must NOT delete
// pwm here because dispatchFadeCallbacks still holds a pointer to it.  Just
// record the result, advance the counter, and queue the actual teardown.
void onRunComplete()
{
    constexpr int64_t expectedUs = (int64_t)TOTAL_FADE_MS * 1000LL;

    TestResult& r   = results[currentConfig];
    r.valid         = true;
    r.devCh0_us     = (tEndCh0 - tStart) - expectedUs;
    r.devCh1_us     = (tEndCh1 - tStart) - expectedUs;
    r.latPerStep_us = TOTAL_STEPS > 0 ? r.devCh1_us / (int64_t)TOTAL_STEPS : 0LL;

    Serial.printf("[%u/%u done] quant=%+lld us  reload=%+lld ms  lat/step=%+lld us\n",
                  (unsigned)(currentConfig + 1), (unsigned)NUM_CONFIGS,
                  r.devCh0_us, r.devCh1_us / 1000LL, r.latPerStep_us);

    ++currentConfig;
    // Defer teardown to after dispatchFadeCallbacks returns
    System.queueCallback(teardownAndRunNext);
}

void tryComplete()
{
    if(ch0Done && ch1Done)
        onRunComplete();
}

// ---------------------------------------------------------------------------
// Run one configuration
// ---------------------------------------------------------------------------
void runConfig(size_t idx)
{
    const TestConfig& cfg = configs[idx];
    Serial.printf("\n--- Config %u/%u: %s  (%lu x %lu ms) ---\n",
                  (unsigned)(idx + 1), (unsigned)NUM_CONFIGS,
                  cfg.label,
                  (unsigned long)TOTAL_STEPS, (unsigned long)MICROFADE_MS);

    // Reset per-run state
    tStart = tEndCh0 = tEndCh1 = 0;
    stepsPushed = 0;
    ch0Done = ch1Done = false;

    // clang-format off
    pwm = new Esp32HardwarePwm(pinList, Esp32HardwarePwm::Config{
        .timer = {
            .resolution = cfg.resolution,
            .frequency  = cfg.frequency,
        },
        .phaseShift = {
            .mode = Esp32HardwarePwm::PhaseShiftMode::OFF,
        },
        .spreadSpectrum = {
            .mode = Esp32HardwarePwm::SpreadSpectrumMode::OFF,
        },
    });
    // clang-format on

    if(!pwm->isInitialized()) {
        Serial.println(_F("PWM init failed — skipping config"));
        results[idx].valid = false;
        delete pwm;
        pwm = nullptr;
        ++currentConfig;
        if(currentConfig < NUM_CONFIGS)
            runConfig(currentConfig);
        else
            printTable();
        return;
    }

    pwm->setOnFadeDoneCallback([](uint8_t ch) {
        if(ch == CH_MICRO && !ch1Done)
            pushMicroSteps();
    });

    pwm->setOnQueueEmptyCallback([](uint8_t ch) {
        int64_t now = esp_timer_get_time();
        if(ch == CH_SINGLE && !ch0Done) {
            tEndCh0 = now;
            ch0Done = true;
            Serial.printf("[CH0] done  elapsed=%lld ms\n", (now - tStart) / 1000LL);
            tryComplete();
        } else if(ch == CH_MICRO && !ch1Done) {
            if(stepsPushed < TOTAL_STEPS) {
                pushMicroSteps(); // spurious drain during pre-fill
                return;
            }
            tEndCh1 = now;
            ch1Done = true;
            Serial.printf("[CH1] done  elapsed=%lld ms\n", (now - tStart) / 1000LL);
            tryComplete();
        }
    });

    // CH_SINGLE uses the default FADE_QUEUE_DEPTH (10) — sufficient for all resolutions
    // (worst case: 12-bit needs 5 segments, ceil(4095/1023)=5)
    pwm->setQueueAutoStart(CH_SINGLE, true);
    pwm->setQueueCapacity(CH_MICRO, MICROFADE_QUEUE_DEPTH);
    pwm->setQueueAutoStart(CH_MICRO, true);

    tStart = esp_timer_get_time();
    pwm->queueFadePercentChan(CH_SINGLE, 100.0f, TOTAL_FADE_MS);
    pushMicroSteps();

    Serial.printf("Running... (~%lu s)\n", (unsigned long)(TOTAL_FADE_MS / 1000));
}

// ---------------------------------------------------------------------------
// Print final comparison table
// ---------------------------------------------------------------------------
void printTable()
{
    constexpr int64_t expectedUs = (int64_t)TOTAL_FADE_MS * 1000LL;

    Serial.println();
    Serial.println(_F("========= Timing Accuracy Results ========="));
    Serial.printf("Test: %lu ms total  |  %lu x %lu ms micro-fades\n\n",
                  (unsigned long)TOTAL_FADE_MS,
                  (unsigned long)TOTAL_STEPS, (unsigned long)MICROFADE_MS);

    Serial.println(_F("Config        | Quantisation error (CH0)      | Reload overhead (CH1)"));
    Serial.println(_F("              | deviation      %% of total     | total dev ms  per step us  timer cyc"));
    Serial.println(_F("--------------|-------------------------------|---------------------------"));

    for(size_t i = 0; i < NUM_CONFIGS; ++i) {
        const TestResult& r = results[i];
        if(!r.valid) {
            Serial.printf("%-13s | (skipped — init failed)\n", configs[i].label);
            continue;
        }
        double pctCh0 = 100.0 * (double)r.devCh0_us / (double)expectedUs;
        // Timer cycles = per-step error / one timer period = latPerStep_us * freq / 1_000_000
        double cyclesDev = (double)r.latPerStep_us * (double)configs[i].frequency / 1000000.0;
        Serial.printf("%-13s | %+9lld us  %+8.4f%%     | %+10lld ms  %+8lld us  %+7.2f cyc\n",
                      configs[i].label,
                      r.devCh0_us,
                      pctCh0,
                      r.devCh1_us / 1000LL,
                      r.latPerStep_us,
                      cyclesDev);
    }
    Serial.println(_F("==========================================="));
}

} // namespace

void init()
{
    Serial.begin(SERIAL_BAUD_RATE);
    Serial.systemDebugOutput(true);

    Serial.println(_F("HwPWM Timing Accuracy Sweep"));
    Serial.printf("  Total fade:   %lu ms\n", (unsigned long)TOTAL_FADE_MS);
    Serial.printf("  Micro-step:   %lu ms  (%lu steps)\n",
                  (unsigned long)MICROFADE_MS, (unsigned long)TOTAL_STEPS);
    Serial.printf("  Queue depth:  %u\n", MICROFADE_QUEUE_DEPTH);
    Serial.printf("  Configs:      %u  (~%lu min total)\n\n",
                  (unsigned)NUM_CONFIGS,
                  (unsigned long)((NUM_CONFIGS * (TOTAL_FADE_MS + 10000)) / 60000));

    runConfig(0);
}
