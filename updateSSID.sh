#! /bin/bash

echo "This script will change your SSID"
echo "Enter SSID in decimal:"
read ssid

FactoryDataWriter --lid ILID_NGC_MAS_SSID eVTUnsigned16 $ssid
LoadMonitorInit

echo "Changes will not take effect until you reboot."
echo "Reboot now? (y/n)"
read answer

if [ $answer = "y" ]
then
	reboot
else
	echo "Exiting."
	exit 0
fi
