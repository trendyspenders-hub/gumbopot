# Gumbo Pot native instrument

The intended installable formats are **VST3** and **Audio Unit (AU)** for macOS 11+ on Apple Silicon and Intel. VST3 is also configured for a Linux verification build. AAX and the obsolete VST2 format are not included.

## If you downloaded a built Mac DMG

Close your DAW, open the DMG, then run **Install-Mac.command**. It copies the plugins to your user Audio/Plug-Ins folders and backs up an existing Gumbo Pot version. No administrator access is needed. Restart the DAW and rescan instruments. The AU is an instrument (`aumu`, subtype `Gpot`, manufacturer `Gmbo`). Logic uses AU; most other supported Mac hosts can use VST3.

This development build uses ad-hoc signing. It is not Developer ID signed or notarized. macOS may require approval before running a downloaded installer/plugin. Do not disable Gatekeeper globally. For wider distribution, supply your own Apple Developer signing identity and notarization workflow.

## If you downloaded the native source ZIP

The source ZIP is not a compiled installer. On your Mac, install Apple Command Line Tools (`xcode-select --install`) and CMake 3.22+ (`brew install cmake` if you use Homebrew).

Open Terminal in the extracted GumboPot folder and run:

```sh
bash Build-Mac-Plugins.command
```

The script fetches the pinned JUCE 8.0.4 tag, compiles universal arm64/x86_64 VST3 and AU bundles, runs the native checks, renders the actual editor PNG, installs the bundles for this user, runs `auval`, and creates a DMG and ZIP in **Output/**. It stops on an error. A first build can take several minutes. No Node.js or browser is needed to run the native instrument.

The included `.github/workflows/native-mac.yml` builds and validates on a GitHub Mac runner. Download the artifact from a successful run. Executed validation results are recorded in NATIVE_VERIFICATION.md.

## First sound

1. Insert **Gumbo Pot** on an instrument track.
2. Click **DEMO**. Five original synthesized loops appear.
3. Start your DAW transport, hold a MIDI note, or click **AUDITION**. Stop Audition when you want to follow only your DAW transport/MIDI.
4. Import WAV/AIFF/FLAC files to replace the ingredients. Drop one file on a bowl to replace that bowl; drop a batch anywhere else to fill unlocked slots. Imports are limited to mono/stereo loops up to 120 seconds and 12 million frames per sample.
5. Source BPM is parsed from numeric filename tokens; otherwise it defaults to 120. Pre-analyzed stack JSON files can supply exact per-layer BPM. This native release has no automatic BPM/key detection. Tempo synchronization is resampling, so pitch changes with tempo.
6. Lock protects a bowl from stirring/replacement; Solo auditions selected bowls. Stir jitters unlocked mix settings and, if alternative imported files exist, replaces unlocked samples from the pantry. It does not claim harmonic/role-aware selection.
7. Save a `.gumbo` recipe or save your DAW session. Source audio remains external; keep files in place. The demo regenerates deterministically on recall.
8. Export a 4/8/16-bar stereo WAV or five WAV stems in a ZIP. Exports use the displayed host tempo (120 BPM if none is available), 44.1 kHz/16-bit PCM and the same fixed 0.3 mixer headroom. Full mix respects Solo; individual stems ignore Solo. Empty bowls produce silent stems.

## Splice

The native instrument does not embed the Splice login/client yet. **OPEN SPLICE COMPANION** opens the separately included local companion at port 5173. Start it from **Companion/** first (Node.js 20+, `npm ci --ignore-scripts`, then `npm run dev`). Authenticate there, download licensed audio, and import it into the native instrument. No provider is represented as connected inside the native UI.

## Checks before release

Read **NATIVE_VERIFICATION.md** for actual executed results. A Linux VST3 build or software-rendered editor screenshot does not prove Mac/AU or DAW compatibility. The source Mac workflow must complete before calling the Mac DMG built/validated. Test session recall, live sample changes, transport seeks/tempo changes, mono/stereo, automation, MIDI, both CPU architectures and exported audio in your actual DAWs.

## Interface motion\n\nThe header **MOTION ON/OFF** switch pauses the decorative steam, bubbles and stirring motion independently of audio. Pots animate during playback; the meter reports actual output. Gain, pan and filter controls use their standard names. The interface artwork is drawn in code without a photographic backdrop.\n