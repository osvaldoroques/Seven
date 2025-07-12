#!/usr/bin/env bash

# Docker-compatible paths
PROTO_DIR=.
OUT_CPP=..
OUT_PY=../python

# Create output directories if they don't exist
mkdir -p $OUT_CPP
mkdir -p $OUT_PY

echo "Generating C++ protobuf files..."
protoc -I=$PROTO_DIR --cpp_out=$OUT_CPP $PROTO_DIR/messages.proto

echo "Generating Python protobuf files..."
protoc -I=$PROTO_DIR --python_out=$OUT_PY $PROTO_DIR/messages.proto

echo "✅ Done! Generated C++ and Python bindings."
