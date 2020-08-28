# This creates two different routing tables, that we use based on the source-address.
ip rule add from 192.168.1.6 table 1
ip rule add from 192.168.0.15 table 2
ip rule add from 172.20.10.6 table 3

# Configure the two different routing tables
ip route add 192.168.1.0/24 dev eno1 scope link table 1
ip route add default via 192.168.1.1 dev eno1 table 1

ip route add 192.168.0.0/24 dev wlxf8d1110aa5f0 scope link table 2
ip route add default via 192.168.0.1 dev wlxf8d1110aa5f0 table 2

ip route add 172.20.10.0/24 dev eth0 scope link table 3
ip route add default via 172.20.10.1 dev eth0 table 3

# default route for the selection process of normal internet-traffic
ip route add default scope global nexthop via 192.168.1.1 dev eno1
