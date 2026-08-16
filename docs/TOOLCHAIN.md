# KallistiOS and a bootable `.cdi`

Follow [dreamcast.wiki — Getting Started](https://dreamcast.wiki/Getting_Started_with_Dreamcast_development). This file is the Dinkcast-specific subset.

**This machine is CachyOS** (Arch-based, `pacman`). Use §1 CachyOS. Debian is only a fallback.

A working disc needs three pieces:

1. **dc-chain** — `sh-elf-gcc` (hours the first time)
2. **KallistiOS** — `kos-cc`, `libkallisti`
3. **mkdcdisc** — ELF + `dink/` data → selfboot `.cdi`

Do **not** commit the toolchain or game data.

## Docker vs native (CachyOS)

**Docker is easier for the first `.cdi`.** Native dc-chain is a multi-hour SH-4 GCC build. A current KOS image skips that: pull, mount this repo + `DINK_DATA`, `make dc` / pack a CDI inside the container, run **Flycast on the host**.

Use a **maintained** image (wiki: [Docker images](https://dreamcast.wiki/Docker_images) — e.g. [kos-builds](https://github.com/orgs/kos-builds/packages?repo_name=KallistiOS) or a recent `maishuji/dc-kos-image`). **Do not** use `nold360/kallistios-sdk` (Debian Jessie, stale).

Sketch (adjust image name to whatever tag you pull):

```bash
# Cachy: docker or podman
sudo pacman -S --needed docker
# or: sudo pacman -S --needed podman

docker pull ghcr.io/kos-builds/kallistiOS:latest   # confirm tag on the packages page

docker run --rm -it \
  -v "$HOME/Source/dinkcast:/src" \
  -v "$HOME/Source/freedink-data-1.08.20190120/dink:/dink:ro" \
  -e DINK_DATA=/dink \
  -w /src \
  IMAGE_NAME \
  bash -lc 'source $KOS_BASE/environ.sh && make dc && make cdi'
```

Then on the host: `make emu` (Flycast is not inside the SDK image).

**Stay native** if you already finished dc-chain, or you want `dcload` / serial without extra USB/device mapping.

Tradeoff: the image’s KOS version may be newer or older than `v2.2.x`. If `Makefile.dc` / `vid_clear` / PVR flags fail, pin a tag or fall back to native `v2.2.x`.

## 1. Dependencies

### CachyOS / Arch (`pacman`)

There is **no** useful official or Cachy repo package that replaces dc-chain. You still compile SH-4 GCC. Host `gcc` from CachyOS is only used to *build* that cross-compiler.

```bash
sudo pacman -S --needed \
  base-devel gawk patch bzip2 tar make gmp mpfr libmpc gettext wget \
  libelf texinfo bison flex sed git diffutils curl libjpeg-turbo libpng \
  python meson ninja cmake pkgconf libisofs ruby-rake which gcc
```

`base-devel` pulls the usual compile tools. `libisofs` + `meson` are for **mkdcdisc**. `ruby-rake` matches the wiki; we do not need it for Bite 3.4.

Optional: `paru -S --needed` if you prefer the AUR helper; these packages are in extra/community, not AUR.

Set `makejobs` in `Makefile.cfg` to `nproc` (Cachy boxes are usually fine at full width; drop to `1` only if the chain errors).

### Debian/Ubuntu (if you are not on Cachy)

```bash
sudo apt-get update
sudo apt install gawk patch bzip2 tar make libgmp-dev libmpfr-dev libmpc-dev \
  gettext wget libelf-dev texinfo bison flex sed git build-essential diffutils \
  curl libjpeg-dev libpng-dev python3 pkg-config cmake libisofs-dev meson \
  ninja-build rake
```

## 2. Tree

```bash
sudo mkdir -p /opt/toolchains/dc
sudo chown -R "$(id -u):$(id -g)" /opt/toolchains/dc
git clone https://github.com/KallistiOS/KallistiOS.git -b v2.2.x /opt/toolchains/dc/kos
```

Use `v2.2.x` unless you know you want `master`.

## 3. dc-chain (SH-4 compiler)

```bash
cd /opt/toolchains/dc/kos/utils/dc-chain
cp Makefile.default.cfg Makefile.cfg   # or Makefile.dreamcast.cfg on master
# optional: set makejobs=N in Makefile.cfg
make
```

This can take **30 minutes to a few hours**. Then:

```bash
make clean distclean
```

## 4. Build KOS

```bash
cd /opt/toolchains/dc/kos
cp doc/environ.sh.sample environ.sh
```

**bash / zsh** (every new shell before `make dc`):

```bash
source /opt/toolchains/dc/kos/environ.sh
```

**fish** (`environ.sh` is bash):

```fish
bass source /opt/toolchains/dc/kos/environ.sh
```

or:

```fish
bash -lc 'source /opt/toolchains/dc/kos/environ.sh && cd /home/benh/Source/dinkcast && make dc cdi'
```

Then:

```bash
cd /opt/toolchains/dc/kos
make
```

`kos-ports` is **not** required for Bite 3.4.

Sanity check:

```bash
echo "$KOS_BASE"    # /opt/toolchains/dc/kos
which kos-cc
cd /opt/toolchains/dc/kos/examples/dreamcast/2ndmix && make
```

## 5. mkdcdisc

```bash
git clone https://gitlab.com/simulant/mkdcdisc.git /opt/toolchains/dc/mkdcdisc
cd /opt/toolchains/dc/mkdcdisc
meson setup builddir
meson compile -C builddir
# optional: put builddir on PATH
export PATH="/opt/toolchains/dc/mkdcdisc/builddir:$PATH"
```

## 6. Dinkcast ELF + CDI

From this repo, with `KOS_BASE` set and `local.mk` / `DINK_DATA` pointing at the **inner** `dink/` tree:

```bash
cd /home/benh/Source/dinkcast
make dc          # build/dinkcast.elf
make cdi         # build/dinkcast.cdi  (needs mkdcdisc + DINK_DATA)
make emu         # Flycast on that CDI
```

`make cdi` stages `build/iso/dink` → your `DINK_DATA` so the DC sees **`/cd/dink`** (our probe order: `/pc/dink`, `/cd/dink`, then compile-time `DINK_DATA`).

A CDI **without** the data tree will boot the color field / red `missing dink.dat` screen. Title splash needs `tiles/Splash.bmp` on the disc.

## 7. Hardware

- Burn **Disc-at-Once** if you use a real GD/CD (or use GDEMU / ODE with the `.cdi`).
- Faster loop: **dcload-ip** / **dcload-serial** + `dc-tool-ip -x build/dinkcast.elf` so you do not burn every change. Hostfs is `/pc/dink` if you point dcload at the data tree.

## Time / disk

- dc-chain: ~2–10 GB while building, ~1–2 GB installed
- First `make` of the chain: plan for lunch
- KOS itself: a few minutes after the chain exists
