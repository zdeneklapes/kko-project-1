# Cryptography Project 2 – LZSS Compression

## Task

Detailed project specification is provided here:
[Cryptography Project Specification](./zadani/kko.proj.zadani25.pdf)

---

## Project Overview

This repository contains an implementation of the Lempel–Ziv–Storer–Szymanski (LZSS) compression algorithm with
enhancements, including optional delta encoding and adaptive block compression. The project is developed in modern
C++17, providing efficient compression and decompression tailored for structured data and images.

---

## Author

- **Zdeněk Lapeš** ([xlapes02](mailto:lapes.zdenek@gmail.com))

---

## Features

- **LZSS Compression**: Implements traditional LZSS sliding window compression.
- **Adaptive Block Compression**: Divides input into `16×16` blocks, evaluating horizontal and vertical traversals for
  optimal compression.
- **Delta Encoding**: Optional preprocessing to further enhance compression ratios, ideal for smoothly varying data.
- **CLI**: Easy-to-use command-line interface with multiple configuration options.

---

## Installation and Compilation

To compile project:

```bash
make
```

The output binary `lz_codec` will be available in the root directory.

---

## Usage

```bash
./lz_codec -c -i input_file -o output_file [-a] [-m] [-w width]
```

### Command-line Arguments:

- `-c` : Compress the input file.
- `-d` : Decompress the input file.
- `-i <input>` : Specify the input file.
- `-o <output>` : Specify the output file.
- `-a` : Enable adaptive block compression (requires width and height divisible by 16).
- `-m` : Enable delta encoding preprocessing.
- `-w <width>` : Image width (required for adaptive compression).

### Examples:

Compress using static mode:

```bash
./lz_codec -c -i tests/in/static/file.raw -o tests/out/file_compressed.lz
```

Compress adaptively with delta encoding:

```bash
./lz_codec -c -a -m -w 512 -i tests/in/static/file.raw -o tests/out/file_compressed.lz
```

Decompress a file:

```bash
./lz_codec -d -i tests/out/file_compressed.lz -o tests/in/static/file_decompressed.raw
```

---

## Testing

Automated tests can be run using the provided Python benchmark script [benchmarks.py](./benchmarks.py). This script
verifies correctness, measures performance, and generates a summary table in LaTeX format.

Run all benchmark tests:

```bash
python3 benchmarks.py
```

This script performs:

- Compression and decompression on predefined test cases.
- Verification by byte-wise comparison.
- Measurement of compression ratios and execution times.

Results are printed to the console and formatted as a LaTeX table for inclusion in reports.

---

## Project Structure

- `src/`: Source code for compression and decompression.
- `tests/in/`: Input data for testing (including custom and benchmark datasets).
- `tests/out/`: Output data from tests.
- `Makefile`: Build instructions.
- `benchmarks.py`: Automated testing and benchmarking script.
- `test.sh`: Run tests on both benchmark and custom datasets (focused on debugging).

---

## Dependencies

- C++17 compatible compiler (GCC, Clang).
- Python 3.x (for testing script).

---

## License

This project is developed for educational purposes. Please refer to the university guidelines for appropriate use.
