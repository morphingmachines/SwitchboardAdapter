// Test fixture for sim_build's run_cosim: echoes every Switchboard packet from
// io_in to io_out. A packet with dest == 32'hDEAD makes the simulator exit
// with an error, so tests can check that run_cosim notices a dead simulator.
module Loopback (
    input  wire         clock,
    input  wire         reset,
    input  wire [415:0] io_in_data,
    input  wire [31:0]  io_in_dest,
    input  wire         io_in_last,
    input  wire         io_in_valid,
    output wire         io_in_ready,
    output wire [415:0] io_out_data,
    output wire [31:0]  io_out_dest,
    output wire         io_out_last,
    output wire         io_out_valid,
    input  wire         io_out_ready
);
    assign io_out_data  = io_in_data;
    assign io_out_dest  = io_in_dest;
    assign io_out_last  = io_in_last;
    // The kill packet is swallowed, never echoed, so the client is still
    // waiting for a reply when the simulator dies.
    wire kill = io_in_valid && io_in_dest == 32'hDEAD;
    assign io_out_valid = io_in_valid && !kill;
    assign io_in_ready  = io_out_ready || kill;

    always @(posedge clock) begin
        if (!reset && kill) begin
            $fatal(1, "Loopback: got kill packet");
        end
    end
endmodule
