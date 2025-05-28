#! /bin/sh
#java -Xmx512M -cp .:lib/ECLA.jar:lib/DTNConsoleConnection.jar:lib/json-20240303.jar core.DTNSim $*
java -Xmx512M -cp ".:lib/*" core.DTNSim $*
