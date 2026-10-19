#ifndef NETDEV_H
#define NETDEV_H

#include <stdint.h>
#include <stddef.h>
#include "../hal/hal.h"

/* Ethernet Standard Sizing */
#define ETH_ALEN           6      /* Octets in one Ethernet address */
#define ETH_HLEN           14     /* Total octets in header */
#define ETH_MIN_FRAME_LEN  60     /* Min octets in frame (without 4-byte FCS) */
#define ETH_MAX_FRAME_LEN  1518   /* Max octets in frame (with 4-byte FCS) */
#define ETH_MTU            1500   /* Maximum Transmission Unit */

/* Network Return & Error Codes */
#define NET_OK                 0
#define NET_ERR_INVALID_PARAM -1
#define NET_ERR_BUFFER_BOUNDS -2
#define NET_ERR_QUEUE_FULL    -3
#define NET_ERR_QUEUE_EMPTY   -4
#define NET_ERR_DEVICE_DOWN   -5
#define NET_ERR_TX_FAILED     -6
#define NET_ERR_MALFORMED     -7
#define NET_ERR_NOT_FOUND     -8
#define NET_ERR_PERMISSION    -9

/* Device Operational Flags */
#define NET_DEV_FLAG_UP        0x01
#define NET_DEV_FLAG_RUNNING   0x02
#define NET_DEV_FLAG_LOOPBACK  0x04
#define NET_DEV_FLAG_BROADCAST 0x08

/* Bounded Receive Queue Sizing */
#define NET_RX_QUEUE_SIZE 16
#define NET_MAX_DEVICES   8

/* In-memory packet slot */
typedef struct {
    uint8_t  data[ETH_MAX_FRAME_LEN];
    uint16_t len;
} net_packet_t;

/* Bounded lock-free circular packet ring buffer */
typedef struct {
    net_packet_t packets[NET_RX_QUEUE_SIZE];
    uint32_t     head;
    uint32_t     tail;
    uint32_t     count;
    uint32_t     capacity;
    uint64_t     dropped_packets;
} net_rx_queue_t;

/* Device Telemetry & Statistics */
typedef struct {
    uint64_t tx_packets;
    uint64_t tx_bytes;
    uint64_t tx_errors;
    uint64_t tx_dropped;

    uint64_t rx_packets;
    uint64_t rx_bytes;
    uint64_t rx_errors;
    uint64_t rx_dropped;
} net_stats_t;

struct net_device;

/* Network Device Abstraction */
typedef struct net_device {
    char               name[16];                /* Interface identifier (e.g., "eth0", "lo0") */
    uint8_t            mac_addr[ETH_ALEN];      /* Hardware MAC Address */
    uint16_t           mtu;                     /* Maximum transmission unit (typically 1500) */
    int                is_up;                   /* 1 if link is active, 0 otherwise */
    int                is_promiscuous;          /* 1 if promiscuous mode enabled */
    uint32_t           flags;                   /* NET_DEV_FLAG_* */
    hal_backend_type_t backend;                 /* Hardware vs Memory-backed vs Virtual */

    net_rx_queue_t     rx_queue;                /* Bounded static receive buffer */
    net_stats_t        stats;                   /* Monotonic operational telemetry */

    /* Driver Operations */
    int (*init)(struct net_device *dev);
    int (*transmit)(struct net_device *dev, const void *frame, uint16_t len);
    int (*poll)(struct net_device *dev);
    int (*set_mac)(struct net_device *dev, const uint8_t *mac);
} net_device_t;

/* Queue Lifecycle Functions */
void netdev_init_queue(net_rx_queue_t *q);
int netdev_enqueue_rx(net_device_t *dev, const void *frame, uint16_t len);
int netdev_dequeue_rx(net_device_t *dev, void *out_frame, uint16_t max_len);

/* Device Management Functions */
int netdev_register(net_device_t *dev);
net_device_t *netdev_get_by_name(const char *name);
uint32_t netdev_get_count(void);
net_device_t *netdev_get_by_index(uint32_t index);

net_device_t *netdev_get_default(void);
void netdev_set_default(net_device_t *dev);
int netdev_transmit(net_device_t *dev, const void *frame, uint16_t len);

/* Built-in Adapters */
net_device_t *netdev_get_loopback(void);
net_device_t *netdev_get_rtl8139(void);

/* Self-test Invariants */
int test_netdev_invariants(void);

#endif /* NETDEV_H */

