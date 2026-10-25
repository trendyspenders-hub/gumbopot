# Native interface artwork

The native editor uses original code-drawn enamel grain, cast-iron pots, handles, soup waveforms, steam, burner indicators, rotary controls, jack sockets and a pot seal. No photographic backdrop or generated raster image is loaded or linked into the native plugins. The former backdrop file is retained only as historical source material and is not used in the build.

Motion runs at 30 Hz on the editor/message thread. Playback state and output energy drive the pots; stirring adds a brief swirl. Motion can be paused independently of audio using the header switch. These decorations are not sample-accurate transport indicators. They require no audio callback work.
