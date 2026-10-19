#!/usr/bin/env bash
# Simulate one transmit pulse of the DDS core with Icarus Verilog and plot it.
#   ./run_dds.sh [WF_SEL] [WIN_OFF] [name]
#   WF_SEL : 0 LFM, 1 geometric, 2 stepped, 3 Barker-13   (default 0)
#   WIN_OFF: 1 = no window                                   (default 0)
# Output: build/<name>.png and build/dds_out.txt
set -e
cd "$(dirname "$0")"
WF=${1:-0}; WO=${2:-0}; NAME=${3:-wf${WF}_win$((1-WO))}
RTL=../de1soc_makara
mkdir -p build
iverilog -g2005 -DWF_SEL=$WF -DWIN_OFF=$WO -o build/tb_dds.out \
    tb_dds.v sample_tick_100MHz.v $RTL/dds_core.v $RTL/pulse_seq.v $RTL/divider_u.v
# the ROM .hex files are loaded by plain name, so run from the RTL folder
(cd $RTL && vvp ../simulation/build/tb_dds.out)
mv $RTL/dds_out.txt build/dds_out.txt
(cd build && python3 ../analyze.py "$NAME")
