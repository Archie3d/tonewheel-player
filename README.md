# Tonewheel VST
This is a VST plugin for rapid instruments and effects prototyping based on [Tonewheel](https://github.com/Archie3d/tonewheel) engine.

The instrument patches are defined using JavaScript.

## Samples
All the samples must be registered with the engine before they can be used in voices:
```js
sample_id = engine.addSample('path/to/sample.wav');
```
Upon registration the engine generates a unique numeric ID for the sample. This ID can be used then to trigger voices based on this sample.

When only a fragment of a sample to be used:
```js
// start and stop positions are integers defined in samples
sample_id = engine.addSampleWithRange('sample.wav', startPosition, stopPosition);
```

## Voices
When triggering a voice a unique ID gets created. This ID can then be used to release the given voice.
```js
voice_id = engine.trigger({
        sample: sample_id, // ID of the sample to play
        bus: 0,            // Bus number to attach this voice to
        key: 24,           // MIDI key number for this voice
        rootKey: 32,       // Root MIDI key for the sample used
        offset: 0,         // Offset (in number of samples) from the sample start
        gain: 1.0,         // Voice amplitude
        tune: 1.0,         // Playback speed factor that affects the tune
        loop: [
            begin: 12400,  // Sample loop begin position
            end:   34600,  // Sample loop end position
            xfade: 1024,   // Number of cross-fade samples for the loop
        ],
        envelope: [        // Envelope
            attack: 0.01,
            decay: 0.1,
            sustain: 0.7,
            release, 1.2
        ],
        fx: [              // Effects chain to be attached to this voice
            {
                id: "lpf", // ID is used to access the effect from modulation script
                tag: "low_pass_filter",
                frequency: 1200.0 // Parameters depend on the effect being used
            },
            //...
        ],
        modulate: {
            expr: "pitch = 1.0 + cc[1]",  // Modulation expression
        }
    });
```
Some parameters of the voice trigger are optional, like `loop`, `envelope`, `fx`, and `modulation`.

A voice can be released by its ID:
```js
engine.release(voice_id);
```

Alternatively the envelope release time can be overriden:
```js
engine.releaseWithTime(voice_id, 2.0);
```

## Buses

Currently VST exposes 16 setereo buses. Voices can be triggered and attached to a specific bus. A bus has a configurable effects chain (post-voices).

```js
// Setting specific bus gain
engine.bus[0].gain = 0.5;
```

## MIDI events
When a MIDI message arrives the `onMidiMessage()` function of the patch script will be called (if the function exists):

```js
function onMidiMessage(msg) {
    if (msg.noteOn) {
        // The message is note-on
        console.log('NOTE ON key=', msg.noteNumber, 'velocity=', msg.velocity);
    } else if (msg.noteOff) {
        // The message is note-off
        console.log('NOTE OFF key=', msg.noteNumber, 'velocity=', msg.velocity);
    } else if (msg.controller) {
        // The message is a CC
        console.log('CC', msg.controllerNumber, '=', msg.controllerValue);
        engine.setCC(msg.controllerNumber, msg.controllerValue);
    }
}
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
