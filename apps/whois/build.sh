#!/bin/sh

#make -b -C  whois all
cc -c -Wall -Wmissing-prototypes  -Os  -DBACDL_BIP=1 -DBBMD_ENABLED=1 -DBBMD_CLIENT_ENABLED -DWEAK_FUNC=  -DPRINT_ENABLED=1 -DBACAPP_ALL -DBACFILE -DINTRINSIC_REPORTING -DBACNET_TIME_MASTER -DBACNET_PROPERTY_LISTS=1 -DBACNET_PROTOCOL_REVISION=17 -I/home/user/workdir/bacnet-stack/src -I/home/user/workdir/bacnet-stack/ports/linux -ffunction-sections -fdata-sections main.c -o main.o

cc -c -Wall -Wmissing-prototypes  -Os  -DBACDL_BIP=1 -DBBMD_ENABLED=1 -DBBMD_CLIENT_ENABLED -DWEAK_FUNC=  -DPRINT_ENABLED=1 -DBACAPP_ALL -DBACFILE -DINTRINSIC_REPORTING -DBACNET_TIME_MASTER -DBACNET_PROPERTY_LISTS=1 -DBACNET_PROTOCOL_REVISION=17 -I/home/user/workdir/bacnet-stack/src -I/home/user/workdir/bacnet-stack/ports/linux -ffunction-sections -fdata-sections /home/user/workdir/bacnet-stack/src/bacnet/basic/object/client/device-client.c -o /home/user/workdir/bacnet-stack/src/bacnet/basic/object/client/device-client.o

cc -c -Wall -Wmissing-prototypes  -Os  -DBACDL_BIP=1 -DBBMD_ENABLED=1 -DBBMD_CLIENT_ENABLED -DWEAK_FUNC=  -DPRINT_ENABLED=1 -DBACAPP_ALL -DBACFILE -DINTRINSIC_REPORTING -DBACNET_TIME_MASTER -DBACNET_PROPERTY_LISTS=1 -DBACNET_PROTOCOL_REVISION=17 -I/home/user/workdir/bacnet-stack/src -I/home/user/workdir/bacnet-stack/ports/linux -ffunction-sections -fdata-sections /home/user/workdir/bacnet-stack/src/bacnet/basic/object/netport.c -o /home/user/workdir/bacnet-stack/src/bacnet/basic/object/netport.o

cc -pthread main.o /home/user/workdir/bacnet-stack/src/bacnet/basic/object/client/device-client.o /home/user/workdir/bacnet-stack/src/bacnet/basic/object/netport.o -Wl,-L/home/user/workdir/bacnet-stack/apps/lib,-lbacnet -Wl,-lc,-lgcc,-lrt,-lm -Wl,--gc-sections -o bacwi


