#ifndef DHCP_H
#define DHCP_H

#include <stdint.h>
#include <stddef.h>
#include "netdev.h"

#define DHCP_SERVER_PORT 67
#define DHCP_CLIENT_PORT 68

#define DHCP_BOOTREQUEST 1
#define DHCP_BOOTREPLY   2

#define DHCP_HTYPE_ETHERNET 1
#define DHCP_HLEN_ETHERNET  6

#define DHCP_MAGIC_COOKIE 0x63825363

#define DHCP_OPT_PAD             0
#define DHCP_OPT_SUBNET_MASK     1
#define DHCP_OPT_ROUTER          3
#define DHCP_OPT_DNS_SERVER      6
#define DHCP_OPT_REQUESTED_IP    50
#define DHCP_OPT_LEASE_TIME      51
#define DHCP_OPT_MESSAGE_TYPE    53
#define DHCP_OPT_SERVER_ID       54
#define DHCP_OPT_PARAMETER_REQ   55
#define DHCP_OPT_END             255

#define DHCP_MSG_DISCOVER 1
#define DHCP_MSG_OFFER    2
#define DHCP_MSG_REQUEST  3
#define DHCP_MSG_DECLINE  4
#define DHCP_MSG_ACK      5
#define DHCP_MSG_NAK      6

typedef enum {
    DHCP_STATE_INIT = 0,
    DHCP_STATE_DISCOVERING,
    DHCP_STATE_REQUESTING,
    DHCP_STATE_BOUND,
    DHCP_STATE_FAILED
} dhcp_state_t;

/* Standard DHCP Packet (RFC 2131) */
typedef struct {
    uint8_t  op;
    uint8_t  htype;
    uint8_t  hlen;
    uint8_t  hops;
    uint32_t xid;
    uint16_t secs;
    uint16_t flags;
    uint32_t ciaddr;
    uint32_t yiaddr;
    uint32_t siaddr;
    uint32_t giaddr;
    uint8_t  chaddr[16];
    char     sname[64];
    char     file[128];
    uint32_t magic_cookie;
    uint8_t  options[308];
} __attribute__((packed)) dhcp_packet_t;

/* DHCP API */
void dhcp_init(net_device_t *dev);
int dhcp_start_discovery(net_device_t *dev);
void dhcp_handle_rx(net_device_t *dev, uint32_t src_ip, uint16_t src_port, const void *payload, uint16_t len);
dhcp_state_t dhcp_get_state(void);
const char *dhcp_state_to_str(dhcp_state_t state);
void dhcp_fallback_static(net_device_t *dev);

/* Invariant Verification */
int test_dhcp_invariants(void);

#endif /* DHCP_H */

