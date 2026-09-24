# fft_32x8x8.bin

## Purpose

Binary dump of 32 FFT channels, each channel representing one `8 x 8` board plane.

## File layout

The file is composed of:

1. A fixed-size header.
2. Raw FFT data stored as pairs of `float` values: real part and imaginary part.

## Header

The current header is `64` bytes.

```c
struct FftBinaryHeader
{
    char magic[8];        // "CHFFT8B0"
    uint32_t channels;    // 32
    uint32_t rows;        // 8
    uint32_t cols;        // 8
    uint32_t reserved;    // 0
};
```

## Data section

Data are stored in this order:

1. channel
2. row
3. column
4. real part
5. imaginary part

For every complex FFT value:

```text
real  -> float32
imag  -> float32
```

## Dimensions

Per channel:

- `8 x 8 = 64` complex values
- each complex value = `2 * float32 = 8 bytes`
- one channel = `64 * 8 = 512 bytes`

For all channels:

- `32 * 512 = 16384 bytes`

Total file size:

- `16384 bytes` of FFT data
- `64 bytes` of header
- `16448 bytes` total

## Memory / alignment features

- The C++ batch structures use `alignas(64)`.
- The output blob is embedded as a mutable array in `main.cpp`.
- The data payload is written in a deterministic layout.
- The format is little-endian on the current platform/toolchain.

## Channel meaning

The 32 channels currently correspond to piece-specific planes:

- 0..7: white pieces
- 8..15: black pieces
- 16..23: black pawns by file
- 24..31: white pawns by file

## Notes

- This file is a printable description of the binary format.
- If the layout changes in code, this document should be updated together with it.
