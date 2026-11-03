# iBuy

An effects plugin shaped like an iPod classic. You work the plugin with its click wheel: turn it to scroll,
press the centre to open or select, MENU to go back.

Formats: **VST3** (Windows, macOS), **AU** (macOS), plus a standalone app.

## Menu

| Menu | What it does |
| --- | --- |
| **EQ Presets** | Flat, Bass Booster, Treble Booster, Lounge, Small Speakers, Spoken Word |
| **Effects > Low Battery** | The sound slows down, drops in pitch and gets muffled while the battery icon drains. Switch it off to power back up. |
| **Effects > Looper** | Pick 1/2, 1/4, 1/8, 1/16, 1/32 or 1/64, then **hold the centre button** to loop the last slice of audio. Release and the sound carries on. Length follows the host tempo. |
| **Effects > Next Room** | The music heard through a wall. |
| **Effects > Earbuds** | Thin, mono, mid-heavy earbud sound. |
| **Effects > Basics** | Reverb, Delay, Chorus, Filter (slow sweep), Bitcrush |
| **Random** | Roll a random EQ preset, random effects, both, or Reset everything. |
| **Now Playing** | Levels, Spectrum and Preview (scope) of the output. |
| **Themes** | Light or Dark screen. |

The ▶II zone of the wheel switches the plugin on and off (clean pass-through when off).
Every switch is a host parameter, so it can be automated.

## Install

Download the zip for your system from the **Actions** tab (latest run, "Artifacts") or from **Releases**.

**Windows**: copy `iBuy.vst3` to `C:\Program Files\Common Files\VST3\`, then rescan plugins in your DAW (FL Studio: Options > Manage plugins > Find installed plugins).

**macOS**: copy `iBuy.vst3` to `~/Library/Audio/Plug-Ins/VST3/` and `iBuy.component` to `~/Library/Audio/Plug-Ins/Components/`.
The files are not notarised, so macOS may block them the first time. Run this once in Terminal, then rescan:

```
xattr -cr ~/Library/Audio/Plug-Ins/VST3/iBuy.vst3 ~/Library/Audio/Plug-Ins/Components/iBuy.component
```

## Build it yourself

Needs CMake 3.22+ and a C++17 compiler (Visual Studio 2022 or Xcode). JUCE is downloaded automatically.

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The plugins end up in `build/iBuy_artefacts/Release/`.

The audio engine (`Source/Engine.h`) has no JUCE dependency. `test/engine_test.cpp` runs it over a WAV file
(`g++ -O2 -std=c++17 -ISource test/engine_test.cpp -o engine_test`, expects `/tmp/in.wav`).

## Licence note

This project uses [JUCE](https://juce.com), which is available under AGPLv3 or a commercial licence.
Sell a closed-source build only with a JUCE licence that allows it.
The iPod picture in `Assets/` is a product photo of an Apple device: replace it with your own artwork before selling the plugin.
