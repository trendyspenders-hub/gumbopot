# Gumbo Pot — browser kitchen with Splice companion

## Run on your Mac

1. Install Node.js 20+ (includes npm): https://nodejs.org/
2. Extract the whole ZIP.
3. In Terminal, type `cd `, drag the extracted GumboPot folder into Terminal, then press Return.
4. Run `bash Run-Gumbo-Pot.command`.
5. The kitchen opens at **http://127.0.0.1:5173**. Keep Terminal open. Ctrl+C stops the server and clears login tokens.

If you already use Node, run these commands from the folder containing package.json:

```sh
npm ci --ignore-scripts
npm test
npm run build
npm run dev
```

This runs on macOS, Windows and Linux with Node.js 20+. The .command launcher is for Mac only. `npm run preview` serves the production build with the same local Splice companion; a static file host alone cannot run the sign-in backend.

## Sign in to Splice — no Claude required

1. Click **Connect to Splice**.
2. Sign in and authorize Gumbo Pot on Splice's own browser page. Your password never goes through Gumbo Pot.
3. Return to the original kitchen tab. It polls for completion every five seconds.
4. Open the **Splice** tab. The tool list comes from Splice's real service after authentication.
5. Select a tool, inspect its description/schema and enter its JSON arguments. The schema is supplied by Splice; do not assume examples from an unrelated client apply.
6. Click **Review action**. Verify the exact tool and arguments, check the approval box, then click **Run approved action**.
7. Service responses appear as text. Returned HTTPS links can be opened explicitly. Import downloaded audio using **Import audio files**.

This release is an authenticated tool bridge, not yet a finished graphical Splice catalog browser. It does not automatically map Splice search results into bowls, fetch arbitrary returned URLs, or silently spend credits. Download/save/share actions may have costs or affect your library; review them individually. No paid action was exercised in preparation.

Connect uses Splice's documented MCP endpoint, OAuth discovery and dynamically registered public client with PKCE S256. Splice may change its beta service or reject a client/account. If sign-in fails, the app remains disconnected. Runtime tool discovery is deliberately used instead of invented tool names or arguments.

Login tokens, verifier and client registration are held only in local server memory, never browser storage or the ZIP. Disconnect or stop the server to clear them locally. That does not revoke server-side consent; manage authorized access with Splice as needed. Restarting requires another sign-in. Do not forward the local server to a public network.

## Cook with your own samples now

- Click **Load original demo sounds** for ten synthesized loops; no provider assets are bundled.
- Or import local files/folders. Files stay local; WAV is the most reliable browser format. Unsupported decode errors are reported.
- Each bowl has gain, pan, low-pass filter, Solo, Lock, editable source BPM and role. Double-click a gain/pan/filter slider to reset it.
- Stir replaces only unlocked samples. If there is no alternative, a bowl may retain its sample or stay empty.
- All playback sources start together. Each loops its own source length. This is resampling BPM sync, not pitch-preserving stretching, key matching, or bar-aligned regeneration.
- Tempo uses onset autocorrelation or a number in the filename. It can be wrong; manually correct each sample BPM. Role tags use filenames/simple signal rules and are editable.
- Compatibility is a transparent heuristic using tempo proximity, distinct roles and RMS balance. It is not a perceptual quality measure. Key/groove matching is explicitly unscored.
- Save recipes stores settings and file identifiers in this browser. Re-import original local files after reopening; demo files can be regenerated. Up to 30 recipes are retained.
- Export a 4/8/16-bar full mix as stereo 44.1 kHz, 16-bit PCM WAV, or five WAV stems in one ZIP. Mix respects Solo; stem export ignores Solo and includes each bowl separately, including a silent file for an empty bowl.
- Playback and exports share a fixed 0.3 headroom factor. Hot input can still clip; reduce gain/master if needed. There is no limiter.
- Imports process at most 100 selected files per batch, 50 MB per file, with a five-minute decoded duration limit. Use short loops and moderate batch sizes to avoid excessive memory use.

## What remains separate

The native JUCE source is under Source/ with CMakeLists.txt. See **START_HERE_NATIVE.md** and **AUDIO_THREAD_AUDIT.md**. The browser design and Splice companion have not been ported into the native editor. Existing C++ parameter IDs/ranges/order remain unchanged. No compiled Mac app, VST3, AU, DMG, AAX or installer is claimed here.

The reference artwork is preserved in design/kitchen-reference.png. The implemented browser UI follows its dark kitchen layout, cast-iron pot, five ingredient colors and pantry/compatibility/export columns; it is a CSS implementation rather than an exact photoreal reproduction.

## Troubleshooting

- `npm` missing: install Node.js and reopen Terminal.
- Port in use: stop another running kitchen first, or run `GUMBO_PORT=5174 npm run dev` on Mac/Linux and use the printed address. OAuth uses that port automatically.
- Blocked popup: the Splice tab shows a sign-in link when the browser blocks opening the popup.
- Callback fails: return to the original tab and click Connect again. Sign-in expires after ten minutes. Restarting during sign-in invalidates it.
- No service tools: sign-in/tool discovery has not completed. This is not represented as connected until both succeed.
- Subscription/credit error: inspect Splice's service response and your account. No retries of paid actions are automatic.

Official references checked October 24, 2026:
- https://support.splice.com/en/articles/14442749-getting-started-with-the-splice-mcp-server-beta
- https://splice.com/tools/mcp-server
- https://github.com/modelcontextprotocol/typescript-sdk/blob/main/docs/clients/oauth.md
