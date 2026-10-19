#include "netstack.h"
#include "../drivers/terminal.h"

/* Network IP Configuration State */
static uint32_t s_net_ip = 0x0A00020F;       /* 10.0.2.15 */
static uint32_t s_net_mask = 0xFFFFFF00;     /* 255.255.255.0 */
static uint32_t s_net_gateway = 0x0A000202;   /* 10.0.2.2 */
static uint32_t s_net_dns = 0x0A000203;       /* 10.0.2.3 */

static int s_netstack_initialized = 0;

void netstack_init(void)
{
    if (s_netstack_initialized) return;

    /* Register standard devices */
    netdev_register(netdev_get_loopback());
    netdev_register(netdev_get_rtl8139());

    /* Initialize ARP table */
    arp_init();

    /* Initialize DHCP */
    net_device_t *eth = netdev_get_by_name("eth0");
    if (eth) {
        dhcp_init(eth);
        dhcp_fallback_static(eth);
    }

    s_netstack_initialized = 1;
}

void netstack_poll(void)
{
    uint32_t dev_count = netdev_get_count();
    uint8_t rx_buf[ETH_MAX_FRAME_LEN];

    for (uint32_t i = 0; i < dev_count; i++) {
        net_device_t *dev = netdev_get_by_index(i);
        if (!dev || !(dev->flags & NET_DEV_FLAG_UP)) continue;

        /* Drain RX queue */
        int rx_len = 0;
        int max_drain = NET_RX_QUEUE_SIZE;
        while (max_drain-- > 0 && (rx_len = netdev_dequeue_rx(dev, rx_buf, sizeof(rx_buf))) > 0) {
            eth_handle_rx(dev, rx_buf, (uint16_t)rx_len);
        }
    }
}

uint32_t netstack_get_ip(void)
{
    return s_net_ip;
}

uint32_t netstack_get_netmask(void)
{
    return s_net_mask;
}

uint32_t netstack_get_gateway(void)
{
    return s_net_gateway;
}

uint32_t netstack_get_dns(void)
{
    return s_net_dns;
}

void netstack_set_ip(uint32_t ip, uint32_t netmask, uint32_t gateway, uint32_t dns)
{
    s_net_ip = ip;
    s_net_mask = netmask;
    s_net_gateway = gateway;
    s_net_dns = dns;
}

void netstack_print_status(void)
{
    terminal_write("=====================================================\n");
    terminal_write("         JARVIS-OS MODULAR NETWORKING STACK          \n");
    terminal_write("=====================================================\n");

    char ip_str[20];
    char mask_str[20];
    char gw_str[20];
    char dns_str[20];

    net_ip_to_str(s_net_ip, ip_str, sizeof(ip_str));
    net_ip_to_str(s_net_mask, mask_str, sizeof(mask_str));
    net_ip_to_str(s_net_gateway, gw_str, sizeof(gw_str));
    net_ip_to_str(s_net_dns, dns_str, sizeof(dns_str));

    terminal_write("IPv4 Address   : ");
    terminal_write(ip_str);
    terminal_write("\nSubnet Mask    : ");
    terminal_write(mask_str);
    terminal_write("\nDefault Gateway: ");
    terminal_write(gw_str);
    terminal_write("\nPrimary DNS    : ");
    terminal_write(dns_str);
    terminal_write("\nDHCP State     : ");
    terminal_write(dhcp_state_to_str(dhcp_get_state()));
    terminal_write("\n-----------------------------------------------------\n");
    terminal_write("INTERFACE   MAC ADDRESS        TYPE        TX/RX PACKETS\n");
    terminal_write("-----------------------------------------------------\n");

    uint32_t count = netdev_get_count();
    char mac_buf[24];

    for (uint32_t i = 0; i < count; i++) {
        net_device_t *dev = netdev_get_by_index(i);
        if (!dev) continue;

        eth_format_mac(dev->mac_addr, mac_buf, sizeof(mac_buf));
        terminal_write(dev->name);
        terminal_write("         ");
        terminal_write(mac_buf);
        terminal_write("  ");

        if (dev->flags & NET_DEV_FLAG_LOOPBACK) {
            terminal_write("LOOPBACK    ");
        } else {
            terminal_write("ETHERNET    ");
        }

        terminal_write_number(dev->stats.tx_packets);
        terminal_putchar('/');
        terminal_write_number(dev->stats.rx_packets);
        terminal_write("\n            Bytes: TX ");
        terminal_write_number(dev->stats.tx_bytes);
        terminal_write(", RX ");
        terminal_write_number(dev->stats.rx_bytes);
        terminal_write(", Drop ");
        terminal_write_number(dev->stats.rx_dropped);
        terminal_write(", Err ");
        terminal_write_number(dev->stats.rx_errors);
        terminal_write("\n");
    }
    terminal_write("=====================================================\n");
}

int netstack_ping(const char *dest_ip_str, uint16_t count)
{
    if (!dest_ip_str) return NET_ERR_INVALID_PARAM;

    uint32_t target_ip = net_str_to_ip(dest_ip_str);
    if (target_ip == 0) {
        terminal_write("ping: invalid IPv4 address format\n");
        return NET_ERR_INVALID_PARAM;
    }

    net_device_t *dev = NULL;
    if ((target_ip & 0xFF000000) == 0x7F000000 || target_ip == s_net_ip) {
        dev = netdev_get_by_name("lo0");
    } else {
        dev = netdev_get_by_name("eth0");
    }

    if (!dev) {
        terminal_write("ping: no suitable network device available\n");
        return NET_ERR_NOT_FOUND;
    }

    terminal_write("PING ");
    terminal_write(dest_ip_str);
    terminal_write(" (56 bytes of data):\n");

    icmp_reset_ping_stats();

    for (uint16_t seq = 1; seq <= count; seq++) {
        const char *payload = "JARVIS-OS-PING";
        int send_res = icmp_send_echo(dev, target_ip, 0x1337, seq, payload, 14);
        if (send_res != NET_OK) {
            terminal_write("ping: transmit error\n");
            continue;
        }

        /* Poll stack to process loopback / replies */
        netstack_poll();

        icmp_ping_stats_t stats;
        icmp_get_ping_stats(&stats);
        if (stats.packets_received >= seq) {
            terminal_write("64 bytes from ");
            terminal_write(dest_ip_str);
            terminal_write(": icmp_seq=");
            terminal_write_number(seq);
            terminal_write(" ttl=64 time<1ms\n");
        } else {
            terminal_write("Request timeout for icmp_seq ");
            terminal_write_number(seq);
            terminal_write("\n");
        }
    }

    icmp_ping_stats_t final_stats;
    icmp_get_ping_stats(&final_stats);
    terminal_write("--- ");
    terminal_write(dest_ip_str);
    terminal_write(" ping statistics ---\n");
    terminal_write_number(final_stats.packets_transmitted);
    terminal_write(" packets transmitted, ");
    terminal_write_number(final_stats.packets_received);
    terminal_write(" received\n");

    return NET_OK;
}

/* End-to-end loopback integration test helper */
static volatile int s_e2e_udp_received = 0;
static void e2e_udp_callback(net_device_t *dev, uint32_t src_ip, uint16_t src_port, const void *payload, uint16_t len)
{
    (void)dev;
    (void)src_ip;
    (void)src_port;
    if (len == 5 && ((const char *)payload)[0] == 'H' && ((const char *)payload)[4] == 'O') {
        s_e2e_udp_received = 1;
    }
}

/* Comprehensive Unified Invariant Verification Suite */
int test_networking_invariants(void)
{
    /* 1. Device Invariants */
    if (test_netdev_invariants() != 0) return -1;

    /* 2. Ethernet Layer Invariants */
    if (test_ethernet_invariants() != 0) return -2;

    /* 3. ARP Layer Invariants */
    if (test_arp_invariants() != 0) return -3;

    /* 4. IPv4 Layer Invariants */
    if (test_ipv4_invariants() != 0) return -4;

    /* 5. ICMP Layer Invariants */
    if (test_icmp_invariants() != 0) return -5;

    /* 6. UDP Layer Invariants */
    if (test_udp_invariants() != 0) return -6;

    /* 7. DHCP Client Invariants */
    if (test_dhcp_invariants() != 0) return -7;

    /* 8. End-to-End Loopback UDP Packet Flow */
    netstack_init();
    net_device_t *lo = netdev_get_by_name("lo0");
    if (!lo) return -8;

    s_e2e_udp_received = 0;
    if (udp_bind(7777, e2e_udp_callback) != NET_OK) return -9;

    /* Transmit UDP packet to 127.0.0.1 on lo0 */
    uint32_t loopback_ip = 0x7F000001;
    int tx_res = udp_transmit(lo, loopback_ip, 54321, 7777, "HELLO", 5);
    if (tx_res != NET_OK) return -10;

    /* Poll network stack to drain loopback receive queue and dispatch */
    netstack_poll();
    if (s_e2e_udp_received != 1) return -11;

    udp_unbind(7777);

    /* 9. End-to-End Loopback ICMP Echo Request / Reply Flow */
    icmp_reset_ping_stats();
    int ping_tx = icmp_send_echo(lo, loopback_ip, 0x4321, 1, "ECHO_INVARIANT", 14);
    if (ping_tx != NET_OK) return -12;

    /* Poll network stack: processes request, transmits reply into loopback queue, then processes reply */
    netstack_poll(); /* processes request, queues echo reply */
    netstack_poll(); /* processes echo reply */

    icmp_ping_stats_t ping_stats;
    icmp_get_ping_stats(&ping_stats);
    if (ping_stats.packets_received != 1 || ping_stats.last_id_received != 0x4321 || ping_stats.last_seq_received != 1) {
        return -13;
    }

    return 0;
}
