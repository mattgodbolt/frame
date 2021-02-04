#include "images.hpp"

#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/binary_info.h"
#include "pico/stdlib.h"
#include <cstdio>

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
    send_data(data...);
  }
  void send_data1(uint8_t data) {
    gpio_put(Pins::Dc, true);
    cs_select();
    spi_write_blocking(Pins::SpiInst, &data, 1);
    cs_deselect();
  }
  template <typename... Args> void send_data(Args... data) {
    int x[sizeof...(Args)] = {(send_data1(data), 0)...};
  }

  void busy_high() const {
    puts("busy high\n");
    while (!gpio_get(Pins::Busy))
      delay();
    puts("busy high over\n");
  }
  void busy_low() const {
    puts("busy low\n");
    while (gpio_get(Pins::Busy))
      delay();
    puts("busy over\n");
  }

public:
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
    send_command(0x00, 0xef, 0x08);
    send_command(0x01, 0x37, 0x00, 0x23, 0x23);
    send_command(0x03, 0x00);
    send_command(0x06, 0xc7, 0xc7, 0x1d);
    send_command(0x30, 0x3c);
    send_command(0x40, 0x00);
    send_command(0x50, 0x37);
    send_command(0x60, 0x22);
    send_command(0x61, 0x02, 0x58, 0x01, 0xc0);
    send_command(0xe3, 0xaa);
    sleep_ms(100);
    send_command(0x50, 0x37);
  }

  void clear(uint colour) {
    set_res();
    for (auto i = 0; i < 600 * 448 / 2; ++i)
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
    sleep_ms(500);
  }
  void set_res() {
    send_command(0x61, 0x02, 0x58, 0x01, 0xc0);
    send_command(0x10);
  }
  void image(const char *data) {
    set_res();
    for (auto i = 0; i < 600 * 448 / 2; ++i)
      send_data(data[i]);
    screen_refresh();
  }
};

int main() {
  //  bi_decl(bi_program_description("This is a test binary."));
  //  bi_decl(bi_1pin_with_name(Pins::Led, "On-board LED"));

  stdio_init_all();
  sleep_ms(2000);

  puts("ctor\n");
  Screen screen;
  puts("init\n");
  screen.init();

  gpio_init(Pins::Led);
  gpio_set_dir(Pins::Led, GPIO_OUT);
  int image_id = 0;
  for (;;) {
    gpio_put(Pins::Led, false);
    sleep_ms(250);
    gpio_put(Pins::Led, true);
    puts("Hello World yo\n");
    puts("clear\n");
    screen.clear(0x1);
    puts("done\n");

    puts("image: ");
    puts(Image::Images[image_id].name);
    puts("\n");
    screen.image(Image::Images[image_id].data);
    puts("done\n");
    sleep_ms(30000);
    image_id++;
    if (image_id >= Image::NumImages)
      image_id = 0;
  }
}
