#! /bin/sh
java -Xlog:gc*:file=gc.log:time -Xmx512M -cp .:lib/ECLA.jar:lib/DTNConsoleConnection.jar:lib/json-20240303.jar core.DTNSim $*
