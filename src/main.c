/*
 * K2 Zephyr Application - LED Control
 * 
 * This application demonstrates basic embedded programming concepts:
 * 1. GPIO control (blinking LED)
 * 2. Real-time scheduling (periodic tasks)
 * 3. Logging system for debug output
 * 
 * Target Hardware: ST NUCLEO-F767ZI development board
 * - Green LED on pin PA5 (controlled via GPIO)
 * - UART console for debug messages (115200 baud via ST-LINK USB)
 */

// Include Zephyr RTOS headers - these provide the APIs we need
#include <zephyr/kernel.h>          // Core RTOS functions (threads, timing, etc.)
#include <zephyr/drivers/gpio.h>    // GPIO driver for LED control
#include <zephyr/logging/log.h>     // Logging system for debug messages

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
    
    // Configure the pin as an output with active state
    // GPIO_OUTPUT_ACTIVE means the pin will drive high when "active"
    int ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        LOG_ERR("Failed to configure LED GPIO: %d", ret);
    } else {
        LOG_INF("LED initialized successfully on pin PA5");
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
    LOG_INF("Features: LED control");
    
    /* 
     * INITIALIZATION PHASE
     * Set up all hardware and software components before main loop
     */
    
    // Initialize the LED GPIO pin
    led_init();
    
    /*
     * MAIN APPLICATION LOOP
     * 
     * This infinite loop is the "main thread" of our application.
     * In a real-time system like Zephyr, multiple threads can run
     * concurrently, but this simple example uses just one thread.
     */
    bool led_state = false;  // Track current LED state (on/off)
    uint32_t loop_count = 0; // Count how many times we've blinked
    
    LOG_INF("Starting main loop - LED will blink every second");
    
    while (1) {  // Infinite loop - runs forever
        // Toggle LED state: if it was off, turn it on (and vice versa)
        gpio_pin_set_dt(&led, led_state);
        led_state = !led_state;  // Flip the boolean value
        
        // Increment counter and log current state
        loop_count++;
        LOG_INF("Loop #%u: LED toggled - state: %s", 
                loop_count, led_state ? "ON" : "OFF");
        
        /*
         * Sleep for 1 second
         * 
         * k_sleep() suspends this thread and lets other threads run.
         * This is crucial in real-time systems - we don't want to
         * "busy wait" and waste CPU cycles.
         * 
         * K_SECONDS(1) creates a timeout value of 1 second.
         * During this time, the RTOS can:
         * - Run system maintenance tasks
         * - Put CPU in low-power mode if no other work to do
         */
        k_sleep(K_SECONDS(1));
    }
    
    // This line never executes in an embedded system, but good practice
    // to include it for completeness
    return 0;
}