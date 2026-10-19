// -----------------------------------------------------------------------------
// dac_r2r.v : signed sample to offset-binary byte, plus the scope trigger.
//
// trig goes high for exactly the duration of the pulse.  Wire it to an
// oscilloscope's external trigger input and every capture lands on the same
// point of the chirp.  It costs one pin and saves hours.
// -----------------------------------------------------------------------------
module dac_r2r (
    input  wire               clk,
    input  wire               rst_n,
    input  wire               tick,
    input  wire signed [12:0] sample_i,
    input  wire               pulse_active,
    output reg  [7:0]         dac_o,
    output reg                trig_o
);
    wire [12:0] offset = sample_i + 13'sd4096;   // -4096..4095  ->  0..8191

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            dac_o  <= 8'd128;        // mid-scale: the rail the filter sits at
            trig_o <= 1'b0;
        end else if (tick) begin
            dac_o  <= pulse_active ? offset[12:5] : 8'd128;
            trig_o <= pulse_active;
        end
    end
endmodule
