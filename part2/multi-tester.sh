#!/bin/bash

# Usage: ./run_tests.sh <num_runs>

if [ $# -ne 1 ]; then
    echo "Usage: $0 <number_of_runs>"
    exit 1
fi

N=$1

echo "Running 'make test' $N times..."
echo

for ((i=1; i<=N; i++)); do
    echo "=== Run $i ==="

    # Capture output, extract 5th-to-last line
    result=$(make test 2>&1)
    fifth_last=$(echo "$result" | tail -n 5 | head -n 1)

    echo "$fifth_last"
    echo
done
