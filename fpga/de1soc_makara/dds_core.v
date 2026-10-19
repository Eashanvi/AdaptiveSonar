// -----------------------------------------------------------------------------
// dds_core.v : the synthesis path.  One sample every 25 clocks, no division,
// no iteration, no branching on data.  Four modulation families:
//
//   WF_LFM   linear frequency sweep   - freq_acc += constant
//   WF_GEOM  geometric (exponential)  - freq_acc += freq_acc * k_geo
//   WF_STEP  stepped staircase        - freq_acc jumps every step_len samples
//   WF_BARK  Barker-13 phase code     - fixed carrier, 180 deg flips per chip
//   WF_CW    plain tone               - fixed carrier
//
// The output is windowed and amplitude-scaled by two DSP multipliers.
// -----------------------------------------------------------------------------
module dds_core (
    input  wire               clk,
    input  wire               rst_n,
    input  wire               tick,          // 4 MHz sample enable
    input  wire               pulse_start,   // single tick, start of pulse
    input  wire               pulse_active,

    // parameters, latched by pulse_seq and stable for the whole pulse
    input  wire [47:0]        freq_acc_init, // tuning word << 16
    input  wire signed [47:0] freq_delta,    // LFM increment per sample
    input  wire [31:0]        k_geo,         // Q32 exponential rate
    input  wire [47:0]        step_delta,    // staircase jump
    input  wire [15:0]        step_len,      // samples per stair
    input  wire [31:0]        win_step,      // 2^32 / pulse_samples
    input  wire [15:0]        amp_q,         // Q1.15
    input  wire [2:0]         wf_type,
    input  wire [15:0]        chip_len,      // samples per Barker chip
    input  wire               window_off,    // bench switch: bypass the window

    output reg  signed [12:0] sample_o
);
    localparam [2:0] WF_LFM  = 3'd0,
                     WF_GEOM = 3'd1,
                     WF_STEP = 3'd2,
                     WF_BARK = 3'd3,
                     WF_CW   = 3'd4;

    // Barker-13, MSB transmitted first.  1 = +1, 0 = -1.
    localparam [12:0] BARKER13 = 13'b1111100110101;

    // ---------------------------------------------------------------- ROMs
    reg signed [15:0] sine_rom [0:1023];
    reg        [15:0] win_rom  [0:1023];
    initial $readmemh("sine_q12.hex",   sine_rom);
    initial $readmemh("window_tukey.hex", win_rom);

    reg signed [15:0] sine_q;
    reg        [15:0] win_q;

    // ------------------------------------------------------- accumulators
    reg [47:0] freq_acc;
    reg [31:0] phase_acc;
    reg [31:0] win_phase;
    reg [15:0] step_cnt;
    reg [15:0] chip_cnt;
    reg [3:0]  chip_idx;
    reg        chip_neg;

    // Exponential increment: df/dn proportional to f.  48 x 32 -> take [79:32].
    wire [79:0] geo_mult = freq_acc * k_geo;
    wire [47:0] geo_inc  = geo_mult[79:32];

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            freq_acc  <= 48'd0;  phase_acc <= 32'd0;  win_phase <= 32'd0;
            step_cnt  <= 16'd0;  chip_cnt  <= 16'd0;
            chip_idx  <= 4'd0;   chip_neg  <= 1'b0;
        end else if (pulse_start) begin
            // pulse_start is a single system-clock pulse from pulse_seq and is
            // NOT aligned to tick, so the load must not be gated by tick --
            // otherwise it is missed 24 times out of 25 and the sweep starts
            // from wherever the accumulator happened to be.
            freq_acc  <= freq_acc_init;
            phase_acc <= 32'd0;
            win_phase <= 32'd0;
            step_cnt  <= 16'd0;
            chip_cnt  <= 16'd0;
            chip_idx  <= 4'd0;
            chip_neg  <= ~BARKER13[12];
        end else if (tick && pulse_active) begin
            // ---- frequency update, one case per modulation family
            case (wf_type)
                WF_LFM:  freq_acc <= freq_acc + freq_delta;
                WF_GEOM: freq_acc <= freq_acc + geo_inc;
                WF_STEP: if (step_cnt == step_len - 1'b1) begin
                             freq_acc <= freq_acc + step_delta;
                             step_cnt <= 16'd0;
                         end else begin
                             step_cnt <= step_cnt + 1'b1;
                         end
                default: freq_acc <= freq_acc;   // WF_BARK, WF_CW
            endcase

            // ---- phase and window always advance
            phase_acc <= phase_acc + freq_acc[47:16];
            win_phase <= win_phase + win_step;

            // ---- Barker chip boundary
            if (wf_type == WF_BARK) begin
                if (chip_cnt == chip_len - 1'b1) begin
                    chip_cnt <= 16'd0;
                    // chip_len is floor(N/13), so the last chip runs a few
                    // samples long.  Clamp rather than index off the end of
                    // the code, which would drive the output to X.
                    if (chip_idx < 4'd12) begin
                        chip_idx <= chip_idx + 1'b1;
                        chip_neg <= ~BARKER13[12 - (chip_idx + 1'b1)];
                    end
                end else begin
                    chip_cnt <= chip_cnt + 1'b1;
                end
            end
        end
    end

    // ------------------------------------------------- four-stage pipeline
    // tick -> address ROMs -> multiply by amplitude -> multiply by window
    reg [2:0] pipe;
    always @(posedge clk or negedge rst_n)
        if (!rst_n) pipe <= 3'd0; else pipe <= {pipe[1:0], tick};

    reg signed [15:0] sine_s;
    reg signed [32:0] prod_a;
    reg        [15:0] win_eff;

    // Q1.12 sine  x  Q1.15 amplitude  x  Q1.15 window, each product shifted
    // back down by 15.  Peak output is +/-2047, which is exactly full scale
    // for the 13-bit signed sample bus.
    wire signed [17:0] s_after_amp = prod_a[32:15];
    wire signed [34:0] prod_w      = s_after_amp * $signed({1'b0, win_eff});

    always @(posedge clk) begin
        if (tick) begin                       // stage 0: registered ROM read
            sine_q <= sine_rom[phase_acc[31:22]];
            win_q  <= win_rom [win_phase[31:22]];
        end
        if (pipe[0]) begin                    // stage 1: Barker sign, window sel
            sine_s  <= chip_neg ? -sine_q : sine_q;
            win_eff <= window_off ? 16'h7FFF : win_q;
        end
        if (pipe[1]) begin                    // stage 2: x amplitude
            prod_a <= sine_s * $signed({1'b0, amp_q});
        end
        if (pipe[2]) begin                    // stage 3: x window, out to DAC
            sample_o <= pulse_active ? prod_w[27:15] : 13'sd0;
        end
    end
endmodule
