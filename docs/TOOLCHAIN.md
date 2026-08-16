# KallistiOS and a bootable `.cdi`

Follow [dreamcast.wiki — Getting Started](https://dreamcast.wiki/Getting_Started_with_Dreamcast_development). This file is the Dinkcast-specific subset for **Linux**.

A working disc needs three pieces:

1. **dc-chain** — `sh-elf-gcc` (hours the first time)
2. **KallistiOS** — `kos-cc`, `libkallisti`
3. **mkdcdisc** — ELF + `dink/` data → selfboot `.cdi`

Do **not** commit the toolchain or game data.

## 1. Dependencies (Debian/Ubuntu)

```bash
sudo apt-get update
sudo apt install gawk patch bzip2 tar make libgmp-dev libmpfr-dev libmpc-dev \
  gettext wget libelf-dev texinfo bison flex sed git build-essential diffutils \
  curl libjpeg-dev libpng-dev python3 pkg-config cmake libisofs-dev meson \
  ninja-build rake
```

Arch / Fedora: same wiki page.

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
