#ifndef IPV4_H
#define IPV4_H

#include <stdint.h>
#include <stddef.h>
#include "ethernet.h"

#define IPV4_PROTO_ICMP 1
#define IPV4_PROTO_TCP  6
#define IPV4_PROTO_UDP  17

#define IPV4_DEFAULT_TTL 64
#define IPV4_MIN_HLEN    20

/* 20-byte Standard IPv4 Header */
typedef struct {
    uint8_t  version_ihl;   /* Version (4 bits: 4) | IHL (4 bits: 5 for 20 bytes) */
    uint8_t  tos;           /* Differentiated Services / Type of Service */
    uint16_t total_length;  /* Total packet length in octets */
    uint16_t id;            /* Identification */
    uint16_t flags_fragment;/* Flags (3 bits) | Fragment offset (13 bits) */
    uint8_t  ttl;           /* Time to Live */
    uint8_t  protocol;      /* Transport protocol */
    uint16_t checksum;      /* 16-bit One's Complement Header Checksum */
    uint32_t src_ip;        /* Source IP Address (Big-Endian) */
    uint32_t dest_ip;       /* Destination IP Address (Big-Endian) */
} __attribute__((packed)) ipv4_header_t;

/* Internet Checksum & IP String Conversion */
uint16_t net_checksum(const void *data, size_t len);
void net_ip_to_str(uint32_t ip, char *buf, size_t max_len);
uint32_t net_str_to_ip(const char *str);

/* IPv4 Packet Construction & Parsing */
int ipv4_build_packet(void *out_buf,
                      uint16_t max_buf_len,
                      uint32_t dest_ip,
                      uint32_t src_ip,
                      uint8_t protocol,
                      const void *payload,
                      uint16_t payload_len);

int ipv4_parse_packet(const void *packet,
                      uint16_t packet_len,
                      ipv4_header_t *out_hdr,
                      const uint8_t **out_payload,
                      uint16_t *out_payload_len);

int ipv4_handle_packet(net_device_t *dev, const void *payload, uint16_t len);
int ipv4_transmit(net_device_t *dev, uint32_t dest_ip, uint8_t protocol, const void *payload, uint16_t payload_len);

/* Invariant Verification */
int test_ipv4_invariants(void);

#endif /* IPV4_H */

