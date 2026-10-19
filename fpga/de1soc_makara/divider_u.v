// -----------------------------------------------------------------------------
// divider_u.v : unsigned restoring divider, W cycles per result.
// Used exactly twice per pulse, in the gap between pulses, where we have
// milliseconds of slack.  Never put this in the synthesis path.
// -----------------------------------------------------------------------------
module divider_u #(parameter integer W = 48) (
    input  wire          clk,
    input  wire          rst_n,
    input  wire          start,
    input  wire [W-1:0]  num,
    input  wire [W-1:0]  den,
    output reg  [W-1:0]  quo,
    output reg           done,
    output reg           busy
);
    reg [W-1:0] n_reg, d_reg, q_reg;
    reg [W:0]   rem;
    reg [7:0]   i;

    wire [W:0] rem_sh  = {rem[W-1:0], n_reg[W-1]};
    wire [W:0] rem_sub = rem_sh - {1'b0, d_reg};
    wire       fits    = (rem_sub[W] == 1'b0);

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            busy <= 1'b0;  done <= 1'b0;  quo <= {W{1'b0}};  i <= 8'd0;
        end else begin
            done <= 1'b0;
            if (start && !busy) begin
                if (den == {W{1'b0}}) begin
                    quo  <= {W{1'b1}};      // divide-by-zero guard: saturate
                    done <= 1'b1;
                end else begin
                    n_reg <= num;   d_reg <= den;
                    rem   <= {(W+1){1'b0}};
                    q_reg <= {W{1'b0}};
                    i     <= W[7:0];
                    busy  <= 1'b1;
                end
            end else if (busy) begin
                rem   <= fits ? rem_sub : rem_sh;
                q_reg <= {q_reg[W-2:0], fits};
                n_reg <= {n_reg[W-2:0], 1'b0};
                i     <= i - 8'd1;
                if (i == 8'd1) begin
                    busy <= 1'b0;
                    done <= 1'b1;
                    quo  <= {q_reg[W-2:0], fits};
                end
            end
        end
    end
endmodule
