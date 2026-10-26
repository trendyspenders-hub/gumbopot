# Native verification

## Current animated-interface Mac build

Source commit 9986bedbe5a857b67273ada9bae0d4aea615bf68 compiled universal arm64/x86_64 VST3 and AU with JUCE 8.0.4. Build and packaging succeeded:
https://github.com/trendyspenders-hub/gumbopot/actions/runs/40421957045

All 26 native checks passed. These exercise playback, tempo ratio, pan, stereo preservation, wrapping, unchanged 20 parameter IDs/order, worker demo loading, transport, audition, locks, exports, session recall, missing files and live sample replacement. Editor checks rendered the actual native PNG, confirmed animation advances during playback and confirmed motion can pause without disabling audition. Sixteen native animation frames are included in the workflow artifact.

Apple auval reported AU VALIDATION SUCCEEDED, including rendering, MIDI and parameter checks. Bundle signatures and both CPU slices were verified. Execution was on Apple Silicon; Intel slices were verified but not separately executed on Intel hardware.

## Interface and test changes

The interface now uses custom code-drawn enamel, cast-iron pots, waveform soup, steam, bubbles, burner indicators, rotary controls and a pot seal. The photographic backdrop is no longer linked or loaded. The header motion switch pauses decoration independently of audio. Decorative motion is not sample-accurate transport information and runs only on the editor/message thread.

The imported-gain test now polls its actual asynchronous postcondition with a bounded two-second deadline. The assertion and tolerance are unchanged; loader completion alone does not imply that the separate 20 Hz message-thread gain service has run.

## Distribution and remaining validation

DMG and ZIP contain universal VST3/AU bundles, a per-user installer, the actual interface PNG and validation logs. Bundles are ad-hoc signed, without Developer ID signing or notarization. Disposable CI may require system-scope AU discovery; the user installer remains per-user.

No physical listening test or broad DAW certification is claimed. Test your DAWs, automation, repeated editor open/close, missing samples, live sample changes, session recall and export re-import.

Callback allocation instrumentation counts calling-thread C++ new/delete; it is not a general malloc detector or formal concurrency proof. Loading, decoding and reclamation run on a worker using immutable banks and hazard pointers.

The optional browser Splice companion runs separately. Authenticated Splice operations still require account testing.
