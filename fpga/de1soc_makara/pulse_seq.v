// -----------------------------------------------------------------------------
// pulse_seq.v : pulse timing, and the parameter latch that isolates the fast
// path from the slow one.
//
// A knob turned halfway through a chirp must not corrupt that chirp.  So the
// decision registers are sampled exactly once, in S_LATCH, and nothing the
// decision path does afterwards can reach the DDS until the next pulse.
//
// The two divides live here, in the gap, where there are milliseconds of slack.
// -----------------------------------------------------------------------------
module pulse_seq (
    input  wire        clk,
    input  wire        rst_n,
    input  wire        tick,
    input  wire        fast_repeat,       // bench switch: shorten the gap

    // live from adapt_decide
    input  wire [31:0] d_tw_start,
    input  wire [31:0] d_tw_end,
    input  wire [31:0] d_pulse_samples,
    input  wire [15:0] d_amp_q,
    input  wire [2:0]  d_wf_type,
    input  wire [31:0] d_k_geo,

    // frozen for the duration of one pulse
    output reg  [47:0]        freq_acc_init,
    output reg  signed [47:0] freq_delta,
    output reg  [31:0]        k_geo,
    output reg  [47:0]        step_delta,
    output reg  [15:0]        step_len,
    output reg  [31:0]        win_step,
    output reg  [15:0]        amp_q,
    output reg  [2:0]         wf_type,
    output reg  [15:0]        chip_len,

    output reg         pulse_start,
    output reg         pulse_active,
    output reg  [31:0] ping_index
);
    localparam [2:0] S_IDLE = 3'd0, S_LATCH = 3'd1, S_DIV_WIN = 3'd2,
                     S_DIV_FRQ = 3'd3, S_PULSE = 3'd4, S_GAP = 3'd5;

    reg [2:0]  st;
    reg [31:0] n_samples, samp_cnt, gap_cnt, gap_len;
    reg [47:0] span_sh;                 // (tw_end - tw_start) << 16
    reg [31:0] l_tw_start;

    reg        div_start;
    reg [47:0] div_num, div_den;
    wire [47:0] div_quo;
    wire        div_done, div_busy;

    divider_u #(.W(48)) u_div (
        .clk(clk), .rst_n(rst_n), .start(div_start),
        .num(div_num), .den(div_den),
        .quo(div_quo), .done(div_done), .busy(div_busy)
    );

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            st <= S_IDLE; pulse_start <= 1'b0; pulse_active <= 1'b0;
            ping_index <= 32'd0; div_start <= 1'b0;
        end else begin
            pulse_start <= 1'b0;
            div_start   <= 1'b0;

            case (st)
            // ------------------------------------------------------------
            S_IDLE: st <= S_LATCH;

            // ---- freeze every decision register in one clock ------------
            S_LATCH: begin
                l_tw_start <= d_tw_start;
                n_samples  <= d_pulse_samples;
                amp_q      <= d_amp_q;
                wf_type    <= d_wf_type;
                k_geo      <= d_k_geo;
                span_sh    <= {(d_tw_end - d_tw_start), 16'd0};

                freq_acc_init <= {d_tw_start, 16'd0};
                step_len      <= d_pulse_samples[18:3];          // N / 8
                step_delta    <= {(d_tw_end - d_tw_start), 16'd0} >> 3;
                // chip_len = N / 13, as a multiply by 1/13 in Q16
                chip_len      <= (d_pulse_samples * 32'd5041) >> 16;

                // first divide: win_step = 2^32 / N
                div_num   <= 48'h0001_0000_0000;
                div_den   <= {16'd0, d_pulse_samples};
                div_start <= 1'b1;
                st        <= S_DIV_WIN;
            end

            S_DIV_WIN: if (div_done) begin
                win_step  <= div_quo[31:0];
                // second divide: freq_delta = ((tw_end - tw_start) << 16) / N
                div_num   <= span_sh;
                div_den   <= {16'd0, n_samples};
                div_start <= 1'b1;
                st        <= S_DIV_FRQ;
            end

            S_DIV_FRQ: if (div_done) begin
                freq_delta   <= $signed(div_quo);
                samp_cnt     <= 32'd0;
                gap_len      <= fast_repeat ? (n_samples << 1)      // 50 % duty
                                            : (n_samples * 32'd19); //  5 % duty
                pulse_start  <= 1'b1;
                pulse_active <= 1'b1;
                ping_index   <= ping_index + 32'd1;
                st           <= S_PULSE;
            end

            // ---- streaming --------------------------------------------
            S_PULSE: if (tick) begin
                if (samp_cnt == n_samples - 32'd1) begin
                    pulse_active <= 1'b0;
                    gap_cnt      <= 32'd0;
                    st           <= S_GAP;
                end else begin
                    samp_cnt <= samp_cnt + 32'd1;
                end
            end

            S_GAP: if (tick) begin
                if (gap_cnt == gap_len - 32'd1) st <= S_LATCH;
                else                            gap_cnt <= gap_cnt + 32'd1;
            end

            default: st <= S_IDLE;
            endcase
        end
    end
endmodule
