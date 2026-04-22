@echo off
set xv_path=C:\\Xilinx\\Vivado\\2015.2\\bin
call %xv_path%/xsim TB_toplevel_behav -key {Behavioral:sim_top:Functional:TB_toplevel} -tclbatch TB_toplevel.tcl -view C:/Users/lab/Documents/EmbeddedSystems/Wednesday/AidanThomasTinu/SoC/DES_SoC/Hardware/TB_toplevel_behav.wcfg -log simulate.log
if "%errorlevel%"=="0" goto SUCCESS
if "%errorlevel%"=="1" goto END
:END
exit 1
:SUCCESS
exit 0
