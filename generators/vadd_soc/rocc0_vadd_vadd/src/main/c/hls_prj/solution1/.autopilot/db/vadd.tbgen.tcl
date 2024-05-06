set C_TypeInfoList {{ 
"vadd" : [[], {"return": [[], {"scalar": "int"}] }, [{"ExternC" : 0}], [ {"length_a": [[],{ "pointer":  {"scalar": "int"}}] }, {"b_c": [[],{ "pointer":  {"scalar": "int"}}] }],[],""]
}}
set moduleName vadd
set isTaskLevelControl 1
set isCombinational 0
set isDatapathOnly 0
set isPipelined 0
set pipeline_type none
set FunctionProtocol ap_ctrl_hs
set isOneStateSeq 0
set ProfileFlag 0
set StallSigGenFlag 0
set isEnableWaveformDebug 1
set C_modelName {vadd}
set C_modelType { int 32 }
set C_modelArgList {
	{ length_a int 32 regular {bus 0}  }
	{ b_c int 32 regular {bus 2}  }
}
set C_modelArgMapList {[ 
	{ "Name" : "length_a", "interface" : "bus", "bitwidth" : 32, "direction" : "READONLY", "bitSlice":[{"low":0,"up":31,"cElement": [{"cName": "length_a","cData": "int","bit_use": { "low": 0,"up": 31},"cArray": [{"low" : 0,"up" : 9,"step" : 1}]}]}]} , 
 	{ "Name" : "b_c", "interface" : "bus", "bitwidth" : 32, "direction" : "READWRITE", "bitSlice":[{"low":0,"up":31,"cElement": [{"cName": "b_c","cData": "int","bit_use": { "low": 0,"up": 31},"cArray": [{"low" : 0,"up" : 9,"step" : 1}]}]}]} , 
 	{ "Name" : "ap_return", "interface" : "wire", "bitwidth" : 32,"bitSlice":[{"low":0,"up":31,"cElement": [{"cName": "return","cData": "int","bit_use": { "low": 0,"up": 31},"cArray": [{"low" : 0,"up" : 1,"step" : 0}]}]}]} ]}
# RTL Port declarations: 
set portNum 25
set portList { 
	{ ap_clk sc_in sc_logic 1 clock -1 } 
	{ ap_rst sc_in sc_logic 1 reset -1 active_high_sync } 
	{ ap_start sc_in sc_logic 1 start -1 } 
	{ ap_done sc_out sc_logic 1 predone -1 } 
	{ ap_idle sc_out sc_logic 1 done -1 } 
	{ ap_ready sc_out sc_logic 1 ready -1 } 
	{ length_a_req_din sc_out sc_logic 1 signal 0 } 
	{ length_a_req_full_n sc_in sc_logic 1 signal 0 } 
	{ length_a_req_write sc_out sc_logic 1 signal 0 } 
	{ length_a_rsp_empty_n sc_in sc_logic 1 signal 0 } 
	{ length_a_rsp_read sc_out sc_logic 1 signal 0 } 
	{ length_a_address sc_out sc_lv 32 signal 0 } 
	{ length_a_datain sc_in sc_lv 32 signal 0 } 
	{ length_a_dataout sc_out sc_lv 32 signal 0 } 
	{ length_a_size sc_out sc_lv 32 signal 0 } 
	{ b_c_req_din sc_out sc_logic 1 signal 1 } 
	{ b_c_req_full_n sc_in sc_logic 1 signal 1 } 
	{ b_c_req_write sc_out sc_logic 1 signal 1 } 
	{ b_c_rsp_empty_n sc_in sc_logic 1 signal 1 } 
	{ b_c_rsp_read sc_out sc_logic 1 signal 1 } 
	{ b_c_address sc_out sc_lv 32 signal 1 } 
	{ b_c_datain sc_in sc_lv 32 signal 1 } 
	{ b_c_dataout sc_out sc_lv 32 signal 1 } 
	{ b_c_size sc_out sc_lv 32 signal 1 } 
	{ ap_return sc_out sc_lv 32 signal -1 } 
}
set NewPortList {[ 
	{ "name": "ap_clk", "direction": "in", "datatype": "sc_logic", "bitwidth":1, "type": "clock", "bundle":{"name": "ap_clk", "role": "default" }} , 
 	{ "name": "ap_rst", "direction": "in", "datatype": "sc_logic", "bitwidth":1, "type": "reset", "bundle":{"name": "ap_rst", "role": "default" }} , 
 	{ "name": "ap_start", "direction": "in", "datatype": "sc_logic", "bitwidth":1, "type": "start", "bundle":{"name": "ap_start", "role": "default" }} , 
 	{ "name": "ap_done", "direction": "out", "datatype": "sc_logic", "bitwidth":1, "type": "predone", "bundle":{"name": "ap_done", "role": "default" }} , 
 	{ "name": "ap_idle", "direction": "out", "datatype": "sc_logic", "bitwidth":1, "type": "done", "bundle":{"name": "ap_idle", "role": "default" }} , 
 	{ "name": "ap_ready", "direction": "out", "datatype": "sc_logic", "bitwidth":1, "type": "ready", "bundle":{"name": "ap_ready", "role": "default" }} , 
 	{ "name": "length_a_req_din", "direction": "out", "datatype": "sc_logic", "bitwidth":1, "type": "signal", "bundle":{"name": "length_a", "role": "req_din" }} , 
 	{ "name": "length_a_req_full_n", "direction": "in", "datatype": "sc_logic", "bitwidth":1, "type": "signal", "bundle":{"name": "length_a", "role": "req_full_n" }} , 
 	{ "name": "length_a_req_write", "direction": "out", "datatype": "sc_logic", "bitwidth":1, "type": "signal", "bundle":{"name": "length_a", "role": "req_write" }} , 
 	{ "name": "length_a_rsp_empty_n", "direction": "in", "datatype": "sc_logic", "bitwidth":1, "type": "signal", "bundle":{"name": "length_a", "role": "rsp_empty_n" }} , 
 	{ "name": "length_a_rsp_read", "direction": "out", "datatype": "sc_logic", "bitwidth":1, "type": "signal", "bundle":{"name": "length_a", "role": "rsp_read" }} , 
 	{ "name": "length_a_address", "direction": "out", "datatype": "sc_lv", "bitwidth":32, "type": "signal", "bundle":{"name": "length_a", "role": "address" }} , 
 	{ "name": "length_a_datain", "direction": "in", "datatype": "sc_lv", "bitwidth":32, "type": "signal", "bundle":{"name": "length_a", "role": "datain" }} , 
 	{ "name": "length_a_dataout", "direction": "out", "datatype": "sc_lv", "bitwidth":32, "type": "signal", "bundle":{"name": "length_a", "role": "dataout" }} , 
 	{ "name": "length_a_size", "direction": "out", "datatype": "sc_lv", "bitwidth":32, "type": "signal", "bundle":{"name": "length_a", "role": "size" }} , 
 	{ "name": "b_c_req_din", "direction": "out", "datatype": "sc_logic", "bitwidth":1, "type": "signal", "bundle":{"name": "b_c", "role": "req_din" }} , 
 	{ "name": "b_c_req_full_n", "direction": "in", "datatype": "sc_logic", "bitwidth":1, "type": "signal", "bundle":{"name": "b_c", "role": "req_full_n" }} , 
 	{ "name": "b_c_req_write", "direction": "out", "datatype": "sc_logic", "bitwidth":1, "type": "signal", "bundle":{"name": "b_c", "role": "req_write" }} , 
 	{ "name": "b_c_rsp_empty_n", "direction": "in", "datatype": "sc_logic", "bitwidth":1, "type": "signal", "bundle":{"name": "b_c", "role": "rsp_empty_n" }} , 
 	{ "name": "b_c_rsp_read", "direction": "out", "datatype": "sc_logic", "bitwidth":1, "type": "signal", "bundle":{"name": "b_c", "role": "rsp_read" }} , 
 	{ "name": "b_c_address", "direction": "out", "datatype": "sc_lv", "bitwidth":32, "type": "signal", "bundle":{"name": "b_c", "role": "address" }} , 
 	{ "name": "b_c_datain", "direction": "in", "datatype": "sc_lv", "bitwidth":32, "type": "signal", "bundle":{"name": "b_c", "role": "datain" }} , 
 	{ "name": "b_c_dataout", "direction": "out", "datatype": "sc_lv", "bitwidth":32, "type": "signal", "bundle":{"name": "b_c", "role": "dataout" }} , 
 	{ "name": "b_c_size", "direction": "out", "datatype": "sc_lv", "bitwidth":32, "type": "signal", "bundle":{"name": "b_c", "role": "size" }} , 
 	{ "name": "ap_return", "direction": "out", "datatype": "sc_lv", "bitwidth":32, "type": "signal", "bundle":{"name": "ap_return", "role": "default" }}  ]}

set RtlHierarchyInfo {[
	{"ID" : "0", "Level" : "0", "Path" : "`AUTOTB_DUT_INST", "Parent" : "",
		"CDFG" : "vadd",
		"Protocol" : "ap_ctrl_hs",
		"ControlExist" : "1", "ap_start" : "1", "ap_ready" : "1", "ap_done" : "1", "ap_continue" : "0", "ap_idle" : "1",
		"Pipeline" : "None", "UnalignedPipeline" : "0", "RewindPipeline" : "0", "ProcessNetwork" : "0",
		"II" : "0",
		"VariableLatency" : "1", "ExactLatency" : "-1", "EstimateLatencyMin" : "-1", "EstimateLatencyMax" : "-1",
		"Combinational" : "0",
		"Datapath" : "0",
		"ClockEnable" : "0",
		"HasSubDataflow" : "0",
		"InDataflowNetwork" : "0",
		"HasNonBlockingOperation" : "0",
		"Port" : [
			{"Name" : "length_a", "Type" : "Bus", "Direction" : "I"},
			{"Name" : "b_c", "Type" : "Bus", "Direction" : "IO"}]}]}


set ArgLastReadFirstWriteLatency {
	vadd {
		length_a {Type I LastRead 42 FirstWrite -1}
		b_c {Type IO LastRead 44 FirstWrite 9}}}

set hasDtUnsupportedChannel 0

set PerformanceInfo {[
	{"Name" : "Latency", "Min" : "-1", "Max" : "-1"}
	, {"Name" : "Interval", "Min" : "0", "Max" : "0"}
]}

set PipelineEnableSignalInfo {[
]}

set Spec2ImplPortList { 
	length_a { ap_bus {  { length_a_req_din fifo_data 1 1 }  { length_a_req_full_n fifo_status 0 1 }  { length_a_req_write fifo_update 1 1 }  { length_a_rsp_empty_n fifo_status 0 1 }  { length_a_rsp_read fifo_update 1 1 }  { length_a_address unknown 1 32 }  { length_a_datain unknown 0 32 }  { length_a_dataout unknown 1 32 }  { length_a_size unknown 1 32 } } }
	b_c { ap_bus {  { b_c_req_din fifo_data 1 1 }  { b_c_req_full_n fifo_status 0 1 }  { b_c_req_write fifo_update 1 1 }  { b_c_rsp_empty_n fifo_status 0 1 }  { b_c_rsp_read fifo_update 1 1 }  { b_c_address unknown 1 32 }  { b_c_datain unknown 0 32 }  { b_c_dataout unknown 1 32 }  { b_c_size unknown 1 32 } } }
}

set busDeadlockParameterList { 
}

# RTL port scheduling information:
set fifoSchedulingInfoList { 
}

# RTL bus port read request latency information:
set busReadReqLatencyList { 
	{ length_a 1 }
	{ b_c 1 }
}

# RTL bus port write response latency information:
set busWriteResLatencyList { 
	{ length_a 1 }
	{ b_c 1 }
}

# RTL array port load latency information:
set memoryLoadLatencyList { 
}
