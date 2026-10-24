# CHATGPT BUILD PACKAGE - from zip to compiled plugin

## STEP 0 - WHAT TO UPLOAD TO CHATGPT (once, stays in the project)
Attach these 8 files from the zip (they never change in session 1):
    CMakeLists.txt
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
    Source/PluginEditor.h
    Source/PluginEditor.cpp
    Source/LayerPlayer.h
    Source/LayerPlayer.cpp
    BUILD_FIRST.md
Paste the SESSION 1 prompt below. Do NOT paste the big spec docs yet.

====================================================================
## SESSION 1 PROMPT (paste this)
```
You are a meticulous JUCE maintenance engineer, not an architect. I have a
JUCE 8.0.4 plugin project ("Gumbo Pot", a 5-layer tempo-synced sample
stacker). The 7 source files and CMakeLists are attached. BUILD_FIRST.md is
attached. Your ONLY job this session: get GumboPot_Standalone compiling and
correct on macOS with minimal changes.

HARD RULES - violate these and the session has failed:
1. NO new features. NO refactors. NO "improvements". NO new dependencies.
2. Change the smallest possible thing per error. Report every change as:
   FILE / LINE / BEFORE -> AFTER / WHY.
3. Parameter IDs are a frozen public API (automation + stack.json depend on
   them): master, gumbo, weirdness, seed, reroll, layer1..5 {gain,pan,filter}.
   Never rename, reorder, or re-range them.
4. JUCE 8.0.4, C++17, macOS. If an API differs in 8.0.4, adapt TO the API,
   not the other way.
5. If a fix requires judgment, present 2 options with a recommendation
   instead of silently choosing.

PROTOCOL - work like a compiler oracle:
- I paste a build error -> you reply with the exact edit (file, lines,
  replacement code) and nothing else.
- When configure succeeds -> tell me the exact build command.
- When the build succeeds -> give me the 5-minute smoke test steps and stop.
- Expected issues (from BUILD_FIRST.md): FontOptions on older JUCE, AU
  validation noise, rare filesystem linking. Know them before I paste them.

CONTEXT you do not need to verify, just respect: this code was written by an
AI and has never seen a compiler. Expect small API mismatches. The Python
toolchain (indexer, stack brain) is already built and tested - the plugin
loads stack.json files it produces. Say READY and wait for my first error.
```
====================================================================
## THE ERROR LOOP (every compile error, same mini-prompt)
```
Error (file, line, message):
[paste]
```
Expected reply format: file / lines / before -> after / why. Apply, rebuild,
paste next error. Do not batch errors - one at a time keeps the diff clean.

====================================================================
## AFTER STANDALONE BUILDS (same session, or session 2)
```
Build now succeeds. Next, in order, with the same minimal-change rules:
1. Walk me through the smoke test: indexer -> stack_brain -> load stack ->
   verify tempo sync + RE-ROLL. Give me exactly what I should hear/observe.
2. Build GumboPot_VST3 and give me the install + DAW rescan steps.
3. Review the 7 files for ONE class of bug only: audio-thread safety
   (allocations, locks, message-thread calls from processBlock). List
   findings as FILE/LINE/SEVERITY/FIX - do not apply yet.
```

====================================================================
## SESSION 2+ - FEATURES (new chat per feature, attach current sources)
Order (do not skip; each session ends with "builds clean"):
    S2  V3 s1: WarpEngine interface + NearestWarp fallback (no Rubber Band
        yet - factory returns NearestWarp) + crossfade swapTo on LayerPlayer
    S3  V3 s1.3: SmoothedValue mixer gains; setLatencySamples honesty
    S4  V3 s4:  MIDI learn + undo snapshots (SECONDS ring buffer)
    S5  V2 s4:  tuneup params + pitch ratio in LayerPlayer (mode 1 only)
    S6  V4 s1:  quiz UI (backend contract in tools/quiz.py - read it)
    S7  V4 s4:  MIDI out (progression + groove) - hardest, give it its own day
    S8  V5 s4:  variation engine (bar-boundary scheduler, seeded)
    S9  V5 s5:  make-room ducking matrix
    S10 V6 s4:  EXTRA SCOOP round-robin bowls
    S11 V6:     provenance tags + reduce-motion
    S12 DESIGN_PROMPT.md output: LookAndFeel + full themed UI (last - after
        the engine is trustworthy)
Each session opener:
```
Attached: current Source/* + CMakeLists (post your last session) + this
spec section: [paste the one section]. Implement ONLY this section with the
Session 1 hard rules. Confirm the parameter freeze list before starting.
```

====================================================================
## GUARDRAILS (memorize these)
- A green build is not "done" - the smoke test hearing a cook is "done".
- If ChatGPT wants to redesign anything, paste: "minimal changes only,
  per the hard rules. Diff what you proposed vs that."
- If it claims something is impossible, ask for the JUCE 8.0.4 header
  citation. It is almost never impossible.
- Keep every working version: commit or zip the Source/ folder after each
  green session. Green sessions are your save points.
