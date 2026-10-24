# Native verification

## Completed locally

- JUCE 8.0.4 native source compiles and links as a Linux VST3 bundle. JUCE's VST3 manifest helper loads the module and creates moduleinfo.json.
- The headless native audio suite has exercised sample playback, tempo ratio, pan, stereo preservation, short-loop wrapping, low-rate filter stability, unchanged parameter IDs/order, worker demo loading, transport stop, audition, callback C++ allocations, locks, WAV/stem output, demo/session recall, relative stack paths, shorter-stack clearing, missing-file failure, live bank replacement and host tempo/transport.
- One test's wait for message-thread gain application was lengthened to cover the 20 Hz service timer; this avoids falsely checking before the queued gain has been applied.
- The native editor screenshot is being validated on an actual graphics display. A no-display run cannot prove a GUI works.

## Mac build workflow

The workflow compiles **universal arm64/x86_64 VST3 and AU**, executes the same native checks and renders the native editor, verifies the bundle signatures/CPU slices, installs for the runner user, runs Apple's `auval`, then packages DMG and ZIP. Consult the workflow result and its logs for the current Mac build status. No Mac result is claimed solely from local Linux results.

The callback ownership protocol uses immutable sample banks and sequentially consistent publication/hazard pointers. Loading/decoding/reclamation is on a worker; processBlock does not lock, decode files, post message-thread callbacks, or construct parameter-name strings. Callback allocation instrumentation counts C++ new/delete on the calling thread, including during sample replacement and SysEx input; it is not a general malloc detector or a formal concurrency proof.

## Release checks still required

Test the finished Mac bundles in your DAWs: Logic/AU, another VST3 host, recall with missing files, repeated editor open/close, live sample swaps, sample-rate/block-size changes, loops at multiple tempos, MIDI/transport start-stop, automation, export re-import, and both CPU architectures. No physical listening test or broad DAW certification is claimed.

The optional Companion/ browser component has separate OAuth/approval/audio tests. It does not make the native instrument itself a Splice catalog client.
