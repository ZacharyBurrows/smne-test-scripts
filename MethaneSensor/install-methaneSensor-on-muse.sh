sensor="MethaneSensor"
thumb_path="/media/sda1"
thumb_code_path="$thumb_path/""$sensor/""$sensor""Code"
edge_code_path="/root/""$sensor""Code"

echo "===================================================================="
echo "This script installs the riva agent, and assumes a thumb drive with"
echo "a folder titled \"$sensor\" is mounted on the MUSE riva edge board"
echo "under $thumb_path."
echo " "
echo "For support contact:"
echo "Zachary Burrows - zachary.burrows@itron.com"
echo "====================================================================\n"


#Ask if user wants to proceed
read -r -p "Install $sensor code and rivaagent? [y/n] " prompt
if [[ $prompt != "y" ]]
then
        echo "EXITING"
        exit 0
fi

# Check if thumb drive path exists
if [ ! -d $thumb_path ]; then
        echo "$thumb_path does not exist"
        echo "EXITING"
        exit 0
fi

# Set up directories
mkdir $edge_code_path
cp -r $thumb_path/$sensor/rivaagent /etc/

# Set up rivaagent
cp MC99-rivaagent /etc/mcontrol.d/
chmod 755 /etc/rivaagent/start-agent.sh
bash /etc/rivaagent/start-agent.sh

# Copy compiled code over
cp -r $thumb_code_path /root/
chmod 755 /root/MethaneSensorCode/MethaneLEL_Muse
chmod 755 /root/MethaneSensorCode/MethaneTemp_Muse

# Append NGC_MAC to agent.conf
NGC_MAC=$(LoadMonitorInit --verify | grep ILID_NGC_MAC)
NGC_MAC=$(echo ${NGC_MAC##*=})
NGC_MAC=${NGC_MAC%?}
printf '\nserial = %s\n' $NGC_MAC >> /etc/rivaagent/agent.conf

#echo "These changes require a reboot. Reboot now? (y/n)"