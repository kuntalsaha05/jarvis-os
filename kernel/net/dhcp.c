#include "dhcp.h"
#include "udp.h"
#include "netstack.h"

static dhcp_state_t s_dhcp_state = DHCP_STATE_INIT;
static uint32_t s_dhcp_xid = 0x55AA1234;
static uint32_t s_offered_ip = 0;
static uint32_t s_server_ip = 0;

static void net_memcpy(void *dest, const void *src, size_t n)
{
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
}

static void net_memset(void *dest, uint8_t val, size_t n)
{
    uint8_t *d = (uint8_t *)dest;
    for (size_t i = 0; i < n; i++) {
        d[i] = val;
    }
}

void dhcp_init(net_device_t *dev)
{
    (void)dev;
    s_dhcp_state = DHCP_STATE_INIT;
    s_dhcp_xid = 0x55AA1234;
    s_offered_ip = 0;
    s_server_ip = 0;

    udp_bind(DHCP_CLIENT_PORT, dhcp_handle_rx);
}

dhcp_state_t dhcp_get_state(void)
{
    return s_dhcp_state;
}

const char *dhcp_state_to_str(dhcp_state_t state)
{
    switch (state) {
        case DHCP_STATE_INIT:        return "INIT";
        case DHCP_STATE_DISCOVERING: return "DISCOVERING";
        case DHCP_STATE_REQUESTING:  return "REQUESTING";
        case DHCP_STATE_BOUND:       return "BOUND";
        case DHCP_STATE_FAILED:      return "FAILED";
        default:                     return "UNKNOWN";
    }
}

int dhcp_start_discovery(net_device_t *dev)
{
    if (!dev) return NET_ERR_INVALID_PARAM;

    dhcp_packet_t pkt;
    net_memset(&pkt, 0, sizeof(pkt));

    pkt.op = DHCP_BOOTREQUEST;
    pkt.htype = DHCP_HTYPE_ETHERNET;
    pkt.hlen = DHCP_HLEN_ETHERNET;
    pkt.hops = 0;
    pkt.xid = net_htonl(s_dhcp_xid);
    pkt.secs = 0;
    pkt.flags = net_htons(0x8000); /* Broadcast */

    net_memcpy(pkt.chaddr, dev->mac_addr, 6);
    pkt.magic_cookie = net_htonl(DHCP_MAGIC_COOKIE);

    int opt_idx = 0;
    /* Option 53: DHCP Message Type = DHCP_MSG_DISCOVER */
    pkt.options[opt_idx++] = DHCP_OPT_MESSAGE_TYPE;
    pkt.options[opt_idx++] = 1;
    pkt.options[opt_idx++] = DHCP_MSG_DISCOVER;

    /* Option 55: Parameter Request List */
    pkt.options[opt_idx++] = DHCP_OPT_PARAMETER_REQ;
    pkt.options[opt_idx++] = 3;
    pkt.options[opt_idx++] = DHCP_OPT_SUBNET_MASK;
    pkt.options[opt_idx++] = DHCP_OPT_ROUTER;
    pkt.options[opt_idx++] = DHCP_OPT_DNS_SERVER;

    /* Option 255: End */
    pkt.options[opt_idx++] = DHCP_OPT_END;

    s_dhcp_state = DHCP_STATE_DISCOVERING;

    /* Send broadcast to 255.255.255.255 */
    return udp_transmit(dev, 0xFFFFFFFF, DHCP_CLIENT_PORT, DHCP_SERVER_PORT, &pkt, sizeof(pkt));
}

static int dhcp_send_request(net_device_t *dev, uint32_t requested_ip, uint32_t server_ip)
{
    if (!dev) return NET_ERR_INVALID_PARAM;

    dhcp_packet_t pkt;
    net_memset(&pkt, 0, sizeof(pkt));

    pkt.op = DHCP_BOOTREQUEST;
    pkt.htype = DHCP_HTYPE_ETHERNET;
    pkt.hlen = DHCP_HLEN_ETHERNET;
    pkt.hops = 0;
    pkt.xid = net_htonl(s_dhcp_xid);
    pkt.secs = 0;
    pkt.flags = net_htons(0x8000); /* Broadcast */

    net_memcpy(pkt.chaddr, dev->mac_addr, 6);
    pkt.magic_cookie = net_htonl(DHCP_MAGIC_COOKIE);

    int opt_idx = 0;
    /* Option 53: DHCP Message Type = DHCP_MSG_REQUEST */
    pkt.options[opt_idx++] = DHCP_OPT_MESSAGE_TYPE;
    pkt.options[opt_idx++] = 1;
    pkt.options[opt_idx++] = DHCP_MSG_REQUEST;

    /* Option 50: Requested IP Address */
    pkt.options[opt_idx++] = DHCP_OPT_REQUESTED_IP;
    pkt.options[opt_idx++] = 4;
    uint32_t be_req = net_htonl(requested_ip);
    net_memcpy(&pkt.options[opt_idx], &be_req, 4);
    opt_idx += 4;

    /* Option 54: Server Identifier */
    pkt.options[opt_idx++] = DHCP_OPT_SERVER_ID;
    pkt.options[opt_idx++] = 4;
    uint32_t be_srv = net_htonl(server_ip);
    net_memcpy(&pkt.options[opt_idx], &be_srv, 4);
    opt_idx += 4;

    /* Option 255: End */
    pkt.options[opt_idx++] = DHCP_OPT_END;

    s_dhcp_state = DHCP_STATE_REQUESTING;

    return udp_transmit(dev, 0xFFFFFFFF, DHCP_CLIENT_PORT, DHCP_SERVER_PORT, &pkt, sizeof(pkt));
}

void dhcp_handle_rx(net_device_t *dev, uint32_t src_ip, uint16_t src_port, const void *payload, uint16_t len)
{
    (void)src_ip;
    (void)src_port;
    if (!dev || !payload || len < 240) return;

    const dhcp_packet_t *pkt = (const dhcp_packet_t *)payload;

    if (pkt->op != DHCP_BOOTREPLY) return;
    if (net_ntohl(pkt->magic_cookie) != DHCP_MAGIC_COOKIE) return;
    if (net_ntohl(pkt->xid) != s_dhcp_xid) return;

    /* Parse options */
    uint8_t msg_type = 0;
    uint32_t subnet_mask = 0xFFFFFF00; /* Default /24 */
    uint32_t router = 0;
    uint32_t dns = 0;
    uint32_t srv_id = 0;

    int idx = 0;
    int max_opt = len - 240;
    if (max_opt > (int)sizeof(pkt->options)) max_opt = sizeof(pkt->options);

    while (idx < max_opt) {
        uint8_t opt = pkt->options[idx++];
        if (opt == DHCP_OPT_END) break;
        if (opt == DHCP_OPT_PAD) continue;

        if (idx >= max_opt) break;
        uint8_t opt_len = pkt->options[idx++];
        if (idx + opt_len > max_opt) break;

        if (opt == DHCP_OPT_MESSAGE_TYPE && opt_len == 1) {
            msg_type = pkt->options[idx];
        } else if (opt == DHCP_OPT_SUBNET_MASK && opt_len == 4) {
            uint32_t be_val;
            net_memcpy(&be_val, &pkt->options[idx], 4);
            subnet_mask = net_ntohl(be_val);
        } else if (opt == DHCP_OPT_ROUTER && opt_len >= 4) {
            uint32_t be_val;
            net_memcpy(&be_val, &pkt->options[idx], 4);
            router = net_ntohl(be_val);
        } else if (opt == DHCP_OPT_DNS_SERVER && opt_len >= 4) {
            uint32_t be_val;
            net_memcpy(&be_val, &pkt->options[idx], 4);
            dns = net_ntohl(be_val);
        } else if (opt == DHCP_OPT_SERVER_ID && opt_len == 4) {
            uint32_t be_val;
            net_memcpy(&be_val, &pkt->options[idx], 4);
            srv_id = net_ntohl(be_val);
        }
        idx += opt_len;
    }

    if (s_dhcp_state == DHCP_STATE_DISCOVERING && msg_type == DHCP_MSG_OFFER) {
        s_offered_ip = net_ntohl(pkt->yiaddr);
        s_server_ip = srv_id;
        dhcp_send_request(dev, s_offered_ip, s_server_ip);
    } else if (s_dhcp_state == DHCP_STATE_REQUESTING && msg_type == DHCP_MSG_ACK) {
        uint32_t assigned_ip = net_ntohl(pkt->yiaddr);
        netstack_set_ip(assigned_ip, subnet_mask, router, dns);
        s_dhcp_state = DHCP_STATE_BOUND;
    } else if (msg_type == DHCP_MSG_NAK) {
        s_dhcp_state = DHCP_STATE_FAILED;
    }
}

void dhcp_fallback_static(net_device_t *dev)
{
    (void)dev;
    /* Static QEMU standard IP network configuration */
    uint32_t static_ip = 0x0A00020F;      /* 10.0.2.15 */
    uint32_t static_mask = 0xFFFFFF00;    /* 255.255.255.0 */
    uint32_t static_gw = 0x0A000202;      /* 10.0.2.2 */
    uint32_t static_dns = 0x0A000203;     /* 10.0.2.3 */

    netstack_set_ip(static_ip, static_mask, static_gw, static_dns);
    s_dhcp_state = DHCP_STATE_BOUND;
}

/* Invariant Verification for DHCP */
int test_dhcp_invariants(void)
{
    net_device_t *lo = netdev_get_by_name("lo0");
    if (!lo) return -1;

    dhcp_init(lo);
    if (dhcp_get_state() != DHCP_STATE_INIT) return -2;

    /* Test Fallback Static */
    dhcp_fallback_static(lo);
    if (dhcp_get_state() != DHCP_STATE_BOUND) return -3;
    if (netstack_get_ip() != 0x0A00020F) return -4;
    if (netstack_get_netmask() != 0xFFFFFF00) return -5;
    if (netstack_get_gateway() != 0x0A000202) return -6;
    if (netstack_get_dns() != 0x0A000203) return -7;

    /* Test Packet Generation */
    s_dhcp_state = DHCP_STATE_INIT;
    int disc_res = dhcp_start_discovery(lo);
    if (disc_res != NET_OK) return -8;
    if (dhcp_get_state() != DHCP_STATE_DISCOVERING) return -9;

    /* Test Simulated Offer RX */
    dhcp_packet_t offer_pkt;
    net_memset(&offer_pkt, 0, sizeof(offer_pkt));
    offer_pkt.op = DHCP_BOOTREPLY;
    offer_pkt.magic_cookie = net_htonl(DHCP_MAGIC_COOKIE);
    offer_pkt.xid = net_htonl(s_dhcp_xid);
    offer_pkt.yiaddr = net_htonl(0x0A000264); /* 10.0.2.100 */

    int opt_i = 0;
    offer_pkt.options[opt_i++] = DHCP_OPT_MESSAGE_TYPE;
    offer_pkt.options[opt_i++] = 1;
    offer_pkt.options[opt_i++] = DHCP_MSG_OFFER;

    offer_pkt.options[opt_i++] = DHCP_OPT_SERVER_ID;
    offer_pkt.options[opt_i++] = 4;
    uint32_t srv_be = net_htonl(0x0A000202);
    net_memcpy(&offer_pkt.options[opt_i], &srv_be, 4);
    opt_i += 4;
    offer_pkt.options[opt_i++] = DHCP_OPT_END;

    dhcp_handle_rx(lo, 0x0A000202, DHCP_SERVER_PORT, &offer_pkt, sizeof(offer_pkt));
    if (dhcp_get_state() != DHCP_STATE_REQUESTING) return -10;

    /* Test Simulated ACK RX */
    dhcp_packet_t ack_pkt;
    net_memset(&ack_pkt, 0, sizeof(ack_pkt));
    ack_pkt.op = DHCP_BOOTREPLY;
    ack_pkt.magic_cookie = net_htonl(DHCP_MAGIC_COOKIE);
    ack_pkt.xid = net_htonl(s_dhcp_xid);
    ack_pkt.yiaddr = net_htonl(0x0A000264);

    opt_i = 0;
    ack_pkt.options[opt_i++] = DHCP_OPT_MESSAGE_TYPE;
    ack_pkt.options[opt_i++] = 1;
    ack_pkt.options[opt_i++] = DHCP_MSG_ACK;

    ack_pkt.options[opt_i++] = DHCP_OPT_SUBNET_MASK;
    ack_pkt.options[opt_i++] = 4;
    uint32_t mask_be = net_htonl(0xFFFFFF00);
    net_memcpy(&ack_pkt.options[opt_i], &mask_be, 4);
    opt_i += 4;

    ack_pkt.options[opt_i++] = DHCP_OPT_ROUTER;
    ack_pkt.options[opt_i++] = 4;
    uint32_t rtr_be = net_htonl(0x0A000202);
    net_memcpy(&ack_pkt.options[opt_i], &rtr_be, 4);
    opt_i += 4;
    ack_pkt.options[opt_i++] = DHCP_OPT_END;

    dhcp_handle_rx(lo, 0x0A000202, DHCP_SERVER_PORT, &ack_pkt, sizeof(ack_pkt));
    if (dhcp_get_state() != DHCP_STATE_BOUND) return -11;
    if (netstack_get_ip() != 0x0A000264) return -12;

    /* Restore static IP for continued stability */
    dhcp_fallback_static(lo);

    return 0;
}
