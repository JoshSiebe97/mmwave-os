#!/bin/bash
#
# tests/run_all.sh
#
# Quick test runner. Builds everything, runs all suites, reports results.
# Exit code: 0 if all pass, 1 if any fail.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

echo "Building test suite..."
make clean 2>/dev/null || true
make test
