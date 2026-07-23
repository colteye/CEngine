# CEngine Audio System

> **Status: implemented engine-facing contract and support matrix.**

## Product goal

`AudioSystem` is the same kind of engine-owned facade as rendering, physics,
input, and windowing. Game and entity code uses CEngine values and
generation-checked `VoiceHandle` values. SDL and miniaudio types stay inside
their compiled backends.

The implementation deliberately divides responsibility:

- SDL3 owns cross-platform audio-device creation and the playback stream;
- miniaudio runs device-less and owns decoding, sample-rate conversion, mixing,
  the node graph, filters, reverb, and 3D spatialization;
- CEngine owns voice lifetime, priority policy, buses, validation, diagnostics,
  and the API used by game code.

This avoids SDL_mixer and avoids compiling miniaudio's second platform-device
layer.

## Initial feature set

The baseline required for ordinary game audio is implemented:

- one-shot and looping playback;
- decoded WAV, FLAC, MP3, and Ogg/Vorbis assets;
- 2D playback and listener-relative 3D playback;
- gain, pitch, start offset, fade-in, fade-out, pause, resume, seek, and cursor;
- master, music, sound-effects, and dialog buses with gain, mute, and fades;
- one listener with position, orientation, and velocity;
- source position, direction, and velocity;
- inverse, linear, exponential, or disabled distance attenuation;
- minimum/maximum distance, rolloff, Doppler factor, and directional cones;
- a fixed voice budget with deterministic priority/age stealing;
- generation-checked handles and stale-handle rejection;
- optional no-device degradation or required-device startup;
- device, voice, rejection, steal, completion, and backend-failure diagnostics.

## Richer game-audio set

The implementation also includes the common higher-level features needed by
larger games:

- shared decoded-PCM caching for frequently reused effects;
- per-voice streamed decoding from retained compressed bytes for music and
  long ambience without expanding the complete clip to PCM;
- an environmental reverb path for the sound-effects bus with room size,
  damping, stereo width, wet level, and dry level;
- per-voice obstruction and occlusion controls mapped to gain and low-pass
  filtering;
- spatial source cones and Doppler using explicit listener/source velocities;
- sample-rate conversion into one fixed engine output format;
- thread-safe graph mutation against SDL3's pull callback;
- no allocations performed by CEngine's mixing loop after initialization.

Music and dialog route directly to the master bus. Sound effects route through
the environment processor, so changing a room does not add reverb to music or
dialog. A voice can intentionally bypass environmental processing by selecting
the master bus.

## Ownership and update rules

Audio assets are immutable `shared_ptr<const Assets::Audio>` values from
`Store`. A live voice retains its source bytes. Decoded effects additionally
retain shared cached PCM; streamed voices retain a private decoder cursor.

The game thread creates and updates voices. The SDL3 audio callback only asks
the miniaudio engine for more stereo floating-point frames and submits them to
the SDL stream. `AudioSystem::Update` retires completed voices and must run once
per client frame. The listener should be updated after the active camera or
player transform is final for that frame.

## Build and portability

`CENGINE_ENABLE_AUDIO` removes the complete engine audio implementation when
off. The vendored miniaudio build defines `MA_NO_DEVICE_IO`,
`MA_NO_ENCODING`, `MA_NO_GENERATION`, and `MA_NO_RESOURCE_MANAGER`.

SDL3 is pinned for reproducible desktop builds or can be supplied by a platform
SDK with `CENGINE_USE_SYSTEM_SDL3=ON`. Window, input, renderer-surface, and
audio-device access are all behind CEngine backend boundaries. This lets a
mobile port use SDL's Android/iOS implementations and lets a console port
provide its licensed SDL package or replace only the platform backends without
changing game code.

The currently verified target is macOS. Android, iOS, and console packages,
graphics backends, lifecycle interruption handling, touch/gamepad bindings,
and platform certification still require their own target builds and tests;
the engine API does not expose desktop or SDL types that would force a game-code
rewrite.

## Deliberate exclusions

The current system does not include audio capture, voice chat, MIDI,
procedural synthesis, editor-only waveform tools, arbitrary user DSP plugins,
or a runtime backend registry. Those are separate products, not standard
playback requirements.

There is currently one listener and one global sound-effects environment.
Per-room transition logic belongs to gameplay and can set `Environment`;
multiple simultaneous acoustic zones should only be added with a concrete game
that requires them.
