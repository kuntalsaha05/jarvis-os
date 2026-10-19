#ifndef ICMP_H
#define ICMP_H

#include <stdint.h>
#include <stddef.h>
#include "netdev.h"
#include "ipv4.h"

#define ICMP_TYPE_ECHO_REPLY   0
#define ICMP_TYPE_ECHO_REQUEST 8

#define ICMP_MIN_LEN           8

/* ICMP Echo Header (RFC 792) */
typedef struct {
    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;
    uint16_t id;
    uint16_t sequence;
} __attribute__((packed)) icmp_echo_header_t;

/* ICMP Packet Handling & Transmission */
int icmp_handle_packet(net_device_t *dev, uint32_t src_ip, const void *payload, uint16_t len);
int icmp_send_echo(net_device_t *dev, uint32_t dest_ip, uint16_t id, uint16_t seq, const void *data, uint16_t data_len);

/* Ping Statistics & Queries */
typedef struct {
    uint32_t packets_transmitted;
    uint32_t packets_received;
    uint16_t last_seq_received;
    uint16_t last_id_received;
} icmp_ping_stats_t;

void icmp_get_ping_stats(icmp_ping_stats_t *out_stats);
void icmp_reset_ping_stats(void);

/* Invariant Verification */
int test_icmp_invariants(void);

#endif /* ICMP_H */

