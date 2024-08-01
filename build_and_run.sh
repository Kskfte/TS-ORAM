#!/bin/bash

if [ "$#" -lt 2 ]; then
  echo "Usage: $0 <scheme> <test_one|test_all> <params>"
  exit 1
fi

SCHEME=$1
PROGRAM_TYPE=$2
shift 2

PARAMS=$@

build() {
  folder=$1
  if [ ! -d "$folder/build" ]; then
    mkdir -p "$folder/build"
  fi
  cd $folder/build
  cmake ..
  make
  cd ../../..
}

run() {
  folder=$1
  binary=$2
  cd $folder/build || exit 1
  ./$binary $PARAMS &
  cd ../../..
}

# Build
build "${SCHEME}/Server_0"
build "${SCHEME}/Server_1"
build "${SCHEME}/Client"

# Run
if [ "$PROGRAM_TYPE" == "test_one" ]; then
  run "${SCHEME}/Server_0" test_one_server0
  run "${SCHEME}/Server_1" test_one_server1
  sleep 1
  run "${SCHEME}/Client" test_one_client
elif [ "$PROGRAM_TYPE" == "test_all" ]; then
  run "${SCHEME}/Server_0" test_all_server0
  run "${SCHEME}/Server_1" test_all_server1
  sleep 1
  run "${SCHEME}/Client" test_all_client
else
  echo "Invalid program type: $PROGRAM_TYPE. Use 'test_one' or 'test_all'."
  exit 1
fi

wait
