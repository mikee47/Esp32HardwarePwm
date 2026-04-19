# Plan: Per-channel Fade Queue + Completion Callbacks

## TL;DR

Add a per-channel circular fade queue (depth configurable via `#define FADE_QUEUE_DEPTH 10`) to
`Esp32HardwarePwm`. When the LEDC hardware fade ISR fires, an ISR-safe bitmask +
`System.queueCallback` pattern hands off to task context, which drains the bitmask, starts the
next queued fade, and fires two separate `Delegate<void(uint8_t channel)>` callbacks:

- `onFadeDone_` — fires after **every** individual fade completes (even if more are queued)
- `onQueueEmpty_` — fires only when the queue drains to empty and the channel goes idle

On the last fade in a sequence both fire, in that order. A new `queueFadeChan()` method sits
alongside the unchanged `fadeToValueChan()`.

---

## Decisions

- Both `onFadeDone` AND `onQueueEmpty` as separate `Delegate<void(uint8_t)>` members
- New `queueFadeChan()` method; existing `fadeToValueChan()` is **unchanged**
- Two queue modes per channel, set via `setFadeQueueMode(channel, mode)` before filling the queue:
  - `FIFO` — queue drains and stops; `onQueueEmpty` fires when exhausted
  - `CYCLIC` — when the last entry is consumed, playback wraps to entry 0; queue never empties; `onQueueEmpty` never fires; instead `onCyclicWrap_` fires each time the sequence loops back to the beginning
- Mode is a queue-wide property, not per-entry; `queueFadeChan` does **not** take a mode parameter
- `ledc_set_fade_time_and_start` is **not ISR-safe** (uses mutex) → all queue processing in task context
- `fadeDoneCallback` ISR loop replaced with O(1) index lookup: `i = param->channel - pins_[0].channel`
- `uint32_t` bitmask for pending callbacks — safe for up to 32 channels (`SOC_LEDC_CHANNEL_NUM` ≤ 8 on classic ESP32); guarded by `static_assert`

---

## Files to Modify

- `src/include/Esp32HardwarePwm.h`
- `src/Esp32HardwarePwm.cpp`
- `samples/Advanced_HwPWM/app/application.cpp`

---

## Steps

### Phase 1 — Header (`Esp32HardwarePwm.h`)

1. Add `#include <Delegate.h>`
2. Add configurable queue depth:
   ```cpp
   #ifndef FADE_QUEUE_DEPTH
   #define FADE_QUEUE_DEPTH 10
   #endif
   ```
3. Add two private structs:
   ```cpp
   struct FadeEntry { uint32_t targetDuty; uint32_t fadeTimeMs; };

   enum class FadeQueueMode : uint8_t { FIFO, CYCLIC };

   struct ChannelFadeQueue {
       FadeEntry entries[FADE_QUEUE_DEPTH];
       uint8_t head = 0;           ///< Next entry to consume
       uint8_t tail = 0;           ///< Next free slot for writes
       uint8_t count = 0;          ///< Entries enqueued (FIFO: decrements on pop; CYCLIC: fixed after seeding)
       uint8_t cycleLen = 0;       ///< Number of entries in the cycle (CYCLIC only)
       FadeQueueMode mode = FadeQueueMode::FIFO;
   };
   ```
4. `PinConfig` is **not changed**. Instead add a private member:
   - `std::vector<ChannelFadeQueue> fadeQueues_` — sized alongside `pins_` in the constructor
5. Add public API declarations:
   - `void setFadeQueueMode(uint8_t channel, FadeQueueMode mode)` — must be called before filling the queue; changing mode while the queue is running has undefined behaviour (document this)
   - `FadeQueueMode getFadeQueueMode(uint8_t channel) const`
   - `bool queueFadeChan(uint8_t channel, uint32_t targetDuty, uint32_t fadeTimeMs)` — returns false if queue full
   - `bool queueFadePercentChan(uint8_t channel, float targetPct, uint32_t fadeTimeMs)` (inline wrapper)
   - `void setOnFadeDoneCallback(Delegate<void(uint8_t)> cb)`
   - `void setOnQueueEmptyCallback(Delegate<void(uint8_t)> cb)`
   - `void setOnCyclicWrapCallback(Delegate<void(uint8_t)> cb)` — fires each time CYCLIC mode loops back to entry 0
   - `uint8_t getFadeQueueCount(uint8_t channel) const`
   - `void resetFadeQueue(uint8_t channel)` — stops cycling, clears queue, resets mode to FIFO
6. Add private members:
   - `Delegate<void(uint8_t)> onFadeDone_`
   - `Delegate<void(uint8_t)> onQueueEmpty_`
   - `Delegate<void(uint8_t)> onCyclicWrap_`
   - `std::vector<ChannelFadeQueue> fadeQueues_`
   - `volatile uint32_t pendingFadeCallbacks_ = 0`
   - `volatile bool fadeCallbackQueued_ = false`
7. Add private method declarations:
   - `static void dispatchFadeCallbacks(uint32_t param)` — task context, no `IRAM_ATTR`
   - `bool popAndStartNextFade(uint8_t channel_idx)`

### Phase 2 — Implementation (`Esp32HardwarePwm.cpp`)

8. Verify/add `#include <Platform/System.h>` for `System.queueCallback`
9. **Modify `fadeDoneCallback`** (ISR — keep `IRAM_ATTR`):
   - Replace loop with O(1): `size_t i = param->channel - self->pins_[0].channel`
   - Keep: `currentDuty = targetDuty`, `fadeDone_[i] = true`
   - Add: `self->pendingFadeCallbacks_ |= (1u << i)`
   - Add: if `!self->fadeCallbackQueued_` → set flag, call `System.queueCallback(dispatchFadeCallbacks, reinterpret_cast<uint32_t>(self))`
10. **Implement `dispatchFadeCallbacks`** (static, task context):
    - Clear `fadeCallbackQueued_`
    - Snapshot and zero `pendingFadeCallbacks_` atomically (local copy)
    - For each set bit `i`:
      1. Call `onFadeDone_(i)` if delegate is set
      2. Call `popAndStartNextFade(i)`
      3. If that returns false (queue empty): call `onQueueEmpty_(i)` if delegate is set
11. **Implement `popAndStartNextFade(channel_idx)`**:
    - Get `ChannelFadeQueue& q = fadeQueues_[channel_idx]`
    - **FIFO mode**: if `q.count == 0` → return false; read `q.entries[q.head]`, advance head (`(q.head+1) % FADE_QUEUE_DEPTH`), decrement `q.count`
    - **CYCLIC mode**: if `q.cycleLen == 0` → return false; read `q.entries[q.head]`; compute `nextHead = (q.head+1) % q.cycleLen`; if `nextHead == 0` fire `onCyclicWrap_(channel_idx)` if delegate is set; advance `q.head = nextHead` — count is never decremented, queue never exhausts
    - Call `fadeToValueChan(channel_idx, entry.targetDuty, entry.fadeTimeMs)` and return its result
12. **Implement `queueFadeChan(channel, targetDuty, fadeTimeMs)`**:
    - Guard: `initialized_`, `fadeInstalled_`, channel in range
    - Get `ChannelFadeQueue& q = fadeQueues_[channel]`; read `q.mode` (set externally via `setFadeQueueMode`)
    - Capacity check: for FIFO use `q.count`, for CYCLIC use `q.cycleLen`; if `>= FADE_QUEUE_DEPTH` → `debug_e(...)` and return false
    - Write entry to `q.entries[q.tail]`, advance tail (`(q.tail+1) % FADE_QUEUE_DEPTH`); increment `q.count` (FIFO) or `q.cycleLen` (CYCLIC)
    - If `!isFadingChan(channel)` AND this was the first entry → call `popAndStartNextFade(channel)` immediately to kick off playback; return true
13. Implement trivial helpers:
    - `getFadeQueueCount(channel)` → `fadeQueues_[channel].count` (FIFO) or `fadeQueues_[channel].cycleLen` (CYCLIC)
    - `getFadeQueueMode(channel)` → `fadeQueues_[channel].mode`
    - `setFadeQueueMode(channel, mode)` → `fadeQueues_[channel].mode = mode` (with in-range guard)
    - `setOnFadeDoneCallback(cb)` → `onFadeDone_ = cb`
    - `setOnQueueEmptyCallback(cb)` → `onQueueEmpty_ = cb`
    - `setOnCyclicWrapCallback(cb)` → `onCyclicWrap_ = cb`
    - `resetFadeQueue(channel)` → zero all fields of `fadeQueues_[channel]`, reset `mode = FIFO`

### Phase 3 — Sample app (`samples/Advanced_HwPWM/app/application.cpp`)

14. In `init()`, register both callbacks with `Serial.printf` log messages showing channel and which event fired
15. Pre-load 3 fades per channel using `queueFadeChan` to demonstrate queue chaining

---

## Verification

1. `make` — zero compile errors
2. Flash to device — serial output shows the correct callback sequence
3. **FIFO**: pre-load 3 fades on channel 0; confirm: `onFadeDone(0)` × 3, then `onQueueEmpty(0)` × 1
4. Confirm `onFadeDone` fires for mid-queue fades (not just the last)
5. Call `queueFadeChan` FADE_QUEUE_DEPTH+1 times without draining — confirm returns false on the extra call
6. **CYCLIC**: call `setFadeQueueMode(1, CYCLIC)`, pre-load 3 entries on channel 1; confirm `onFadeDone(1)` keeps firing indefinitely, `onCyclicWrap(1)` fires once every 3 fades, and `onQueueEmpty(1)` never fires
7. Call `resetFadeQueue(1)` mid-cycle; confirm the channel stops after its current fade completes and no further callbacks fire
8. Call `fadeToValueChan` directly on a channel — confirm it still works with no queue involvement

---

## Further Considerations

1. **Bitmask width**: `static_assert(SOC_LEDC_CHANNEL_NUM <= 32, "pendingFadeCallbacks_ bitmask too narrow")` added in .cpp
2. **Snapshot atomicity**: On ESP32 (single-core Xtensa), a local copy of `volatile uint32_t` before clearing is sufficient; no `portENTER_CRITICAL` needed in task context
