#!/usr/bin/env python3
import os
import subprocess
import time

import math

###############################################################################
# Configuration
###############################################################################
# Path to your lz_codec binary after compilation. By default, we assume it's
# built in the current directory.
EXECUTABLE = "./lz_codec"

# The directory containing your "kko.proj.data" files (uncompressed).
# Modify this path as needed if your input data is stored elsewhere.
KKO_DATA_PATH = "tests/in/kko.proj.data"

# The directory for output compressed files
OUT_PATH = "tests/out"

# The directory to store decompressed files for verification
DECOMPRESSED_PATH = "tests/in/kko.proj.data"  # reused folder, or separate if you prefer

# The list of raw files you want to test. Adjust to your actual file names.
KKO_FILES = [
    "cb.raw", "cb2.raw",
    "df1h.raw", "df1hvx.raw", "df1v.raw",
    "shp.raw", "shp1.raw", "shp2.raw",
    "nk01.raw"
]

# Optionally set a default width if needed:
DEFAULT_WIDTH = 512

# If you want to measure times, set True
MEASURE_TIMES = True


###############################################################################
# Helper Functions
###############################################################################
# somewhere above main(), e.g. next to your other helper functions:
def compute_entropy(file_path):
    """
    Reads the entire file as bytes, builds a histogram of [0..255],
    and computes -sum(p_i * log2(p_i)) in bits/symbol.
    """
    if not os.path.isfile(file_path):
        return 0.0
    with open(file_path, "rb") as f:
        data = f.read()
    size = len(data)
    if size == 0:
        return 0.0
    freq = [0] * 256
    for b in data:
        freq[b] += 1
    entropy = 0.0
    for c in freq:
        if c > 0:
            p = c / size
            entropy -= p * math.log2(p)
    return entropy


def run_command(cmd):
    """
    Runs a shell command given as a list of strings and returns (exit_code, stdout, stderr).
    """
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, check=False)
        return result.returncode, result.stdout, result.stderr
    except FileNotFoundError:
        return 1, "", "Command not found"


def file_size(file_path):
    """
    Returns the file size in bytes for file_path.
    """
    if not os.path.isfile(file_path):
        return -1
    return os.path.getsize(file_path)


def compression_ratio(orig_size, comp_size):
    """
    Returns ratio as a percentage if orig_size > 0.
    Otherwise returns 0 or None as fallback.
    """
    if orig_size <= 0:
        return None
    return (comp_size / orig_size) * 100.0


def generate_output_file_name(file_name, suffix):
    """
    Creates an output file name in the OUT_PATH directory with the given suffix.
    E.g., for 'cb.raw' -> 'cb.raw{suffix}'
    """
    base = os.path.basename(file_name)
    return os.path.join(OUT_PATH, f"{base}{suffix}")


def generate_decompressed_file_name(file_name):
    """
    Creates a path for the decompressed file inside DECOMPRESSED_PATH
    E.g., for 'cb.raw' -> 'cb.raw-decompressed.raw'
    """
    base = os.path.basename(file_name)
    # You can adjust extension as needed:
    return os.path.join(DECOMPRESSED_PATH, f"{base}-decompressed")


def measure_time(func, *args, **kwargs):
    """
    Helper function that times execution of 'func' with the provided arguments.
    Returns (elapsed_time_in_seconds, return_value).
    """
    start = time.perf_counter()
    result = func(*args, **kwargs)
    end = time.perf_counter()
    return (end - start), result


def run_test(description, input_file, output_file, decompressed_file,
             compress_args, decompress_args, entropy_val):
    """
    Runs a single test:
      1) Compress with 'compress_args'
      2) Check compressed output
      3) Decompress with 'decompress_args'
      4) Compare to original
    Returns a dict with results and stats.
    """
    print(f"Running test: {description}")
    res = {
        "description": description,
        "input_file": input_file,
        "output_file": output_file,
        "decompressed_file": decompressed_file,
        "orig_size": file_size(input_file),
        "comp_size": None,
        "ratio": None,
        "ok": False,
        "compression_time": 0.0,
        "decompression_time": 0.0,
        "error": "",
        "entropy": entropy_val,  # store it in results
    }

    # 1) Compression
    if MEASURE_TIMES:
        ctime, (exit_code, out, err) = measure_time(run_command, compress_args)
    else:
        exit_code, out, err = run_command(compress_args)
        ctime = 0.0

    res["compression_time"] = ctime
    if exit_code != 0:
        res["error"] = f"Compression failed. Stderr:\n{err}"
        return res

    # 2) Size check
    res["comp_size"] = file_size(output_file)
    if res["comp_size"] < 0:
        res["error"] = f"Compressed file {output_file} does not exist."
        return res

    if res["orig_size"] > 0:
        res["ratio"] = compression_ratio(res["orig_size"], res["comp_size"])

    # 3) Decompression
    if MEASURE_TIMES:
        dtime, (exit_code, out, err) = measure_time(run_command, decompress_args)
    else:
        exit_code, out, err = run_command(decompress_args)
        dtime = 0.0

    res["decompression_time"] = dtime
    if exit_code != 0:
        res["error"] = f"Decompression failed. Stderr:\n{err}"
        return res

    # 4) Compare
    if not os.path.isfile(decompressed_file):
        res["error"] = f"Decompressed file {decompressed_file} not found."
        return res

    # Optional: compare contents
    # A full bitwise comparison:
    diff_cmd = ["diff", input_file, decompressed_file]
    exit_code, out, err = run_command(diff_cmd)
    if exit_code == 0:  # 0 => no differences
        res["ok"] = True
    else:
        res["error"] = "Files differ."

    return res


###############################################################################
# Main
###############################################################################
def main():
    # 0) Compile if needed:
    print("Compiling project with 'make' ...")
    exit_code, out, err = run_command(["make"])
    if exit_code != 0:
        print("❌ Compilation failed:\n", err)
        return

    entropy_map = {}
    for kko_file in KKO_FILES:
        input_path = os.path.join(KKO_DATA_PATH, kko_file)
        ent = compute_entropy(input_path)
        entropy_map[kko_file] = ent

    # Prepare data structure to hold results
    results = []

    # 1) For each KKO file, test 4 modes:
    for kko_file in KKO_FILES:
        input_path = os.path.join(KKO_DATA_PATH, kko_file)
        # we ensure these dirs exist:
        os.makedirs(OUT_PATH, exist_ok=True)
        os.makedirs(DECOMPRESSED_PATH, exist_ok=True)

        # Some users store compressed => file + suffix, or we can do this:
        # example: "cb.raw" -> "cb.raw.compressed"
        # but let's keep it short:
        # 1a) static
        out_static = generate_output_file_name(kko_file, "_static.lz")
        dec_static = generate_decompressed_file_name(kko_file) + "_static"

        cargs_static = [
            EXECUTABLE,
            "-i", input_path,
            "-o", out_static,
            "-w", str(DEFAULT_WIDTH),
            "-c"  # compress
        ]
        dargs_static = [
            EXECUTABLE,
            "-i", out_static,
            "-o", dec_static,
            "-d"  # decompress
        ]
        r = run_test(f"{kko_file} (static)", input_path, out_static, dec_static,
                     cargs_static, dargs_static, entropy_map[kko_file])
        results.append(r)

        # 1b) static + preprocess
        out_static_m = generate_output_file_name(kko_file, "_static_m.lz")
        dec_static_m = generate_decompressed_file_name(kko_file) + "_static_m"

        cargs_static_m = [
            EXECUTABLE,
            "-i", input_path,
            "-o", out_static_m,
            "-w", str(DEFAULT_WIDTH),
            "-c",  # compress
            "-m"  # model => delta
        ]
        dargs_static_m = [
            EXECUTABLE,
            "-i", out_static_m,
            "-o", dec_static_m,
            "-d"
            # NOTE: You might or might not pass "-m" for decompression depending on your code design
            # If your code automatically detects, you can omit it.
            # If you require a command line flag for "preprocessed" decode, add it here:
        ]
        r = run_test(f"{kko_file} (static + preprocess)", input_path, out_static_m, dec_static_m,
                     cargs_static_m, dargs_static_m, entropy_map[kko_file])
        results.append(r)

        # 1c) adaptive
        out_adaptive = generate_output_file_name(kko_file, "_adaptive.lz")
        dec_adaptive = generate_decompressed_file_name(kko_file) + "_adaptive"

        cargs_adaptive = [
            EXECUTABLE,
            "-i", input_path,
            "-o", out_adaptive,
            "-w", str(DEFAULT_WIDTH),
            "-c",  # compress
            "-a"
        ]
        dargs_adaptive = [
            EXECUTABLE,
            "-i", out_adaptive,
            "-o", dec_adaptive,
            "-d"
        ]
        r = run_test(f"{kko_file} (adaptive)", input_path, out_adaptive, dec_adaptive,
                     cargs_adaptive, dargs_adaptive, entropy_map[kko_file])
        results.append(r)

        # 1d) adaptive + preprocess
        out_adaptive_m = generate_output_file_name(kko_file, "_adaptive_m.lz")
        dec_adaptive_m = generate_decompressed_file_name(kko_file) + "_adaptive_m"

        cargs_adaptive_m = [
            EXECUTABLE,
            "-i", input_path,
            "-o", out_adaptive_m,
            "-w", str(DEFAULT_WIDTH),
            "-c",
            "-a",
            "-m"
        ]
        dargs_adaptive_m = [
            EXECUTABLE,
            "-i", out_adaptive_m,
            "-o", dec_adaptive_m,
            "-d"
            # Similarly, if your code needs "-m" for decoding the preprocessed scenario, add it:
        ]
        r = run_test(f"{kko_file} (adaptive + preprocess)", input_path, out_adaptive_m, dec_adaptive_m,
                     cargs_adaptive_m, dargs_adaptive_m, entropy_map[kko_file])
        results.append(r)

    # 2) Print final summary with a LaTeX table
    print("\nAll tests done. Printing results as a LaTeX table:\n")
    print(r"\begin{tabular}{lrrrrrrr}")
    print(r"\hline")
    print(r"Test & Original(B) & Compressed(B) & Ratio(\%) & Entropy & OK? & Times(s)\\")
    print(r"\hline")

    for r in results:
        # ratio in a nice format
        ratio_str = f"{r['ratio']:.2f}" if r["ratio"] is not None else "-"
        ok_str = "OK" if r["ok"] else "FAIL"
        # times
        ctime_str = f"{r['compression_time']:.3f}"
        dtime_str = f"{r['decompression_time']:.3f}"
        time_str = f"C:{ctime_str} D:{dtime_str}"
        ent_str = f"{r['entropy']:.2f}"

        print( f"{r['description']} & {r['orig_size']} & {r['comp_size']} & {ratio_str} & {ent_str} & {ok_str} & {time_str} \\\\")

    print(r"\hline")
    print(r"\end{tabular}")
    print()

    # Optionally detect number of fails/warnings
    fails = sum(not x["ok"] for x in results)
    if fails > 0:
        print(f"There were {fails} failing tests. See above for details.")
    else:
        print("All tests appear to have succeeded.")


if __name__ == "__main__":
    main()
