# Tonewheel VST
This is a VST plugin for [Tonewheel](https://github.com/Archie3d/tonewheel) engine.

The instrument patches are defined using JavaScript.

## Buses

Currently VST exposes 16 setereo buses. Voices can be triggered and attached to a specific bus. A bus has a configurable effects chain (post-voices).

```js
// Setting specific bus gain
engine.bus[0].gain = 0.5;
```
## Effects

Effects can be added to a bus or to triggered voices.

```js
// Adding effect to a bus 0
var lowPassFx = engine.bus[0].addEffect("low_pass_filter");
lowPassFx.frequency = 1200.0 // 1.2kHz
lowPassFx.q = 1.2 // resonance
```

Here are all the available effects so far:

### `low_pass_filter`
### `high_pass_filter`
### `low_sheld_filter`
### `high_shelf_filter`
### `band_pass_filter`
### `notch_filter`
### `all_pass_filter`

These are the usual buquad filters.

| Parameter | Description          |
|:----------|:---------------------|
|`frequency`| Filter frequency, Hz |
|`q`        | Filter Q-factor      |

### `delay`

A delay line with a feedback.

| Parameter | Description                               |
|:----------|:------------------------------------------|
|`dry`      | Signal dry level                          |
|`wet`      | Signal wet level                          |
|`delay`    | Delay in seconds                          |
|`max_delay`| Maximum delay in seconds (default is 10s) |
|`feedback` | Feedback level (default is 0.5)           |

### `send`

This effect allows sending audio from one bus to another.

| Parameter | Description              |
|:----------|:-------------------------|
|`bus`      | Bus number to send to    |
|`gain`     | Level of the send signal |

> All the sends will be mixed together on the target bus along with the triggered voices before applying the effects chain of that target bus.

### `pitch_shift`

This is a simple granular pitch shifter.

| Parameter | Description              |
|:----------|:-------------------------|
|`pitch`    | Pitch shift factor, 0..4 |

### `frequency_shift`

Frequency shifter using Hilbert transform based on all-pass filters.

| Parameter | Description                |
|:----------|:---------------------------|
|`frequency`| Shift in Hz, -22kHz..22kHz |

### `reverb`

Freeverb-like reverb with additional shimmer effect.

| Parameter | Description                                  |
|:----------|:---------------------------------------------|
|`dry`      | Dry signal level                             |
|`wet`      | Wet reverb level                             |
|`roomSize` | Room size value, 0..1                        |
|`damp`     | Dampening factor, 0..1                       |
|`width`    | Stereo width, 0..1                           |
|`pitch`    | Pitch shift factor for shimmer, default is 1 |
|`feedback` | Pitch shifted feedback level, default is 0   |

### Vocoder

Vocoder consists of two effects: `vocoder_analyzer` and `vocoder_synthesizer`.
The analyzer perform 32-bands levels detection from the input signal. It has not configurable parameters, and it's actually a pass-through effect. Vocoder synthesizer has a single `analyzer_bus` parameter, which specified the bus number the analyzer is sitting on. The synthesizer transfers the spectral bands levels to shape the input signal that goes through the analyzer effect.
