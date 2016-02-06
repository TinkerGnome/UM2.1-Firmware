#!/usr/bin/env bash

# This script is to package the Marlin package for Arduino
# This script should run under Linux and Mac OS X, as well as Windows with Cygwin.

#############################
# CONFIGURATION
#############################

##Which version name are we appending to the final archive
export BUILD_NAME=16.02.3

#############################
# Actual build script
#############################

if [ -z `which make` ]; then
	MAKE=mingw32-make
else
	MAKE=make
fi


# Change working directory to the directory the script is in
# http://stackoverflow.com/a/246128
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$SCRIPT_DIR"

#############################
# Build the required firmwares
#############################

if [ -d "C:/arduino-1.0.3" ]; then
	ARDUINO_PATH=C:/arduino-1.0.3
	ARDUINO_VERSION=103
elif [ -d "/Applications/Arduino.app/Contents/Resources/Java" ]; then
	ARDUINO_PATH=/Applications/Arduino.app/Contents/Resources/Java
	ARDUINO_VERSION=105
elif [ -d "C:/Arduino" ]; then
	ARDUINO_PATH=C:/Arduino
	ARDUINO_VERSION=165
else
	ARDUINO_PATH=/usr/share/arduino
	ARDUINO_VERSION=105
fi


#Build the Ultimaker2 firmwares.
# $MAKE -j 3 HARDWARE_MOTHERBOARD=72 ARDUINO_INSTALL_DIR=${ARDUINO_PATH} ARDUINO_VERSION=${ARDUINO_VERSION} BUILD_DIR=_Mark2 clean
# sleep 1
# mkdir _Mark2
# $MAKE -j 3 HARDWARE_MOTHERBOARD=72 ARDUINO_INSTALL_DIR=${ARDUINO_PATH} ARDUINO_VERSION=${ARDUINO_VERSION} BUILD_DIR=_Mark2 DEFINES="'STRING_CONFIG_H_AUTHOR=\"Mark2_${BUILD_NAME}\"' TEMP_SENSOR_1=0 EXTRUDERS=1 BABYSTEPPING HEATER_0_MAXTEMP=275 HEATER_1_MAXTEMP=275"
# sleep 1
# cp _Mark2/Marlin.hex firmware/Mark2-${BUILD_NAME}.hex

# BOGUS_TEMPERATURE_FAILSAFE_OVERRIDE

$MAKE -j 3 HARDWARE_MOTHERBOARD=72 ARDUINO_INSTALL_DIR=${ARDUINO_PATH} ARDUINO_VERSION=${ARDUINO_VERSION} BUILD_DIR=_Mark2Dual clean
sleep 1
mkdir _Mark2Dual
$MAKE -j 3 HARDWARE_MOTHERBOARD=72 ARDUINO_INSTALL_DIR=${ARDUINO_PATH} ARDUINO_VERSION=${ARDUINO_VERSION} BUILD_DIR=_Mark2Dual DEFINES="'STRING_CONFIG_H_AUTHOR=\"Mark2_${BUILD_NAME}\"' TEMP_SENSOR_1=20 EXTRUDERS=2 MARK2HEAD BABYSTEPPING HEATER_0_MAXTEMP=275 HEATER_1_MAXTEMP=275 'EEPROM_VERSION=\"V12\"'"
#cd -
sleep 1
cp _Mark2Dual/Marlin.hex firmware/Mark2-dual-${BUILD_NAME}.hex

