#include "arp.h"
#include "../drivers/terminal.h"

/* Forward declaration of netstack IP retrieval */
uint32_t netstack_get_ip(void);
void net_ip_to_str(uint32_t ip, char *buf, size_t max_len);

static arp_entry_t s_arp_cache[ARP_CACHE_CAPACITY];

static void net_memcpy(void *dest, const void *src, size_t n)
{
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
}

void arp_init(void)
{
    for (uint32_t i = 0; i < ARP_CACHE_CAPACITY; i++) {
        s_arp_cache[i].ip = 0;
        s_arp_cache[i].is_valid = 0;
        s_arp_cache[i].last_updated_ticks = 0;
        for (int j = 0; j < ETH_ALEN; j++) {
            s_arp_cache[i].mac[j] = 0;
        }
    }
}

int arp_lookup(uint32_t ip, uint8_t *out_mac)
{
    if (!out_mac || ip == 0) return NET_ERR_INVALID_PARAM;

    for (uint32_t i = 0; i < ARP_CACHE_CAPACITY; i++) {
        if (s_arp_cache[i].is_valid && s_arp_cache[i].ip == ip) {
            net_memcpy(out_mac, s_arp_cache[i].mac, ETH_ALEN);
            return NET_OK;
        }
    }
    return NET_ERR_NOT_FOUND;
}

void arp_update_cache(uint32_t ip, const uint8_t *mac)
{
    if (ip == 0 || !mac) return;

    /* 1. Update existing entry if present */
    for (uint32_t i = 0; i < ARP_CACHE_CAPACITY; i++) {
        if (s_arp_cache[i].is_valid && s_arp_cache[i].ip == ip) {
            net_memcpy(s_arp_cache[i].mac, mac, ETH_ALEN);
            s_arp_cache[i].last_updated_ticks++;
            return;
        }
    }

    /* 2. Find first free slot */
    for (uint32_t i = 0; i < ARP_CACHE_CAPACITY; i++) {
        if (!s_arp_cache[i].is_valid) {
            s_arp_cache[i].ip = ip;
            net_memcpy(s_arp_cache[i].mac, mac, ETH_ALEN);
            s_arp_cache[i].is_valid = 1;
            s_arp_cache[i].last_updated_ticks = 1;
            return;
        }
    }

    /* 3. Evict slot 0 if cache is completely saturated */
    s_arp_cache[0].ip = ip;
    net_memcpy(s_arp_cache[0].mac, mac, ETH_ALEN);
    s_arp_cache[0].is_valid = 1;
    s_arp_cache[0].last_updated_ticks = 1;
}

int arp_handle_packet(net_device_t *dev, const void *payload, uint16_t len)
{
    if (!dev || !payload) return NET_ERR_INVALID_PARAM;
    if (len < sizeof(arp_header_t)) {
        dev->stats.rx_errors++;
        return NET_ERR_MALFORMED;
    }

    const arp_header_t *arp = (const arp_header_t *)payload;

    /* Validate Hardware and Protocol types */
    if (net_ntohs(arp->htype) != ARP_HTYPE_ETHERNET ||
        net_ntohs(arp->ptype) != ARP_PTYPE_IPV4 ||
        arp->hlen != ARP_HLEN_ETHERNET ||
        arp->plen != ARP_PLEN_IPV4) {
        dev->stats.rx_errors++;
        return NET_ERR_MALFORMED;
    }

    uint16_t oper = net_ntohs(arp->oper);

    /* Opportunistically update ARP cache from sender */
    arp_update_cache(arp->sender_ip, arp->sender_mac);

    /* If ARP Request targeted at our local IP, construct and transmit ARP Reply */
    if (oper == ARP_OP_REQUEST) {
        uint32_t my_ip = netstack_get_ip();
        if (arp->target_ip == my_ip && my_ip != 0) {
            arp_header_t reply_arp;
            reply_arp.htype = net_htons(ARP_HTYPE_ETHERNET);
            reply_arp.ptype = net_htons(ARP_PTYPE_IPV4);
            reply_arp.hlen  = ARP_HLEN_ETHERNET;
            reply_arp.plen  = ARP_PLEN_IPV4;
            reply_arp.oper  = net_htons(ARP_OP_REPLY);

            net_memcpy(reply_arp.sender_mac, dev->mac_addr, ETH_ALEN);
            reply_arp.sender_ip = my_ip;

            net_memcpy(reply_arp.target_mac, arp->sender_mac, ETH_ALEN);
            reply_arp.target_ip = arp->sender_ip;

            uint8_t out_frame[ETH_MAX_FRAME_LEN];
            int frame_len = eth_build_frame(out_frame, sizeof(out_frame),
                                            arp->sender_mac, dev->mac_addr,
                                            ETHERTYPE_ARP,
                                            &reply_arp, sizeof(reply_arp));
            if (frame_len > 0) {
                return netdev_transmit(dev, out_frame, (uint16_t)frame_len);
            }
        }
    }

    return NET_OK;
}

int arp_send_request(net_device_t *dev, uint32_t target_ip)
{
    if (!dev || target_ip == 0) return NET_ERR_INVALID_PARAM;

    arp_header_t req;
    req.htype = net_htons(ARP_HTYPE_ETHERNET);
    req.ptype = net_htons(ARP_PTYPE_IPV4);
    req.hlen  = ARP_HLEN_ETHERNET;
    req.plen  = ARP_PLEN_IPV4;
    req.oper  = net_htons(ARP_OP_REQUEST);

    net_memcpy(req.sender_mac, dev->mac_addr, ETH_ALEN);
    req.sender_ip = netstack_get_ip();

    for (int i = 0; i < ETH_ALEN; i++) req.target_mac[i] = 0x00;
    req.target_ip = target_ip;

    static const uint8_t bcast[ETH_ALEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    uint8_t out_frame[ETH_MAX_FRAME_LEN];
    int frame_len = eth_build_frame(out_frame, sizeof(out_frame),
                                    bcast, dev->mac_addr,
                                    ETHERTYPE_ARP,
                                    &req, sizeof(req));
    if (frame_len > 0) {
        return netdev_transmit(dev, out_frame, (uint16_t)frame_len);
    }
    return NET_ERR_TX_FAILED;
}

void arp_print_table(void)
{
    terminal_write("==================================================\n");
    terminal_write("         JARVIS-OS ARP CACHE RESOLUTION           \n");
    terminal_write("==================================================\n");
    terminal_write("IP ADDRESS       MAC ADDRESS        STATUS        \n");
    terminal_write("--------------------------------------------------\n");

    uint32_t valid_entries = 0;
    char ip_buf[16];
    char mac_buf[20];

    for (uint32_t i = 0; i < ARP_CACHE_CAPACITY; i++) {
        if (s_arp_cache[i].is_valid) {
            valid_entries++;
            net_ip_to_str(s_arp_cache[i].ip, ip_buf, sizeof(ip_buf));
            eth_format_mac(s_arp_cache[i].mac, mac_buf, sizeof(mac_buf));

            terminal_write(ip_buf);
            for (size_t s = 0; s < 17 - (size_t)(ip_buf[0] ? 10 : 0); s++) terminal_putchar(' ');
            terminal_write(mac_buf);
            terminal_write("  DYNAMIC\n");
        }
    }

    if (valid_entries == 0) {
        terminal_write("(ARP cache empty - no hosts resolved yet)\n");
    }
    terminal_write("--------------------------------------------------\n");
    terminal_write("Cache entries: ");
    terminal_write_number(valid_entries);
    terminal_write(" / Capacity: ");
    terminal_write_number(ARP_CACHE_CAPACITY);
    terminal_write("\n==================================================\n");
}

/* Invariant Verification */
int test_arp_invariants(void)
{
    arp_init();

    /* 1. Cache insertion and lookup */
    uint32_t ip1 = 0x0A000202; /* 10.0.2.2 */
    uint8_t mac1[6] = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x02 };
    arp_update_cache(ip1, mac1);

    uint8_t out_mac[6];
    if (arp_lookup(ip1, out_mac) != NET_OK) return -1;
    for (int i = 0; i < 6; i++) {
        if (out_mac[i] != mac1[i]) return -2;
    }

    /* 2. Miss lookup */
    if (arp_lookup(0x0A000203, out_mac) != NET_ERR_NOT_FOUND) return -3;

    /* 3. Underflow rejection */
    net_device_t *loop = netdev_get_loopback();
    uint8_t short_arp[20];
    if (arp_handle_packet(loop, short_arp, 20) != NET_ERR_MALFORMED) return -4;

    /* 4. Valid Request Packet parsing */
    arp_header_t req;
    req.htype = net_htons(ARP_HTYPE_ETHERNET);
    req.ptype = net_htons(ARP_PTYPE_IPV4);
    req.hlen  = ARP_HLEN_ETHERNET;
    req.plen  = ARP_PLEN_IPV4;
    req.oper  = net_htons(ARP_OP_REQUEST);
    net_memcpy(req.sender_mac, mac1, ETH_ALEN);
    req.sender_ip = ip1;
    for (int i = 0; i < ETH_ALEN; i++) req.target_mac[i] = 0;
    req.target_ip = 0x0A00020F; /* 10.0.2.15 */

    if (arp_handle_packet(loop, &req, sizeof(req)) != NET_OK) return -5;

    return 0;
}

