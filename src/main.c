/*
 * K2 Zephyr Application - LED Control
 * 
 * This application demonstrates basic embedded programming concepts:
 * 1. GPIO control (blinking LED)
 * 2. Real-time scheduling (periodic tasks)
 * 3. Logging system for debug output
 * 4. Networking (UDP server)
 * 
 * Target Hardware: ST NUCLEO-F767ZI development board
 * - Green LED on pin PA5 (controlled via GPIO)
 * - UART console for debug messages (115200 baud via ST-LINK USB)
 */

// Include Zephyr RTOS headers - these provide the APIs we need
#include <zephyr/kernel.h>          // Core RTOS functions (threads, timing, etc.)
#include <zephyr/drivers/gpio.h>    // GPIO driver for LED control
#include <zephyr/logging/log.h>     // Logging system for debug messages
#include <zephyr/net/socket.h>      // BSD socket API for UDP networking
#include <zephyr/net/net_if.h>      // Network interface management
#include <zephyr/net/net_mgmt.h>    // Network management events
#include <zephyr/net/dhcpv4.h>      // DHCP client functions
#include <stdio.h>                  // For snprintf
#include <string.h>                 // For strlen, memset
#include <stdlib.h>                 // For strtol
#include <errno.h>                  // For errno

// Register this source file as a log module named "k2_app" with INFO level
// This allows us to use LOG_INF(), LOG_ERR(), etc. in our code
LOG_MODULE_REGISTER(k2_app, LOG_LEVEL_INF);

/*
 * LED Configuration
 * 
 * In Zephyr, hardware is described using Device Tree (DT).
 * The board's device tree defines an alias "led0" that points to the green LED.
 * GPIO_DT_SPEC_GET() creates a structure containing:
 * - GPIO port (which GPIO controller)
 * - Pin number (which pin on that controller) 
 * - Flags (active high/low, pull-up/down, etc.)
 */
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

/*
 * Initialize the LED GPIO pin
 * 
 * This function:
 * 1. Checks if the GPIO controller is ready (hardware initialized)
 * 2. Configures the pin as an output (so we can control the LED)
 * 3. Sets initial state based on the device tree configuration
 */
static void led_init(void)
{
    // Check if the GPIO controller hardware is ready to use
    if (!gpio_is_ready_dt(&led)) {
        LOG_ERR("LED GPIO device not ready");
        return;
    }
    
    // Configure as output; set initial level explicitly for portability
    int ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT);
    if (ret < 0) {
        LOG_ERR("Failed to configure LED GPIO: %d", ret);
    } else {
        // Ensure LED starts OFF
        gpio_pin_set_dt(&led, 0);
        LOG_INF("LED initialized successfully on pin PA5");
    }
}

// Network configuration
#define UDP_PORT 12345
#define RECV_BUFFER_SIZE 64

// Network management callback for DHCP events
static struct net_mgmt_event_callback mgmt_cb;
static bool network_ready = false;

// UDP socket and thread variables
static int udp_sock = -1;
static K_THREAD_STACK_DEFINE(udp_thread_stack, 2048);
static struct k_thread udp_thread_data;

/*
 * Network management event handler
 * Called when network events occur (like DHCP lease acquired)
 */
static void net_mgmt_event_handler(struct net_mgmt_event_callback *cb,
                                   uint64_t mgmt_event, struct net_if *iface)
{
    if (mgmt_event == NET_EVENT_IPV4_DHCP_BOUND) {
        LOG_INF("DHCP lease acquired - network is ready");
        network_ready = true;
    }
}

/*
 * Initialize networking subsystem
 * Sets up DHCP and network event callbacks
 */
static void network_init(void)
{
    struct net_if *iface;

    LOG_INF("Initializing network...");

    // Get the default network interface (Ethernet)
    iface = net_if_get_default();
    if (!iface) {
        LOG_ERR("No network interface found");
        return;
    }

    // Setup network management event callback for DHCP
    net_mgmt_init_event_callback(&mgmt_cb, net_mgmt_event_handler,
                                 NET_EVENT_IPV4_DHCP_BOUND);
    net_mgmt_add_event_callback(&mgmt_cb);

    // Start DHCP client
    net_dhcpv4_start(iface);
    LOG_INF("DHCP client started - waiting for IP address...");
}

/*
 * UDP server thread
 * Listens for UDP packets and blinks LED when integer is received
 */
static void udp_server_thread(void *arg1, void *arg2, void *arg3)
{
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    struct sockaddr_in bind_addr;
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    char recv_buffer[RECV_BUFFER_SIZE];
    int ret;

    // Wait for network to be ready
    while (!network_ready) {
        k_sleep(K_MSEC(100));
    }

    LOG_INF("Starting UDP server on port %d", UDP_PORT);

    // Create UDP socket using Zephyr socket API
    udp_sock = zsock_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udp_sock < 0) {
        LOG_ERR("Failed to create UDP socket: %d", udp_sock);
        return;
    }

    // Bind socket to port
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
        // Receive UDP packet
        ret = zsock_recvfrom(udp_sock, recv_buffer, sizeof(recv_buffer) - 1, 0,
                             (struct sockaddr *)&client_addr, &client_addr_len);

        if (ret > 0) {
            recv_buffer[ret] = '\0';  // Null terminate
            
            // Try to parse as integer
            char *endptr;
            long received_int = strtol(recv_buffer, &endptr, 10);
            
            if (endptr != recv_buffer && *endptr == '\0') {
                // Successfully parsed integer - blink LED
                LOG_INF("Received integer: %ld from client - blinking LED", received_int);
                
                // Blink LED 3 times quickly
                for (int i = 0; i < 3; i++) {
                    gpio_pin_set_dt(&led, 1);
                    k_sleep(K_MSEC(100));
                    gpio_pin_set_dt(&led, 0);
                    k_sleep(K_MSEC(100));
                }
                
                // Send acknowledgment back to client
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

/*
 * Main Application Entry Point
 * 
 * In embedded systems, main() typically:
 * 1. Initializes hardware and subsystems
 * 2. Sets up any required callbacks or interrupts  
 * 3. Enters an infinite loop to handle the main application logic
 * 
 * Unlike desktop programs that exit when done, embedded applications
 * run continuously until power is removed.
 */
int main(void)
{
    LOG_INF("=== K2 Zephyr Application Starting ===");
    LOG_INF("Board: %s", CONFIG_BOARD);
    LOG_INF("Features: LED control, Ethernet, UDP server");
    
    /* 
     * INITIALIZATION PHASE
     * Set up all hardware and software components before main loop
     */
    
    // Initialize the LED GPIO pin
    led_init();
    
    // Initialize networking
    network_init();
    
    // Start UDP server thread
    k_thread_create(&udp_thread_data, udp_thread_stack,
                    K_THREAD_STACK_SIZEOF(udp_thread_stack),
                    udp_server_thread, NULL, NULL, NULL,
                    K_PRIO_COOP(7), 0, K_NO_WAIT);

    /*
     * MAIN APPLICATION LOOP
     * 
     * Continue blinking LED normally, but UDP thread will handle
     * network packets and do additional LED blinking when integers arrive
     */
    bool led_state = false;  // Track current LED state (on/off)
    uint32_t loop_count = 0; // Count how many times we've blinked
    
    LOG_INF("Starting main loop - LED will blink every 2 seconds");
    LOG_INF("UDP server will blink LED rapidly when receiving integers");
    
    while (1) {  // Infinite loop - runs forever
        // Toggle LED state: if it was off, turn it on (and vice versa)
        gpio_pin_set_dt(&led, led_state);
        led_state = !led_state;  // Flip the boolean value
        
        // Increment counter and log current state
        loop_count++;
        if (network_ready) {
            LOG_INF("Loop #%u: LED toggled - Network ready, UDP listening", loop_count);
        } else {
            LOG_INF("Loop #%u: LED toggled - Waiting for network...", loop_count);
        }
        
        // Sleep for 2 seconds (longer interval to distinguish from UDP blinks)
        k_sleep(K_SECONDS(2));
    }
    
    // Cleanup (never reached in embedded systems)
    if (udp_sock >= 0) {
        zsock_close(udp_sock);
    }
    return 0;
}