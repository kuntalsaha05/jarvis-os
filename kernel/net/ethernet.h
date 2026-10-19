#ifndef ETHERNET_H
#define ETHERNET_H

#include <stdint.h>
#include <stddef.h>
#include "netdev.h"

/* Standard EtherTypes (Network Byte Order = Big Endian) */
#define ETHERTYPE_IPV4 0x0800
#define ETHERTYPE_ARP  0x0806
#define ETHERTYPE_IPV6 0x86DD

/* Byte-order Endianness Conversion */
static inline uint16_t net_htons(uint16_t hostshort) {
    return (uint16_t)((hostshort >> 8) | (hostshort << 8));
}

static inline uint16_t net_ntohs(uint16_t netshort) {
    return net_htons(netshort);
}

static inline uint32_t net_htonl(uint32_t hostlong) {
    return ((hostlong >> 24) & 0x000000FF) |
           ((hostlong >> 8)  & 0x0000FF00) |
           ((hostlong << 8)  & 0x00FF0000) |
           ((hostlong << 24) & 0xFF000000);
}

static inline uint32_t net_ntohl(uint32_t netlong) {
    return net_htonl(netlong);
}

/* 14-byte Standard Ethernet Header */
typedef struct {
    uint8_t  dest_mac[ETH_ALEN];
    uint8_t  src_mac[ETH_ALEN];
    uint16_t ethertype;
} __attribute__((packed)) eth_header_t;

/* Address Utilities */
int eth_is_broadcast(const uint8_t *mac);
int eth_mac_equal(const uint8_t *mac1, const uint8_t *mac2);
void eth_format_mac(const uint8_t *mac, char *out_str, size_t max_len);

/* Frame Construction & Parsing */
int eth_build_frame(void *out_buf,
                    uint16_t max_buf_len,
                    const uint8_t *dest_mac,
                    const uint8_t *src_mac,
                    uint16_t ethertype,
                    const void *payload,
                    uint16_t payload_len);

int eth_parse_frame(const void *frame,
                    uint16_t frame_len,
                    eth_header_t *out_hdr,
                    const uint8_t **out_payload,
                    uint16_t *out_payload_len);

int eth_handle_rx(net_device_t *dev, const void *frame, uint16_t frame_len);

/* Invariant Verification */
int test_ethernet_invariants(void);

#endif /* ETHERNET_H */

