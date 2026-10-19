#include "netdev.h"

/* Static Device Registry */
static net_device_t *s_devices[NET_MAX_DEVICES];
static uint32_t s_device_count = 0;

/* Helper Memory Copy */
static void net_memcpy(void *dest, const void *src, size_t n)
{
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
}

/* Helper Memory Zero */
static void net_memset(void *dest, uint8_t val, size_t n)
{
    uint8_t *d = (uint8_t *)dest;
    for (size_t i = 0; i < n; i++) {
        d[i] = val;
    }
}

/* Helper String Compare */
static int net_strcmp(const char *s1, const char *s2)
{
    if (!s1 && !s2) return 0;
    if (!s1 || !s2) return (s1 ? 1 : -1);
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

/* Queue Lifecycle Functions */
void netdev_init_queue(net_rx_queue_t *q)
{
    if (!q) return;
    q->head = 0;
    q->tail = 0;
    q->count = 0;
    q->capacity = NET_RX_QUEUE_SIZE;
    q->dropped_packets = 0;
    for (uint32_t i = 0; i < NET_RX_QUEUE_SIZE; i++) {
        q->packets[i].len = 0;
    }
}

int netdev_enqueue_rx(net_device_t *dev, const void *frame, uint16_t len)
{
    if (!dev || !frame) return NET_ERR_INVALID_PARAM;
    if (len < ETH_HLEN || len > ETH_MAX_FRAME_LEN) return NET_ERR_BUFFER_BOUNDS;

    net_rx_queue_t *q = &dev->rx_queue;
    if (q->count >= q->capacity) {
        q->dropped_packets++;
        dev->stats.rx_dropped++;
        return NET_ERR_QUEUE_FULL;
    }

    net_packet_t *pkt = &q->packets[q->head];
    net_memcpy(pkt->data, frame, len);
    pkt->len = len;

    q->head = (q->head + 1) % q->capacity;
    q->count++;

    dev->stats.rx_packets++;
    dev->stats.rx_bytes += len;
    return NET_OK;
}

int netdev_dequeue_rx(net_device_t *dev, void *out_frame, uint16_t max_len)
{
    if (!dev || !out_frame) return NET_ERR_INVALID_PARAM;

    net_rx_queue_t *q = &dev->rx_queue;
    if (q->count == 0) {
        return NET_ERR_QUEUE_EMPTY;
    }

    net_packet_t *pkt = &q->packets[q->tail];
    if (max_len < pkt->len) {
        return NET_ERR_BUFFER_BOUNDS;
    }

    uint16_t len = pkt->len;
    net_memcpy(out_frame, pkt->data, len);

    q->tail = (q->tail + 1) % q->capacity;
    q->count--;

    return (int)len;
}

int netdev_transmit(net_device_t *dev, const void *frame, uint16_t len)
{
    if (!dev || !frame) return NET_ERR_INVALID_PARAM;
    if (!dev->is_up) return NET_ERR_DEVICE_DOWN;
    if (len < ETH_HLEN || len > ETH_MAX_FRAME_LEN) return NET_ERR_BUFFER_BOUNDS;

    if (!dev->transmit) {
        dev->stats.tx_errors++;
        return NET_ERR_TX_FAILED;
    }

    int res = dev->transmit(dev, frame, len);
    if (res == NET_OK) {
        dev->stats.tx_packets++;
        dev->stats.tx_bytes += len;
    } else {
        dev->stats.tx_errors++;
    }
    return res;
}

/* Device Registry Management */
int netdev_register(net_device_t *dev)
{
    if (!dev) return NET_ERR_INVALID_PARAM;

    /* Check if already registered */
    for (uint32_t i = 0; i < s_device_count; i++) {
        if (s_devices[i] == dev || net_strcmp(s_devices[i]->name, dev->name) == 0) {
            s_devices[i] = dev;
            return NET_OK;
        }
    }

    if (s_device_count >= NET_MAX_DEVICES) {
        return NET_ERR_QUEUE_FULL;
    }

    s_devices[s_device_count++] = dev;
    if (dev->init) {
        dev->init(dev);
    }
    return NET_OK;
}

net_device_t *netdev_get_by_name(const char *name)
{
    if (!name) return NULL;
    for (uint32_t i = 0; i < s_device_count; i++) {
        if (s_devices[i] && net_strcmp(s_devices[i]->name, name) == 0) {
            return s_devices[i];
        }
    }
    /* Fallback directly to built-in singletons */
    if (net_strcmp(name, "lo0") == 0) return netdev_get_loopback();
    if (net_strcmp(name, "eth0") == 0) return netdev_get_rtl8139();

    return NULL;
}

uint32_t netdev_get_count(void)
{
    return s_device_count;
}

net_device_t *netdev_get_by_index(uint32_t index)
{
    if (index >= s_device_count) return NULL;
    return s_devices[index];
}

/* Forward declarations */
static int loopback_transmit(net_device_t *dev, const void *frame, uint16_t len);
static int loopback_init(net_device_t *dev);
static int loopback_poll(net_device_t *dev);
static int loopback_set_mac(net_device_t *dev, const uint8_t *mac);

static int rtl8139_transmit_dev(net_device_t *dev, const void *frame, uint16_t len);
static int rtl8139_init_dev(net_device_t *dev);
static int rtl8139_poll_dev(net_device_t *dev);

/* Loopback Network Adapter Implementation */
static int loopback_transmit(net_device_t *dev, const void *frame, uint16_t len)
{
    /* Loopback immediately queues packet to its own RX queue */
    return netdev_enqueue_rx(dev, frame, len);
}

static int loopback_init(net_device_t *dev)
{
    if (!dev) return -1;
    dev->is_up = 1;
    dev->flags = NET_DEV_FLAG_UP | NET_DEV_FLAG_RUNNING | NET_DEV_FLAG_LOOPBACK;
    dev->transmit = loopback_transmit;
    dev->poll = loopback_poll;
    dev->set_mac = loopback_set_mac;
    return 0;
}

static int loopback_poll(net_device_t *dev)
{
    (void)dev;
    return 0;
}

static int loopback_set_mac(net_device_t *dev, const uint8_t *mac)
{
    if (!dev || !mac) return -1;
    net_memcpy(dev->mac_addr, mac, ETH_ALEN);
    return 0;
}

static net_device_t s_loopback_dev = {
    .name = "lo0",
    .mac_addr = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x01 }, /* Locally Administered Unicast */
    .mtu = ETH_MTU,
    .is_up = 1,
    .is_promiscuous = 0,
    .flags = NET_DEV_FLAG_UP | NET_DEV_FLAG_RUNNING | NET_DEV_FLAG_LOOPBACK,
    .backend = HAL_BACKEND_MEMORY_BACKED,
    .init = loopback_init,
    .transmit = loopback_transmit,
    .poll = loopback_poll,
    .set_mac = loopback_set_mac
};

net_device_t *netdev_get_loopback(void)
{
    static int initialized = 0;
    if (!initialized) {
        netdev_init_queue(&s_loopback_dev.rx_queue);
        net_memset(&s_loopback_dev.stats, 0, sizeof(s_loopback_dev.stats));
        s_loopback_dev.init = loopback_init;
        s_loopback_dev.transmit = loopback_transmit;
        s_loopback_dev.poll = loopback_poll;
        s_loopback_dev.set_mac = loopback_set_mac;
        s_loopback_dev.is_up = 1;
        s_loopback_dev.flags = NET_DEV_FLAG_UP | NET_DEV_FLAG_RUNNING | NET_DEV_FLAG_LOOPBACK;
        initialized = 1;
    }
    return &s_loopback_dev;
}

/* RTL8139 Emulated Network Adapter Implementation */
static int rtl8139_init_dev(net_device_t *dev)
{
    if (!dev) return -1;
    /* Device is available in emulated QEMU environment */
    dev->is_up = 1;
    dev->flags = NET_DEV_FLAG_UP | NET_DEV_FLAG_RUNNING | NET_DEV_FLAG_BROADCAST;
    dev->transmit = rtl8139_transmit_dev;
    dev->poll = rtl8139_poll_dev;
    dev->set_mac = loopback_set_mac;
    return 0;
}

static int rtl8139_transmit_dev(net_device_t *dev, const void *frame, uint16_t len)
{
    (void)dev;
    (void)frame;
    (void)len;
    /* Hardware transmit hook */
    return NET_OK;
}

static int rtl8139_poll_dev(net_device_t *dev)
{
    (void)dev;
    return 0;
}

static net_device_t s_rtl8139_dev = {
    .name = "eth0",
    .mac_addr = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x56 }, /* Standard QEMU Default MAC */
    .mtu = ETH_MTU,
    .is_up = 1,
    .is_promiscuous = 0,
    .flags = NET_DEV_FLAG_UP | NET_DEV_FLAG_RUNNING | NET_DEV_FLAG_BROADCAST,
    .backend = HAL_BACKEND_HARDWARE,
    .init = rtl8139_init_dev,
    .transmit = rtl8139_transmit_dev,
    .poll = rtl8139_poll_dev,
    .set_mac = loopback_set_mac
};

net_device_t *netdev_get_rtl8139(void)
{
    static int initialized = 0;
    if (!initialized) {
        netdev_init_queue(&s_rtl8139_dev.rx_queue);
        net_memset(&s_rtl8139_dev.stats, 0, sizeof(s_rtl8139_dev.stats));
        s_rtl8139_dev.init = rtl8139_init_dev;
        s_rtl8139_dev.transmit = rtl8139_transmit_dev;
        s_rtl8139_dev.poll = rtl8139_poll_dev;
        s_rtl8139_dev.set_mac = loopback_set_mac;
        s_rtl8139_dev.is_up = 1;
        s_rtl8139_dev.flags = NET_DEV_FLAG_UP | NET_DEV_FLAG_RUNNING | NET_DEV_FLAG_BROADCAST;
        initialized = 1;
    }
    return &s_rtl8139_dev;
}

/* Default Network Device Management */
static net_device_t *s_default_dev = NULL;

net_device_t *netdev_get_default(void)
{
    if (!s_default_dev) {
        s_default_dev = netdev_get_loopback();
    }
    return s_default_dev;
}

void netdev_set_default(net_device_t *dev)
{
    if (dev) {
        s_default_dev = dev;
    }
}

/* Self-test Invariants */
int test_netdev_invariants(void)
{
    net_device_t test_dev;
    net_memset(&test_dev, 0, sizeof(test_dev));
    test_dev.name[0] = 't'; test_dev.name[1] = '0'; test_dev.name[2] = '\0';
    test_dev.is_up = 1;
    test_dev.mtu = 1500;
    test_dev.transmit = loopback_transmit;
    netdev_init_queue(&test_dev.rx_queue);

    /* 1. Buffer length boundaries */
    uint8_t small_buf[10];
    if (netdev_enqueue_rx(&test_dev, small_buf, 10) != NET_ERR_BUFFER_BOUNDS) return -1;
    if (netdev_enqueue_rx(&test_dev, small_buf, 0) != NET_ERR_BUFFER_BOUNDS) return -2;

    uint8_t valid_frame[64];
    net_memset(valid_frame, 0xAA, 64);

    /* 2. Fill queue to capacity (NET_RX_QUEUE_SIZE = 16) */
    for (uint32_t i = 0; i < NET_RX_QUEUE_SIZE; i++) {
        if (netdev_enqueue_rx(&test_dev, valid_frame, 64) != NET_OK) return -3;
    }
    if (test_dev.rx_queue.count != NET_RX_QUEUE_SIZE) return -4;

    /* 3. Queue overflow test: 17th packet must be rejected */
    if (netdev_enqueue_rx(&test_dev, valid_frame, 64) != NET_ERR_QUEUE_FULL) return -5;
    if (test_dev.rx_queue.dropped_packets != 1) return -6;
    if (test_dev.stats.rx_dropped != 1) return -7;

    /* 4. Dequeue and verify FIFO integrity */
    uint8_t out_buf[128];
    for (uint32_t i = 0; i < NET_RX_QUEUE_SIZE; i++) {
        int len = netdev_dequeue_rx(&test_dev, out_buf, sizeof(out_buf));
        if (len != 64) return -8;
        if (out_buf[0] != 0xAA) return -9;
    }
    if (test_dev.rx_queue.count != 0) return -10;

    /* 5. Underflow test: dequeue from empty queue */
    if (netdev_dequeue_rx(&test_dev, out_buf, sizeof(out_buf)) != NET_ERR_QUEUE_EMPTY) return -11;

    /* 6. Device Down Transmit Check */
    test_dev.is_up = 0;
    if (netdev_transmit(&test_dev, valid_frame, 64) != NET_ERR_DEVICE_DOWN) return -12;

    return 0;
}

