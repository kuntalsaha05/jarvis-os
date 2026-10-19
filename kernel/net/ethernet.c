#include "ethernet.h"
#include "../security/security.h"

/* Forward protocol handler declarations */
int arp_handle_packet(net_device_t *dev, const void *payload, uint16_t len);
int ipv4_handle_packet(net_device_t *dev, const void *payload, uint16_t len);

/* Address Utilities */
int eth_is_broadcast(const uint8_t *mac)
{
    if (!mac) return 0;
    for (int i = 0; i < ETH_ALEN; i++) {
        if (mac[i] != 0xFF) return 0;
    }
    return 1;
}

int eth_mac_equal(const uint8_t *mac1, const uint8_t *mac2)
{
    if (!mac1 || !mac2) return 0;
    for (int i = 0; i < ETH_ALEN; i++) {
        if (mac1[i] != mac2[i]) return 0;
    }
    return 1;
}

void eth_format_mac(const uint8_t *mac, char *out_str, size_t max_len)
{
    if (!mac || !out_str || max_len < 18) return;
    static const char hex[] = "0123456789ABCDEF";
    int idx = 0;
    for (int i = 0; i < ETH_ALEN; i++) {
        out_str[idx++] = hex[(mac[i] >> 4) & 0x0F];
        out_str[idx++] = hex[mac[i] & 0x0F];
        if (i < ETH_ALEN - 1) {
            out_str[idx++] = ':';
        }
    }
    out_str[idx] = '\0';
}

/* Frame Construction & Parsing */
int eth_build_frame(void *out_buf,
                    uint16_t max_buf_len,
                    const uint8_t *dest_mac,
                    const uint8_t *src_mac,
                    uint16_t ethertype,
                    const void *payload,
                    uint16_t payload_len)
{
    if (!out_buf || !dest_mac || !src_mac) return NET_ERR_INVALID_PARAM;
    if (payload_len > (ETH_MTU)) return NET_ERR_BUFFER_BOUNDS;

    uint16_t total_len = ETH_HLEN + payload_len;
    uint16_t padded_len = (total_len < ETH_MIN_FRAME_LEN) ? ETH_MIN_FRAME_LEN : total_len;

    if (padded_len > max_buf_len || padded_len > ETH_MAX_FRAME_LEN) {
        return NET_ERR_BUFFER_BOUNDS;
    }

    uint8_t *buf = (uint8_t *)out_buf;
    eth_header_t *hdr = (eth_header_t *)buf;

    for (int i = 0; i < ETH_ALEN; i++) {
        hdr->dest_mac[i] = dest_mac[i];
        hdr->src_mac[i] = src_mac[i];
    }
    hdr->ethertype = net_htons(ethertype);

    /* Copy payload */
    if (payload && payload_len > 0) {
        const uint8_t *src = (const uint8_t *)payload;
        for (uint16_t i = 0; i < payload_len; i++) {
            buf[ETH_HLEN + i] = src[i];
        }
    }

    /* Zero-pad short frames to Ethernet minimum 60 octets */
    if (padded_len > total_len) {
        for (uint16_t i = total_len; i < padded_len; i++) {
            buf[i] = 0x00;
        }
    }

    return (int)padded_len;
}

int eth_parse_frame(const void *frame,
                    uint16_t frame_len,
                    eth_header_t *out_hdr,
                    const uint8_t **out_payload,
                    uint16_t *out_payload_len)
{
    if (!frame || frame_len < ETH_HLEN) {
        return NET_ERR_MALFORMED;
    }

    const uint8_t *buf = (const uint8_t *)frame;
    const eth_header_t *hdr = (const eth_header_t *)buf;

    if (out_hdr) {
        for (int i = 0; i < ETH_ALEN; i++) {
            out_hdr->dest_mac[i] = hdr->dest_mac[i];
            out_hdr->src_mac[i] = hdr->src_mac[i];
        }
        out_hdr->ethertype = net_ntohs(hdr->ethertype);
    }

    if (out_payload) {
        *out_payload = buf + ETH_HLEN;
    }
    if (out_payload_len) {
        *out_payload_len = frame_len - ETH_HLEN;
    }

    return NET_OK;
}

int eth_handle_rx(net_device_t *dev, const void *frame, uint16_t frame_len)
{
    if (!dev || !frame) return NET_ERR_INVALID_PARAM;

    eth_header_t hdr;
    const uint8_t *payload = NULL;
    uint16_t payload_len = 0;

    int parse_res = eth_parse_frame(frame, frame_len, &hdr, &payload, &payload_len);
    if (parse_res != NET_OK) {
        dev->stats.rx_errors++;
        security_audit_log(SEC_ACT_NET_RX, PERM_NET_ACCESS, SEC_AUDIT_FAULT, "malformed_frame", (int32_t)frame_len);
        return parse_res;
    }

    /* Destination MAC Filtering (unless in promiscuous mode) */
    if (!dev->is_promiscuous) {
        int is_for_us = eth_mac_equal(hdr.dest_mac, dev->mac_addr);
        int is_bcast = eth_is_broadcast(hdr.dest_mac);
        if (!is_for_us && !is_bcast) {
            /* Discard frame intended for other host */
            return NET_OK;
        }
    }

    /* Dispatch to Network Protocol Layer */
    switch (hdr.ethertype) {
        case ETHERTYPE_ARP:
            return arp_handle_packet(dev, payload, payload_len);

        case ETHERTYPE_IPV4:
            return ipv4_handle_packet(dev, payload, payload_len);

        default:
            /* Unsupported EtherType */
            dev->stats.rx_dropped++;
            return NET_ERR_NOT_FOUND;
    }
}

/* Invariant Verification */
int test_ethernet_invariants(void)
{
    uint8_t frame_buf[ETH_MAX_FRAME_LEN];
    uint8_t src[6]  = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x01 };
    uint8_t dest[6] = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x02 };
    uint8_t payload[20] = "JARVIS_ETH_PAYLOAD!";

    /* 1. Build Ethernet frame with automatic padding */
    int frame_len = eth_build_frame(frame_buf, sizeof(frame_buf), dest, src, ETHERTYPE_IPV4, payload, 20);
    /* 14 + 20 = 34, which is < 60, so frame must be padded to 60 */
    if (frame_len != 60) return -1;

    /* 2. Parse Ethernet frame */
    eth_header_t hdr;
    const uint8_t *out_payload = NULL;
    uint16_t out_len = 0;
    if (eth_parse_frame(frame_buf, (uint16_t)frame_len, &hdr, &out_payload, &out_len) != NET_OK) return -2;

    if (!eth_mac_equal(hdr.src_mac, src)) return -3;
    if (!eth_mac_equal(hdr.dest_mac, dest)) return -4;
    if (hdr.ethertype != ETHERTYPE_IPV4) return -5;
    if (out_len != 46) return -6; /* 60 - 14 = 46 (payload + padding) */

    /* 3. Underflow rejection */
    if (eth_parse_frame(frame_buf, 10, &hdr, &out_payload, &out_len) != NET_ERR_MALFORMED) return -7;

    /* 4. Broadcast MAC detection */
    uint8_t bcast[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    if (!eth_is_broadcast(bcast)) return -8;
    if (eth_is_broadcast(dest)) return -9;

    /* 5. MAC string formatting */
    char mac_str[20];
    eth_format_mac(src, mac_str, sizeof(mac_str));
    if (mac_str[0] != '0' || mac_str[1] != '2' || mac_str[2] != ':') return -10;

    return 0;
}

