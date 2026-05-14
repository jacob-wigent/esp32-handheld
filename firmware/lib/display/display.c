// #include "display.h"
// #include "neopixel.h"
// #include "../../src/pins.h"

// #define DISPLAY_WIDTH   20
// #define DISPLAY_HEIGHT  16
// #define NUM_SEGMENTS    8
// #define SEG_HEIGHT      2
// #define SEG_PIXELS      40

// typedef struct {
//     uint8_t segment;
//     uint8_t index;
// } location_t;

// static const gpio_num_t segment_gpios[NUM_SEGMENTS] = {DISP1, DISP2, DISP3, DISP4, DISP5, DISP6, DISP7, DISP8};
// static tNeopixelContext *segments[NUM_SEGMENTS]; // Pointers to neopixel contexts for each segment of the display
// static tNeopixel buffer[NUM_SEGMENTS][SEG_PIXELS]; // Buffers for each segment (8 x 40 pixels)

// static location_t xy_to_location(uint8_t x, uint8_t y) {
//     location_t loc;
//     loc.segment = y >> 1; // Equal to y / 2, since each segment has 2 rows of pixels
//     uint8_t row = y & 1; // Equal to y % 2, to determine if we're in the top or bottom row of the segment
//     loc.index = row * DISPLAY_WIDTH + (row == 0 ? x : (DISPLAY_WIDTH - 1) - x);
//     return loc;
// }

// void display_clear(uint32_t rgb) {
//     for (int seg = 0; seg < NUM_SEGMENTS; seg++)
//     {
//         for (int i = 0; i < SEG_PIXELS  ; i++)
//         {
//             buffer[seg][i].rgb = rgb;
//         }
//     }
// }

// void display_show() {
//     for (int seg = 0; seg < NUM_SEGMENTS; seg++)
//     {
//         neopixel_SetPixel(segments[seg], buffer[seg], SEG_PIXELS);
//     }
// }

// bool display_init() {

//     // Initialize each segment of the display
//     for (int seg = 0; seg < NUM_SEGMENTS; seg++)
//     {
//         segments[seg] = neopixel_Init(SEG_PIXELS, segment_gpios[seg]);

//         if (!segments[seg])
//         {
//             printf("ERROR: Failed to initialize neopixel segment %i\n", seg);
//             return false;
//         }

//         for (int i = 0; i < SEG_PIXELS; i++)
//         {
//             buffer[seg][i].index = i;
//             buffer[seg][i].rgb = 0;
//         }
//     }

//     // display_clear(0); // Clear the display to black
//     // display_show(); // Update the display to show the cleared state

//     return true;
// }

// void display_set_pixel(uint8_t x, uint8_t y, uint32_t rgb) {

//     // Validate x and y coordinates
//     if (x >= DISPLAY_WIDTH || y >= DISPLAY_HEIGHT)
//         return;

//     location_t loc = xy_to_location(x, y);
//     buffer[loc.segment][loc.index].rgb = rgb;
// }

