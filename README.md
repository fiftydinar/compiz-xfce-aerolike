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

These assume the phoenix X server (`xorg-server-phoenix`, dec05eba's Xorg fork)
that xfce-aerolike ships. In particular the vsync, swap-buffer and pacing
choices rely on that server presenting frames without tearing at any rate.

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
