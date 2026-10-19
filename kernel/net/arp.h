#ifndef ARP_H
#define ARP_H

#include <stdint.h>
#include <stddef.h>
#include "ethernet.h"

#define ARP_HTYPE_ETHERNET 1
#define ARP_PTYPE_IPV4     0x0800
#define ARP_HLEN_ETHERNET  6
#define ARP_PLEN_IPV4      4
#define ARP_OP_REQUEST     1
#define ARP_OP_REPLY       2

#define ARP_CACHE_CAPACITY 16

/* 28-byte Standard ARP Header */
typedef struct {
    uint16_t htype;
    uint16_t ptype;
    uint8_t  hlen;
    uint8_t  plen;
    uint16_t oper;
    uint8_t  sender_mac[ETH_ALEN];
    uint32_t sender_ip;
    uint8_t  target_mac[ETH_ALEN];
    uint32_t target_ip;
} __attribute__((packed)) arp_header_t;

/* Static ARP Cache Table Entry */
typedef struct {
    uint32_t ip;
    uint8_t  mac[ETH_ALEN];
    int      is_valid;
    uint64_t last_updated_ticks;
} arp_entry_t;

/* ARP Subsystem APIs */
void arp_init(void);
int arp_lookup(uint32_t ip, uint8_t *out_mac);
void arp_update_cache(uint32_t ip, const uint8_t *mac);
int arp_send_request(net_device_t *dev, uint32_t target_ip);
int arp_handle_packet(net_device_t *dev, const void *payload, uint16_t len);
void arp_print_table(void);

/* Invariant Verification */
int test_arp_invariants(void);

#endif /* ARP_H */

