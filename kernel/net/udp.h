#ifndef UDP_H
#define UDP_H

#include <stdint.h>
#include <stddef.h>
#include "netdev.h"
#include "ipv4.h"

#define UDP_HEADER_LEN 8
#define UDP_MAX_PORTS  16

/* Standard UDP Header (RFC 768) */
typedef struct {
    uint16_t src_port;
    uint16_t dest_port;
    uint16_t length;
    uint16_t checksum;
} __attribute__((packed)) udp_header_t;

typedef void (*udp_rx_callback_t)(net_device_t *dev,
                                  uint32_t src_ip,
                                  uint16_t src_port,
                                  const void *payload,
                                  uint16_t len);

/* UDP Port Registration & Operations */
int udp_bind(uint16_t port, udp_rx_callback_t callback);
int udp_unbind(uint16_t port);

int udp_handle_packet(net_device_t *dev, uint32_t src_ip, const void *payload, uint16_t len);
int udp_transmit(net_device_t *dev,
                 uint32_t dest_ip,
                 uint16_t src_port,
                 uint16_t dest_port,
                 const void *payload,
                 uint16_t payload_len);

/* Invariant Verification */
int test_udp_invariants(void);

#endif /* UDP_H */

