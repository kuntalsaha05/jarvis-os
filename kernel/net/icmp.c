#include "icmp.h"
#include "netdev.h"
#include "ipv4.h"

static icmp_ping_stats_t s_ping_stats = {0, 0, 0, 0};

static void net_memcpy(void *dest, const void *src, size_t n)
{
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
}

void icmp_get_ping_stats(icmp_ping_stats_t *out_stats)
{
    if (out_stats) {
        *out_stats = s_ping_stats;
    }
}

void icmp_reset_ping_stats(void)
{
    s_ping_stats.packets_transmitted = 0;
    s_ping_stats.packets_received = 0;
    s_ping_stats.last_seq_received = 0;
    s_ping_stats.last_id_received = 0;
}

int icmp_handle_packet(net_device_t *dev, uint32_t src_ip, const void *payload, uint16_t len)
{
    if (!dev || !payload) return NET_ERR_INVALID_PARAM;
    if (len < ICMP_MIN_LEN) return NET_ERR_MALFORMED;

    /* Verify ICMP checksum across header and payload */
    if (net_checksum(payload, len) != 0) {
        return NET_ERR_MALFORMED;
    }

    const icmp_echo_header_t *hdr = (const icmp_echo_header_t *)payload;

    if (hdr->type == ICMP_TYPE_ECHO_REQUEST && hdr->code == 0) {
        /* Construct Echo Reply */
        uint8_t reply_buf[ETH_MTU];
        if (len > sizeof(reply_buf)) return NET_ERR_BUFFER_BOUNDS;

        net_memcpy(reply_buf, payload, len);
        icmp_echo_header_t *reply_hdr = (icmp_echo_header_t *)reply_buf;
        reply_hdr->type = ICMP_TYPE_ECHO_REPLY;
        reply_hdr->code = 0;
        reply_hdr->checksum = 0;
        reply_hdr->checksum = net_htons(net_checksum(reply_buf, len));

        return ipv4_transmit(dev, src_ip, IPV4_PROTO_ICMP, reply_buf, len);
    } else if (hdr->type == ICMP_TYPE_ECHO_REPLY && hdr->code == 0) {
        s_ping_stats.packets_received++;
        s_ping_stats.last_id_received = net_ntohs(hdr->id);
        s_ping_stats.last_seq_received = net_ntohs(hdr->sequence);
        return NET_OK;
    }

    return NET_OK;
}

int icmp_send_echo(net_device_t *dev, uint32_t dest_ip, uint16_t id, uint16_t seq, const void *data, uint16_t data_len)
{
    if (!dev) return NET_ERR_INVALID_PARAM;

    uint16_t total_len = ICMP_MIN_LEN + data_len;
    if (total_len > (ETH_MTU - IPV4_MIN_HLEN)) {
        return NET_ERR_BUFFER_BOUNDS;
    }

    uint8_t packet[ETH_MTU];
    icmp_echo_header_t *hdr = (icmp_echo_header_t *)packet;
    hdr->type = ICMP_TYPE_ECHO_REQUEST;
    hdr->code = 0;
    hdr->checksum = 0;
    hdr->id = net_htons(id);
    hdr->sequence = net_htons(seq);

    if (data && data_len > 0) {
        net_memcpy(packet + ICMP_MIN_LEN, data, data_len);
    }

    hdr->checksum = net_htons(net_checksum(packet, total_len));

    s_ping_stats.packets_transmitted++;
    return ipv4_transmit(dev, dest_ip, IPV4_PROTO_ICMP, packet, total_len);
}

/* Invariant Verification for ICMP */
int test_icmp_invariants(void)
{
    /* Test 1: Malformed short packet */
    uint8_t short_pkt[4] = {0x08, 0x00, 0x00, 0x00};
    net_device_t *lo = netdev_get_by_name("lo0");
    if (!lo) return -1;

    if (icmp_handle_packet(lo, 0x7F000001, short_pkt, sizeof(short_pkt)) != NET_ERR_MALFORMED) {
        return -2;
    }

    /* Test 2: Invalid checksum rejection */
    uint8_t echo_req[ICMP_MIN_LEN + 4] = {
        0x08, 0x00, 0x00, 0x00, /* Type 8, Code 0, Bad Checksum 0 */
        0x12, 0x34, 0x00, 0x01, /* ID 0x1234, Seq 1 */
        'T', 'E', 'S', 'T'
    };
    if (icmp_handle_packet(lo, 0x7F000001, echo_req, sizeof(echo_req)) != NET_ERR_MALFORMED) {
        return -3;
    }

    /* Test 3: Correct checksum Echo Request and reply verification */
    icmp_echo_header_t *hdr = (icmp_echo_header_t *)echo_req;
    hdr->checksum = 0;
    hdr->checksum = net_htons(net_checksum(echo_req, sizeof(echo_req)));

    /* Verify checksum computes to 0 across packet */
    if (net_checksum(echo_req, sizeof(echo_req)) != 0) {
        return -4;
    }

    /* Reset ping stats and test echo reply handling */
    icmp_reset_ping_stats();
    uint8_t echo_rep[ICMP_MIN_LEN] = {
        0x00, 0x00, 0x00, 0x00,
        0x56, 0x78, 0x00, 0x05
    };
    icmp_echo_header_t *rep_hdr = (icmp_echo_header_t *)echo_rep;
    rep_hdr->checksum = net_htons(net_checksum(echo_rep, sizeof(echo_rep)));

    if (icmp_handle_packet(lo, 0x7F000001, echo_rep, sizeof(echo_rep)) != NET_OK) {
        return -5;
    }

    icmp_ping_stats_t stats;
    icmp_get_ping_stats(&stats);
    if (stats.packets_received != 1 || stats.last_id_received != 0x5678 || stats.last_seq_received != 5) {
        return -6;
    }

    return 0;
}

