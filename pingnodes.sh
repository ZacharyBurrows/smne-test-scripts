#!/bin/bash
baseIP="fd16:e8ac:e37c:"
itronsensors="fd16:e8ac:e37c::1"

#CHANGE 'root' value to match yours and numNodes to match number of nodes
root="1021"
numNodes=10
rootprefix="$baseIP$root::"


echo "Pinging itronsensors"
ping6 -c 3 "$itronsensors"

# Change these to match your number of nodes and associated short addresses
node1="fb54"
node2="fb5b"
node3="f8e0"
node4="f8a9"
node5="fb54"
node6="fb5b"
node7="f8e0"
node8="f8a9"
node9="fb54"
node10="fb5b"



#allNodes=( node1 node2 node3 node4 node5 node6 node7 node8 node9 node10 )

COUNTER=1
for i in `seq 1 $numNodes`; do
    #ping6 -c 3 "$rootprefix$node${allNodes[@]}"
    curNode="node""$COUNTER"
    
    echo " "
    echo " "
    eval "echo "PINGING: ""$rootprefix""\$$curNode""
    eval "ping6 -c 3 "$rootprefix""\$$curNode""
    let COUNTER=COUNTER+1
done

# The trick to this was the double reference syntax:
#	eval "echo \$$curNode"
