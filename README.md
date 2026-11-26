# compiz-xfce-aerolike

compiz 0.9 for [xfce-aerolike](https://github.com/fiftydinar/xfce-aerolike),
built from this repository instead of launchpad.

## Branches

| branch | contents |
|---|---|
| `upstream` | pristine upstream, exactly the commit the Arch AUR package pins (`ff7ff00`, `0.9.14.2.r25`). Nothing added, nothing changed. |
| `main` | `upstream` plus every change below, each as its own commit, plus the `PKGBUILD`. |

`main` is the default branch because that is what gets built.

## Why this repo exists

xfce-aerolike used to build compiz from the AUR package and then apply a stack of
patch files, fetched over HTTPS at build time and applied with `patch -F100`.
That had three problems:

1. Patches were applied with fuzz 100, so a stale patch could land its changes in
   the wrong place without failing. Auditing the AUR patch set against
   `ff7ff00` showed that four of its five patches are obsolete there and one is
   truncated; replaying one of them with a permissive `patch` touched 16
   unrelated files.
2. The patch fetch was pinned to `refs/heads/main`, so a branch or pull request
   build tested `main`'s patches rather than its own and reported success while
   proving nothing.
3. Our own `0001-compiz-defaults` patch was malformed (context lines missing
   their leading space, hunk counts off by one), so it only applied by GNU
   patch's goodwill.

Here the changes are commits. A build is `git checkout` plus `makepkg`, there is
no patching step at all, and what CI tests is exactly what shipped.

## Changes on `main`

| commit | what and why |
|---|---|
| Do not recompile gsettings schemas during make install | From the AUR package. The package installs the generated schemas itself; compiling them in-tree during install fails without a session bus. |
| Set Aero-like defaults | Default plugin list, virtual desktop 1x1, Glide 2 instead of Magic Lamp, composite/unredirect and opengl/vsync values, grid colors, place defaults. |
| Detect fullscreen windows per output | Upstream compared against the bounding box of all monitors, so it missed a window filling one monitor and matched one spanning all. |
| Invert ring and staticswitcher keybindings | |
| Fix resetting the profile in ccsm | |
| Fix resetting settings in ccsm | |
| Ignore self-minimize requests from unfocused windows | |
| Invalidate the config cache by content hash, not mtime | A changed option default kept serving the stale metadata cache. |
| Bind a key to toggle fullscreen in extrawm | |
| Default opengl/always_swap_buffers to false | Non-full-screen frames take the blit path and copy only the damaged region. Full-screen frames still swap. |
| Pace frames from the enabled outputs | Replaces the composite plugin's `detect_refresh_rate`/`refresh_rate` options. A single screen-wide rate is meaningless with a framebuffer per monitor, and it was queried with the deprecated screen-wide RandR call, which reports an arbitrary output. Pacing follows the fastest enabled output, so a 144Hz panel is not held to 60 by a 60Hz one. |

## Running on the phoenix X server

All of the above assumes `xorg-server-phoenix` (dec05eba's Xorg fork), which is
what xfce-aerolike ships.

Here compiz owns compositing: the server's own compositor is turned off with
`Option "Compositing" "false"`, so compiz claims `_NET_WM_CM_S<screen>` and draws
the desktop the way it always has. That split decides everything below.

### What works

- **Tear-free output at any rate.** The modesetting driver presents per CRTC,
  timing each flip or region copy to land just before that monitor's vblank.
  Nothing to configure; it is what the server does.
- **Per-monitor framebuffers.** Each monitor has its own, instead of one buffer
  for the whole desktop.
- **Per-output frame pacing.** Compiz paces to the fastest output the pending
  damage actually touches, so an update confined to a 60Hz monitor is produced at
  60Hz rather than at a faster monitor's rate. See below.
- **`_NET_WM_FRAME_DRAWN` and `_NET_WM_FRAME_TIMINGS`.** Compiz sends both, so
  toolkit frame clocks in GTK and Qt applications run at the real refresh rate
  instead of a ~60Hz fallback timer. Works regardless of who composites. See
  below.
- **All compiz effects.** Blur, wobbly, animations, expo, ring, cube, scale - the
  whole reason for using compiz. Every one of them needs the composite plugin.
- **Per-monitor DPI**, faster X11 rendering under load, and high-priority server
  threads (`CAP_SYS_NICE`).
- **`_NET_WM_WINDOW_OPACITY` and ARGB transparency**, via compiz rather than the
  server.

### What does not work

All five below need the **server** to composite. Turning the server's compositor
back on is not an option, because `decor`, `move`, `resize`, `place`, `snap`,
`compiztoolbox` and `extrawm` all hard-require the composite plugin in their
`<deps>` - compiz cannot act as a window manager without it. So these are
mutually exclusive with the effects above, not merely unimplemented:

- **HDR.** Compiz composites every window into the overlay window from
  `XCompositeGetOverlayWindow`, which inherits the root window's depth: the
  desktop is 8 bits per channel before it reaches the screen. Per-window tone
  mapping only runs inside the server's own paint path. See below.
- **Overlay planes**, for the same reason: they are used when the server scans
  out its own composition.
- **Direct scanout of application windows.** `compServerCanDefer()` requires the
  server to be the only redirector, and compiz redirects toplevels manually.
- **Variable refresh.** Enabling it is gated on that same scanout. See below.
- **Server-side synchronized window resizing.** Compiz trades it for its own
  effects, deliberately.

Two server features are inert rather than broken, because compiz gets in the way
of neither but triggers neither: `HiddenWindowFPS` throttling (a window is only
treated as hidden while the server composited it) and `AllowTearing` (nothing is
ever scanned out directly).

### The two "vsync"s are different layers

`tear-free` above and compiz's `sync_to_vblank` are not the same mechanism:

- **Server side, always on.** The modesetting driver computes the target vblank
  for each CRTC (`ms_tearfree_next_vblank()`) and times the flip or region copy
  to it. This is what prevents tearing, whatever compiz does.
- **Compiz side, `sync_to_vblank`, `false` in this build.** It only decides when
  compiz hands a frame over and whether it blocks: `DoubleBuffer::VSYNC` makes
  `glXSwapBuffers`/`vsync(Blit)` wait for vblank. It is a producer throttle.

Keeping it off is deliberate and it interacts with pacing: with it on,
`hasVSync()` returns true and `scheduleRepaint()` takes the `delay = 1` branch,
deferring pacing to the blocking swap and bypassing the per-output table. The two
are alternatives. Blocking in swap would also buy nothing, since presentation is
already vblank-timed one layer down - it would only add latency.

### Frame pacing is per output

The server keeps a framebuffer per monitor and shows what the compositor produced
at each monitor's vblank, so a single screen refresh rate is meaningless.
`detectRefreshRate()` builds a table of enabled outputs with the rate their mode
runs at, and `scheduleRepaint()` paces to the fastest output that the **pending
damage** touches:

- damage confined to a 60Hz monitor is produced at 60Hz, not at 144Hz because
  another monitor is capable of it;
- damage spanning several outputs paces at the fastest of them, so a second
  monitor is never starved;
- no damage at all schedules no frame, as before.

`redrawTime` stays the observed frame interval and `optimalRedrawTime` stays the
fastest output overall, because plugins read the latter as "the" frame interval
for animation timing; neither changes with what happens to be damaged.

The table is logged once per screen change at info level:

```
compiz (composite): Outputs: 2560x1440@144Hz 1920x1080@60Hz (pacing cap 144Hz)
```

### Why HDR cannot work

Beyond the depth of the overlay window, per-window tone mapping lives in the
server's paint path (`compServerPaintLayer()` calls `PaintHdr`), which does not
run for a compositor other than itself. The fork says so directly: "With
compositing turned off, HDR windows that a separate compositor such as picom
draws are shown the way that compositor draws them." An HDR window is composited
as if it were SDR and appears washed out.

### Why variable refresh does not engage

VRR is enabled from the scanout decision, so with compiz compositing it never is:

- `ms_compositor_update_output_mode()` calls
  `drmmode_crtc_set_vrr(crtc, window && ms_compositor_window_has_variable_refresh(...))`,
  where `window` comes from `CompositeServerCoveredScanoutWindow()`;
- that returns NULL when the server is not the compositor, so VRR is set to
  disabled on every update;
- the Present-flip path has the same shape: it checks `_VARIABLE_REFRESH` on
  `ms->flip_window`, which is compiz's output window, not the application's.

It additionally needs `Option "VariableRefresh" "true"` in the Device section,
off by default, and a window carrying `_VARIABLE_REFRESH`. Adding the option
alone changes nothing, and VRR under a fixed-rate pacer would pin the panel to
that rate instead of adapting to the application.

### GTK/Qt frame timing

A compositor drives a toolkit's frame clock with `_NET_WM_FRAME_DRAWN` and
`_NET_WM_FRAME_TIMINGS`; without them GTK falls back to its own ~60Hz timer, so
animations inside applications do not use a fast monitor no matter how fast
frames are produced.

Compiz sends both. They are advertised in `_NET_SUPPORTED` and sent to each
window after a frame containing it has been drawn, reusing the
`_NET_WM_SYNC_REQUEST_COUNTER` already monitored for resize synchronization:

- the counter value the client set is echoed in `data[0..1]`, so the client
  matches the message to its own frame;
- `data[2..3]` of `_NET_WM_FRAME_DRAWN` is the draw time in monotonic
  microseconds - the same clock and unit as the server's messages and as
  `g_get_monotonic_time()`, so no conversion is needed;
- `_NET_WM_FRAME_TIMINGS` reports `data[3]` as the interval the frame was paced
  to, and `data[2]`, the presentation offset, as 0: the vblank phase is not
  knowable from a client, so it is not invented.

Cost is two ClientMessages per frame per interested window, sent from the paint
call, with no round trips.

## Building

```sh
makepkg -s
```

The `PKGBUILD` uses `source=()` and builds the checkout in place, so there is
nothing to download and a given commit always builds itself.

## Updating from upstream

```sh
git fetch https://git.launchpad.net/compiz master
git checkout upstream
git merge --ff-only FETCH_HEAD     # or reset to a new pinned commit
git checkout main
git rebase upstream                # resolve conflicts if upstream touched our files
```

Keep `upstream` a plain mirror of what we track; the changes live only on `main`.
AUR's `compiz` package applies no patches that we still need beyond the schema
one already in `main` - re-check that before rebasing, since the AUR patch set
changes over time.
