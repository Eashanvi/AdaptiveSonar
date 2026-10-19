// -----------------------------------------------------------------------------
// makara_top.v : THE board design for the Terasic DE1-SoC.  (Final, 26 Sept.)
//
// Same sonar as sonar_top_sw.v, with two changes that make it foolproof:
//   1. NO PLL.  It runs straight from the 50 MHz board clock and uses an
//      exact fractional divider for the 4 MHz sample rate, so every output
//      frequency is identical to the simulations.  Nothing to generate.
//   2. LEDR9 BLINKS about once a second.  If LEDR9 is blinking, THIS design
//      is on the board (the old hello test never blinks anything).
//
//   SW[1:0]  depth      00 = 20 m    01 = 80 m    10 = 150 m   11 = 250 m
//   SW[3:2]  turbidity  00 = 10 %    01 = 45 %    10 = 80 %    11 = 95 %
//   SW[8]    fast repeat (pings close together - use for the scope)
//   SW[9]    window off (the abrupt-edge comparison)
//   KEY[0]   reset
//
//   HEX5..HEX3  decided start frequency in kHz
//   HEX2..HEX0  decided end frequency in kHz
//   LEDR[2:0] waveform   LEDR[4:3] turbidity class   LEDR[6:5] depth class
//   LEDR[7] pulse on     LEDR[8] off                 LEDR[9] heartbeat
//
//   GPIO_0[7:0]  8-bit sample (JP1 pins 1-8, bit 7 = MSB on JP1 pin 8)
//   GPIO_0[12]   high while a ping plays (JP1 pin 15) - scope trigger
// -----------------------------------------------------------------------------
module makara_top #(
    // How often the decision path re-evaluates: once every 2^ENV_DIV_BITS
    // clocks.  19 gives about 190 updates a second on hardware, which is far
    // faster than a hand can turn a switch.  Simulation overrides this with a
    // small value so a testbench does not have to run for milliseconds.
    parameter integer ENV_DIV_BITS = 19
) (
    input  wire        CLOCK_50,
    input  wire [3:0]  KEY,
    input  wire [9:0]  SW,
    output wire [9:0]  LEDR,
    output wire [6:0]  HEX0, HEX1, HEX2, HEX3, HEX4, HEX5,
    inout  wire [35:0] GPIO_0
);
    // ------------------------------------------------------------ clocking
    wire clk   = CLOCK_50;
    wire rst_n = KEY[0];

    wire tick;
    sample_tick_frac #(.NUM(2), .DEN(25)) u_tick (.clk(clk), .rst_n(rst_n), .tick(tick));

    // heartbeat: bit 25 of a 50 MHz counter toggles about every 0.67 s
    reg [25:0] hb;
    always @(posedge clk or negedge rst_n)
        if (!rst_n) hb <= 26'd0; else hb <= hb + 1'b1;

    // ------------------------------------------- switches to Q16.16 values
    // Q16.16 means the integer part sits in the top 16 bits, so a value of
    // N metres is simply N << 16.
    reg [31:0] depth_m, turb_pct;
    always @(*) begin
        case (SW[1:0])
            2'd0: depth_m = 32'd20  << 16;
            2'd1: depth_m = 32'd80  << 16;
            2'd2: depth_m = 32'd150 << 16;
            default: depth_m = 32'd250 << 16;
        endcase
        case (SW[3:2])
            2'd0: turb_pct = 32'd10 << 16;
            2'd1: turb_pct = 32'd45 << 16;
            2'd2: turb_pct = 32'd80 << 16;
            default: turb_pct = 32'd95 << 16;
        endcase
    end

    // Fixed temperature and salinity: 15 C and 35 PSU, typical coastal water.
    localparam [31:0] TEMP_C  = 32'd15 << 16;
    localparam [31:0] SAL_PSU = 32'd35 << 16;

    // A slow tick so the decision path re-evaluates about 200 times a second.
    reg [ENV_DIV_BITS-1:0] slow_cnt;
    wire env_update = (slow_cnt == {ENV_DIV_BITS{1'b0}});
    always @(posedge clk or negedge rst_n)
        if (!rst_n) slow_cnt <= {ENV_DIV_BITS{1'b0}};
        else        slow_cnt <= slow_cnt + 1'b1;

    wire [1:0] depth_state, turb_state;
    env_state u_state (.clk(clk), .rst_n(rst_n), .update(env_update),
        .depth_m(depth_m), .turb_pct(turb_pct),
        .depth_state(depth_state), .turb_state(turb_state));

    // -------------------------------------------------------- decision path
    wire [9:0]  f_start_khz, f_end_khz;
    wire [15:0] c_q124, alpha_q88;

    phys_rom u_phys (.clk(clk),
        .temp_c(TEMP_C), .sal_psu(SAL_PSU), .depth_m(depth_m),
        .f_khz(f_start_khz), .c_q124(c_q124), .alpha_q88(alpha_q88));

    wire [31:0] d_tw_start, d_tw_end, d_pulse_samples, d_k_geo;
    wire [15:0] d_amp_q;
    wire [2:0]  d_wf_type;

    adapt_decide u_adapt (.clk(clk), .rst_n(rst_n), .update(env_update),
        .depth_state(depth_state), .turb_state(turb_state),
        .alpha_q88(alpha_q88),
        .tw_start(d_tw_start), .tw_end(d_tw_end),
        .pulse_samples(d_pulse_samples), .amp_q(d_amp_q),
        .wf_type(d_wf_type), .k_geo(d_k_geo),
        .f_start_khz_o(f_start_khz), .f_end_khz_o(f_end_khz));

    // ------------------------------------------------------ synthesis path
    wire [47:0]        freq_acc_init, step_delta;
    wire signed [47:0] freq_delta;
    wire [31:0]        k_geo, win_step, ping_index;
    wire [15:0]        step_len, amp_q, chip_len;
    wire [2:0]         wf_type;
    wire               pulse_start, pulse_active;

    pulse_seq u_seq (.clk(clk), .rst_n(rst_n), .tick(tick),
        .fast_repeat(SW[8]),
        .d_tw_start(d_tw_start), .d_tw_end(d_tw_end),
        .d_pulse_samples(d_pulse_samples), .d_amp_q(d_amp_q),
        .d_wf_type(d_wf_type), .d_k_geo(d_k_geo),
        .freq_acc_init(freq_acc_init), .freq_delta(freq_delta),
        .k_geo(k_geo), .step_delta(step_delta), .step_len(step_len),
        .win_step(win_step), .amp_q(amp_q), .wf_type(wf_type),
        .chip_len(chip_len),
        .pulse_start(pulse_start), .pulse_active(pulse_active),
        .ping_index(ping_index));

    wire signed [12:0] sample;
    dds_core u_dds (.clk(clk), .rst_n(rst_n), .tick(tick),
        .pulse_start(pulse_start), .pulse_active(pulse_active),
        .freq_acc_init(freq_acc_init), .freq_delta(freq_delta),
        .k_geo(k_geo), .step_delta(step_delta), .step_len(step_len),
        .win_step(win_step), .amp_q(amp_q), .wf_type(wf_type),
        .chip_len(chip_len), .window_off(SW[9]),
        .sample_o(sample));

    wire [7:0] dac_byte;
    wire       trig;
    dac_r2r u_dac (.clk(clk), .rst_n(rst_n), .tick(tick),
        .sample_i(sample), .pulse_active(pulse_active),
        .dac_o(dac_byte), .trig_o(trig));

    assign GPIO_0[7:0]   = dac_byte;
    assign GPIO_0[11:8]  = 4'b0000;
    assign GPIO_0[12]    = trig;
    assign GPIO_0[35:13] = {23{1'bz}};     // unused header pins left floating

    // ------------------------------------------------------ status display
    wire [3:0] f0_h = (f_start_khz / 100) % 10;
    wire [3:0] f0_t = (f_start_khz /  10) % 10;
    wire [3:0] f0_u =  f_start_khz        % 10;
    wire [3:0] f1_h = (f_end_khz   / 100) % 10;
    wire [3:0] f1_t = (f_end_khz   /  10) % 10;
    wire [3:0] f1_u =  f_end_khz          % 10;

    seg7 s5 (.d(f0_h), .seg(HEX5));  seg7 s4 (.d(f0_t), .seg(HEX4));
    seg7 s3 (.d(f0_u), .seg(HEX3));  seg7 s2 (.d(f1_h), .seg(HEX2));
    seg7 s1 (.d(f1_t), .seg(HEX1));  seg7 s0 (.d(f1_u), .seg(HEX0));

    assign LEDR[2:0] = wf_type;
    assign LEDR[4:3] = turb_state;
    assign LEDR[6:5] = depth_state;
    assign LEDR[7]   = pulse_active;
    assign LEDR[8]   = 1'b0;
    assign LEDR[9]   = hb[25];
endmodule

// Common-anode seven-segment decoder: a 0 lights a segment.
// Included here so that sonar_top_sw.v is self-contained -- use this file
// INSTEAD of sonar_top.v, not alongside it.
module seg7 (input wire [3:0] d, output reg [6:0] seg);
    always @(*) case (d)
        4'h0: seg = 7'b1000000;  4'h1: seg = 7'b1111001;
        4'h2: seg = 7'b0100100;  4'h3: seg = 7'b0110000;
        4'h4: seg = 7'b0011001;  4'h5: seg = 7'b0010010;
        4'h6: seg = 7'b0000010;  4'h7: seg = 7'b1111000;
        4'h8: seg = 7'b0000000;  4'h9: seg = 7'b0010000;
        default: seg = 7'b0111111;
    endcase
endmodule
