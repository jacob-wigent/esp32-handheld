#include "display.h"
#include "neopixel.h"

typedef struct {
    uint8_t segment;
    uint8_t index;
} location_t;

static neopixel_handle_t *segments[NUM_SEGMENTS]; // Pointers to neopixel handles for each segment of the display

static location_t xy_to_location(uint8_t x, uint8_t y) {
    location_t loc;
    loc.segment = y >> 1; // Equal to y / 2, since each segment has 2 rows of pixels
    uint8_t row = y & 1; // Equal to y % 2, to determine if we're in the top or bottom row of the segment
    loc.index = row * DISPLAY_WIDTH + (row == 0 ? x : (DISPLAY_WIDTH - 1) - x);
    return loc;
}

void display_clear() {
    for (int seg = 0; seg < NUM_SEGMENTS; seg++) {
        neopixel_clear(segments[seg]);
    }
}

void display_show() {
    for (int seg = 0; seg < NUM_SEGMENTS; seg++) {
        neopixel_show(segments[seg]);
    }
}

bool display_init(gpio_num_t gpios[NUM_SEGMENTS]) {
    // Initialize each segment of the display
    for (int seg = 0; seg < NUM_SEGMENTS; seg++) {
        segments[seg] = neopixel_new(gpios[seg], SEG_PIXELS);

        if (!segments[seg]) {
            printf("ERROR: Failed to initialize neopixel segment %i\n", seg);
            return false;
        }
    }

    // display_clear(0); // Clear the display to black
    // display_show(); // Update the display to show the cleared state

    return true;
}

void display_set_pixel(uint8_t x, uint8_t y, uint8_t r, uint8_t g, uint8_t b) {

    // Validate x and y coordinates
    if (x >= DISPLAY_WIDTH || y >= DISPLAY_HEIGHT)
        return;

    location_t loc = xy_to_location(x, y);
    neopixel_set_pixel(segments[loc.segment], loc.index, r, g, b);
}

