#!/bin/bash

# Usage: ./run_tests.sh <num_runs> <init_port>

if [ $# -ne 2 ]; then
    echo "Usage: $0 <number_of_runs> <init_port>"
    exit 1
fi

N=$1
OUTPUT_DIR="test_outputs"
FAILED_DL_DIR="failed_downloads"

mkdir -p "$OUTPUT_DIR"
mkdir -p "$FAILED_DL_DIR"

echo "Running 'make test' $N times and skipping ports that return curl (7) errors..."
echo

port=$2

for ((i=1; i<=N; i++)); do

    # Probe port using curl — if curl fails with code 7, port assumed free
    while true; do
        curl -s http://localhost:$port >/dev/null 2>&1
        curl_status=$?

        if [ $curl_status -eq 7 ]; then
            break
        else
            echo "Port $port appears active (curl exit=$curl_status) — trying next..."
            port=$((port + 1))
        fi
    done

    echo "=== Run $i using port=$port ==="
    output_file="$OUTPUT_DIR/run_${i}_port_${port}.txt"

    # Run test and save full output
    make test port=$port > "$output_file" 2>&1

    # Get the 5th-to-last line
    fifth_last=$(tail -n 5 "$output_file" | head -n 1)
    echo "$fifth_last"
    echo

    # Check whether the line contains "Passed"
    if [[ "$fifth_last" != *"Passed"* ]]; then
        # Mark output file as failed
        failed_file="${output_file%.txt}_failed.txt"
        mv "$output_file" "$failed_file"
        output_file="$failed_file"

        # ---- NEW: SAVE DOWNLOADED FILES SNAPSHOT ----
        dl_snapshot="$FAILED_DL_DIR/run_${i}_port_${port}"
        mkdir -p "$dl_snapshot"

        if [ -d "downloaded_files" ]; then
            cp -r "downloaded_files" "$dl_snapshot/"
            echo "Saved failed downloaded_files to: $dl_snapshot/"
        else
            echo "Warning: downloaded_files directory missing!"
        fi
        # ----------------------------------------------

    fi

    port=$((port + 1))
done

echo "Complete — outputs saved in '$OUTPUT_DIR/' and failed downloads in '$FAILED_DL_DIR/'"
