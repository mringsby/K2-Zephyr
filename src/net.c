#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/net_ip.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "led.h"

LOG_MODULE_DECLARE(k2_app);

// Network configuration
#define UDP_PORT 12345
#define RECV_BUFFER_SIZE 64

// Static IP configuration
#define STATIC_IP_ADDR "192.168.1.100"
#define STATIC_NETMASK "255.255.255.0"
#define STATIC_GATEWAY "192.168.1.1"

// Network management callback for interface events
static struct net_mgmt_event_callback mgmt_cb;
bool network_ready = false;

// UDP socket and thread variables
int udp_sock = -1;
K_THREAD_STACK_DEFINE(udp_thread_stack, 2048);
struct k_thread udp_thread_data;

static void net_mgmt_event_handler(struct net_mgmt_event_callback *cb,
                                   uint64_t mgmt_event, struct net_if *iface)
{
    if (mgmt_event == NET_EVENT_IF_UP) {
        LOG_INF("Network interface is up - network is ready");
        network_ready = true;
    } else if (mgmt_event == NET_EVENT_IF_DOWN) {
        LOG_WRN("Network interface is down");
        network_ready = false;
    }
}

static int parse_ipv4_addr(const char *str, struct in_addr *addr)
{
    unsigned int a, b, c, d;
    
    if (sscanf(str, "%u.%u.%u.%u", &a, &b, &c, &d) != 4) {
        return -EINVAL;
    }
    
    if (a > 255 || b > 255 || c > 255 || d > 255) {
        return -EINVAL;
    }
    
    addr->s_addr = htonl((a << 24) | (b << 16) | (c << 8) | d);
    return 0;
}

static int configure_static_ip(struct net_if *iface)
{
    struct in_addr addr;
    struct in_addr netmask;
    struct in_addr gateway;
    int ret;

    // Configure IP address
    ret = parse_ipv4_addr(STATIC_IP_ADDR, &addr);
    if (ret < 0) {
        LOG_ERR("Invalid IP address format: %s", STATIC_IP_ADDR);
        return -EINVAL;
    }
    net_if_ipv4_addr_add(iface, &addr, NET_ADDR_MANUAL, 0);

    // Configure netmask
    ret = parse_ipv4_addr(STATIC_NETMASK, &netmask);
    if (ret < 0) {
        LOG_ERR("Invalid netmask format: %s", STATIC_NETMASK);
        return -EINVAL;
    }
    net_if_ipv4_set_netmask_by_addr(iface, &addr, &netmask);

    // Configure gateway
    ret = parse_ipv4_addr(STATIC_GATEWAY, &gateway);
    if (ret < 0) {
        LOG_ERR("Invalid gateway format: %s", STATIC_GATEWAY);
        return -EINVAL;
    }
    net_if_ipv4_set_gw(iface, &gateway);

    LOG_INF("Static IP configuration:");
    LOG_INF("  IP: %s", STATIC_IP_ADDR);
    LOG_INF("  Netmask: %s", STATIC_NETMASK);
    LOG_INF("  Gateway: %s", STATIC_GATEWAY);

    return 0;
}

void network_init(void)
{
    struct net_if *iface;
    int ret;

    LOG_INF("Initializing network with static IP...");

    iface = net_if_get_default();
    if (!iface) {
        LOG_ERR("No network interface found");
        return;
    }

    net_mgmt_init_event_callback(&mgmt_cb, net_mgmt_event_handler,
                                 NET_EVENT_IF_UP | NET_EVENT_IF_DOWN);
    net_mgmt_add_event_callback(&mgmt_cb);

    ret = configure_static_ip(iface);
    if (ret < 0) {
        LOG_ERR("Failed to configure static IP: %d", ret);
        return;
    }

    net_if_up(iface);
    
    // Give some time for the interface to come up
    k_sleep(K_MSEC(500));
    network_ready = true;
    
    LOG_INF("Static IP configuration complete");
}

void udp_server_thread(void *arg1, void *arg2, void *arg3)
{
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    struct sockaddr_in bind_addr;
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    char recv_buffer[RECV_BUFFER_SIZE];
    int ret;

    while (!network_ready) {
        k_sleep(K_MSEC(100));
    }

    LOG_INF("Starting UDP server on port %d", UDP_PORT);

    udp_sock = zsock_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udp_sock < 0) {
        LOG_ERR("Failed to create UDP socket: %d", udp_sock);
        return;
    }

    memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_addr.s_addr = INADDR_ANY;
    bind_addr.sin_port = htons(UDP_PORT);

    ret = zsock_bind(udp_sock, (struct sockaddr *)&bind_addr, sizeof(bind_addr));
    if (ret < 0) {
        LOG_ERR("Failed to bind UDP socket: %d (errno: %d)", ret, errno);
        zsock_close(udp_sock);
        return;
    }

    LOG_INF("UDP server listening on port %d", UDP_PORT);

    while (1) {
        ret = zsock_recvfrom(udp_sock, recv_buffer, sizeof(recv_buffer) - 1, 0,
                             (struct sockaddr *)&client_addr, &client_addr_len);

        if (ret > 0) {
            recv_buffer[ret] = '\0';
            
            char *endptr;
            long received_int = strtol(recv_buffer, &endptr, 10);
            
            if (endptr != recv_buffer && *endptr == '\0') {
                LOG_INF("Received integer: %ld from client - blinking LED", received_int);
                
                for (int i = 0; i < 3; i++) {
                    gpio_pin_set_dt(&led, 1);
                    k_sleep(K_MSEC(100));
                    gpio_pin_set_dt(&led, 0);
                    k_sleep(K_MSEC(100));
                }
                
                char ack_msg[32];
                snprintf(ack_msg, sizeof(ack_msg), "ACK: %ld", received_int);
                zsock_sendto(udp_sock, ack_msg, strlen(ack_msg), 0,
                             (struct sockaddr *)&client_addr, client_addr_len);
            } else {
                LOG_WRN("Received non-integer data: %s", recv_buffer);
            }
        } else if (ret < 0) {
            LOG_ERR("UDP receive error: %d (errno: %d)", ret, errno);
            k_sleep(K_MSEC(100));
        }
    }
}

void udp_server_start(void)
{
    k_thread_create(&udp_thread_data, udp_thread_stack,
                    K_THREAD_STACK_SIZEOF(udp_thread_stack),
                    udp_server_thread, NULL, NULL, NULL,
                    K_PRIO_COOP(7), 0, K_NO_WAIT);
}
