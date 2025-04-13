#!/bin/bash
#set -e

# Path to the executable
EXECUTABLE=./lz_codec

#make clean
make

ERRORS=0
WARNINGS=0
OK=0

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
    if ! $EXECUTABLE ${comp_args}; then
        echo "❌ Compression failed for ${test_name}"
        ((ERRORS++))
        return
    fi

    # Extract the compressed file path from the compressor arguments (-o option)
    local compressed_file
    compressed_file=$(echo "$comp_args" | sed -E 's/.*-o[[:space:]]+([^[:space:]]+).*/\1/')

    if [[ ! -f "$original_file" || ! -f "$compressed_file" ]]; then
        echo "❌ Error: either original file ($original_file) or compressed file ($compressed_file) is missing."
        ((ERRORS++))
        return
    fi

    local original_size compressed_size
    original_size=$(stat -c %s "$original_file")
    compressed_size=$(stat -c %s "$compressed_file")
    echo "Checking size: Original = ${original_size} bytes, Compressed = ${compressed_size} bytes"

    if (( compressed_size > original_size / 2 )); then
        echo "⚠️ Test ${test_name} size check: WARNING (Compressed file size exceeds 50% of original): ${compressed_file}: ${compressed_size} | ${original_file}: ${original_size}"
        ((WARNINGS++))
    else
        echo "✅ Test ${test_name} size check: OK"
    fi

    echo "Decompression: ${decomp_args}"
    if ! $EXECUTABLE ${decomp_args}; then
        echo "❌ Decompression failed for ${test_name}"
        ((ERRORS++))
        return
    fi

    echo "Comparing ${original_file} with ${decompressed_file}"
    if diff "${original_file}" "${decompressed_file}" >/dev/null; then
        echo "✅ Test ${test_name}: OK"
        ((OK++))
    else
        echo "❌ Test ${test_name}: FAIL (Files differ)"
        ((ERRORS++))
    fi
    echo "----------------------------------------"
    echo ""
}

####################################
# STATIC TESTS: from tests/in/static
####################################
static_tests=(t1 t2 t3 t4 t5 t6-a t6-b t7 t8 t9 t10 t11 t12)

for test_name in "${static_tests[@]}"; do
    input_file="tests/in/static/${test_name}.txt"
    output_file="tests/out/${test_name}.txt"
    decompressed_file="tests/in/static/${test_name}-decompressed.txt"

    # STATIC
    run_test "${test_name}" \
        "-i ${input_file} -o ${output_file} -w 512 -c" \
        "-i ${output_file} -o ${decompressed_file} -d" \
        "${input_file}" \
        "${decompressed_file}"


    # STATIC + PREPROCESS
#    run_test "${test_name} (static + preprocess)" \
#        "-i ${input_file} -o ${output_file} -w 512 -c -m" \
#        "-i ${output_file} -o ${decompressed_file} -d -m" \
#        "${input_file}" \
#        "${decompressed_file}"
done

########################################
# KKO.PROJ.DATA STATIC + ADAPTIVE TESTS
########################################
kko_files=(cb.raw cb2.raw df1h.raw df1hvx.raw df1v.raw shp.raw shp1.raw shp2.raw nk01.raw)
#kko_files=(cb.raw cb2.raw df1h.raw)

for file in "${kko_files[@]}"; do
    name=$(basename "$file" .raw)

    # STATIC
    run_test "${file} (static)" \
        "-i tests/in/kko.proj.data/${file} -o tests/out/${file} -w 512 -c" \
        "-i tests/out/${file} -o tests/in/kko.proj.data/${file}-decompressed.txt -d" \
        "tests/in/kko.proj.data/${file}" \
        "tests/in/kko.proj.data/${file}-decompressed.txt"

    # STATIC + PREPROCESS
    run_test "${file} (static + preprocess)" \
        "-i tests/in/kko.proj.data/${file} -o tests/out/${file} -w 512 -c -m" \
        "-i tests/out/${file} -o tests/in/kko.proj.data/${file}-decompressed.txt -d" \
        "tests/in/kko.proj.data/${file}" \
        "tests/in/kko.proj.data/${file}-decompressed.txt"

    # ADAPTIVE
    run_test "${file} (adaptive)" \
        "-i tests/in/kko.proj.data/${file} -o tests/out/${file} -w 512 -c -a" \
        "-i tests/out/${file} -o tests/in/kko.proj.data/${file}-decompressed.txt -d" \
        "tests/in/kko.proj.data/${file}" \
        "tests/in/kko.proj.data/${file}-decompressed.txt"

    # ADAPTIVE + PREPROCESS
    run_test "${file} (adaptive + preprocess)" \
        "-i tests/in/kko.proj.data/${file} -o tests/out/${file} -w 512 -c -a -m" \
        "-i tests/out/${file} -o tests/in/kko.proj.data/${file}-decompressed.txt -d" \
        "tests/in/kko.proj.data/${file}" \
        "tests/in/kko.proj.data/${file}-decompressed.txt"
done

########################################
# FINAL SUMMARY
########################################
echo ""
echo "======================="
echo "✅ OK:       ${OK}"
echo "⚠️ Warnings: ${WARNINGS}"
echo "❌ Errors:   ${ERRORS}"
echo "======================="

if (( ERRORS > 0 )); then
    echo "❌ Some tests failed."
    exit 1
else
    echo "✅ All tests completed successfully."
    exit 0
fi
