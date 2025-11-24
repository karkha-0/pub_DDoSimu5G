#!/bin/sh
# Run the ONE Simulator in batch mode and measure the execution time
#time java -Xmx512M -cp .:lib/ECLA.jar:lib/DTNConsoleConnection.jar:lib/json-20240303.jar core.DTNSim -b 1 $*

# This here is for taking everything under /lib instead of one by one
time java -Xmx1G -XX:+UseG1GC -XX:MaxGCPauseMillis=200 -cp ".:lib/*" core.DTNSim -b 1 $*

# This one line if I want to have memory dump log
#time java -Xmx512M -Xlog:gc*:file=gc.log -XX:+HeapDumpOnOutOfMemoryError -XX:HeapDumpPath=heap_dump.hprof -cp ".:lib/*" core.DTNSim -b 1 $*



