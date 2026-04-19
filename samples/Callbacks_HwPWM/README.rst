FadeQueue_HwPWM
===============

Demonstrates the per-channel fade queue API of ``Esp32HardwarePwm``.

Two queue modes are shown side by side:

Channel 0 — **FIFO mode** (default)
    Fades are pre-loaded before the channel starts.  The library
    chains them automatically without application involvement.  When the
    last entry completes the ``onQueueEmpty`` callback fires.

Channel 1 — **CYCLIC mode**
    Three entries are seeded.  When the last entry finishes playback wraps
    back to entry 0 and continues indefinitely.  ``onCyclicWrap`` fires on
    each loop.  ``resetQueue`` is called after three full cycles to stop
    the channel.

Common callback
    ``onFadeDone`` fires after every individual fade on any channel and
    prints the channel index and current duty value to Serial.

Hardware
--------

Tested on ESP32 classic.  Adjust ``pinList`` for your board; avoid GPIO
6–11 (SPI flash) and GPIO 34–39 (input-only).
