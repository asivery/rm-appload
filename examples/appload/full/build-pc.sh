#!/bin/bash
rm -rf output
mkdir -p output/backend
cp icon.png manifest.json output
rcc --binary -o output/resources.rcc application.qrc
cd backend
cargo build --release
cp target/release/backend ../output/backend/entry
cd ..

