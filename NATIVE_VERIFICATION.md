# Native verification

## Completed Mac build

Native source commit ea238ea84f4ed0aeae2e66e6fb5786b3126ca582 compiled universal arm64/x86_64 VST3 and AU using JUCE 8.0.4. All 23 native checks passed, including actual editor PNG rendering, playback, tempo, pan, stereo preservation, wrapping, parameter compatibility, demo/session recall, imports, locks, export, host transport and callback C++ new/delete instrumentation.

Packaging and Apple AU validation succeeded in https://github.com/trendyspenders-hub/gumbopot/actions/runs/40142540539. Bundle signatures and both CPU slices were verified. Apple's auval reported AU VALIDATION SUCCEEDED, including rendering, MIDI and parameter checks. The runner executed on Apple Silicon; the Intel slice was verified but was not separately executed on Intel hardware.

The downloadable DMG and ZIP contain both plugin formats, a per-user installer, the actual native interface PNG and validation logs. Bundles are ad-hoc signed, without Developer ID signing or notarization.

The disposable CI runner required system-scope AU registration for discovery. This recovery is restricted to GitHub Actions; the user installer installs per-user.

## Scope and remaining validation

No physical listening test or broad DAW certification is claimed. Test the finished plugins in your DAWs, including Logic/AU and a VST3 host, automation, repeated editor open/close, missing sample recall, live sample changes and export re-import.

The callback ownership protocol uses immutable banks and sequentially consistent publication/hazard pointers. Loading, decoding and reclamation run on a worker. Instrumentation counts calling-thread C++ new/delete; it is not a general malloc detector or formal concurrency proof.

The optional browser Splice companion runs separately. It does not make the native instrument a Splice catalog client; authenticated Splice operations still require account testing.
