// -----------------------------------------------------------------------------
// env_state.v : Schmitt-trigger classification of depth and turbidity.
// The hysteresis is not decoration.  Without it, a knob sitting exactly on a
// threshold makes the waveform type oscillate every ping, which looks like a
// bug on the bench and sounds like one on the transducer.  Thresholds and
// hysteresis widths match adaptation.c on the STM32 exactly.
// -----------------------------------------------------------------------------
module env_state (
    input  wire        clk,
    input  wire        rst_n,
    input  wire        update,            // one pulse per decision cycle
    input  wire [31:0] depth_m,           // Q16.16
    input  wire [31:0] turb_pct,          // Q16.16
    output reg  [1:0]  depth_state,
    output reg  [1:0]  turb_state
);
    localparam [1:0] DEPTH_SHALLOW = 2'd0, DEPTH_MEDIUM = 2'd1, DEPTH_DEEP = 2'd2;
    localparam [1:0] TURB_LOW      = 2'd0, TURB_MEDIUM  = 2'd1, TURB_HIGH  = 2'd2;

    // Q16.16 constants.  50 m, 120 m, +/- 8 m.  30 %, 70 %, +/- 5 %.
    localparam [31:0] D_LO = 32'd3276800,  D_HI = 32'd7864320,  D_H = 32'd524288;
    localparam [31:0] T_LO = 32'd1966080,  T_HI = 32'd4587520,  T_H = 32'd327680;

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            depth_state <= DEPTH_MEDIUM;
            turb_state  <= TURB_MEDIUM;
        end else if (update) begin
            case (depth_state)
                DEPTH_SHALLOW: if (depth_m > D_LO + D_H) depth_state <= DEPTH_MEDIUM;
                DEPTH_DEEP:    if (depth_m < D_HI - D_H) depth_state <= DEPTH_MEDIUM;
                default: begin
                    if      (depth_m < D_LO - D_H) depth_state <= DEPTH_SHALLOW;
                    else if (depth_m > D_HI + D_H) depth_state <= DEPTH_DEEP;
                end
            endcase
            case (turb_state)
                TURB_LOW:  if (turb_pct > T_LO + T_H) turb_state <= TURB_MEDIUM;
                TURB_HIGH: if (turb_pct < T_HI - T_H) turb_state <= TURB_MEDIUM;
                default: begin
                    if      (turb_pct < T_LO - T_H) turb_state <= TURB_LOW;
                    else if (turb_pct > T_HI + T_H) turb_state <= TURB_HIGH;
                end
            endcase
        end
    end
endmodule
