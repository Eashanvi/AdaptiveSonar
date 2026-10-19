// -----------------------------------------------------------------------------
// adapt_decide.v : the decision table.
//
// Frequencies are carried in kHz and pulse lengths in microseconds, because
// those are the units the physics is argued in and the units a judge will ask
// about.  The conversion to tuning words happens here, once, with a single
// constant:
//
//     tuning word = f_khz * 2^32 / f_s(kHz) = f_khz * 2^32 / 4000
//                 = f_khz * 1073741.824  ~  f_khz * 1073742   (0.16 ppm error)
//
// Waveform selection reproduces adapt_select_waveform() from the MCU firmware,
// minus the hyperbolic case (the MCU covers that one; see the Demo tab).
// -----------------------------------------------------------------------------
module adapt_decide (
    input  wire        clk,
    input  wire        rst_n,
    input  wire        update,
    input  wire [1:0]  depth_state,
    input  wire [1:0]  turb_state,
    input  wire [15:0] alpha_q88,        // absorption, Q8.8 dB/km, from phys_rom

    output reg  [31:0] tw_start,
    output reg  [31:0] tw_end,
    output reg  [31:0] pulse_samples,
    output reg  [15:0] amp_q,
    output reg  [2:0]  wf_type,
    output reg  [31:0] k_geo,
    output reg  [9:0]  f_start_khz_o,    // for the seven-segment display
    output reg  [9:0]  f_end_khz_o
);
    localparam [1:0] DEPTH_SHALLOW = 2'd0, DEPTH_DEEP = 2'd2;
    localparam [1:0] TURB_LOW      = 2'd0, TURB_HIGH  = 2'd2;
    localparam [2:0] WF_LFM = 3'd0, WF_GEOM = 3'd1, WF_STEP = 3'd2,
                     WF_BARK = 3'd3, WF_CW  = 3'd4;

    localparam [20:0] TW_PER_KHZ = 21'd1073742;

    reg [9:0]  f0_khz, f1_khz;
    reg [15:0] pulse_us;

    // Absorption trim: every 10 dB/km of loss pulls the top of the sweep down
    // by 8 kHz, because energy above that point will not come back.
    wire [9:0] trim = {2'd0, alpha_q88[15:8]} * 10'd8 / 10'd10;

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            f0_khz <= 10'd250; f1_khz <= 10'd390; pulse_us <= 16'd4500;
            amp_q  <= 16'd27853; wf_type <= WF_LFM; k_geo <= 32'd0;
        end else if (update) begin
            // ------------------------------------------- base band by turbidity
            case (turb_state)
                TURB_LOW:  begin f0_khz <= 10'd300; f1_khz <= 10'd445;
                                 pulse_us <= 16'd3600; amp_q <= 16'd22938;   // 0.70
                                 k_geo <= 32'd0; end
                TURB_HIGH: begin f0_khz <= 10'd200; f1_khz <= 10'd340;
                                 pulse_us <= 16'd5400; amp_q <= 16'd32767;   // 1.00
                                 k_geo <= 32'd105500; end                    // ln(1.70)/21600
                default:   begin f0_khz <= 10'd250; f1_khz <= 10'd390;
                                 pulse_us <= 16'd4500; amp_q <= 16'd27853;   // 0.85
                                 k_geo <= 32'd106300; end                    // ln(1.56)/18000
            endcase

            // --------------------------------------------- waveform selection
            if (depth_state == DEPTH_SHALLOW && turb_state == TURB_LOW)
                wf_type <= WF_BARK;      // sharpest range cut, rectangular envelope
            else if (depth_state == DEPTH_DEEP && turb_state == TURB_HIGH)
                wf_type <= WF_GEOM;      // spend more time low, where the channel passes
            else if (depth_state == DEPTH_DEEP && turb_state == TURB_LOW)
                wf_type <= WF_STEP;      // constant-envelope burst, cheapest in transit
            else
                wf_type <= WF_LFM;       // the workhorse
        end
    end

    // Registered outputs, one cycle behind the decision above.
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            tw_start <= 32'd0; tw_end <= 32'd0; pulse_samples <= 32'd18000;
            f_start_khz_o <= 10'd0; f_end_khz_o <= 10'd0;
        end else begin
            f_start_khz_o <= f0_khz;
            f_end_khz_o   <= (f1_khz > trim) ? (f1_khz - trim) : f0_khz + 10'd20;
            tw_start      <= f0_khz * TW_PER_KHZ;
            tw_end        <= f_end_khz_o * TW_PER_KHZ;
            pulse_samples <= pulse_us * 32'd4;      // 4 samples per microsecond
        end
    end
endmodule
