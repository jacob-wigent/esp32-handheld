# ESP32 Handheld Firmware

## 4-LED Prototype Setup

### Hardware Connections
1. Connect NeoPixel strip data input to DISP1 (GPIO48)
2. Power NeoPixels with 5V and GND
3. Buttons:
   - StickBtn / USER_SW (GPIO15): Cycles through animation patterns
   - SELECT / BOOT (GPIO0): Secondary control (currently unused)

### Program Operation

#### Initialization Sequence
1. Serial debug output initialized at 115200 baud
2. Buttons configured with internal pull-ups
3. NeoPixel strip initialized with:
   - 4 LEDs
   - 32/255 brightness (12.5%) for power efficiency
   - RGB color order at 800KHz data rate
4. Startup test pattern: Red → Green → Blue sequence

#### Animation Patterns
The program features three distinct patterns that can be cycled through using the StickBtn:

1. **Rainbow Cycle** (Pattern 0)
   - Creates a smooth rainbow animation across all LEDs
   - Each LED shows a different color that shifts over time
   - Updates every 20ms for smooth transitions

2. **Chase Pattern** (Pattern 1)
   - Single red dot moving across the strip
   - Complete cycle every 800ms (200ms per LED)
   - Clear background for high contrast

3. **Breathing Effect** (Pattern 2)
   - All LEDs display blue with synchronized brightness pulsing
   - Uses sine wave for smooth brightness transitions
   - ~1.6 second cycle (0.002 radians per millisecond)

#### Technical Details
- Uses ESP32-S3's RMT (Remote Control) peripheral for efficient LED control
- Button debouncing implemented with 20ms check interval
- Efficient 32-bit color handling using predefined color values
- Math optimizations for smooth animations:
  - Bitwise operations for rainbow cycle
  - Sine-based breathing effect
  - Modulo-based chase pattern timing

### Performance Features
- Hardware-accelerated LED control via RMT peripheral
- Efficient memory usage with static color definitions
- Non-blocking button handling
- Smooth animations at consistent frame rates
- Low brightness default for power efficiency

### Development and Testing
1. Upload firmware using PlatformIO
2. Monitor serial output at 115200 baud for:
   - Initialization status
   - Pattern change notifications
3. Test functionality:
   - Press StickBtn to cycle through patterns (0-2)
   - Observe smooth transitions between patterns
   - Check serial output for pattern number confirmation

### Next Steps
1. Expand to full LED matrix configuration
2. Add interactive game patterns
3. Implement brightness control via buttons
4. Add pattern speed control
5. Create game-specific animations
6. Add dual-button combo features
7. Implement power-saving modes