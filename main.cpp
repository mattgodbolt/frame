#include "images.hpp"
#include "miniz.h"

#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/binary_info.h"
#include "pico/stdlib.h" // NOLINT(modernize-deprecated-headers)
#include <array>
#include <cstdio>
#include <utility>

struct Pins {
  static constexpr auto Mosi = 19;
  static constexpr auto ChipSel = 17;
  static constexpr auto Clock = 18;
  static constexpr auto Led = 25;
  static constexpr auto Dc = 15;
  static constexpr auto Reset = 14;
  static constexpr auto Busy = 13;
  static const inline auto SpiInst = spi0;
};
bi_decl(bi_4pins_with_names(Pins::ChipSel, "E-ink chip select", Pins::Dc,
                            "E-ink command", Pins::Reset, "E-ink reset",
                            Pins::Busy, "E-ink busy"));
bi_decl(bi_1pin_with_name(Pins::Led, "On-board LED"));
bi_decl(bi_3pins_with_func(Pins::Mosi, Pins::Clock, Pins::Dc, GPIO_FUNC_SPI));

constexpr uint8_t low_byte(size_t value) { return value & 0xff; }
constexpr uint8_t high_byte(size_t value) { return (value >> 8) & 0xff; }
[[gnu::noinline]] static void delayNs(size_t nanos) {
  static constexpr auto nsPerNop = 5; // 7.5ish at 133MHz, but...paranoia?
  auto numNops = (nanos + nsPerNop - 1) / nsPerNop;
  for (auto i = 0; i < numNops; ++i)
    asm volatile("nop");
}

class Screen {
  static inline void cs_select() {
    delayNs(20);                    // hold time
    gpio_put(Pins::ChipSel, false); // Active low
    delayNs(60);                    // setup time
  }

  static inline void cs_deselect() {
    delayNs(65); // hold time
    gpio_put(Pins::ChipSel, true);
    delayNs(40); // setup time
  }
  template <typename... Args> void send_command(uint8_t command, Args... data) {
    gpio_put(Pins::Dc, false);
    cs_select();
    spi_write_blocking(Pins::SpiInst, &command, 1);
    cs_deselect();
    send_data(std::forward<Args>(data)...);
  }
  void send_data1(uint8_t data) {
    gpio_put(Pins::Dc, true);
    cs_select();
    spi_write_blocking(Pins::SpiInst, &data, 1);
    cs_deselect();
  }
  void send_data() {}
  void send_data(const uint8_t *data, size_t length) {
    gpio_put(Pins::Dc, true);
    cs_select();
    spi_write_blocking(Pins::SpiInst, data, length);
    cs_deselect();
  }
  void send_repeated_data(uint8_t data, size_t length) {
    gpio_put(Pins::Dc, true);
    cs_select();
    for (size_t i = 0; i < length; ++i)
      spi_write_blocking(Pins::SpiInst, &data, 1);
    cs_deselect();
  }
  template <typename... Args> void send_data(uint8_t first, Args... rest) {
    send_data1(first);
    int x[sizeof...(Args)] = {(send_data1(rest), 0)...};
  }

  void busy_high() const {
    delayNs(60); // unlikely to be needed (seen blank screen issues)
    while (!gpio_get(Pins::Busy))
      /*spin*/;
    delayNs(60); // unlikely to be needed (seen blank screen issues)
  }
  void busy_low() const {
    delayNs(60); // unlikely to be needed (seen blank screen issues)
    while (gpio_get(Pins::Busy))
      /*spin*/;
    delayNs(60); // unlikely to be needed (seen blank screen issues)
  }

public:
  static constexpr auto Width = 600;
  static constexpr auto Height = 448;
  Screen() {
    // Enable SPI at 1 MHz and connect to GPIOs
    spi_init(Pins::SpiInst, 2'000'000);
    spi_set_format(Pins::SpiInst, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    spi_set_slave(Pins::SpiInst, false);
    gpio_init(Pins::ChipSel);
    gpio_set_function(Pins::Clock, GPIO_FUNC_SPI);
    gpio_set_function(Pins::Mosi, GPIO_FUNC_SPI);

    // Chip select is active-low, so we'll initialise it to a driven-high state
    gpio_init(Pins::ChipSel);
    gpio_set_dir(Pins::ChipSel, GPIO_OUT);
    gpio_put(Pins::ChipSel, true);

    // Reset select is active-low, so we'll initialise it to a driven-high state
    gpio_init(Pins::Reset);
    gpio_set_dir(Pins::Reset, GPIO_OUT);
    gpio_put(Pins::Reset, true);

    gpio_init(Pins::Dc);
    gpio_set_dir(Pins::Dc, GPIO_OUT);
    gpio_init(Pins::Busy);
    gpio_set_dir(Pins::Busy, GPIO_IN);
  }

  void reset() {
    gpio_put(Pins::Reset, true);
    sleep_ms(200);
    gpio_put(Pins::Reset, false);
    sleep_ms(2);
    gpio_put(Pins::Reset, true);
    busy_high();
  }

  void init() {
    reset();
    // App manual agrees
    send_command(0x00, 0xef, 0x08);
    // App manual says send 0x01 0x37 0x00 0x05 0x05.
    send_command(0x01, 0x37, 0x00, 0x23, 0x23);
    // App manual agrees
    send_command(0x03, 0x00);
    // App manual agrees
    send_command(0x06, 0xc7, 0xc7, 0x1d);
    // App manual says "flash frame rate" here for data
    send_command(0x30, 0x3c);
    // App manual says command 0x41 here, data 0
    send_command(0x40, 0x00);
    // App manual agrees
    // This is "VCOM and Data interval settings"
    // VBD[2:0] | DDX | CDI[3:0]
    //          Vbd D CDI
    //          | | | |  |
    // 0x37 = 0b001 1 0111
    // VBD of 001 is "white" (it's a colour, the "vertical back porch").
    // DDX = 1 is LUT one "default" (b/w/g/b/r/y/o/X)
    // CDI is "data interval", 7 is default of "10"
    // timing diagram shows vsync/hsync timings, frame data is delayed by this
    // many (hsyncs?) units.
    send_command(0x50, 0x37);
    // App manual agrees, though 0x60 is not listed in the data sheet.
    send_command(0x60, 0x22);
    // App manual agrees
    set_res();
    // App manual agrees
    send_command(0xe3, 0xaa);
    // App manual says 0x82 and "flash vcom".
    // Datasheet says "Vcom_DC setting" and mentions voltages, from -0.1V down
    // to -4V. VCOM is "common voltage" which is presumably the power to the
    // screen? Referenced in many display docs, and is usually negative.
  }

  void clear(uint colour) {
    send_command(0x10);
    send_repeated_data(colour | (colour << 4), Width * Height / 2);
    screen_refresh();
  }
  void screen_refresh() {
    send_command(0x04);
    busy_high();
    send_command(0x12);
    busy_high();
    send_command(0x02);
    busy_low();
    sleep_ms(
        200); // TODO: have seen "blank image" without it BUT SRSLY can't be!
  }
  void set_res() {
    // This is setting the screen resolution.
    send_command(0x61, high_byte(Width), low_byte(Width), high_byte(Height),
                 low_byte(Height));
  }
  void image(const uint8_t *data) {
    set_res();
    send_command(0x10, data, Width * Height / 2);
    screen_refresh();
  }
  void sleep() { send_command(0x07, 0xa5); }
};

int main() {
  bi_decl(bi_program_name("photo"));
  bi_decl(bi_program_description("Photo frame driver"));
  bi_decl(bi_program_url("https://github.com/mattgodbolt/photo"));
  bi_decl(bi_program_build_date_string(__TIME__));

  stdio_init_all();

  Screen screen;
  screen.init();

  gpio_init(Pins::Led);
  gpio_set_dir(Pins::Led, GPIO_OUT);
  int image_id = 0;
#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
  for (;;) {
    gpio_put(Pins::Led, true);
    screen.clear(0x7);
    gpio_put(Pins::Led, false);

    const auto &image = Image::Images[image_id];
    printf("image: %s\n", image.name);
    static std::array<uint8_t, Screen::Width * Screen::Height / 2> decom_buf;
    auto dest_len = static_cast<mz_ulong>(decom_buf.size());
    auto result = mz_uncompress(decom_buf.data(), &dest_len,
                                image.compressed_data, image.compressed_size);
    printf("decompress results: %d\n", result);
    screen.image(decom_buf.data());
    puts("done");
    screen.sleep();
    sleep_ms(5 * 60 * 1000);
    screen.init();
    image_id++;
    if (image_id >= Image::NumImages)
      image_id = 0;
  }
#pragma clang diagnostic pop
}
