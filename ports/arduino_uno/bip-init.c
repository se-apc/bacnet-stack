/**************************************************************************
 *
 * Copyright (C) 2005 Steve Karg
 *
 * SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0
 *
 *********************************************************************/
#include <stdint.h> /* for standard integer types uint8_t etc. */
#include <stdbool.h> /* for the standard bool type. */
#include <stdio.h>
#include "bacnet/bacdcode.h"
#include "bacnet/datalink/bip.h"
#include "socketWrapper.h"
#include "w5100Wrapper.h"
// #include "bacport.h"

/** @file linux/bip-init.c  Initializes BACnet/IP interface (Linux). */

static bool BIP_Debug = false;

/**
 * @brief Enabled debug printing of BACnet/IPv4
 */
void bip_debug_enable(void)
{
    BIP_Debug = true;
}

/**
 * @brief Disalbe debug printing of BACnet/IPv4
 */
void bip_debug_disable(void)
{
    BIP_Debug = false;
}

/* gets an IP address by name, where name can be a
   string that is an IP address in dotted form, or
   a name that is a domain name
   returns 0 if not found, or
   an IP address in network byte order */
long bip_getaddrbyname(const char *host_name)
{
    return 0;
}

/** Gets the local IP address and local broadcast address from the system,
 *  and saves it into the BACnet/IP data structures.
 *
 * @param ifname [in] The named interface to use for the network layer.
 *        Eg, for Linux, ifname is eth0, ath0, arc0, and others.
 */
void bip_set_interface(const char *ifname)
{
    uint8_t local_address[] = { 0, 0, 0, 0 };
    uint8_t broadcast_address[] = { 0, 0, 0, 0 };
    uint8_t netmask[] = { 0, 0, 0, 0 };
    uint8_t invertedNetmask[] = { 0, 0, 0, 0 };

    getIPAddress_func(CW5100Class_new(), local_address);
    bip_set_addr(local_address);
    if (BIP_Debug) {
        fprintf(stderr, "Interface: %s\n", ifname);
        fprintf(
            stderr, "IP Address: %d.%d.%d.%d\n", local_address[0],
            local_address[1], local_address[2], local_address[3]);
    }

    /* setup local broadcast address */
    getSubnetMask_func(CW5100Class_new(), netmask);
    for (int i = 0; i < 4; i++) { // FIXME: IPv4 ?
        invertedNetmask[i] = ~netmask[i];
        broadcast_address[i] = (local_address[i] | invertedNetmask[i]);
    }

    bip_set_broadcast_addr(broadcast_address);
    if (BIP_Debug) {
        fprintf(
            stderr, "IP Broadcast Address: %d.%d.%d.%d\n", broadcast_address[0],
            broadcast_address[1], broadcast_address[2], broadcast_address[3]);
    }
}

/** Initialize the BACnet/IP services at the given interface.
 * @ingroup DLBIP
 * -# Gets the local IP address and local broadcast address from the system,
 *  and saves it into the BACnet/IP data structures.
 * -# Opens a UDP socket
 * -# Configures the socket for sending and receiving
 * -# Configures the socket so it can send broadcasts
 * -# Binds the socket to the local IP address at the specified port for
 *    BACnet/IP (by default, 0xBAC0 = 47808).
 *
 * @note For Linux, ifname is eth0, ath0, arc0, and others.
 *
 * @param ifname [in] The named interface to use for the network layer.
 *        If NULL, the "eth0" interface is assigned.
 * @return True if the socket is successfully opened for BACnet/IP,
 *         else False if the socket functions fail.
 */
bool bip_init(char *ifname)
{
    uint8_t sock_fd = 0;
    bool isOpen = false;

    if (ifname) {
        bip_set_interface(ifname);
    } else {
        bip_set_interface("eth0");
    }

    /* assumes that the driver has already been initialized */
    for (sock_fd = 0; sock_fd < MAX_SOCK_NUM; sock_fd++) {
        if (readSnSR_func(CW5100Class_new(), sock_fd) == SnSR_CLOSED()) {
            socket_func(sock_fd, SnMR_UDP(), (uint16_t)47808, 0);
            listen_func(sock_fd);
            isOpen = true;
            break;
        }
    }

    if (!isOpen) {
        bip_set_socket(MAX_SOCK_NUM);
        return false;
    } else {
        bip_set_socket(sock_fd);
    }

    return true;
}

/** Cleanup and close out the BACnet/IP services by closing the socket.
 * @ingroup DLBIP
 */
void bip_cleanup(void)
{
    int sock_fd = 0;

    if (bip_valid()) {
        sock_fd = bip_socket();
        close_func(sock_fd);
    }
    bip_set_socket(MAX_SOCK_NUM);

    return;
}

/** Get the netmask of the BACnet/IP's interface via an ioctl() call.
 * @param netmask [out] The netmask, in host order.
 * @return 0 on success, else the error from the ioctl() call.
 */
int bip_get_local_netmask(uint8_t *netmask)
{
    getSubnetMask_func(CW5100Class_new(), netmask);
    return 0;
}
