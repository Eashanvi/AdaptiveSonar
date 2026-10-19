// -----------------------------------------------------------------------------
// sample_tick.v : divides the 100 MHz system clock down to the 4 MHz sample
// enable.  100 / 25 = 4 exactly, which is the whole reason DIV is 25 and not
// some rounder-looking number.  A non-integer divide gives a permanent
// frequency error that an FFT will find at the worst possible moment.
// -----------------------------------------------------------------------------
module sample_tick #(parameter integer DIV = 25) (
    input  wire clk,
    input  wire rst_n,
    output reg  tick
);
    localparam integer W = 8;
    reg [W-1:0] cnt;

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            cnt  <= {W{1'b0}};
            tick <= 1'b0;
        end else if (cnt == DIV[W-1:0] - 1'b1) begin
            cnt  <= {W{1'b0}};
            tick <= 1'b1;
        end else begin
            cnt  <= cnt + 1'b1;
            tick <= 1'b0;
        end
    end
endmodule
