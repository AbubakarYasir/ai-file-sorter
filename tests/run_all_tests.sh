#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

declare -a TEST_SCRIPTS=(
    "$ROOT_DIR/tests/run_database_tests.sh"
    "$ROOT_DIR/tests/run_translation_tests.sh"
    "$ROOT_DIR/tests/run_personal_index_tests.sh"
    "$ROOT_DIR/tests/run_personal_index_query_tests.sh"
)

echo "Running AI File Sorter test suite"
echo "================================="

for script in "${TEST_SCRIPTS[@]}"; do
    if [[ ! -f "$script" ]]; then
        echo "ERROR: Test script '$script' is missing." >&2
        exit 1
    fi

    name="$(basename "$script")"
    echo ""
    echo ">>> $name"
    bash "$script"
done

echo ""
echo "All tests completed successfully."
