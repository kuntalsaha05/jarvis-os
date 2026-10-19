#include "udp.h"
#include "netdev.h"
#include "ipv4.h"

typedef struct {
    uint16_t port;
    udp_rx_callback_t callback;
    uint8_t active;
} udp_port_entry_t;

static udp_port_entry_t s_udp_ports[UDP_MAX_PORTS];

static void net_memcpy(void *dest, const void *src, size_t n)
{
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
}

int udp_bind(uint16_t port, udp_rx_callback_t callback)
{
    if (port == 0 || !callback) return NET_ERR_INVALID_PARAM;

    /* Update existing binding if present */
    for (int i = 0; i < UDP_MAX_PORTS; i++) {
        if (s_udp_ports[i].active && s_udp_ports[i].port == port) {
            s_udp_ports[i].callback = callback;
            return NET_OK;
        }
    }

    /* Find an empty slot */
    for (int i = 0; i < UDP_MAX_PORTS; i++) {
        if (!s_udp_ports[i].active) {
            s_udp_ports[i].port = port;
            s_udp_ports[i].callback = callback;
            s_udp_ports[i].active = 1;
            return NET_OK;
        }
    }

    return NET_ERR_QUEUE_FULL;
}

int udp_unbind(uint16_t port)
{
    for (int i = 0; i < UDP_MAX_PORTS; i++) {
        if (s_udp_ports[i].active && s_udp_ports[i].port == port) {
            s_udp_ports[i].active = 0;
            s_udp_ports[i].port = 0;
            s_udp_ports[i].callback = NULL;
            return NET_OK;
        }
    }
    return NET_ERR_NOT_FOUND;
}

int udp_handle_packet(net_device_t *dev, uint32_t src_ip, const void *payload, uint16_t len)
{
    if (!dev || !payload) return NET_ERR_INVALID_PARAM;
    if (len < UDP_HEADER_LEN) return NET_ERR_MALFORMED;

    const udp_header_t *hdr = (const udp_header_t *)payload;
    uint16_t udp_len = net_ntohs(hdr->length);

    if (udp_len < UDP_HEADER_LEN || udp_len > len) {
        return NET_ERR_MALFORMED;
    }

    uint16_t src_port = net_ntohs(hdr->src_port);
    uint16_t dest_port = net_ntohs(hdr->dest_port);
    const uint8_t *udp_data = ((const uint8_t *)payload) + UDP_HEADER_LEN;
    uint16_t data_len = udp_len - UDP_HEADER_LEN;

    /* Lookup registered port handler */
    for (int i = 0; i < UDP_MAX_PORTS; i++) {
        if (s_udp_ports[i].active && s_udp_ports[i].port == dest_port) {
            s_udp_ports[i].callback(dev, src_ip, src_port, udp_data, data_len);
            return NET_OK;
        }
    }

    /* No listening socket found */
    return NET_ERR_NOT_FOUND;
}

int udp_transmit(net_device_t *dev,
                 uint32_t dest_ip,
                 uint16_t src_port,
                 uint16_t dest_port,
                 const void *payload,
                 uint16_t payload_len)
{
    if (!dev) return NET_ERR_INVALID_PARAM;

    uint16_t total_len = UDP_HEADER_LEN + payload_len;
    if (total_len > (ETH_MTU - IPV4_MIN_HLEN)) {
        return NET_ERR_BUFFER_BOUNDS;
    }

    uint8_t packet[ETH_MTU];
    udp_header_t *hdr = (udp_header_t *)packet;
    hdr->src_port = net_htons(src_port);
    hdr->dest_port = net_htons(dest_port);
    hdr->length = net_htons(total_len);
    hdr->checksum = 0; /* Optional in IPv4 */

    if (payload && payload_len > 0) {
        net_memcpy(packet + UDP_HEADER_LEN, payload, payload_len);
    }

    return ipv4_transmit(dev, dest_ip, IPV4_PROTO_UDP, packet, total_len);
}

/* Invariant Verification for UDP */
static int s_test_udp_received = 0;
static uint16_t s_test_udp_len = 0;

static void test_udp_handler(net_device_t *dev, uint32_t src_ip, uint16_t src_port, const void *payload, uint16_t len)
{
    (void)dev;
    (void)src_ip;
    (void)src_port;
    (void)payload;
    s_test_udp_received = 1;
    s_test_udp_len = len;
}

int test_udp_invariants(void)
{
    /* Test 1: Binding and dispatch */
    s_test_udp_received = 0;
    s_test_udp_len = 0;

    int bind_res = udp_bind(9999, test_udp_handler);
    if (bind_res != NET_OK) return -1;

    net_device_t *lo = netdev_get_by_name("lo0");
    if (!lo) return -2;

    uint8_t valid_udp[UDP_HEADER_LEN + 4] = {
        0x12, 0x34,             /* src port 0x1234 */
        0x27, 0x0F,             /* dest port 9999 (0x270F) */
        0x00, 0x0C,             /* length = 12 */
        0x00, 0x00,             /* checksum 0 */
        'P', 'I', 'N', 'G'      /* payload 4 bytes */
    };

    int handle_res = udp_handle_packet(lo, 0x7F000001, valid_udp, sizeof(valid_udp));
    if (handle_res != NET_OK || s_test_udp_received != 1 || s_test_udp_len != 4) {
        return -3;
    }

    /* Test 2: Unbound port rejection */
    uint8_t unbound_udp[UDP_HEADER_LEN + 2] = {
        0x12, 0x34,
        0x11, 0x22,             /* dest port 4386 (unbound) */
        0x00, 0x0A,
        0x00, 0x00,
        'O', 'K'
    };
    if (udp_handle_packet(lo, 0x7F000001, unbound_udp, sizeof(unbound_udp)) != NET_ERR_NOT_FOUND) {
        return -4;
    }

    /* Test 3: Malformed packet lengths */
    uint8_t short_udp[4] = {0x12, 0x34, 0x27, 0x0F};
    if (udp_handle_packet(lo, 0x7F000001, short_udp, sizeof(short_udp)) != NET_ERR_MALFORMED) {
        return -5;
    }

    /* Test 4: Length field larger than buffer */
    valid_udp[2] = 0x27; valid_udp[3] = 0x0F;
    valid_udp[4] = 0x00; valid_udp[5] = 0x50; /* claims 80 bytes */
    if (udp_handle_packet(lo, 0x7F000001, valid_udp, sizeof(valid_udp)) != NET_ERR_MALFORMED) {
        return -6;
    }

    /* Test 5: Clean unbind */
    if (udp_unbind(9999) != NET_OK) return -7;
    valid_udp[4] = 0x00; valid_udp[5] = 0x0C; /* restore valid length 12 */
    s_test_udp_received = 0;
    if (udp_handle_packet(lo, 0x7F000001, valid_udp, sizeof(valid_udp)) != NET_ERR_NOT_FOUND) {
        return -8;
    }
    if (s_test_udp_received != 0) return -9;

    return 0;
}

