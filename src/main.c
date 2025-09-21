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
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h> // added for zsock_close
#include <stdint.h>
#include "led.h"
#include "net.h"

// Register this source file as a log module named "k2_app" with INFO level
// This allows us to use LOG_INF(), LOG_ERR(), etc. in our code
LOG_MODULE_REGISTER(k2_app, LOG_LEVEL_INF);

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
    udp_server_start();

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