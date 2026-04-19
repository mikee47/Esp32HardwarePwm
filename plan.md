# Esp32HardwarePwm — Reload Overhead Correction Plan

## Background

The LEDC hardware fade queue in the ESP32 accumulates timing error when chaining many
short micro-fades.  After each fade completes, the ISR fires, Sming posts a task-queue
message, the main task wakes and calls `ledc_set_fade_time_and_start`.  LEDC then waits
for the next timer edge before starting the new fade.  This "reload overhead" eats into
each step's requested duration and accumulates over hundreds of steps.

### Two separate error sources (measured by TimingTest_HwPWM)

| Channel | What it measures | Symptom |
|---------|-----------------|---------|
| CH0 — single long fade | LEDC duty-step quantisation | Duration always a bit short; deterministic |
| CH1 — 500 × 20 ms micro-fades | Per-step reload overhead accumulation | Gets longer as frequency / bit-depth increases |

---

## Current state (as of 2026-04-19)

### Build errors (Steps 1) — FIXED
`%u` → `%lu` format fixes already applied.

### Steps 2 + 4 — DONE (this session)

**Formula-based correction is now in the library** (`startNextFade()`):
```
overheadUs = (1_000_000 / frequency) + DISPATCH_LATENCY_US   // DISPATCH_LATENCY_US = 500
```
A per-`ChannelFadeQueue` carry accumulator prevents sub-ms rounding accumulation.

- `ChannelFadeQueue` has `reloadOverheadUs` + `carryUs` fields.
- `initialize()` and `setFrequency()` auto-compute and broadcast the overhead to all queues.
- `setReloadOverheadUs(channel, us)` / `getReloadOverheadUs(channel)` allow manual override.
- `resetQueue()` preserves `reloadOverheadUs` (resets `carryUs`).

**TimingTest app updated:**
- `MICROFADE_MS = 20` (50 Hz — unchanged), `TOTAL_FADE_MS = 20000`, `TOTAL_STEPS = 1000` (goal: ≤ 20 ms total deviation).
- `overheadCycles` removed from `TestConfig` and all 26 `configs[]` entries.
- `pushMicroSteps()` passes nominal `MICROFADE_MS` directly; library corrects internally.

---

## Plan

### Step 1 — Fix build errors  *(DONE)*

### Step 2 — Drop the empirical table; use a formula  *(DONE)*

Implemented in `startNextFade()`.  Formula:
```
overheadUs = (1_000_000 / frequency) + DISPATCH_LATENCY_US
```
`DISPATCH_LATENCY_US = 500` µs (compile-time constant in the .cpp; tune after hardware run).

### Step 3 — Tune DISPATCH_LATENCY_US  *(next hardware run)*

Flash and run.  Look at CH1 `per step us` column:
- If all configs show a small **positive** residual → increase `DISPATCH_LATENCY_US`.
- If all configs show a small **negative** residual → decrease `DISPATCH_LATENCY_US`.
- If residuals vary by frequency → the 1-period term dominates at low freq; consider
  checking whether an additional fractional-period component is needed.

Target: `|per step us| < 0.5 × periodUs` for all configs.

### Step 4 — Move correction into the library  *(DONE)*

Correction is now entirely inside `Esp32HardwarePwm::startNextFade()`.  The test app
passes nominal durations; the library deducts the carry-corrected overhead internally.
`setReloadOverheadUs(channel, us)` allows per-channel manual override.

### Step 5 — Address CH0 step_num clamping  *(separate bug)*

For long fades at low frequency + high bit-depth, `step_num` hits the LEDC hardware limit
of 1023.  Options:
- In `fadeToValueChan()`, detect when `requested_steps > 1023` and split into multiple
  hardware fades seamlessly.
- Or document the limitation clearly.

---

## File locations

| File | Role |
|------|------|
| `samples/TimingTest_HwPWM/app/application.cpp` | Timing sweep test app |
| `src/Esp32HardwarePwm.cpp` | Library implementation |
| `src/include/Esp32HardwarePwm.h` | Library header |

---

## Latest hardware run (with empirical overheadCycles table — for reference)

```
Config        | Quantisation error (CH0)      | Reload overhead (CH1)
              | deviation      % of total     | total dev ms  per step us
--------------|-------------------------------|---------------------------
 1kHz/ 8-bit  | -    53752 us   -0.5375%     |        989 ms      1978 us
 4kHz/ 8-bit  | -    54740 us   -0.5474%     |        740 ms      1480 us
 1kHz/10-bit  | -   792779 us   -7.9278%     | -         8 ms  -      17 us
 2kHz/10-bit  | -   281027 us   -2.8103%     |        490 ms       980 us
 3kHz/10-bit  | -   110036 us   -1.1004%     | -       174 ms  -     348 us
 4kHz/10-bit  | -    25438 us   -0.2544%     |        116 ms       233 us
 5kHz/10-bit  | -   178977 us   -1.7898%     | -       166 ms  -     333 us
 6kHz/10-bit  | -   111832 us   -1.1183%     | -       493 ms  -     987 us
 7kHz/10-bit  | -    62505 us   -0.6251%     | -       336 ms  -     672 us
 8kHz/10-bit  | -    25506 us   -0.2551%     | -       694 ms  -    1388 us
 1kHz/11-bit  | -  1812436 us  -18.1244%     | -         8 ms  -      16 us  ← step_num clamping
 2kHz/11-bit  | -   791041 us   -7.9104%     |        241 ms       482 us
 3kHz/11-bit  | -   451691 us   -4.5169%     | -         7 ms  -      15 us
 4kHz/11-bit  | -   280451 us   -2.8045%     |        116 ms       233 us
 5kHz/11-bit  | -   178303 us   -1.7830%     | -         8 ms  -      16 us
 6kHz/11-bit  | -   108372 us   -1.0837%     |        243 ms       487 us
 7kHz/11-bit  | -    58678 us   -0.5868%     | -       167 ms  -     335 us
 8kHz/11-bit  | -    25149 us   -0.2515%     | -       332 ms  -     665 us
 1kHz/12-bit  | -  1805433 us  -18.0543%     | -         8 ms  -      16 us  ← step_num clamping
 2kHz/12-bit  | -  1807533 us  -18.0753%     |        241 ms       482 us    ← step_num clamping
 3kHz/12-bit  | -   441477 us   -4.4148%     | -         5 ms  -      10 us
 4kHz/12-bit  | -   784946 us   -7.8495%     |          0 ms         1 us    ← step_num clamping?
 5kHz/12-bit  | -   170897 us   -1.7090%     | -        67 ms  -     135 us
 6kHz/12-bit  | -   447884 us   -4.4788%     |        168 ms       336 us
 7kHz/12-bit  | -    58203 us   -0.5820%     |         18 ms        36 us
 8kHz/12-bit  | -   273674 us   -2.7367%     |          0 ms         1 us
```

CH1 residuals with empirical table: still ±500–1400 µs/step for 10-bit high-freq configs.
Formula approach expected to be more consistent.
15 us
 4kHz/11-bit  | -   280451 us   -2.8045%     |        116 ms       23315 us
 4kHz/11-bit  | -   280451 us   -2.8045%     |        116 ms       233


 new run with current CH1 fixes 
 ========= Timing Accuracy Results =========
Test: 20000 ms total  |  1000 x 20 ms micro-fades

Config        | Quantisation error (CH0)      | Reload overhead (CH1)
              | deviation      %% of total     | total dev ms  per step us  timer cyc
--------------|-------------------------------|---------------------------
 1kHz/ 8-bit  | -   109328 us   -0.5466%     |       1488 ms      1488 us     1.49 cyc
 4kHz/ 8-bit  | -    46071 us   -0.2304%     |       1490 ms      1490 us     5.96 cyc
 1kHz/10-bit  | -   562536 us   -2.8127%     |       1489 ms      1489 us     1.49 cyc
 2kHz/10-bit  | -    51204 us   -0.2560%     |        990 ms       990 us     1.98 cyc
 3kHz/10-bit  | -   220792 us   -1.1040%     |        991 ms       991 us     2.97 cyc
 4kHz/10-bit  | -    51377 us   -0.2569%     |        677 ms       677 us     2.71 cyc
 5kHz/10-bit  | -   153572 us   -0.7679%     |        991 ms       991 us     4.96 cyc
 6kHz/10-bit  | -    53363 us   -0.2668%     |       1654 ms      1654 us     9.92 cyc
 7kHz/10-bit  | -   125337 us   -0.6267%     |       1275 ms      1275 us     8.93 cyc
 8kHz/10-bit  | -    51323 us   -0.2566%     |       1615 ms      1615 us    12.92 cyc
 1kHz/11-bit  | -  1582277 us   -7.9114%     |       1489 ms      1489 us     1.49 cyc
 2kHz/11-bit  | -   561295 us   -2.8065%     |        491 ms       491 us     0.98 cyc
 3kHz/11-bit  | -   222715 us   -1.1136%     |        433 ms       433 us     1.30 cyc
 4kHz/11-bit  | -    50619 us   -0.2531%     |        428 ms       428 us     1.71 cyc
 5kHz/11-bit  | -   357568 us   -1.7878%     |        291 ms       291 us     1.46 cyc
 6kHz/11-bit  | -   217397 us   -1.0870%     | -       281 ms  -     281 us    -1.69 cyc
 7kHz/11-bit  | -   118072 us   -0.5904%     |        365 ms       365 us     2.56 cyc
 8kHz/11-bit  | -    50943 us   -0.2547%     |        599 ms       599 us     4.79 cyc
 1kHz/12-bit  | -  3615277 us  -18.0764%     |       1489 ms      1489 us     1.49 cyc
 2kHz/12-bit  | -  1570290 us   -7.8514%     |        491 ms       491 us     0.98 cyc
 3kHz/12-bit  | -   884568 us   -4.4228%     | -       171 ms  -     171 us    -0.51 cyc
 4kHz/12-bit  | -   547622 us   -2.7381%     |        178 ms       178 us     0.71 cyc
 5kHz/12-bit  | -   342973 us   -1.7149%     |         11 ms        11 us     0.06 cyc
 6kHz/12-bit  | -   214658 us   -1.0733%     | -        71 ms  -      71 us    -0.43 cyc
 7kHz/12-bit  | -   117289 us   -0.5864%     |         96 ms        96 us     0.67 cyc
 8kHz/12-bit  | -    36224 us   -0.1811%     |        272 ms       272 us     2.18 cyc
===========================================
