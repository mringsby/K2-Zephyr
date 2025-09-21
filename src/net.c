// Zephyr RTOS kernel and logging headers
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
// Zephyr networking stack headers for socket operations
#include <zephyr/net/socket.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/net_ip.h>
// Standard C library headers for string manipulation and I/O
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

// Include LED control header for visual feedback
#include "led.h"

// Declare this module for logging purposes
LOG_MODULE_DECLARE(k2_app);

// Network configuration constants
#define UDP_PORT 12345          // Port number for UDP server to listen on
#define RECV_BUFFER_SIZE 64     // Buffer size for incoming UDP messages (increased for struct)

// Packet structure definition
typedef struct {
    uint32_t sequence;  // Sequence number
    uint64_t payload;   // Payload data
    uint32_t crc32;     // CRC32 checksum
} __attribute__((packed)) udp_packet_t;

// Static IP configuration - customize these for your network
#define STATIC_IP_ADDR "192.168.1.100"   // Device's static IP address
#define STATIC_NETMASK "255.255.255.0"   // Subnet mask
#define STATIC_GATEWAY "192.168.1.1"     // Default gateway address

// Network management callback structure for handling interface events
static struct net_mgmt_event_callback mgmt_cb;
bool network_ready = false;  // Flag to track network interface status

// UDP socket and thread management variables
int udp_sock = -1;                                    // UDP socket file descriptor
K_THREAD_STACK_DEFINE(udp_thread_stack, 2048);      // Stack space for UDP thread
struct k_thread udp_thread_data;                     // Thread control block

/**
 * Network management event handler - called when network interface events occur
 * @param cb: Callback structure (unused)
 * @param mgmt_event: Type of network management event
 * @param iface: Network interface that generated the event
 */
static void net_mgmt_event_handler(struct net_mgmt_event_callback *cb,
                                   uint64_t mgmt_event, struct net_if *iface)
{
    // Check if network interface came up
    if (mgmt_event == NET_EVENT_IF_UP) {
        LOG_INF("Network interface is up - network is ready");
        network_ready = true;  // Set flag to indicate network is available
    } 
    // Check if network interface went down
    else if (mgmt_event == NET_EVENT_IF_DOWN) {
        LOG_WRN("Network interface is down");
        network_ready = false;  // Clear network ready flag
    }
}

/**
 * Parse IPv4 address string into binary format
 * @param str: String representation of IP address (e.g., "192.168.1.100")
 * @param addr: Output structure to store parsed address
 * @return: 0 on success, negative error code on failure
 */
static int parse_ipv4_addr(const char *str, struct in_addr *addr)
{
    unsigned int a, b, c, d;  // Individual octets of IP address
    
    // Parse the IP address string into four octets
    if (sscanf(str, "%u.%u.%u.%u", &a, &b, &c, &d) != 4) {
        return -EINVAL;  // Invalid format - couldn't parse 4 numbers
    }
    
    // Validate that each octet is in valid range (0-255)
    if (a > 255 || b > 255 || c > 255 || d > 255) {
        return -EINVAL;  // One or more octets out of range
    }
    
    // Convert to network byte order and store in structure
    addr->s_addr = htonl((a << 24) | (b << 16) | (c << 8) | d);
    return 0;  // Success
}

/**
 * Configure static IP settings for the network interface
 * @param iface: Network interface to configure
 * @return: 0 on success, negative error code on failure
 */
static int configure_static_ip(struct net_if *iface)
{
    struct in_addr addr;     // IP address structure
    struct in_addr netmask;  // Netmask structure
    struct in_addr gateway;  // Gateway address structure
    int ret;

    // Configure IP address
    ret = parse_ipv4_addr(STATIC_IP_ADDR, &addr);
    if (ret < 0) {
        LOG_ERR("Invalid IP address format: %s", STATIC_IP_ADDR);
        return -EINVAL;
    }
    // Add the IP address to the interface with manual configuration
    net_if_ipv4_addr_add(iface, &addr, NET_ADDR_MANUAL, 0);

    // Configure netmask (subnet mask)
    ret = parse_ipv4_addr(STATIC_NETMASK, &netmask);
    if (ret < 0) {
        LOG_ERR("Invalid netmask format: %s", STATIC_NETMASK);
        return -EINVAL;
    }
    // Set the netmask for the configured IP address
    net_if_ipv4_set_netmask_by_addr(iface, &addr, &netmask);

    // Configure gateway (default route)
    ret = parse_ipv4_addr(STATIC_GATEWAY, &gateway);
    if (ret < 0) {
        LOG_ERR("Invalid gateway format: %s", STATIC_GATEWAY);
        return -EINVAL;
    }
    // Set the default gateway for the interface
    net_if_ipv4_set_gw(iface, &gateway);

    // Log the complete static IP configuration for verification
    LOG_INF("Static IP configuration:");
    LOG_INF("  IP: %s", STATIC_IP_ADDR);
    LOG_INF("  Netmask: %s", STATIC_NETMASK);
    LOG_INF("  Gateway: %s", STATIC_GATEWAY);

    return 0;  // Configuration successful
}

/**
 * Initialize the network subsystem with static IP configuration
 * This function sets up the network interface and applies static IP settings
 */
void network_init(void)
{
    struct net_if *iface;  // Network interface handle
    int ret;

    LOG_INF("Initializing network with static IP...");

    // Get the default network interface
    iface = net_if_get_default();
    if (!iface) {
        LOG_ERR("No network interface found");
        return;  // Cannot proceed without a network interface
    }

    // Initialize network management event callback to monitor interface status
    net_mgmt_init_event_callback(&mgmt_cb, net_mgmt_event_handler,
                                 NET_EVENT_IF_UP | NET_EVENT_IF_DOWN);
    // Register the callback with the network management subsystem
    net_mgmt_add_event_callback(&mgmt_cb);

    // Apply static IP configuration to the interface
    ret = configure_static_ip(iface);
    if (ret < 0) {
        LOG_ERR("Failed to configure static IP: %d", ret);
        return;  // Configuration failed
    }

    // Bring the network interface up (activate it)
    net_if_up(iface);
    
    // Allow some time for the interface to become operational
    k_sleep(K_MSEC(500));
    network_ready = true;  // Mark network as ready for use
    
    LOG_INF("Static IP configuration complete");
}

/**
 * Calculate CRC32 checksum using simple polynomial
 * @param data: Pointer to data to calculate CRC for
 * @param length: Length of data in bytes
 * @return: Calculated CRC32 value
 */
static uint32_t calculate_crc32(const void *data, size_t length)
{
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFF;
    
    // CRC32 polynomial (IEEE 802.3)
    const uint32_t polynomial = 0xEDB88320;
    
    for (size_t i = 0; i < length; i++) {
        crc ^= bytes[i];
        for (int bit = 0; bit < 8; bit++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ polynomial;
            } else {
                crc = crc >> 1;
            }
        }
    }
    
    return ~crc;  // Final inversion
}

/**
 * Convert 64-bit value from network byte order to host byte order
 * @param value: 64-bit value in network byte order
 * @return: 64-bit value in host byte order
 */
static uint64_t net_to_host_64(uint64_t value)
{
    // Check if system is little-endian (most common case)
    if (sys_cpu_to_le16(1) == 1) {
        // Little-endian: need to swap bytes
        return ((uint64_t)ntohl((uint32_t)(value & 0xFFFFFFFF)) << 32) |
               ntohl((uint32_t)(value >> 32));
    } else {
        // Big-endian: no conversion needed
        return value;
    }
}

/**
 * UDP server thread function - handles incoming UDP messages
 * This thread runs continuously, listening for UDP packets and responding
 * @param arg1, arg2, arg3: Thread arguments (unused)
 */
void udp_server_thread(void *arg1, void *arg2, void *arg3)
{
    // Mark unused parameters to avoid compiler warnings
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    struct sockaddr_in bind_addr;    // Address to bind UDP socket to
    struct sockaddr_in client_addr;  // Client address for incoming packets
    socklen_t client_addr_len = sizeof(client_addr);  // Size of client address structure
    char recv_buffer[RECV_BUFFER_SIZE];  // Buffer for received data
    int ret;

    // Wait for network to be ready before starting UDP server
    while (!network_ready) {
        k_sleep(K_MSEC(100));  // Check every 100ms
    }

    LOG_INF("Starting UDP server on port %d", UDP_PORT);
    LOG_INF("Expected packet size: %d bytes (seq:%d + payload:%d + crc:%d)", 
            sizeof(udp_packet_t), sizeof(uint32_t), sizeof(uint64_t), sizeof(uint32_t));

    // Create UDP socket
    udp_sock = zsock_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udp_sock < 0) {
        LOG_ERR("Failed to create UDP socket: %d", udp_sock);
        return;  // Cannot proceed without a socket
    }

    // Set up bind address structure
    memset(&bind_addr, 0, sizeof(bind_addr));  // Clear structure
    bind_addr.sin_family = AF_INET;            // IPv4
    bind_addr.sin_addr.s_addr = INADDR_ANY;    // Listen on all interfaces
    bind_addr.sin_port = htons(UDP_PORT);      // Convert port to network byte order

    // Bind socket to the specified port
    ret = zsock_bind(udp_sock, (struct sockaddr *)&bind_addr, sizeof(bind_addr));
    if (ret < 0) {
        LOG_ERR("Failed to bind UDP socket: %d (errno: %d)", ret, errno);
        zsock_close(udp_sock);  // Clean up socket on failure
        return;
    }

    LOG_INF("UDP server listening on port %d", UDP_PORT);

    // Main server loop - continuously process incoming UDP packets
    while (1) {
        // Receive data from UDP socket (blocking call)
        ret = zsock_recvfrom(udp_sock, recv_buffer, sizeof(recv_buffer), 0,
                             (struct sockaddr *)&client_addr, &client_addr_len);

        if (ret > 0) {
            // Check if received data matches expected packet size
            if (ret != sizeof(udp_packet_t)) {
                LOG_WRN("Packet discarded: wrong size %d bytes, expected %d bytes", 
                        ret, sizeof(udp_packet_t));
                continue;  // Skip this packet
            }
            
            // Cast received data to packet structure
            const udp_packet_t *packet = (const udp_packet_t *)recv_buffer;
            
            // Convert from network byte order to host byte order for processing
            uint32_t sequence = ntohl(packet->sequence);
            uint64_t payload = net_to_host_64(packet->payload);
            uint32_t received_crc = ntohl(packet->crc32);
            
            // Calculate CRC32 on the original network-order data (sequence + payload only)
            // This matches what the client does before sending
            uint32_t calculated_crc = calculate_crc32(recv_buffer, sizeof(uint32_t) + sizeof(uint64_t));
            
            // Validate packet CRC32
            if (calculated_crc != received_crc) {
                LOG_WRN("CRC32 mismatch: calculated=0x%08X, received=0x%08X (seq=%u)", 
                        calculated_crc, received_crc, sequence);
                continue;  // Skip this packet
            }
            
            // Packet is valid - process it
            LOG_INF("Valid packet received - Sequence: %u, Payload: 0x%016llX", 
                    sequence, payload);
            
        } else if (ret < 0) {
            // Error occurred during receive operation
            LOG_ERR("UDP receive error: %d (errno: %d)", ret, errno);
            k_sleep(K_MSEC(100));  // Brief delay before retrying
        }
        // Note: ret == 0 case (connection closed) doesn't apply to UDP
    }
}

/**
 * Start the UDP server by creating and launching the server thread
 * This function creates a new thread that will handle all UDP server operations
 */
void udp_server_start(void)
{
    k_tid_t thread_id;
    
    // Create and start the UDP server thread
    thread_id = k_thread_create(&udp_thread_data,
                               udp_thread_stack,
                               K_THREAD_STACK_SIZEOF(udp_thread_stack),
                               udp_server_thread,
                               NULL, NULL, NULL,
                               K_PRIO_COOP(7),
                               0,
                               K_NO_WAIT);
    
    if (thread_id != NULL) {
        LOG_INF("UDP server thread created successfully");
    } else {
        LOG_ERR("Failed to create UDP server thread");
    }
}