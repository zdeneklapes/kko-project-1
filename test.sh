#!/bin/bash

#!/bin/bash
set -e

# Path to the executable (assumes it was built already)
EXECUTABLE=./lz_codec

# Arguments:
#   $1 -> Test name (for logging)
#   $2 -> Compression arguments
#   $3 -> Decompression arguments
#   $4 -> Path to original file (to compare sizes and later diff)
#   $5 -> Path to decompressed file (to compare)
run_test() {
    local test_name="$1"
    local comp_args="$2"
    local decomp_args="$3"
    local original_file="$4"
    local decompressed_file="$5"

    echo "----------------------------------------"
    echo "Running test: ${test_name}"
    echo "Compression: ${comp_args}"
    $EXECUTABLE ${comp_args}

    # Extract the compressed file path from the compressor arguments (-o option)
    local compressed_file
    compressed_file=$(echo "$comp_args" | sed -E 's/.*-o[[:space:]]+([^[:space:]]+).*/\1/')

    # Check that the compressed file is at most 50% of the size of the original file
    if [[ ! -f "$original_file" || ! -f "$compressed_file" ]]; then
        echo "Error: either original file ($original_file) or compressed file ($compressed_file) is missing."
        exit 1
    fi

    local original_size compressed_size
    original_size=$(stat -c %s "$original_file")
    compressed_size=$(stat -c %s "$compressed_file")
    echo "Checking size: Original ($original_file) = ${original_size} bytes, Compressed ($compressed_file) = ${compressed_size} bytes"

    if (( compressed_size > original_size / 2 )); then
        echo "Test ${test_name} size check: FAIL (Compressed file size exceeds 50% of original)"
        exit 1
    else
        echo "Test ${test_name} size check: OK"
    fi

    echo "Decompression: ${decomp_args}"
    $EXECUTABLE ${decomp_args}

    echo "Comparing ${original_file} with ${decompressed_file}"
    if diff "${original_file}" "${decompressed_file}" >/dev/null; then
        echo "Test ${test_name}: OK"
    else
        echo "Test ${test_name}: FAIL (Files differ)"
    fi
    echo "----------------------------------------"
    echo ""
}

# Test cases

# t1
run_test "t1" \
    "-i tmp/tests/in/t1.txt -o tmp/tests/out/t1.txt -w 512 -c" \
    "-i tmp/tests/out/t1.txt -o tmp/tests/in/t1-decompressed.txt -d" \
    "tmp/tests/in/t1.txt" \
    "tmp/tests/in/t1-decompressed.txt"

# t2
run_test "t2" \
    "-i tmp/tests/in/t2.txt -o tmp/tests/out/t2.txt -w 512 -c" \
    "-i tmp/tests/out/t2.txt -o tmp/tests/in/t2-decompressed.txt -d" \
    "tmp/tests/in/t2.txt" \
    "tmp/tests/in/t2-decompressed.txt"

# t3
run_test "t3" \
    "-i tmp/tests/in/t3.txt -o tmp/tests/out/t3.txt -w 512 -c" \
    "-i tmp/tests/out/t3.txt -o tmp/tests/in/t3-decompressed.txt -d" \
    "tmp/tests/in/t3.txt" \
    "tmp/tests/in/t3-decompressed.txt"

# t4
run_test "t4" \
    "-i tmp/tests/in/t4.txt -o tmp/tests/out/t4.txt -w 512 -c" \
    "-i tmp/tests/out/t4.txt -o tmp/tests/in/t4-decompressed.txt -d" \
    "tmp/tests/in/t4.txt" \
    "tmp/tests/in/t4-decompressed.txt"

# t5
run_test "t5" \
    "-i tmp/tests/in/t5.txt -o tmp/tests/out/t5.txt -w 512 -c" \
    "-i tmp/tests/out/t5.txt -o tmp/tests/in/t5-decompressed.txt -d" \
    "tmp/tests/in/t5.txt" \
    "tmp/tests/in/t5-decompressed.txt"

# t6-a
run_test "t6-a" \
    "-i tmp/tests/in/t6-a.txt -o tmp/tests/out/t6-a.txt -w 512 -c" \
    "-i tmp/tests/out/t6-a.txt -o tmp/tests/in/t6-a-decompressed.txt -d" \
    "tmp/tests/in/t6-a.txt" \
    "tmp/tests/in/t6-a-decompressed.txt"

# t6-b
run_test "t6-b" \
    "-i tmp/tests/in/t6-b.txt -o tmp/tests/out/t6-b.txt -w 512 -c" \
    "-i tmp/tests/out/t6-b.txt -o tmp/tests/in/t6-b-decompressed.txt -d" \
    "tmp/tests/in/t6-b.txt" \
    "tmp/tests/in/t6-b-decompressed.txt"

# t7
run_test "t7" \
    "-i tmp/tests/in/t7.txt -o tmp/tests/out/t7.txt -w 512 -c" \
    "-i tmp/tests/out/t7.txt -o tmp/tests/in/t7-decompressed.txt -d" \
    "tmp/tests/in/t7.txt" \
    "tmp/tests/in/t7-decompressed.txt"

# t8
run_test "t8" \
    "-i tmp/tests/in/t8.txt -o tmp/tests/out/t8.txt -w 512 -c" \
    "-i tmp/tests/out/t8.txt -o tmp/tests/in/t8-decompressed.txt -d" \
    "tmp/tests/in/t8.txt" \
    "tmp/tests/in/t8-decompressed.txt"

# t9
run_test "t9" \
    "-i tmp/tests/in/t9.txt -o tmp/tests/out/t9.txt -w 512 -c" \
    "-i tmp/tests/out/t9.txt -o tmp/tests/in/t9-decompressed.txt -d" \
    "tmp/tests/in/t9.txt" \
    "tmp/tests/in/t9-decompressed.txt"

# t10
run_test "t10" \
    "-i tmp/tests/in/t10.txt -o tmp/tests/out/t10.txt -w 512 -c" \
    "-i tmp/tests/out/t10.txt -o tmp/tests/in/t10-decompressed.txt -d" \
    "tmp/tests/in/t10.txt" \
    "tmp/tests/in/t10-decompressed.txt"

# t11
run_test "t11" \
    "-i tmp/tests/in/t11.txt -o tmp/tests/out/t11.txt -w 512 -c" \
    "-i tmp/tests/out/t11.txt -o tmp/tests/in/t11-decompressed.txt -d" \
    "tmp/tests/in/t11.txt" \
    "tmp/tests/in/t11-decompressed.txt"

# t12 (static compressor with additional -w 512 for decompression)
run_test "t12" \
    "-i tmp/tests/in/t12.txt -o tmp/tests/out/t12.txt -w 512 -c" \
    "-i tmp/tests/out/t12.txt -o tmp/tests/in/t12-decompressed.txt -w 512 -d" \
    "tmp/tests/in/t12.txt" \
    "tmp/tests/in/t12-decompressed.txt"

# t13 (adaptive compressor)
run_test "t13" \
    "-i tmp/tests/in/t13.txt -o tmp/tests/out/t13.txt -w 4 -c -a" \
    "-i tmp/tests/out/t13.txt -o tmp/tests/in/t13-decompressed.txt -d" \
    "tmp/tests/in/t13.txt" \
    "tmp/tests/in/t13-decompressed.txt"

# kko.proj.data files
# cb.raw
run_test "cb.raw" \
    "-i tmp/tests/in/kko.proj.data/cb.raw -o tmp/tests/out/cb.raw -w 512 -c" \
    "-i tmp/tests/out/cb.raw -o tmp/tests/in/kko.proj.data/cb.raw-decompressed.txt -d" \
    "tmp/tests/in/kko.proj.data/cb.raw" \
    "tmp/tests/in/kko.proj.data/cb.raw-decompressed.txt"

# cb2.raw
run_test "cb2.raw" \
    "-i tmp/tests/in/kko.proj.data/cb2.raw -o tmp/tests/out/cb2.raw -w 512 -c" \
    "-i tmp/tests/out/cb2.raw -o tmp/tests/in/kko.proj.data/cb2.raw-decompressed.txt -d" \
    "tmp/tests/in/kko.proj.data/cb2.raw" \
    "tmp/tests/in/kko.proj.data/cb2.raw-decompressed.txt"

# df1h.raw
run_test "df1h.raw" \
    "-i tmp/tests/in/kko.proj.data/df1h.raw -o tmp/tests/out/df1h.raw -w 512 -c" \
    "-i tmp/tests/out/df1h.raw -o tmp/tests/in/kko.proj.data/df1h.raw-decompressed.txt -d" \
    "tmp/tests/in/kko.proj.data/df1h.raw" \
    "tmp/tests/in/kko.proj.data/df1h.raw-decompressed.txt"

# df1hvx.raw
run_test "df1hvx.raw" \
    "-i tmp/tests/in/kko.proj.data/df1hvx.raw -o tmp/tests/out/df1hvx.raw -w 512 -c" \
    "-i tmp/tests/out/df1hvx.raw -o tmp/tests/in/kko.proj.data/df1hvx.raw-decompressed.txt -d" \
    "tmp/tests/in/kko.proj.data/df1hvx.raw" \
    "tmp/tests/in/kko.proj.data/df1hvx.raw-decompressed.txt"

# df1v.raw
run_test "df1v.raw" \
    "-i tmp/tests/in/kko.proj.data/df1v.raw -o tmp/tests/out/df1v.raw -w 512 -c" \
    "-i tmp/tests/out/df1v.raw -o tmp/tests/in/kko.proj.data/df1v.raw-decompressed.txt -d" \
    "tmp/tests/in/kko.proj.data/df1v.raw" \
    "tmp/tests/in/kko.proj.data/df1v.raw-decompressed.txt"

# shp.raw
run_test "shp.raw" \
    "-i tmp/tests/in/kko.proj.data/shp.raw -o tmp/tests/out/shp.raw -w 512 -c" \
    "-i tmp/tests/out/shp.raw -o tmp/tests/in/kko.proj.data/shp.raw-decompressed.txt -d" \
    "tmp/tests/in/kko.proj.data/shp.raw" \
    "tmp/tests/in/kko.proj.data/shp.raw-decompressed.txt"

# shp1.raw
run_test "shp1.raw" \
    "-i tmp/tests/in/kko.proj.data/shp1.raw -o tmp/tests/out/shp1.raw -w 512 -c" \
    "-i tmp/tests/out/shp1.raw -o tmp/tests/in/kko.proj.data/shp1.raw-decompressed.txt -d" \
    "tmp/tests/in/kko.proj.data/shp1.raw" \
    "tmp/tests/in/kko.proj.data/shp1.raw-decompressed.txt"

# shp2.raw
run_test "shp2.raw" \
    "-i tmp/tests/in/kko.proj.data/shp2.raw -o tmp/tests/out/shp2.raw -w 512 -c" \
    "-i tmp/tests/out/shp2.raw -o tmp/tests/in/kko.proj.data/shp2.raw-decompressed.txt -d" \
    "tmp/tests/in/kko.proj.data/shp2.raw" \
    "tmp/tests/in/kko.proj.data/shp2.raw-decompressed.txt"

echo "All tests completed."
