#include "images.hpp"
#include "miniz.h"

#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/binary_info.h"
#include "pico/stdlib.h"
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

static inline void delay() { asm volatile("nop\nnop\nnop"); }

class Screen {
  static inline void cs_select() {
    delay();
    gpio_put(Pins::ChipSel, false); // Active low
    delay();
  }

  static inline void cs_deselect() {
    delay();
    gpio_put(Pins::ChipSel, true);
    delay();
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
  template <typename... Args> void send_data(uint8_t first, Args... rest) {
    send_data1(first);
    int x[sizeof...(Args)] = {(send_data1(rest), 0)...};
  }

  void busy_high() const {
    while (!gpio_get(Pins::Busy))
      delay();
  }
  void busy_low() const {
    while (gpio_get(Pins::Busy))
      delay();
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
  }

  void init() {
    reset();
    busy_high();
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
    sleep_ms(100); // no mention of delays
    // This is a repeat of the earlier VCOM and data interval settings
    send_command(0x50, 0x37);
  }

  void clear(uint colour) {
    send_command(0x10);
    for (auto i = 0; i < Width * Height / 2; ++i)
      send_data(colour | (colour << 4));
    screen_refresh();
  }
  void screen_refresh() {
    send_command(0x04);
    busy_high();
    send_command(0x12);
    busy_high();
    send_command(0x02);
    busy_low();
    sleep_ms(500); // no mention of delays in manual
  }
  void set_res() {
    // This is setting the screen resolution 0x258 = 600, 0x1c0 = 448.
    send_command(0x61, 0x02, 0x58, 0x01, 0xc0);
  }
  void image(const uint8_t *data) {
    set_res();
    send_command(0x10, data, Width * Height / 2);
    screen_refresh();
  }
};

int main() {
  //  bi_decl(bi_program_description("This is a test binary."));
  //  bi_decl(bi_1pin_with_name(Pins::Led, "On-board LED"));

  stdio_init_all();
  sleep_ms(2000);

  Screen screen;
  screen.init();

  gpio_init(Pins::Led);
  gpio_set_dir(Pins::Led, GPIO_OUT);
  int image_id = 0;
#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
  for (;;) {
    gpio_put(Pins::Led, false);
    sleep_ms(250);
    gpio_put(Pins::Led, true);
    screen.clear(0x7);

    const auto &image = Image::Images[image_id];
    printf("image: %s\n", image.name);
    static std::array<uint8_t, Screen::Width * Screen::Height / 2> decom_buf;
    auto dest_len = static_cast<mz_ulong>(decom_buf.size());
    auto result = mz_uncompress(decom_buf.data(), &dest_len,
                                image.compressed_data, image.compressed_size);
    printf("decompress results: %d\n", result);
    screen.image(decom_buf.data());
    puts("done");
    sleep_ms(5 * 60 * 1000);
    image_id++;
    if (image_id >= Image::NumImages)
      image_id = 0;
  }
#pragma clang diagnostic pop
}
