# Post-mortem: the red square taskbar icon

**Date:** 2026-09-02
**Component:** Anar Browser (Brave/Chromium fork), Windows taskbar icon
**Outcome:** Fixed. Cost: roughly a dozen rebuild cycles, most of them
spent on wrong hypotheses.

---

## Symptom

After rebranding the fork, the Windows taskbar button for a running Anar
Browser window showed a **solid red square** instead of the app logo.
Everything else about the rebrand looked correct: window title, About
page, New Tab page, the welcome page hero logo, and the `.exe` file icon
in Explorer.

The red survived: rebuilds, replacing every `.ico` in the tree four
different ways, clearing the Windows icon cache, restarting Explorer, and
a **full machine reboot**.

## Actual root cause

The red is not a corrupted logo. It is Chromium's *"missing resource"*
placeholder, pure `#FF0000`:

```cpp
// ui/base/resource/resource_bundle.cc:1245
// The placeholder bitmap is bright red so people notice the problem.
```

The **profile avatar** image resource is missing in this build, so
`profiles::GetWin2xAvatarImage()` returns that red bitmap. It reached the
taskbar through **two independent paths**:

1. **The generated profile icon file.**
   `CreateOrUpdateShortcutIconForProfile()` writes
   `User Data\<Profile>\Google Profile.ico`, and
   `BrowserWindowPropertyManager` points
   `PKEY_AppUserModel_RelaunchIconResource` at it. The red was baked into
   that file **on disk** — which is exactly why rebuilds and a reboot
   changed nothing. The bug lived in the user's profile directory, not in
   the binary.

2. **The window icon itself.**
   `BrowserDesktopWindowTreeHostWin::SetWindowIcon(badged=true)` badges
   the window icon with the profile avatar — but only when **more than
   one profile exists**. With a single profile that path never runs, so it
   stayed invisible through all single-profile testing and only surfaced
   when a test profile was created late in the process.

Fixing (1) without (2) looked like a fix and then regressed. That is the
main reason this dragged on.

## The false trails

| # | Hypothesis | Why it was plausible | What disproved it | Cost |
|---|---|---|---|---|
| 1 | Corrupted regenerated logo/icon files | 126 image files had just been bulk-regenerated | Reverted all 136 image files, rebuilt — still red | 1 build |
| 2 | Stale/partial `content.dll` build | `content.dll` was hours older than `chrome.dll` | Forced full recompile — identical crash offset, still red | 1 long build |
| 3 | Corrupted `<ph>` placeholders from the 3,276-substitution string pass | A blanket regex had run over 123 `.grd` files | Greps found no corruption; the regex was case-sensitive so `BRAVE_*` attributes were untouched | — |
| 4 | `.ico` missing small sizes (16/32/48) | Source `.ico` had only a 256px frame | Built a proper multi-size `.ico` — still red | 1 build |
| 5 | PNG-compressed frames rejected by `IconUtil` | That code path genuinely does skip non-32bpp frames | Hand-built BMP-encoded `.ico` — still red | 1 build |
| 6 | Windows shell icon cache stuck | The cache is genuinely notorious | Cleared cache, restarted Explorer, **rebooted** — still red | — |
| 7 | Space in `"Anar Browser.exe"` breaking the `path,index` property | `brave.exe` worked, `Anar Browser.exe` did not | The profile `.ico` path that *did* work contains spaces too | 2 builds |

Each was a plausible story that fit *some* of the evidence. None was
checked against **all** the evidence already in hand before a build was
spent on it.

## What actually cracked it

Four diagnostics, in order of value:

1. **Sampling the actual pixel colour.** The square was exactly
   `(255,0,0)` — 900 pixels of it. Not anti-aliased, not a scaling
   artifact. A pure sentinel value means *code chose this*, which turned
   the question from "why does my icon look wrong" into "who writes pure
   red" — and `grep` answered that in one shot.

2. **Probing the live window instead of theorising.** `WM_GETICON` and
   `GetClassLongPtr` on the running HWND proved the window's own icons
   were correct silver with zero red pixels, eliminating the entire
   window-icon path and pointing at the shell property.

3. **Reading the compiled artifacts.** Parsing the PE resource directory
   and extracting resource 101 from the built `chrome.dll` showed the
   correct logo *was* embedded, killing several "the build is wrong"
   theories at once.

4. **Adversarial verification.** Deliberately restoring the *red* file and
   confirming the taskbar stayed correct. A clean-state pass would have
   proven nothing; this proved the fix genuinely bypassed the bad input.

## The fix

All under `src/chrome/browser/`:

- **`profiles/profile_shortcut_manager_win.cc`** —
  `CreateOrUpdateShortcutIconForProfile()` copies `<DIR_MODULE>\anar.ico`
  verbatim instead of generating the icon from ResourceBundle images.
- **`ui/views/frame/browser_desktop_window_tree_host_win.cc`** —
  `SetWindowIcon()` forces `badged = false`, so the avatar can never reach
  the window icon.
- **`brave/BUILD.gn`** — `copy("anar_app_icon")` ships
  `brave/app/theme/brave/win/anar_app.ico` to `$root_out_dir/anar.ico`;
  `group("brave")` depends on it under `if (is_win)`.

Net effect: one fixed application icon for every profile, independent of
profile avatars and of the generated profile icon file.

Worth recording: the taskbar would **only** resolve
`PKEY_AppUserModel_RelaunchIconResource` from the profile `.ico` *file
path*. Pointing it at the `.exe`, at `chrome.dll`, or at a separate
`anar.ico` each produced a blank document icon — Windows finding nothing
at all. So the fix had to control the file's **content**, not its
location.

## Still open

- The **missing profile avatar resource** was never identified. It is
  cosmetically irrelevant now, but real per-profile avatar badges would
  need it.
- Hardcoded `"Brave"` C++ string literals (the rebrand only swept
  `.grd`/`.grdp` message text).
- `components/vector_icons/brave/product.icon` — a vector path DSL that
  cannot embed a raster logo; currently a hand-drawn approximation.

## Lessons

1. **A sentinel colour is a message.** Pure `#FF0000`/`#FF00FF` in a UI is
   almost never a rendering bug; it is a deliberate "I could not find
   this" marker. Sample the pixels before theorising about encoding.

2. **State persists outside the build.** Hours went into rebuilding a
   binary while the bad data sat in a cached file in the user's profile
   directory. When a rebuild *and a reboot* both change nothing, stop
   rebuilding — the state lives somewhere else. Go find where.

3. **Check a new hypothesis against evidence already collected.** The
   "space in the filename" theory was disproved by a fact observed hours
   earlier: the working `.ico` path contained spaces. Two builds were
   spent on a theory that five minutes of re-reading would have killed.

4. **Verify by the element's distinguishing state, not its position.**
   "Rightmost non-background pixels" locked onto a *pinned* shortcut
   rather than the running app's button, producing two confident false
   "confirmed fixed" reports. The discriminator was the highlight box.
   Name the discriminator, then actually look at the image.

5. **Dormant code paths hide behind preconditions.** The badging path only
   runs with 2+ profiles, so single-profile testing could never see it.
   When a fix works and then regresses, look for a second path gated by a
   condition the tests never met.

6. **Instrument before guessing.** One round of `base::AppendToFile`
   logging answered questions that several speculative rebuilds had not.
   Caveat learned the hard way: `AppendToFile` will not create a missing
   file in that context — pre-create it, or three separate "this code
   never runs" conclusions will be false negatives, as happened here.

7. **Say "I do not know" earlier.** The honest checkpoint — *the C++ code
   is provably correct, so this is not where I have been looking* — was
   the turning point. It should have come many hours sooner.

## Build environment note

Editing any `BUILD.gn` triggers an implicit `gn gen`, which fails on this
machine unless the local VS toolchain is selected explicitly (Visual
Studio BuildTools is at the unusual `Program Files (x86)` path and the
depot_tools packaged toolchain is unavailable):

```bash
export DEPOT_TOOLS_WIN_TOOLCHAIN=0
export vs2022_install="C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools"
```

Plain `.cc`/`.h` edits do not trigger a regen, which is why this only
surfaced when the icon was wired into the build.
