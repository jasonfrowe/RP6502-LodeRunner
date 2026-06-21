# Lode Runner for the Picocomputer 6502

## Generating graphics assets

```terminal
python ./tools/convert_sprite.py --bpp 4 --mode tile --extract-palette graphics/player.png
```

## Generating level data

```terminal
python ./tools/convert_levels.py
```

