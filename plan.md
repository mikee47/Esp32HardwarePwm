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

full calibration values:
========= Timing Accuracy Results =========
Test: 5000 ms total  |  250 x 20 ms micro-fades

Config        | Quantisation error (CH0)     | Reload overhead (CH1)
              | deviation      %% of total   | total dev ms  per step us  timer cyc
--------------|------------------------------|---------------------------
 1kHz/ 8-bit  | -   146801 us   -2.9360%     | -      136 ms  -     547 us    -0.55 cyc
 2kHz/ 8-bit  | -    27018 us   -0.5404%     | -      143 ms  -     572 us    -1.14 cyc
 3kHz/ 8-bit  | -    74435 us   -1.4887%     |        474 ms      1896 us     5.69 cyc
 4kHz/ 8-bit  | -    27112 us   -0.5422%     |        354 ms      1419 us     5.68 cyc
 5kHz/ 8-bit  | -     1676 us   -0.0335%     |       1475 ms      5902 us    29.51 cyc
 6kHz/ 8-bit  | -    27395 us   -0.5479%     |        396 ms      1585 us     9.51 cyc
 7kHz/ 8-bit  | -     8796 us   -0.1759%     |       1228 ms      4912 us    34.38 cyc
 8kHz/ 8-bit  | -    27244 us   -0.5449%     |       2969 ms     11878 us    95.02 cyc
 9kHz/ 8-bit  | -    13049 us   -0.2610%     |       2084 ms      8337 us    75.03 cyc
10kHz/ 8-bit  | -     1725 us   -0.0345%     |       1375 ms      5503 us    55.03 cyc
11kHz/ 8-bit  | -    15478 us   -0.3096%     |        796 ms      3186 us    35.05 cyc
12kHz/ 8-bit  | -     5797 us   -0.1159%     |        313 ms      1255 us    15.06 cyc
13kHz/ 8-bit  | -    17303 us   -0.3461%     | -      108 ms  -    434 us    -5.64 cyc
14kHz/ 8-bit  | -     9294 us   -0.1859%     | -      257 ms  -   1030 us   -14.42 cyc
15kHz/ 8-bit  | -     2082 us   -0.0416%     | -       29 ms  -    117 us    -1.75 cyc
16kHz/ 8-bit  | -    11358 us   -0.2272%     | -       20 ms  -     81 us    -1.30 cyc
 1kHz/ 9-bit  | -   393104 us   -7.8621%     |        943 ms      3775 us     3.77 cyc
 2kHz/ 9-bit  | -   137623 us   -2.7525%     | -      135 ms  -    541 us    -1.08 cyc
 3kHz/ 9-bit  | -    60022 us   -1.2004%     | -        6 ms  -     24 us    -0.07 cyc
 4kHz/ 9-bit  | -    17414 us   -0.3483%     | -      174 ms  -    699 us    -2.80 cyc
 5kHz/ 9-bit  | -    94084 us   -1.8817%     |        230 ms       922 us     4.61 cyc
 6kHz/ 9-bit  | -    59795 us   -1.1959%     |        397 ms      1590 us     9.54 cyc
 7kHz/ 9-bit  | -    35985 us   -0.7197%     |        172 ms       689 us     4.82 cyc
 8kHz/ 9-bit  | -    17474 us   -0.3495%     |        386 ms      1544 us    12.35 cyc
 9kHz/ 9-bit  | -     3782 us   -0.0756%     |        703 ms      2813 us    25.32 cyc
10kHz/ 9-bit  | -    43030 us   -0.8606%     |       1425 ms      5703 us    57.03 cyc
11kHz/ 9-bit  | -    29598 us   -0.5920%     |        841 ms      3364 us    37.00 cyc
12kHz/ 9-bit  | -    17993 us   -0.3599%     |        354 ms      1418 us    17.02 cyc
13kHz/ 9-bit  | -     7603 us   -0.1521%     | -       56 ms  -    226 us    -2.94 cyc
14kHz/ 9-bit  | -    36044 us   -0.7209%     |       1531 ms      6124 us    85.74 cyc
15kHz/ 9-bit  | -    25411 us   -0.5082%     |       3501 ms     14007 us   210.10 cyc
16kHz/ 9-bit  | -    17531 us   -0.3506%     |       2969 ms     11878 us   190.05 cyc
 1kHz/10-bit  | -   899822 us  -17.9964%     |        113 ms       452 us     0.45 cyc
 2kHz/10-bit  | -   388239 us   -7.7648%     | -      135 ms  -    540 us    -1.08 cyc
 3kHz/10-bit  | -   217656 us   -4.3531%     | -       38 ms  -    155 us    -0.47 cyc
 4kHz/10-bit  | -   132653 us   -2.6531%     | -      182 ms  -    728 us    -2.91 cyc
 5kHz/10-bit  | -    89284 us   -1.7857%     | -      154 ms  -    617 us    -3.08 cyc
 6kHz/10-bit  | -    55716 us   -1.1143%     |          8 ms        34 us     0.20 cyc
 7kHz/10-bit  | -    31132 us   -0.6226%     | -      132 ms  -    531 us    -3.72 cyc
 8kHz/10-bit  | -    12595 us   -0.2519%     | -       81 ms  -    327 us    -2.62 cyc
 9kHz/10-bit  | -   112548 us   -2.2510%     |        153 ms       615 us     5.54 cyc
10kHz/10-bit  | -    89330 us   -1.7866%     |        155 ms       623 us     6.23 cyc
11kHz/10-bit  | -    71236 us   -1.4247%     |        159 ms       638 us     7.02 cyc
12kHz/10-bit  | -    54242 us   -1.0848%     |        356 ms      1426 us    17.11 cyc
13kHz/10-bit  | -    43614 us   -0.8723%     | -       58 ms  -    232 us    -3.02 cyc
14kHz/10-bit  | -    29426 us   -0.5885%     |        233 ms       935 us    13.09 cyc
15kHz/10-bit  | -    22437 us   -0.4487%     |        677 ms      2709 us    40.64 cyc
16kHz/10-bit  | -    12642 us   -0.2528%     |        323 ms      1295 us    20.72 cyc
 1kHz/11-bit  | -   212821 us   -4.2564%     |        113 ms       452 us     0.45 cyc
 2kHz/11-bit  | -   214223 us   -4.2845%     | -      135 ms  -    540 us    -1.08 cyc
 3kHz/11-bit  | -   215393 us   -4.3079%     | -      135 ms  -    542 us    -1.63 cyc
 4kHz/11-bit  | -    44640 us   -0.8928%     | -      182 ms  -    729 us    -2.92 cyc
 5kHz/11-bit  | -    78883 us   -1.5777%     | -      150 ms  -    602 us    -3.01 cyc
 6kHz/11-bit  | -   100729 us   -2.0146%     | -      341 ms  -   1367 us    -8.20 cyc
 7kHz/11-bit  | -    18943 us   -0.3789%     | -      250 ms  -   1002 us    -7.01 cyc
 8kHz/11-bit  | -    44843 us   -0.8969%     | -      225 ms  -    900 us    -7.20 cyc
 9kHz/11-bit  | -    72006 us   -1.4401%     | -       40 ms  -    160 us    -1.44 cyc
10kHz/11-bit  | -    18421 us   -0.3684%     | -      114 ms  -    456 us    -4.56 cyc
11kHz/11-bit  | -    37608 us   -0.7522%     | -       99 ms  -    399 us    -4.39 cyc
12kHz/11-bit  | -    54652 us   -1.0930%     |         14 ms        58 us     0.70 cyc
13kHz/11-bit  | -    14776 us   -0.2955%     | -       58 ms  -    232 us    -3.02 cyc
14kHz/11-bit  | -    30354 us   -0.6071%     | -      128 ms  -    515 us    -7.21 cyc
15kHz/11-bit  | -    38884 us   -0.7777%     | -       98 ms  -    395 us    -5.92 cyc
16kHz/11-bit  | -    10098 us   -0.2020%     | -       73 ms  -    294 us    -4.70 cyc
 1kHz/12-bit  | -    73756 us   -1.4751%     |        113 ms       452 us     0.45 cyc
 2kHz/12-bit  | -    75731 us   -1.5146%     | -      135 ms  -    540 us    -1.08 cyc
 3kHz/12-bit  | -    75588 us   -1.5118%     | -      300 ms  -   1203 us    -3.61 cyc
 4kHz/12-bit  | -    76890 us   -1.5378%     | -      197 ms  -    791 us    -3.16 cyc
 5kHz/12-bit  | -    77283 us   -1.5457%     | -      320 ms  -   1281 us    -6.41 cyc
 6kHz/12-bit  | -    79395 us   -1.5879%     | -      233 ms  -    935 us    -5.61 cyc
 7kHz/12-bit  | -    79435 us   -1.5887%     | -      253 ms  -   1015 us    -7.11 cyc
 8kHz/12-bit  | -    77618 us   -1.5524%     | -      225 ms  -    900 us    -7.20 cyc
 9kHz/12-bit  | -    73651 us   -1.4730%     | -      318 ms  -   1275 us   -11.48 cyc
10kHz/12-bit  |       4209 us    0.0842%     | -      215 ms  -    861 us    -8.61 cyc
11kHz/12-bit  |       1846 us    0.0369%     | -      198 ms  -    792 us    -8.71 cyc
12kHz/12-bit  | -     5462 us   -0.1092%     | -      359 ms  -   1437 us   -17.24 cyc
13kHz/12-bit  | -     9698 us   -0.1940%     | -      210 ms  -    841 us   -10.93 cyc
14kHz/12-bit  | -    21307 us   -0.4261%     | -      281 ms  -   1126 us   -15.76 cyc
15kHz/12-bit  | -    28201 us   -0.5640%     | -      295 ms  -   1183 us   -17.75 cyc
16kHz/12-bit  | -    18630 us   -0.3726%     | -      249 ms  -    996 us   -15.94 cyc
 1kHz/13-bit  |    3199269 us   63.9854%     |        105 ms       421 us     0.42 cyc
 2kHz/13-bit  | -   437231 us   -8.7446%     | -      135 ms  -    540 us    -1.08 cyc
 3kHz/13-bit  | -   137262 us   -2.7452%     | -      220 ms  -    882 us    -2.65 cyc
 4kHz/13-bit  | -   211890 us   -4.2378%     | -      260 ms  -   1040 us    -4.16 cyc
 5kHz/13-bit  | -    75833 us   -1.5167%     | -      285 ms  -   1141 us    -5.71 cyc
 6kHz/13-bit  | -   132984 us   -2.6597%     | -      338 ms  -   1355 us    -8.13 cyc
 7kHz/13-bit  | -    52303 us   -1.0461%     | -      302 ms  -   1208 us    -8.46 cyc
 8kHz/13-bit  | -    91459 us   -1.8292%     | -      346 ms  -   1384 us   -11.07 cyc
 9kHz/13-bit  | -    32063 us   -0.6413%     | -      318 ms  -   1275 us   -11.48 cyc
 1kHz/14-bit  |     466366 us    9.3273%     |        105 ms       421 us     0.42 cyc
 2kHz/14-bit  |    3200171 us   64.0034%     | -      143 ms  -    573 us    -1.15 cyc
 3kHz/14-bit  |     471198 us    9.4240%     | -      305 ms  -   1223 us    -3.67 cyc
 4kHz/14-bit  | -   878583 us  -17.5717%     | -      307 ms  -   1231 us    -4.92 cyc
===========================================
