MAKARA FPGA - FINAL (26 Sept 2026)  -  Terasic DE1-SoC, Quartus 18.0 Lite
========================================================================
Everything the board needs is in this one folder. There is NO PLL and NO
hello test in here, so the board cannot end up running the wrong design.

Top-level design: makara_top  (sonar, runs on the 50 MHz board clock)
Sign that THIS design is on the board: LEDR9 blinks about once a second,
and the six number displays show the start and end frequency in kHz.

Simulated here with Icarus at 50 MHz (sim_only/tb_final.v):
  sample rate 4000 ticks per ms (exact)
  switches deep+clear   -> HEX 300 / 382
  switches deep+muddy   -> HEX 200 / 292
  switches shallow+clear-> HEX 300 / 380
  switches mid+mid      -> HEX 250 / 333, output sweeps 259 -> 321 kHz
sim_only/ is for simulation only: do NOT add it to the Quartus project.
