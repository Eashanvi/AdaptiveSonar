`timescale 1ns/1ps
module tb_final;
    reg CLOCK_50 = 0;  always #10 CLOCK_50 = ~CLOCK_50;
    reg [3:0] KEY = 4'b1110;
    reg [9:0] SW  = 10'b0100000000;
    wire [9:0] LEDR; wire [6:0] HEX0,HEX1,HEX2,HEX3,HEX4,HEX5; wire [35:0] GPIO_0;
    makara_top #(.ENV_DIV_BITS(6)) dut (.CLOCK_50(CLOCK_50), .KEY(KEY), .SW(SW), .LEDR(LEDR),
        .HEX0(HEX0),.HEX1(HEX1),.HEX2(HEX2),.HEX3(HEX3),.HEX4(HEX4),.HEX5(HEX5),.GPIO_0(GPIO_0));
    integer fd, ticks, t0, t1; reg dump = 0; reg prevtrig = 0; integer pulses = 0;
    always @(posedge CLOCK_50) begin
        if (dut.tick) ticks = ticks + 1;
        if (dump && dut.tick && GPIO_0[12]) $fdisplay(fd, "%0d", GPIO_0[7:0]);
        if (GPIO_0[12] && !prevtrig) pulses = pulses + 1;
        prevtrig <= GPIO_0[12];
    end
    task show(input [127:0] label);
        $display("%0s HEX start=%0d end=%0d kHz wf=%0d  HEX0 seg=%b", label, dut.f_start_khz, dut.f_end_khz, LEDR[2:0], HEX0);
    endtask
    initial begin
        ticks = 0;
        fd = $fopen("dac_mid.txt","w");
        #200 KEY = 4'b1111;
        t0 = ticks; #1000000; t1 = ticks;
        $display("tick rate = %0d ticks in 1 ms (expect 4000)", t1-t0);
        SW[3:2]=0; SW[1:0]=3; #15000000; show("deep+clear   ");
        SW[3:2]=3;            #15000000; show("deep+muddy   ");
        SW[3:2]=0; SW[1:0]=0; #15000000; show("shallow+clear");
        SW[3:2]=1; SW[1:0]=1; #15000000; show("mid+mid      ");
        @(negedge GPIO_0[12]); dump = 1; @(posedge GPIO_0[12]); @(negedge GPIO_0[12]); dump = 0;
        $display("pulses seen on JP1 pin15: %0d", pulses);
        $fclose(fd); $finish;
    end
endmodule
