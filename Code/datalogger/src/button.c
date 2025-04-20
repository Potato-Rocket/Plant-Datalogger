#include "button.h"
#include "utils.h"
#include "logging.h"

#define BUTTON_PIN 2u

// debouncing time constand
static const uint32_t button_debounce_ms = 20ul; // 20ms
// minimum duration of a long press
static const uint32_t long_press_min_ms = 3000ul; // 3sec
// maximum duration of a long press, to avoid strange behavior
static const uint32_t long_press_max_ms = 10000ul; // 10sec

// when the next event can be registered
static absolute_time_t debounce = 0;
// when the current long press started
static absolute_time_t press_start = 0;

/**
 * Enumeration to keep track of what event the button should report
 */
typedef enum {
    BUTTON_IDLE,         // no events to report
    BUTTON_PRESSED,      // button pressed but not released or reported
    BUTTON_LONG_PRESSED, // button long pressed but not reported
} ButtonState;

// the current button state, i.e. any events to report
static volatile ButtonState button_state = BUTTON_IDLE;

/**
 * Button callback, whenever the button's state changes. Debounces, then if it
 * is a rising edge, registers a short press. If it is a falling edge, registers
 * a long press if the duration is correct, deregisters the short press.
 */
static void _button_cb(uint __unused, uint32_t events);

void init_button(void)
{
    // set up button
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);
    // set up button callback
    gpio_set_irq_enabled_with_callback(BUTTON_PIN,
                                       GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE,
                                       true, &_button_cb);
    
    log_message(LOG_DEBUG, LOG_BUTTON, "Set up button callback");
    sleep_ms(100);
}

bool check_press(void)
{
    // is there a short press to report?
    bool result = button_state == BUTTON_PRESSED;
    // reset the button state
    if (result)
    {
        log_message(LOG_DEBUG, LOG_BUTTON, "Short press reported");
        button_state = BUTTON_IDLE;
    }
    return result;
}

bool check_long_press(void)
{
    // is there a long press to report?
    bool result = button_state == BUTTON_LONG_PRESSED;
    // reset the button state
    if (result)
    {
        log_message(LOG_DEBUG, LOG_BUTTON, "Long press reported");
        button_state = BUTTON_IDLE;
    }
    return result;
}

static void _button_cb(uint __unused, uint32_t events)
{
    // do some debouncing
    if (!is_timed_out(debounce))
    {
        return;
    }
    debounce = make_timeout_time_ms(button_debounce_ms);

    // if the button has been pressed
    if (events & GPIO_IRQ_EDGE_FALL)
    {
        // update the date and press time
        log_message(LOG_DEBUG, LOG_BUTTON, "Button press registered");
        button_state = BUTTON_PRESSED;
        press_start = get_absolute_time();
    }

    // if the button has been released
    if (events & GPIO_IRQ_EDGE_RISE)
    {
        log_message(LOG_DEBUG, LOG_BUTTON, "Button release registered");

        // if the short press was not reported
        if (button_state == BUTTON_PRESSED)
        {
            uint64_t time_delta = absolute_time_diff_ms(press_start,
                                                        get_absolute_time());

            // if the press was the correct duration
            if (time_delta > long_press_min_ms &&
                time_delta < long_press_max_ms)
            {
                // register a long press
                button_state = BUTTON_LONG_PRESSED;
            }
            else
            {
                // otherwise return to idle
                button_state = BUTTON_IDLE;
            }
        }
    }
}
