
# export BACNET_IFACE=eth0
# export BACNET_BIP6_IFACE=eth1
export BACNET_IFACE=ens37
export BACNET_BIP6_IFACE=ens33

#Also uses these configurations, but defaults to these values if not set:
export BACNET_IP_PORT=47808
export BACNET_BIP6_PORT=47809
export BACNET_BIP6_BROADCAST=FF02
export BACNET_IP_NET=1
export BACNET_IP6_NET=2

# Common to address resolution and forwarded address resolution
export BACNET_BBMD_PORT=47809
export TARGET_VMAC=13
export TARGET_IP6=2001:bb6:c2ab:6c58:2a29:86ff:fe2f:6eef
export TARGET_PORT=47808
export INTERVAL=3

# Forwarded address resolution specific
export ORIGINAL_SOURCE_IP6=2001:bb6:c2ab:6c58:2a29:86ff:cafe:babe
export ORIGINAL_PORT=55555

export SEND_ADDRESS_RESOLUTION=no
export SEND_FORWARDED_ADDRESS_RESOLUTION=yes

apps/addr-res/addres 55
