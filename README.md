# Dinkcast

Dink Smallwood on the Sega Dreamcast (KallistiOS).

This repository is the engine and porting plan. It does **not** ship proprietary game media. Point the build at official freeware Dink and/or GNU FreeDink data with `DINK_DATA`.

- Port spec: [DREAMCAST-PORT-PLAN.md](DREAMCAST-PORT-PLAN.md)
- Contributor / agent workflow: [AGENTS.md](AGENTS.md)

## License

Dinkcast is **free software** under the **GNU General Public License v3.0 or later** ([LICENSE](LICENSE)). SPDX: `GPL-3.0-or-later`.

That matches **GNU FreeDink** (GPLv3), whose interpreter we graft rather than replacing. Combined binaries that include FreeDink-derived code stay under the GPL.

**Game data is separate.** `dink.dat`, graphics, sounds, and `story/*.c` come from the user’s `DINK_DATA` tree and keep *their* licenses. Do not assume this repo’s GPL covers RTSoft or third-party assets.

KallistiOS and `dc-chain` are third-party toolchain pieces with their own licenses.

## Status

Planning and repo bootstrap. No Dreamcast ELF yet. First visual bite in the plan is the original title screen (Bite 3.4).

## Checks

```bash
python3 tools/check_port_plan.py
```

`make host` / `make dc` will appear in a later bite (`KOS_BASE` required for the DC target).

## Remote

No `origin` is configured. Add one when you have a host:

```bash
git remote add origin git@HOST:USER/dinkcast.git
git push -u origin master
```
