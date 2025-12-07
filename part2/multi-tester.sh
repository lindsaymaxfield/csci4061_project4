#!/bin/bash

# Usage: ./run_tests.sh <num_runs> <init_port>

if [ $# -ne 2 ]; then
    echo "Usage: $0 <number_of_runs> <init_port>"
    exit 1
fi

N=$1
OUTPUT_DIR="test_outputs"

mkdir -p "$OUTPUT_DIR"

echo "Running 'make test' $N times and skipping ports that return curl (7) errors..."
echo

port=$2

for ((i=1; i<=N; i++)); do

    # Probe port using curl — if curl fails with code 7, port assumed free
    while true; do
        curl -s http://localhost:$port >/dev/null 2>&1
        curl_status=$?

        if [ $curl_status -eq 7 ]; then
            # No server responding → safe to use this port
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

    # Show 5th-from-last line
    fifth_last=$(tail -n 5 "$output_file" | head -n 1)
    echo "$fifth_last"
    echo

    port=$((port + 1))
done

echo "Complete — outputs saved in '$OUTPUT_DIR/'"
