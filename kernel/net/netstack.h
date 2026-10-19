#ifndef NETSTACK_H
#define NETSTACK_H

#include <stdint.h>
#include <stddef.h>
#include "netdev.h"
#include "ethernet.h"
#include "arp.h"
#include "ipv4.h"
#include "icmp.h"
#include "udp.h"
#include "dhcp.h"

/* Network Stack Lifecycle & Configuration */
void netstack_init(void);
void netstack_poll(void);

uint32_t netstack_get_ip(void);
uint32_t netstack_get_netmask(void);
uint32_t netstack_get_gateway(void);
uint32_t netstack_get_dns(void);
void netstack_set_ip(uint32_t ip, uint32_t netmask, uint32_t gateway, uint32_t dns);

/* Diagnostic & Shell Support */
void netstack_print_status(void);
int netstack_ping(const char *dest_ip_str, uint16_t count);

/* Unified Invariant Verification Suite */
int test_networking_invariants(void);

#endif /* NETSTACK_H */

