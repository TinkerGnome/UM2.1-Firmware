#!/usr/bin/env bash

# This script is to package the Marlin package for Arduino
# This script should run under Linux and Mac OS X, as well as Windows with Cygwin.

#############################
# CONFIGURATION
#############################

##Which version name are we appending to the final archive
export BUILD_NAME=17.01.3

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

if [ -d "D:/arduino-1.8.1" ]; then
	ARDUINO_PATH=D:/arduino-1.8.1
	ARDUINO_VERSION=181
elif [ -d "D:/Arduino" ]; then
	ARDUINO_PATH=D:/Arduino
	ARDUINO_VERSION=165
elif [ -d "C:/arduino-1.0.3" ]; then
	ARDUINO_PATH=C:/arduino-1.0.3
	ARDUINO_VERSION=103
elif [ -d "/Applications/Arduino.app/Contents/Resources/Java" ]; then
	ARDUINO_PATH=/Applications/Arduino.app/Contents/Resources/Java
	ARDUINO_VERSION=105
else
	ARDUINO_PATH=/usr/share/arduino
	ARDUINO_VERSION=105
fi


#Build the Ultimaker2 firmwares.

# BOGUS_TEMPERATURE_FAILSAFE_OVERRIDE

# read optional tool change scripts from sd card
#define TCSDSCRIPT

$MAKE -j 3 HARDWARE_MOTHERBOARD=72 ARDUINO_INSTALL_DIR=${ARDUINO_PATH} ARDUINO_VERSION=${ARDUINO_VERSION} BUILD_DIR=_Mark2DualExtended clean
sleep 2
mkdir _Mark2DualExtended
$MAKE -j 3 HARDWARE_MOTHERBOARD=72 ARDUINO_INSTALL_DIR=${ARDUINO_PATH} ARDUINO_VERSION=${ARDUINO_VERSION} BUILD_DIR=_Mark2DualExtended DEFINES="'STRING_CONFIG_H_AUTHOR=\"Mark2_${BUILD_NAME}\"' TEMP_SENSOR_1=20 EXTRUDERS=2 TCSDSCRIPT HEATER_0_MAXTEMP=275 HEATER_1_MAXTEMP=275 'EEPROM_VERSION=\"V12\"'"

#cd -
sleep 2
cp _Mark2DualExtended/Marlin.hex firmware/Mark2-dual-ext-${BUILD_NAME}.hex

###

$MAKE -j 3 HARDWARE_MOTHERBOARD=72 ARDUINO_INSTALL_DIR=${ARDUINO_PATH} ARDUINO_VERSION=${ARDUINO_VERSION} BUILD_DIR=_Mark2DualFanExtended clean
sleep 2
mkdir _Mark2DualFanExtended
$MAKE -j 3 HARDWARE_MOTHERBOARD=72 ARDUINO_INSTALL_DIR=${ARDUINO_PATH} ARDUINO_VERSION=${ARDUINO_VERSION} BUILD_DIR=_Mark2DualFanExtended DEFINES="'STRING_CONFIG_H_AUTHOR=\"Mark2_${BUILD_NAME}\"' TEMP_SENSOR_1=20 EXTRUDERS=2 TCSDSCRIPT DUAL_FAN HEATER_0_MAXTEMP=275 HEATER_1_MAXTEMP=275 'EEPROM_VERSION=\"V12\"'"

#cd -
sleep 2
cp _Mark2DualFanExtended/Marlin.hex firmware/Mark2-dual-fan-ext-${BUILD_NAME}.hex

