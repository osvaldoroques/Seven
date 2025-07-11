#!/usr/bin/env bash

set -e

./scripts/clean.sh
./scripts/build.sh

echo "✅ Rebuild complete."
