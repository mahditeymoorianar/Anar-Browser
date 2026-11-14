# Anar Browser

This repository captures the Anar Browser source from its Windows x64 development checkout. The import starts a new Git history.

- `src/`: Chromium `152.0.7977.64` (upstream commit `506c834ecceaa943c5f41e6cfe7f68acb5c45346`), with the changes present in the Anar checkout.
- `src/brave/`: Brave Core (upstream commit `df7a06f463376c087424e48b09152b8bb9a50483`), including Anar branding and browser features.
- `build/windows/`: Windows component build overrides and patches for five separately synced dependencies.
- `docs/`: Anar development notes.

The upstream license files are under [Chromium](src/LICENSE) and [Brave Core](src/brave/LICENSE).

## Windows development

The source snapshot does not bundle the Visual Studio toolchain, downloaded Chromium/Brave dependencies, or the browser executable. For the upstream build requirements, see the [Brave Core build guide](src/brave/README.md#install-prerequisites).

The upstream initialization scripts expect separate Chromium and Brave Core Git checkouts in the `src/` and `src/brave/` directories. The import combines those source trees into one Git repository. Use a separate build workspace with the pinned upstream revisions and their dependencies, then overlay the corresponding source trees from this repository. The top-level `.gclient` records the dependency settings used in the Windows checkout. Do not run upstream initialization over the imported source after applying the overlay; it can replace source or reapply patches already included here.

For dependencies synced from their clean pinned revisions, apply the patches listed in [revisions.json](build/windows/dependency-patches/revisions.json) from each dependency's own Git checkout. Run `git apply --check <absolute-path-to-patch>` before applying; a successful reverse check means the changes are already present. The Chromium and Brave Core changes are already in the imported source trees.

For a Windows x64 component build, append [the saved overrides](build/windows/component-overrides.gn) after the generated import in `src/out/Component/args.gn`. Run the Brave Core component build from `src/brave` in the synced build workspace. This source import does not certify a fresh Windows build. The experimental voice swap feature references a separate local backend that is not part of this repository.
