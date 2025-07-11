#!/usr/bin/env bash

echo "🎨 Formatting code with clang-format..."

find libs/ services/ tests/ -type f \( -name "*.cpp" -o -name "*.hpp" -o -name "*.h" \) -exec clang-format -i {} +

echo "✅ Formatting complete."
