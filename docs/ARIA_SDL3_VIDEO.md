# Aria SDL3 video bridge (patch 005)

`AriaRuntime` remains an SDL-independent interface. `AriaSDL3Video` is a small
host-only presentation layer that copies a runtime-provided 240x160 RGBA8888
frame into an SDL3 streaming texture, using nearest-neighbor scaling.

The bridge does **not** parse or execute either ROM, does not use `gbarecomp`,
and does not access the Aria frontend with unspecified licensing.

## Intended integration (not wired into gameplay yet)

After a GPL-compatible driver successfully calls `aria_runtime_open`, use
`aria_runtime_step` followed by `aria_runtime_frame`, then render that view with
`aria_sdl3_video_present`. The SDL renderer and frame buffer are borrowed: the
host owns the renderer and the runtime owns the pixels. Keep the frame alive
through the present call. Destroy the video bridge before the SDL renderer.

Current game rooms remain clearly labeled simulated. No authentic gameplay or
assets are claimed by this change. The user must supply and validate their own
ROMs; no proprietary data is shipped.

## Test

`ctest --test-dir build --output-on-failure` includes `aria_sdl3_video`.
The test uses a software SDL renderer and synthetic pixels, no ROM required.
