# Juicolicious Grand



A sampled grand piano for Windows and macOS, available as a VST3 plugin, an AU plugin, and a standalone app.



## Features



- Full 88 key range, built from real grand piano recordings pitch shifted to every key

- Four velocity layers per note, blended together so tone and volume change smoothly with how hard you play

- Damper release sounds when a key is lifted. The top keys ring out freely, the same way the undamped strings of a real grand do

- MIDI sustain pedal support

- Attack and Release controls

- Three ways to play: a MIDI keyboard, your computer keyboard, or the mouse

- Key labels that show note names and the computer keyboard layout





## Download



Installers for Windows and macOS are on the Releases page:

https://github.com/rmoraldo/juicolicious-grand/releases/latest



Windows gets the VST3 plugin and the standalone app. macOS gets the VST3 plugin, the AU plugin (used by Logic Pro and GarageBand), and the standalone app. The installers place the plugins in the standard folders, so your DAW finds them on its next plugin scan.



The installers are not code signed yet, so both systems warn before running them.



- Windows: on the "Windows protected your PC" screen, click More info, then Run anyway.

- macOS: if the installer is blocked, open System Settings, go to Privacy \& Security, scroll down, and click Open Anyway.



## Playing with a computer keyboard



Click the plugin window first so it receives your key presses. Every computer keyboard note plays at a medium velocity.



- A S D F G H J K L play the white keys C4 D4 E4 F4 G4 A4 B4 C5 D5

- W E T Y U O P play the black keys C#4 D#4 F#4 G#4 A#4 C#5 D#5

- Q plays B3, one note below middle C



## How it works



- Sampling: the recordings are taken every two to three keys. Each key plays the nearest recording, resampled to the right pitch, which keeps the download small while keeping the tone close to the original.

- Velocity: each recorded note has up to four velocity layers. A note played between two layers mixes both recordings, weighted by how close the velocity is to each one.

- Release: a lifted key keeps playing from where it is and decays to silence over the Release time, while a quiet damper sound plays alongside it.

- Polyphony: up to 32 voices. When all are busy, the quietest note that is already fading gets reused first.



## Building from source



Requirements:



- CMake 3.22 or newer

- Visual Studio 2022 or newer on Windows, or Xcode on macOS

- JUCE 9.0.0, commit 0b6e500164d05753af8e2749238ec5d9a3e29937



```

git clone https://github.com/juce-framework/JUCE.git

git -C JUCE checkout 0b6e500164d05753af8e2749238ec5d9a3e29937



cmake -B build-release -DJUCE\_PATH=/path/to/JUCE -DCMAKE\_BUILD\_TYPE=Release

cmake --build build-release --config Release

```



On Windows, run these from the x64 Native Tools Command Prompt that comes with Visual Studio.



The plugin loads its samples from a shared data folder. Copy the Samples folder from this repository to:



- Windows: C:\\ProgramData\\JuicoliciousGrand\\Samples

- macOS: /Library/Application Support/JuicoliciousGrand/Samples



To build the Windows installer, install Inno Setup 6 (https://jrsoftware.org/isdl.php) and run this from the project root:



```

ISCC.exe installer\\windows\\JuicoliciousGrand.iss

```



\## Project layout



- Source: plugin code, including the sample playback engine, the editor, and the keyboard display

- Assets: logo and title font, built into the plugin

- Samples: piano recordings, installed alongside the plugin

- installer: installer scripts



## Credits



Piano samples from the Versilian Community Sample Library by Versilian Studios (https://github.com/sgossner/VCSL), released under CC0.



Built with JUCE (https://juce.com).

