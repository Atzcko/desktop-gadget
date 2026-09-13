# Game Boy cartridges

The Game Boy app (`Games ▸ Game Boy`) loads any `.gb` file from this
directory onto the device. Upload one with:

```bash
tools/rom path/to/game.gb
```

One cartridge boots straight in; several show a picker. Battery saves persist
per cartridge. See
[D056](../docs/decisions/D056%20-%20A%20real%20Game%20Boy%20lives%20in%20the%20arcade.md).

## What ships here

**`libbet.gb` — Libbet and the Magic Floor**, by Damian Yerrick (pinobatch).

A homebrew puzzle game, redistributed here under the **zlib license**, which
permits redistribution in binary form. Source and original releases:
<https://github.com/pinobatch/libbet>

It is included so the emulator has something to run out of the box.

## Anything else you add

**Homebrew, or ROMs you own.** This repository ships exactly one freely
licensed cartridge and will not carry commercial ROMs — those belong to their
publishers, and `tools/rom` uploads from your machine to your device without
anything being committed here.
