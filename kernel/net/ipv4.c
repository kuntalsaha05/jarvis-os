#include "ipv4.h"
#include "arp.h"

/* Forward protocol handler declarations */
int icmp_handle_packet(net_device_t *dev, uint32_t src_ip, const void *payload, uint16_t len);
int udp_handle_packet(net_device_t *dev, uint32_t src_ip, const void *payload, uint16_t len);
uint32_t netstack_get_ip(void);
uint32_t netstack_get_gateway(void);

static void net_memcpy(void *dest, const void *src, size_t n)
{
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
}

/* Internet 16-bit One's Complement Checksum */
uint16_t net_checksum(const void *data, size_t len)
{
    const uint8_t *ptr = (const uint8_t *)data;
    uint32_t sum = 0;

    while (len > 1) {
        uint16_t word = ((uint16_t)ptr[0] << 8) | (uint16_t)ptr[1];
        sum += word;
        ptr += 2;
        len -= 2;
    }

    if (len == 1) {
        sum += ((uint16_t)ptr[0] << 8);
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return (uint16_t)(~sum);
}

/* IPv4 Address String Conversion */
void net_ip_to_str(uint32_t ip, char *buf, size_t max_len)
{
    if (!buf || max_len < 16) return;
    uint8_t b1 = (uint8_t)((ip >> 24) & 0xFF);
    uint8_t b2 = (uint8_t)((ip >> 16) & 0xFF);
    uint8_t b3 = (uint8_t)((ip >> 8)  & 0xFF);
    uint8_t b4 = (uint8_t)(ip & 0xFF);

    uint8_t octets[4] = { b1, b2, b3, b4 };
    size_t idx = 0;

    for (int i = 0; i < 4; i++) {
        uint8_t val = octets[i];
        if (val >= 100) {
            buf[idx++] = '0' + (val / 100);
            val %= 100;
            buf[idx++] = '0' + (val / 10);
            val %= 10;
        } else if (val >= 10) {
            buf[idx++] = '0' + (val / 10);
            val %= 10;
        }
        buf[idx++] = '0' + val;

        if (i < 3) {
            buf[idx++] = '.';
        }
    }
    buf[idx] = '\0';
}

uint32_t net_str_to_ip(const char *str)
{
    if (!str) return 0;
    uint32_t ip = 0;
    uint32_t octet = 0;
    int octet_count = 0;

    while (*str) {
        if (*str >= '0' && *str <= '9') {
            octet = octet * 10 + (*str - '0');
            if (octet > 255) return 0;
        } else if (*str == '.') {
            ip = (ip << 8) | (octet & 0xFF);
            octet = 0;
            octet_count++;
            if (octet_count > 3) return 0;
        } else {
            break;
        }
        str++;
    }

    if (octet_count != 3) return 0;
    ip = (ip << 8) | (octet & 0xFF);
    return ip;
}

/* IPv4 Packet Construction */
static uint16_t s_ipv4_id_seq = 100;

int ipv4_build_packet(void *out_buf,
                      uint16_t max_buf_len,
                      uint32_t dest_ip,
                      uint32_t src_ip,
                      uint8_t protocol,
                      const void *payload,
                      uint16_t payload_len)
{
    if (!out_buf) return NET_ERR_INVALID_PARAM;
    uint16_t total_len = IPV4_MIN_HLEN + payload_len;
    if (total_len > max_buf_len || total_len > ETH_MTU) {
        return NET_ERR_BUFFER_BOUNDS;
    }

    uint8_t *buf = (uint8_t *)out_buf;
    ipv4_header_t *hdr = (ipv4_header_t *)buf;

    hdr->version_ihl = 0x45; /* Version 4, IHL 5 (20 bytes) */
    hdr->tos = 0x00;
    hdr->total_length = net_htons(total_len);
    hdr->id = net_htons(s_ipv4_id_seq++);
    hdr->flags_fragment = net_htons(0x4000); /* Don't Fragment flag */
    hdr->ttl = IPV4_DEFAULT_TTL;
    hdr->protocol = protocol;
    hdr->checksum = 0;
    hdr->src_ip = net_htonl(src_ip);
    hdr->dest_ip = net_htonl(dest_ip);

    /* Calculate header checksum */
    hdr->checksum = net_htons(net_checksum(hdr, IPV4_MIN_HLEN));

    /* Append payload */
    if (payload && payload_len > 0) {
        const uint8_t *src = (const uint8_t *)payload;
        for (uint16_t i = 0; i < payload_len; i++) {
            buf[IPV4_MIN_HLEN + i] = src[i];
        }
    }

    return (int)total_len;
}

int ipv4_parse_packet(const void *packet,
                      uint16_t packet_len,
                      ipv4_header_t *out_hdr,
                      const uint8_t **out_payload,
                      uint16_t *out_payload_len)
{
    if (!packet || packet_len < IPV4_MIN_HLEN) {
        return NET_ERR_MALFORMED;
    }

    const uint8_t *buf = (const uint8_t *)packet;
    const ipv4_header_t *hdr = (const ipv4_header_t *)buf;

    uint8_t version = (hdr->version_ihl >> 4) & 0x0F;
    uint8_t ihl_bytes = (hdr->version_ihl & 0x0F) * 4;

    if (version != 4 || ihl_bytes < IPV4_MIN_HLEN || ihl_bytes > packet_len) {
        return NET_ERR_MALFORMED;
    }

    /* Verify Checksum */
    if (net_checksum(hdr, ihl_bytes) != 0) {
        return NET_ERR_MALFORMED;
    }

    uint16_t total_len = net_ntohs(hdr->total_length);
    if (total_len > packet_len || total_len < ihl_bytes) {
        return NET_ERR_MALFORMED;
    }

    if (out_hdr) {
        *out_hdr = *hdr;
        out_hdr->total_length = total_len;
        out_hdr->src_ip = net_ntohl(hdr->src_ip);
        out_hdr->dest_ip = net_ntohl(hdr->dest_ip);
    }

    if (out_payload) {
        *out_payload = buf + ihl_bytes;
    }
    if (out_payload_len) {
        *out_payload_len = total_len - ihl_bytes;
    }

    return NET_OK;
}

int ipv4_handle_packet(net_device_t *dev, const void *payload, uint16_t len)
{
    if (!dev || !payload) return NET_ERR_INVALID_PARAM;

    ipv4_header_t hdr;
    const uint8_t *l4_payload = NULL;
    uint16_t l4_len = 0;

    int parse_res = ipv4_parse_packet(payload, len, &hdr, &l4_payload, &l4_len);
    if (parse_res != NET_OK) {
        dev->stats.rx_errors++;
        return parse_res;
    }

    /* Destination IP verification */
    uint32_t my_ip = netstack_get_ip();
    int is_for_me = (hdr.dest_ip == my_ip) ||
                    (hdr.dest_ip == 0xFFFFFFFF) ||
                    ((hdr.dest_ip & 0xFF000000) == 0x7F000000) ||
                    (dev->backend == HAL_BACKEND_MEMORY_BACKED);
    if (!is_for_me && my_ip != 0) {
        /* Packet not for us and not broadcast */
        return NET_OK;
    }

    switch (hdr.protocol) {
        case IPV4_PROTO_ICMP:
            return icmp_handle_packet(dev, hdr.src_ip, l4_payload, l4_len);

        case IPV4_PROTO_UDP:
            return udp_handle_packet(dev, hdr.src_ip, l4_payload, l4_len);

        default:
            dev->stats.rx_dropped++;
            return NET_ERR_NOT_FOUND;
    }
}

int ipv4_transmit(net_device_t *dev, uint32_t dest_ip, uint8_t protocol, const void *payload, uint16_t payload_len)
{
    if (!dev) return NET_ERR_INVALID_PARAM;

    uint8_t ip_buf[ETH_MTU];
    uint32_t src_ip = netstack_get_ip();
    int ip_len = ipv4_build_packet(ip_buf, sizeof(ip_buf), dest_ip, src_ip, protocol, payload, payload_len);
    if (ip_len <= 0) return NET_ERR_BUFFER_BOUNDS;

    /* Resolve Destination MAC Address */
    uint8_t dest_mac[ETH_ALEN];
    int mac_resolved = 0;

    if (dest_ip == 0xFFFFFFFF) {
        for (int i = 0; i < ETH_ALEN; i++) dest_mac[i] = 0xFF;
        mac_resolved = 1;
    } else if (dest_ip == src_ip || dev->backend == HAL_BACKEND_MEMORY_BACKED) {
        /* Loopback routing */
        net_memcpy(dest_mac, dev->mac_addr, ETH_ALEN);
        mac_resolved = 1;
    } else if (arp_lookup(dest_ip, dest_mac) == NET_OK) {
        mac_resolved = 1;
    } else {
        /* Check gateway */
        uint32_t gw = netstack_get_gateway();
        if (gw != 0 && arp_lookup(gw, dest_mac) == NET_OK) {
            mac_resolved = 1;
        }
    }

    if (!mac_resolved) {
        /* Trigger ARP request and send broadcast as fallback */
        arp_send_request(dev, dest_ip);
        for (int i = 0; i < ETH_ALEN; i++) dest_mac[i] = 0xFF;
    }

    uint8_t frame_buf[ETH_MAX_FRAME_LEN];
    int frame_len = eth_build_frame(frame_buf, sizeof(frame_buf),
                                    dest_mac, dev->mac_addr,
                                    ETHERTYPE_IPV4,
                                    ip_buf, (uint16_t)ip_len);
    if (frame_len <= 0) {
        return frame_len;
    }

    return netdev_transmit(dev, frame_buf, (uint16_t)frame_len);
}

/* Invariant Verification */
int test_ipv4_invariants(void)
{
    /* 1. IP string conversions */
    uint32_t ip = 0x0A00020F; /* 10.0.2.15 */
    char buf[20];
    net_ip_to_str(ip, buf, sizeof(buf));
    if (buf[0] != '1' || buf[1] != '0' || buf[2] != '.') return -1;
    if (net_str_to_ip(buf) != ip) return -2;

    /* 2. Packet construction and checksum */
    uint8_t pkt_buf[256];
    uint8_t test_payload[16] = "PING_IPV4_TEST!";
    int pkt_len = ipv4_build_packet(pkt_buf, sizeof(pkt_buf),
                                    0x0A000202, 0x0A00020F,
                                    IPV4_PROTO_ICMP,
                                    test_payload, 16);
    if (pkt_len != 36) return -3; /* 20 + 16 = 36 */

    /* 3. Valid packet parsing */
    ipv4_header_t hdr;
    const uint8_t *payload = NULL;
    uint16_t payload_len = 0;
    if (ipv4_parse_packet(pkt_buf, (uint16_t)pkt_len, &hdr, &payload, &payload_len) != NET_OK) return -4;
    if (hdr.protocol != IPV4_PROTO_ICMP) return -5;
    if (payload_len != 16) return -6;

    /* 4. Corrupted checksum rejection */
    pkt_buf[10] ^= 0xFF; /* Flip byte in checksum */
    if (ipv4_parse_packet(pkt_buf, (uint16_t)pkt_len, &hdr, &payload, &payload_len) != NET_ERR_MALFORMED) return -7;

    return 0;
}
