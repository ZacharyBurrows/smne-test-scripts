#/bin/bash

echo "Pinging all nodes on your mesh"
prefix="fd16:e8ac:e37c:1010::"

node1="f84a"
node2="f97f"
node3="f8e0"
node4="f8a9"

ping6 -c 3 "$prefix$node1"
ping6 -c 3 "$prefix$node2"
ping6 -c 3 "$prefix$node3"
ping6 -c 3 "$prefix$node4"