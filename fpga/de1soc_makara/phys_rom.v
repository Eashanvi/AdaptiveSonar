// -----------------------------------------------------------------------------
// phys_rom.v : speed of sound and absorption, by table lookup.
//
// The tables come out of gen_roms.py, which evaluates the same Mackenzie (1981)
// and Francois & Garrison (1982) models the STM32 firmware implements in C.
// One source of truth, two platforms.
//
// Index mapping uses shift-and-multiply approximations of the axis scaling so
// that no divider is needed.  The residual index error is under half a step,
// which is smaller than the quantisation already accepted in the table itself.
// -----------------------------------------------------------------------------
module phys_rom (
    input  wire        clk,
    input  wire [31:0] temp_c,        // Q16.16, 0..35
    input  wire [31:0] sal_psu,       // Q16.16, 0..40
    input  wire [31:0] depth_m,       // Q16.16, 0..300
    input  wire [9:0]  f_khz,         // 80..550
    output reg  [15:0] c_q124,        // speed of sound, Q12.4 m/s
    output reg  [15:0] alpha_q88      // absorption,     Q8.8 dB/km
);
    reg [15:0] ss_rom [0:8191];
    reg [15:0] ab_rom [0:4095];
    initial $readmemh("sound_speed.hex", ss_rom);
    initial $readmemh("absorb.hex",      ab_rom);

    // Integer parts of the engineering values.
    wire [15:0] t_i = temp_c [31:16];
    wire [15:0] s_i = sal_psu[31:16];
    wire [15:0] d_i = depth_m[31:16];

    // Axis scaling.  32 steps over 0..35 C   -> idx = T * 910 / 1024
    //                16 steps over 0..40 PSU -> idx = S * 400 / 1024
    //                16 steps over 0..300 m  -> idx = D *  54 / 1024
    wire [25:0] t_m = t_i * 16'd910;
    wire [25:0] s_m = s_i * 16'd400;
    wire [25:0] d_m = d_i * 16'd54;

    wire [4:0] t_ix = (t_m[25:10] > 16'd31) ? 5'd31 : t_m[14:10];
    wire [3:0] s_ix = (s_m[25:10] > 16'd15) ? 4'd15 : s_m[13:10];
    wire [3:0] d_ix = (d_m[25:10] > 16'd15) ? 4'd15 : d_m[13:10];

    // 64 steps over 80..550 kHz -> idx = (f - 80) * 139 / 1024
    wire [9:0]  f_rel = (f_khz > 10'd80) ? (f_khz - 10'd80) : 10'd0;
    wire [17:0] f_m   = f_rel * 8'd139;
    wire [5:0]  f_ix  = (f_m[17:10] > 8'd63) ? 6'd63 : f_m[15:10];
    wire [1:0]  dq_ix = d_ix[3:2];

    always @(posedge clk) begin
        c_q124    <= ss_rom[{t_ix, s_ix, d_ix}];
        alpha_q88 <= ab_rom[{f_ix, t_ix[4:1], dq_ix}];
    end
endmodule
