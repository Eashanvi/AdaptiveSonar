# FPGA: adaptive sonar transmitter on Cyclone V

Board: Terasic DE1-SoC (5CSEMA5F31C6). Tools: Quartus Prime Lite and Icarus Verilog for simulation.

This runs the same transmitter as the STM32, but at the real 100–500 kHz band. It samples at 4 MS/s into an 8-bit DAC on GPIO_0 (JP1).

## Folders

| Folder | What it is |
|---|---|
| `de1soc_makara/` | Everything the board build needs: RTL, ROM `.hex` files, pin setup (`setup.tcl`) and the clock constraint (`makara.sdc`). This is the version that ran on the board on 26 Sep 2026. |
| `de1soc_makara/sim_only/` | Board-level testbench. Do not add it to the Quartus project. |
| `simulation/` | Single-pulse DDS testbench, plotting script and reference spectrogram pictures |
| `tools/gen_roms.py` | Regenerates the ROM `.hex` files: sine, windows, sound speed and absorption |

## Build for the board

1. Use the New Project Wizard with the project folder set to `fpga/de1soc_makara`. Name the project `makara_fpga`, set the top level to `makara_top`, add no files, and pick device 5CSEMA5F31C6.
2. In the Tcl Console, `cd` to that folder and run `source setup.tcl`. This adds the 9 Verilog files, the SDC and all pin assignments.
3. Run Start Compilation. In the Programmer, use Auto Detect, then load the `.sof` onto the 5CSEMA5F31.
4. Check it is loaded: LEDR9 blinks about once a second, and HEX shows the start and end frequency in kHz.

**Switches:**

| Switches | Selects | Example |
|---|---|---|
| SW[1:0] | Depth | — |
| SW[3:2] | Turbidity | deep + clear gives 300/382 kHz; deep + muddy gives 200/292 kHz |

## Simulate

Board-level run, about 1 minute, from inside `de1soc_makara/`:

```
iverilog -g2005 -o tb.out sim_only/tb_final.v *.v
vvp tb.out
```

Expected output:
- a tick rate of about 4000 per ms
- deep+clear: 300/382
- deep+muddy: 200/292
- shallow+clear: 300/380
- mid+mid: 250/333
- 7 trigger pulses

Single pulse and spectrogram, which needs numpy and matplotlib:

```
fpga/simulation/run_dds.sh 0 0 LFM_window_on
```

The first argument picks the waveform: 0 LFM, 1 geometric, 2 stepped, 3 Barker-13. The second argument, set to 1, turns the window off.

For the default LFM run, the -6 dB band should be about 209–331 kHz.
