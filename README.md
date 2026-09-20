# Heightmap World Generator

Heightmap World Generator converts an equirectangular planetary heightmap into a 3D point cloud. It supports large streamed outputs, optional terrain normals, and realistic RGB coloring from a separate equirectangular color map.

Output formats:

- binary PLY (`.ply`);
- uncompressed LAS 1.4 (`.las`);
- LASzip-compressed LAZ (`.laz`).

## Build

Requires CMake 3.21 or newer and a C++20 compiler. The included zlib and LASzip submodules are built statically.

```sh
git clone --recurse-submodules <repository-url>
cd HeightmapWorldGenerator
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

If necessary, initialize dependencies with `git submodule update --init --recursive`. CMake GUI and a 64-bit Visual Studio generator can be used on Windows.

## Usage

```sh
HeightmapWorldGenerator \
  --input earth-height.png \
  --color-map earth-color.png \
  --output earth.laz \
  --radius 6371000 \
  --point-count 500000000 \
  --height-min 0 \
  --height-max 9000 \
  --height-scale 20 \
  --seed 42 \
  --normals
```

Use exactly one of `--point-count` or `--density`.

| Option | Description |
| --- | --- |
| `--input PATH` | Equirectangular PNG or PGM heightmap. |
| `--output PATH` | `.ply`, `.las`, or `.laz` output file. |
| `--radius METRES` | Reference sphere radius. |
| `--point-count COUNT` | Exact number of points. |
| `--density VALUE` | Points per square kilometre. |
| `--height-min METRES` | Elevation represented by black. |
| `--height-max METRES` | Elevation represented by white. |
| `--height-scale FACTOR` | Relief multiplier; default is `1`. |
| `--color-map PATH` | Optional equirectangular RGB texture. |
| `--normals` | Calculate terrain-aware surface normals. |
| `--seed INTEGER` | Deterministic sampling seed; default is `42`. |

Run `HeightmapWorldGenerator --help` for the built-in summary.

## Notes

- Height and radius values are in metres.
- Heightmaps may be 8-bit or 16-bit PNG, or P2/P5 PGM.
- North is at the top of each input image; longitude wraps at the left and right edges.
- For land-only maps with black oceans, use `--height-min 0` to avoid artificial trenches.
- `--height-scale` values around `10` to `30` make planetary relief more visible from a distance.
- LAS/LAZ use point format 6 without color and point format 7 with color. RGB uses standard 16-bit LAS channels; normals use `normal_x`, `normal_y`, and `normal_z` Extra Bytes.
- Generation is chunked, so memory use stays bounded even for hundreds of millions of points.

## License

Add your chosen project license before publishing. Dependency licenses are included with their source code.
