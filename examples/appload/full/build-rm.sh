#!/bin/bash
rm -rf output-rm
mkdir -p output-rm/backend
cp icon.png manifest.json output-rm
rcc --binary -o output-rm/resources.rcc application.qrc
cd backend
cargo build --target armv7-unknown-linux-gnueabihf --release
cp target/armv7-unknown-linux-gnueabihf/release/backend ../output-rm/backend/entry
cd ..

