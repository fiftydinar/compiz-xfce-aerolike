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

### Frame pacing is per output

The server keeps a framebuffer per monitor and shows what the compositor
produced at each monitor's vblank, so a single screen refresh rate is
meaningless. `detectRefreshRate()` therefore builds a table of enabled outputs
with the rate their mode runs at, and `scheduleRepaint()` paces to the fastest
output that the **pending damage** touches:

- damage confined to a 60Hz monitor is produced at 60Hz, not at 144Hz because
  some other monitor is capable of it;
- damage spanning several outputs paces at the fastest of them, so a second
  monitor is never starved;
- no damage at all schedules no frame, as before.

`redrawTime` stays the observed frame interval and `optimalRedrawTime` stays the
fastest output overall, because plugins read the latter as "the" frame interval
for animation timing; neither changes with what happens to be damaged.

The detected table is logged once per screen change at info level:

```
compiz (composite): Outputs: 2560x1440@144Hz 1920x1080@60Hz (pacing cap 144Hz)
```

so a mixed-refresh setup can be checked from the session log.

### HDR cannot work while compiz composites

This is a property of the arrangement, not a missing feature:

- compiz composites every window into the overlay window it gets from
  `XCompositeGetOverlayWindow`, which inherits the root window's depth. The
  whole desktop is 8 bits per channel before it reaches the screen.
- Tone mapping per window lives in the server's own paint path
  (`compServerPaintLayer()` calls `PaintHdr`), which does not run for a
  compositor other than itself. The fork documents the consequence directly:
  "With compositing turned off, HDR windows that a separate compositor such as
  picom draws are shown the way that compositor draws them."

So an HDR window is composited as if it were SDR and appears washed out. HDR
needs the server to composite, which means giving up the composite plugin - and
`decor`, `move`, `resize`, `place`, `snap`, `compiztoolbox` and `extrawm` all
hard-require it in their `<deps>`, so compiz cannot act as a window manager
without it. HDR and this desktop's effects are mutually exclusive.

What the server still gives with compiz compositing: per-monitor framebuffers,
per-output rates (above), tear-free presentation at any rate, faster X11
rendering and high-priority server threads.

### The two "vsync"s are different layers

`tear-free at any rate` above and compiz's `sync_to_vblank` are not the same
mechanism, which is worth keeping straight:

- **Server side, always on.** The modesetting driver presents per CRTC: it
  computes the target vblank (`ms_tearfree_next_vblank()`) and times the flip or
  the region copy to land just before it. This is what makes the output
  tear-free, and it happens whatever compiz does.
- **Compiz side, `sync_to_vblank`, currently `false`.** This only decides when
  compiz hands a frame over and whether it blocks waiting for the display:
  `DoubleBuffer::VSYNC` makes `glXSwapBuffers`/`vsync(Blit)` wait for vblank.
  It is a producer throttle, not the thing preventing tearing.

Keeping compiz's off is deliberate, and it also interacts with pacing: with it
on, `hasVSync()` returns true and `scheduleRepaint()` takes the
`delay = 1` branch, which repaints at the first opportunity and defers pacing to
the blocking swap. The per-output pacing described above only takes effect in
the other branch, so the two pacing strategies are alternatives - turning
`sync_to_vblank` back on would bypass the per-output table. Blocking in swap
would also buy nothing, because the frame is already presented at vblank one
layer down; it would only add latency.

### Variable refresh does not work here

VRR is gated on a direct scanout:

- `ms_compositor_update_output_mode()` enables it with
  `drmmode_crtc_set_vrr(crtc, window && ms_compositor_window_has_variable_refresh(...))`,
  where `window` comes from `CompositeServerCoveredScanoutWindow()`;
- that returns NULL when the compositor is not the server, so VRR is set to
  disabled on every update;
- the Present-flip path has the same shape: it checks `_VARIABLE_REFRESH` on
  `ms->flip_window`, which is compiz's output window, not the application's.

It also needs `Option "VariableRefresh" "true"` in the Device section - off by
default - and a window carrying `_VARIABLE_REFRESH`. Adding the option without
compositing changes nothing, and enabling VRR while compiz paces at a fixed rate
would pin the panel to that rate rather than adapting to the application.

So VRR joins overlay planes, direct scanout and HDR: available only when the
server composites. What cannot be recovered from the compiz side are those four.

The remaining gap that is *not* in that group has been closed above:
`_NET_WM_FRAME_DRAWN`/`_NET_WM_FRAME_TIMINGS` are plain client messages, so
compiz sends them itself and toolkit frame clocks run at the real refresh rate.

### GTK/Qt frame timing

A compositor drives a toolkit's frame clock with `_NET_WM_FRAME_DRAWN` and
`_NET_WM_FRAME_TIMINGS`; without them GTK falls back to its own ~60Hz timer and
animations inside applications do not use the refresh rate of a fast monitor,
however fast frames are produced.

Compiz sends both now. It advertises them in `_NET_SUPPORTED` and answers each
window after a frame containing it has been drawn, reusing the
`_NET_WM_SYNC_REQUEST_COUNTER` it already monitors for resize synchronization:

- the counter value the client set is echoed back in `data[0..1]`, so the client
  matches the message to its own frame;
- `data[2..3]` of `_NET_WM_FRAME_DRAWN` is the time the frame was drawn, in
  monotonic microseconds - the same clock and unit as the server's own messages
  and as `g_get_monotonic_time()`, so no conversion is needed;
- `_NET_WM_FRAME_TIMINGS` reports `data[3]` as the interval the frame was paced
  to (from the per-output table above) and `data[2]`, the presentation offset,
  as 0: the vblank phase is not knowable from a client, so it is not invented.

This works whatever composites, unlike overlay planes, direct scanout and HDR,
which need the server to composite and are therefore unavailable here (see
below). It round-trips nothing: two ClientMessages per frame per window that
asked for one, sent from the paint call, with the alarm the resize path already
arms on map.

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
