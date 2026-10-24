# Gumbo Pot — Native VST3 / AU

Five looping sample layers in a cast-iron kitchen. The native JUCE 8.0.4 instrument includes a pantry, per-layer gain/balance/filter, locks and solos, DAW tempo/transport following, MIDI-note audition, deterministic synthetic demo sounds, session/recipe recall, WAV mix and five-stem ZIP export.

**Mac builds:** open the repository's Actions tab, choose **Gumbo Pot Mac VST3 and AU**, and run the workflow. Successful artifacts contain the universal Apple Silicon/Intel VST3 and AU bundles, an installer script, validation logs, an actual interface PNG, and a DMG/ZIP.

Local Mac build: install Apple Command Line Tools and CMake 3.22+, then run `bash Build-Mac-Plugins.command`.

See [INSTALL_NATIVE.md](INSTALL_NATIVE.md) and [NATIVE_VERIFICATION.md](NATIVE_VERIFICATION.md). No standalone app is built by default. The separate optional Splice companion is under Companion/; native login/catalog integration remains separate from its browser connection.

## Design

The interface uses a text-free generated bayou/cast-iron backdrop; all functional controls, labels, waveforms and meters are drawn natively in JUCE. The public automation IDs, order, ranges and defaults from the supplied project are preserved. AU version hints are set to 1.

## Current limits

Tempo matching resamples audio and changes pitch. Source BPM comes from filenames or stack JSON (default 120); automatic key/BPM detection and pitch-preserving warp are not included. Stir uses imported alternatives and mix variation; it does not claim role/harmonic matching. Audio files remain external when saving a recipe. Keep the source files in place.

Development signing is ad-hoc. Developer ID signing/notarization and validation in your actual DAWs remain separate release checks. See the verification report for completed checks rather than assuming a platform passed from the presence of source code.
