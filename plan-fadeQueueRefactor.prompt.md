# Plan: Refactor Queue Handling into FadeQueue Class

## TL;DR
Extract `ChannelFadeQueue` + `FadeEntry` + the split-at-insertion logic from `Esp32HardwarePwm` into a standalone `FadeQueue` class in its own header. The class exposes `push()` / `pop()` / state accessors; `Esp32HardwarePwm` shrinks to hardware dispatch only. This makes the queue independently testable and reduces `Esp32HardwarePwm.cpp` (~847 lines) by ~150–200 lines.

## Current State
- `FadeEntry` { targetDuty, fadeTimeMs, isPartial }
- `ChannelFadeQueue` { ring buffer (vector), head, tail, count, cycleLen, mode, autoStart, reloadOverheadUs, carryUs, activeIsIntermediate }
- Queue-related private methods in Esp32HardwarePwm: `dequeueFifo`, `startNextFade`
- Queue-related public methods (13): setQueueMode/getQueueMode, queueFadeChan, queueFadePercentChan, getQueueEntries, resetQueue, startQueue, setQueueCapacity/getQueueCapacity, setQueueAutoStart/getQueueAutoStart, setReloadOverheadUs/getReloadOverheadUs
- Esp32HardwarePwm.cpp: 847 lines

## Key Design Decisions
- `pop()` applies reload overhead correction internally (it's queue-internal state)
- `pop()` sets internal `lastDidWrap` flag (bool); caller reads `q.didWrap()` after pop
- `push(fromDuty, targetDuty, fadeTimeMs, maxDuty)` handles splitting
- No callbacks stored in FadeQueue; cyclic wrap signalled via `didWrap()` query
- `activeIsIntermediate` renamed to `isPopIntermediate()` or similar
- `QueueMode` enum extracted to `src/include/PwmTypes.h` (included by both FadeQueue.h and Esp32HardwarePwm.h)
- `FADE_QUEUE_DEPTH` macro also moves to `PwmTypes.h`

## Files
- **NEW** `src/include/PwmTypes.h` — `QueueMode` enum + `FADE_QUEUE_DEPTH` macro
- **NEW** `src/include/FadeQueue.h` — FadeQueue class + FadeEntry struct
- **NEW** `src/FadeQueue.cpp` — FadeQueue implementation
- **MOD** `src/include/Esp32HardwarePwm.h` — include FadeQueue.h, replace ChannelFadeQueue/FadeEntry with FadeQueue, remove private methods
- **MOD** `src/Esp32HardwarePwm.cpp` — delegate queue methods, simplify startNextFade/dispatchFadeCallbacks
- **MOD** `component.mk` / `Component.json` — add FadeQueue.cpp to sources if needed (Sming auto-discovers src/*.cpp usually)

## Steps

### Phase 1 — PwmTypes.h
1. Create `src/include/PwmTypes.h`:
   - Move `QueueMode` enum from `Esp32HardwarePwm.h`
   - Move `FADE_QUEUE_DEPTH` macro from `Esp32HardwarePwm.h`

### Phase 2 — FadeQueue class (independent)
2. Create `src/include/FadeQueue.h`:
   - `FadeEntry` struct { targetDuty, fadeTimeMs, isPartial }
   - `FadeQueue` class with fields from ChannelFadeQueue
   - Public: `push(fromDuty, targetDuty, fadeTimeMs, maxDuty)`, `pop()→FadeEntry`, `hasEntries()`, `isPopIntermediate()`, `didCyclicWrap()`, `reset()`, setMode/getMode, setCapacity/getCapacity, setAutoStart/getAutoStart, setReloadOverheadUs/getReloadOverheadUs, getEntryCount()
3. Create `src/FadeQueue.cpp` — implement push (split logic), pop (overhead correction), reset, capacity management

### Phase 3 — Esp32HardwarePwm integration
4. Update `Esp32HardwarePwm.h`: include FadeQueue.h, replace `std::vector<ChannelFadeQueue> fadeQueues_` with `std::vector<FadeQueue> fadeQueues_`, remove FadeEntry/ChannelFadeQueue nested structs, remove `dequeueFifo` private method declaration
5. Update `Esp32HardwarePwm.cpp`:
   - Remove `dequeueFifo` implementation
   - `queueFadeChan`: delegate to `fadeQueues_[channel].push(fromDuty, targetDuty, fadeTimeMs, getMaxDuty())`; fromDuty from `pins_[channel].targetDuty` if queue empty else last entry
   - `startNextFade`: call `fadeQueues_[channel_idx].pop()`, call `fadeToValueChan`; fire `onCyclicWrap_` if `q.didCyclicWrap()`
   - `dispatchFadeCallbacks`: check `fadeQueues_[i].isPopIntermediate()` instead of `activeIsIntermediate`
   - Queue public methods become one-liner delegates
   - `initialize()` / `setFrequency()`: call `fadeQueues_[i].setReloadOverheadUs(overhead)` directly

## Verification
1. Build without errors in the Sming build system
2. Run TimingTest_HwPWM — queue dispatch timing should be unchanged
3. Run Callbacks_HwPWM sample to verify onFadeDone_ only fires on the final split segment
4. Check getQueueEntries() returns logical fade count when large ranges are queued (counts segments, not user entries — needs consideration)

## Open Questions
- `getQueueEntries()` currently returns segment count after splitting. If we want it to return *logical* fade count, we'd need to track that separately in FadeQueue. Recommend keeping segment count for now (it's what the capacity check needs), document it.
- Should `FadeQueue` own the `autoStart` logic (i.e. know whether to call a "start" hook), or should `Esp32HardwarePwm` continue to handle the `autoStart && !isFadingChan && count==0` check after `push()`?
