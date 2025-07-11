#!/usr/bin/env bash

# Change these to your local paths
PROTO_DIR=.
OUT_CPP=../build
OUT_PY=../build/python

mkdir -p $OUT_CPP
mkdir -p $OUT_PY

echo "Generating C++..."
protoc -I=$PROTO_DIR --cpp_out=$OUT_CPP $PROTO_DIR/messages.proto

echo "Generating Python..."
protoc -I=$PROTO_DIR --python_out=$OUT_PY $PROTO_DIR/messages.proto

echo "✅ Done! Generated C++ and Python bindings."
