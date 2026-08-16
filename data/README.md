# data/

Do not put Dink Smallwood / FreeDink game files here unless a specific file’s license allows redistribution and you intend to ship it.

Set `DINK_DATA` to your local data tree instead (see the port plan).

GNU `freedink-data-*.tar.gz` unpacks a **source package**. The game files are in the inner **`dink/`** directory (`Dink.dat`, `Map.dat`, `Story/`, …). Example:

```bash
export DINK_DATA=$HOME/Source/freedink-data-1.08.20190120/dink
make data-check
```

Or copy `local.mk.example` to `local.mk` (gitignored).
