// ClockGate replacement that never gates (see chipyard.clocking.PassthroughClockGate).
module PassthroughClockGate(
  input  in,
  input  test_en,
  input  en,
  output out
);
  assign out = in;
endmodule
