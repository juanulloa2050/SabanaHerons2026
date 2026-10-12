# apriltag

Vendored copy of the AprilTag detector C library, used by the maze challenge
(tag36h11 only). Because the project builds with `LANGUAGES CXX`, the library is
precompiled into static archives that CMake imports (same pattern as `Util/libjpeg`).

- Upstream: https://github.com/AprilRobotics/apriltag
- Version: tag `v3.4.5`
- Commit: `94be783968e5091bcc9972c72c84fd63efce2935`
- License: BSD 2-Clause, see `LICENSE.md` (verbatim from upstream).

## Layout

| Path | Content |
| --- | --- |
| `include/apriltag/` | Public headers, upstream layout preserved. Use `#include <apriltag/apriltag.h>` and `#include <apriltag/tag36h11.h>`. |
| `src/` | C sources compiled into the archives. |
| `lib/V6/libapriltag.a` | NAO V6 build (x86_64, silvermont, glibc 2.35 headers from `Util/Buildchain/V6`). |
| `lib/Linux/libapriltag.a` | Desktop Linux x86_64 build. |
| `build.sh` | Rebuilds both archives from `src/`. |

## Files taken from upstream

- `apriltag.c`, `apriltag_pose.c`, `apriltag_quad_thresh.c`, `tag36h11.c`
- all of `common/*.c`
- headers: `apriltag.h`, `apriltag_pose.h`, `apriltag_math.h`, `tag36h11.h`, all of `common/*.h`
  (upstream compiles every `common/*.c`, so we do the same).

## Dropped

- Other tag families (`tag16h5`, `tag25h9`, `tag36h10`, `tagCircle*`, `tagCustom48h12`,
  `tagStandard*`): the maze uses tag36h11 only. To add one, copy its `.c`/`.h` and run `build.sh`.
- Python wrapper, `apriltag_demo` example, upstream tests, CMake/pkg-config files, docs.

## Rebuilding

```
Util/apriltag/build.sh
```

Needs `clang` and `llvm-ar` (or `ar`); no network. Works from any directory.
Flags (common): `-std=gnu99 -O3 -DNDEBUG`, no LTO.

- V6: `--target=x86_64-linux-gnu -march=silvermont -mtune=silvermont -nostdinc` with
  `-isystem` for clang's resource include dir, `Util/Buildchain/V6/include/x86_64-linux-gnu`
  and `Util/Buildchain/V6/include` (mirrors `Make/CMake/NaoToolchain.cmake`).
- Linux: `-fPIC`.

## glibc compatibility

The desktop archive is built on a rolling distro but linked by teammates on Ubuntu 22.04
(glibc 2.35). `nm -u lib/Linux/libapriltag.a` must not list `__isoc23_*` (emitted for
`strtol`/`sscanf` family with glibc >= 2.38 in C23/`_GNU_SOURCE` modes) or other symbols newer
than 2.35. With `-std=gnu99` none appear. Re-check after every rebuild:

```
nm -u Util/apriltag/lib/Linux/libapriltag.a | rg 'isoc23'
```

The archive needs `libm` and pthreads at link time (pthread is part of libc since glibc 2.34).
