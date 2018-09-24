#!/bin/bash

set -e
set -x

./configure --prefix="$(realpath "$(dirname "${0}")/../sdl")"
make -j 24
make -j 24 install

