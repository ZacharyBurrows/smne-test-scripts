#!/bin/bash
# NOTE: Whenever you do a "FactoryDataWriter" / "LoadMonitorInit" command pair, you must reboot
# 		for changes to take effect.


# -----------------------------
# Change SSID
# -----------------------------
echo "Changing SSID"
echo "Enter SSID in decimal:"
read ssid
FactoryDataWriter --lid ILID_NGC_MAS_SSID eVTUnsigned16 $ssid
LoadMonitorInit
#reboot


# -----------------------------
# Change from 11 to 64 Channels
# -----------------------------
echo "Changing to 64 channels and a starting frequency of 902400"
# Change number of channels to 64
FactoryDataWriter --lid ILID_NGC_RFPHY_NUM_OF_CHANNELS eVTUnsigned16 64
LoadMonitorInit

# Change channel mask to 64 channels (1 bit per channel)
FactoryDataWriter --lid ILID_ACT_RFPHY_CHANNEL_MASK eVTString 0000000000000000FFFFFFFFFFFFFFFF
LoadMonitorInit

# Change starting frequency to 902400
FactoryDataWriter --lid ILID_NGC_RFPHY_START_FREQ  eVTUnsigned32 902400
LoadMonitorInit
#reboot


# -----------------------------
# Disable header compression of 6lowpan 
# -----------------------------
echo "6LowPAN: Disabling header compression"
FactoryDataWriter --lid ILID_ACT_ENABLE_6LOWPAN_HC eVTSigned32 0
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