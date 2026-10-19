`timescale 1ns/1ps
// Drives the synthesis path with one fixed decision and dumps every sample.
module tb_dds;
    reg clk = 0, rst_n = 0;
    always #5 clk = ~clk;                       // 100 MHz

    wire tick;
    sample_tick #(.DIV(25)) u_tick (.clk(clk), .rst_n(rst_n), .tick(tick));

    // "Deep and muddy": 200 -> 340 kHz, 5.4 ms, full amplitude, LFM.
    localparam [31:0] TW0 = 32'd214748400;      // 200 kHz
    localparam [31:0] TW1 = 32'd365072280;      // 340 kHz
    localparam [31:0] NS  = 32'd21600;          // 5.4 ms at 4 MSps
    // ---- choose the run from the command line, no editing needed -------
    //   iverilog -DWF_SEL=1 ...     0 LFM  1 geometric  2 stepped  3 Barker-13
    //   iverilog -DWIN_OFF=1 ...    bypass the window (the "no window" run)
`ifndef WF_SEL
  `define WF_SEL 0
`endif
`ifndef WIN_OFF
  `define WIN_OFF 0
`endif
    localparam [2:0]  WF  = `WF_SEL;
    localparam        WO  = `WIN_OFF;
    // geometric rate = ln(340/200) / 21600 samples, in Q32 (same form as adapt_decide.v)
    localparam [31:0] KG  = 32'd105500;

    wire [47:0] fai, sd;  wire signed [47:0] fd;
    wire [31:0] kg, ws, pidx;  wire [15:0] sl, aq, cl;
    wire [2:0]  wt;  wire ps, pa;

    pulse_seq u_seq (.clk(clk), .rst_n(rst_n), .tick(tick), .fast_repeat(1'b0),
        .d_tw_start(TW0), .d_tw_end(TW1), .d_pulse_samples(NS),
        .d_amp_q(16'd32767), .d_wf_type(WF), .d_k_geo(KG),
        .freq_acc_init(fai), .freq_delta(fd), .k_geo(kg), .step_delta(sd),
        .step_len(sl), .win_step(ws), .amp_q(aq), .wf_type(wt), .chip_len(cl),
        .pulse_start(ps), .pulse_active(pa), .ping_index(pidx));

    wire signed [12:0] samp;
    dds_core u_dds (.clk(clk), .rst_n(rst_n), .tick(tick),
        .pulse_start(ps), .pulse_active(pa),
        .freq_acc_init(fai), .freq_delta(fd), .k_geo(kg),
        .step_delta(sd), .step_len(sl), .win_step(ws), .amp_q(aq),
        .wf_type(wt), .chip_len(cl), .window_off(WO[0]), .sample_o(samp));

    integer fh, nsamp = 0;
    initial begin
        fh = $fopen("dds_out.txt", "w");
        #100 rst_n = 1;
    end
    always @(posedge clk) if (tick && pa) begin
        $fwrite(fh, "%0d\n", samp);
        nsamp = nsamp + 1;
    end
    initial begin
        #6_000_000;                              // 6 ms
        $fclose(fh);
        $display("captured %0d samples, ping_index=%0d, waveform=%0d, window_off=%0d",
                 nsamp, pidx, WF, WO);
        $finish;
    end
endmodule
