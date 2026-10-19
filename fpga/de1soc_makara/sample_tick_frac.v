// -----------------------------------------------------------------------------
// sample_tick_frac.v : an EXACT 4 MHz average sample enable from the 50 MHz
// board clock, with no PLL.
//
// 50 / 4 = 12.5, which a plain counter cannot do.  So we add 2 every clock
// and fire when the total reaches 25: that is exactly 2 ticks per 25 clocks
// = 4 MHz on average.  Ticks alternate 12 and 13 clocks apart (240/260 ns),
// which the reconstruction filter never notices, and every frequency the
// design decides comes out exactly right.
// -----------------------------------------------------------------------------
module sample_tick_frac #(parameter integer NUM = 2, parameter integer DEN = 25) (
    input  wire clk,
    input  wire rst_n,
    output reg  tick
);
    reg [7:0] acc;
    wire [7:0] nxt = acc + NUM[7:0];
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            acc  <= 8'd0;
            tick <= 1'b0;
        end else if (nxt >= DEN[7:0]) begin
            acc  <= nxt - DEN[7:0];
            tick <= 1'b1;
        end else begin
            acc  <= nxt;
            tick <= 1'b0;
        end
    end
endmodule
