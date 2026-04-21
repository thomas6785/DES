proc start_step { step } {
  set stopFile ".stop.rst"
  if {[file isfile .stop.rst]} {
    puts ""
    puts "*** Halting run - EA reset detected ***"
    puts ""
    puts ""
    return -code error
  }
  set beginFile ".$step.begin.rst"
  set platform "$::tcl_platform(platform)"
  set user "$::tcl_platform(user)"
  set pid [pid]
  set host ""
  if { [string equal $platform unix] } {
    if { [info exist ::env(HOSTNAME)] } {
      set host $::env(HOSTNAME)
    }
  } else {
    if { [info exist ::env(COMPUTERNAME)] } {
      set host $::env(COMPUTERNAME)
    }
  }
  set ch [open $beginFile w]
  puts $ch "<?xml version=\"1.0\"?>"
  puts $ch "<ProcessHandle Version=\"1\" Minor=\"0\">"
  puts $ch "    <Process Command=\".planAhead.\" Owner=\"$user\" Host=\"$host\" Pid=\"$pid\">"
  puts $ch "    </Process>"
  puts $ch "</ProcessHandle>"
  close $ch
}

proc end_step { step } {
  set endFile ".$step.end.rst"
  set ch [open $endFile w]
  close $ch
}

proc step_failed { step } {
  set endFile ".$step.error.rst"
  set ch [open $endFile w]
  close $ch
}

set_msg_config -id {HDL 9-1061} -limit 100000
set_msg_config -id {HDL 9-1654} -limit 100000
set_msg_config  -id {Synth 8-312}  -suppress 
set_msg_config  -id {IP_Flow 19-3664}  -suppress 

start_step init_design
set rc [catch {
  create_msg_db init_design.pb
  debug::add_scope template.lib 1
  set_property design_mode GateLvl [current_fileset]
  set_property webtalk.parent_dir C:/Users/lab/Documents/EmbeddedSystems/Wednesday/AidanThomasTinu/SoC/DES_SoC/Hardware/DemoSystem.cache/wt [current_project]
  set_property parent.project_path C:/Users/lab/Documents/EmbeddedSystems/Wednesday/AidanThomasTinu/SoC/DES_SoC/Hardware/DemoSystem.xpr [current_project]
  set_property ip_repo_paths c:/Users/lab/Documents/EmbeddedSystems/Wednesday/AidanThomasTinu/SoC/DES_SoC/Hardware/DemoSystem.cache/ip [current_project]
  set_property ip_output_repo c:/Users/lab/Documents/EmbeddedSystems/Wednesday/AidanThomasTinu/SoC/DES_SoC/Hardware/DemoSystem.cache/ip [current_project]
  add_files -quiet C:/Users/lab/Documents/EmbeddedSystems/Wednesday/AidanThomasTinu/SoC/DES_SoC/Hardware/DemoSystem.runs/synth_1/AHBliteTop.dcp
  add_files -quiet C:/Users/lab/Documents/EmbeddedSystems/Wednesday/AidanThomasTinu/SoC/DES_SoC/Hardware/DemoSystem.runs/CORTEXM0DS_synth_1/CORTEXM0DS.dcp
  set_property netlist_only true [get_files C:/Users/lab/Documents/EmbeddedSystems/Wednesday/AidanThomasTinu/SoC/DES_SoC/Hardware/DemoSystem.runs/CORTEXM0DS_synth_1/CORTEXM0DS.dcp]
  add_files -quiet C:/Users/lab/Documents/EmbeddedSystems/Wednesday/AidanThomasTinu/SoC/DES_SoC/Hardware/DemoSystem.runs/blk_mem_4Kword_synth_1/blk_mem_4Kword.dcp
  set_property netlist_only true [get_files C:/Users/lab/Documents/EmbeddedSystems/Wednesday/AidanThomasTinu/SoC/DES_SoC/Hardware/DemoSystem.runs/blk_mem_4Kword_synth_1/blk_mem_4Kword.dcp]
  read_xdc -mode out_of_context -ref blk_mem_4Kword -cells U0 c:/Users/lab/Documents/EmbeddedSystems/Wednesday/AidanThomasTinu/SoC/DES_SoC/Hardware/DemoSystem.srcs/sources_1/ip/blk_mem_4Kword/blk_mem_4Kword_ooc.xdc
  set_property processing_order EARLY [get_files c:/Users/lab/Documents/EmbeddedSystems/Wednesday/AidanThomasTinu/SoC/DES_SoC/Hardware/DemoSystem.srcs/sources_1/ip/blk_mem_4Kword/blk_mem_4Kword_ooc.xdc]
  read_xdc C:/Users/lab/Documents/EmbeddedSystems/Wednesday/AidanThomasTinu/SoC/DES_SoC/Hardware/Constraint/Nexys4_SoC.xdc
  read_xdc -mode out_of_context -ref CORTEXM0DS C:/Users/lab/Documents/EmbeddedSystems/Wednesday/AidanThomasTinu/SoC/DES_SoC/Hardware/DemoSystem.srcs/CORTEXM0DS/new/CORTEXM0DS_ooc.xdc
  link_design -top AHBliteTop -part xc7a100tcsg324-1
  close_msg_db -file init_design.pb
} RESULT]
if {$rc} {
  step_failed init_design
  return -code error $RESULT
} else {
  end_step init_design
}

start_step opt_design
set rc [catch {
  create_msg_db opt_design.pb
  catch {write_debug_probes -quiet -force debug_nets}
  opt_design 
  write_checkpoint -force AHBliteTop_opt.dcp
  catch {report_drc -file AHBliteTop_drc_opted.rpt}
  close_msg_db -file opt_design.pb
} RESULT]
if {$rc} {
  step_failed opt_design
  return -code error $RESULT
} else {
  end_step opt_design
}

start_step place_design
set rc [catch {
  create_msg_db place_design.pb
  catch {write_hwdef -file AHBliteTop.hwdef}
  place_design 
  write_checkpoint -force AHBliteTop_placed.dcp
  catch { report_io -file AHBliteTop_io_placed.rpt }
  catch { report_utilization -file AHBliteTop_utilization_placed.rpt -pb AHBliteTop_utilization_placed.pb }
  catch { report_control_sets -verbose -file AHBliteTop_control_sets_placed.rpt }
  close_msg_db -file place_design.pb
} RESULT]
if {$rc} {
  step_failed place_design
  return -code error $RESULT
} else {
  end_step place_design
}

start_step route_design
set rc [catch {
  create_msg_db route_design.pb
  route_design 
  write_checkpoint -force AHBliteTop_routed.dcp
  catch { report_drc -file AHBliteTop_drc_routed.rpt -pb AHBliteTop_drc_routed.pb }
  catch { report_timing_summary -warn_on_violation -max_paths 10 -file AHBliteTop_timing_summary_routed.rpt -rpx AHBliteTop_timing_summary_routed.rpx }
  catch { report_power -file AHBliteTop_power_routed.rpt -pb AHBliteTop_power_summary_routed.pb }
  catch { report_route_status -file AHBliteTop_route_status.rpt -pb AHBliteTop_route_status.pb }
  catch { report_clock_utilization -file AHBliteTop_clock_utilization_routed.rpt }
  close_msg_db -file route_design.pb
} RESULT]
if {$rc} {
  step_failed route_design
  return -code error $RESULT
} else {
  end_step route_design
}

