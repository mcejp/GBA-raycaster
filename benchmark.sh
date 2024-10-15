#!/bin/sh

printf "0203FFFC:CAFE\n0203FFFE:0001\n" >cmake-build-release/cheats.vba
mgba-rom-test -S 0x03 --cheats cmake-build-release/cheats.vba --log-level 15 cmake-build-release/helloTonc.gba
