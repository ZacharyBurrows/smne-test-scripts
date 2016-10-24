#! /bin/bash

echo "This script is changing your ILID_ACT_MAS_DISABLEMAC to 2"

echo -n "Do you wish to continue (y/n): "
read answer

if [ x$answer == "x" ]; then
	echo "Answer with y or n"
	exit 0
fi
if [ x$answer != "xy" ]; then
	echo "No changes made."
	exit 0
fi

FactoryDataWriter --lid ILID_ACT_MAS_DISABLEMAC eVTUnsigned8 2
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
