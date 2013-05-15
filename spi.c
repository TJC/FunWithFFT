#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <linux/spi/spidev.h>
#include <linux/types.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>

void set_mode_7();

int spi_write(unsigned char* bytes, unsigned int length)
{
    int fd; // I tested with a persistent filehandle; no real difference.
    int ret;

    // See /usr/include/linux/spi/spidev.h for documentation..

    fd = open("/dev/spidev2.0", O_RDWR);
    if (fd == -1) {
        perror("Error opening /dev/spidev2.0");
        exit(2);
    }

    uint8_t bits = 8;
    uint16_t delay = 0;
    // 20 MHz seems to work nicely, but maybe some controllers will want
    // slower speeds? Adjust downwards if you encounter trouble.
    uint32_t speed = 20000000;
    // uint8_t tx[4096];

    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)bytes,
        .rx_buf = (void *)NULL,
        .len = length,
        .delay_usecs = delay,
        .speed_hz = speed,
        .bits_per_word = bits,
    };

    ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
    if (ret == -1) {
        perror("failed to send SPI");
        exit(2);
    }
    if (close(fd) == -1) {
        perror("failed to close SPI fiehandle");
        exit(2);
    }
    return ret;
}

int tc_open(char *filename)
{
    int fd;
    char str[256];
    fd = open(filename, O_WRONLY);
    if (fd == -1) {
        snprintf(str, 256, "Failed to open: %s", filename);
        perror(str);
        exit(1);
    }
    return fd;
}

unsigned char init_commands[24] = {
    0xAE,
    0xD5,
    0x80,
    0xA8,
    0x1F,
    0xD3,
    0x00,
    0x8D,
    0x14,
    0xDA,
    0x02,
    0x81,
    0x8F,
    0xD9,
    0xF1,
    0xDB,
    0x40,
    0x20,
    0x00,
    0x22,
    0x0, 
    0x3, 
    0xA4,
    0xAF,
};

void init_lcd_screen()
{
    char *dc_pin = "P8_05"; // gpio34
    char *rst_pin = "P8_03";// gpio38
    int fd_dc, fd_rst;
    int fd;

    fd = tc_open("/sys/kernel/debug/omap_mux/gpmc_ad2");
    write(fd, "7", 1);
    close(fd);

    fd = tc_open("/sys/kernel/debug/omap_mux/gpmc_ad6");
    write(fd, "7", 1);
    close(fd);

    fd = tc_open("/sys/class/gpio/export");
    write(fd, "38", 2);
    close(fd);

    fd = tc_open("/sys/class/gpio/export");
    write(fd, "34", 2);
    close(fd);

    fd = tc_open("/sys/class/gpio/gpio34/direction");
    write(fd, "out", 3);
    close(fd);

    fd = tc_open("/sys/class/gpio/gpio38/direction");
    write(fd, "out", 3);
    close(fd);

    fd_dc = tc_open("/sys/class/gpio/gpio34/value");
    fd_rst = tc_open("/sys/class/gpio/gpio38/value");


    write(fd_rst, "1", 1);
    usleep(1000);
    write(fd_rst, "0", 1);
    usleep(10000);
    write(fd_rst, "1", 1);
    usleep(10000);

    write(fd_dc, "0", 1); // command


    spi_write(init_commands, 24);

    write(fd_dc, "1", 1); // data from here on it..

}

int led_fd[4] = {0,0,0,0};
void switch_led(int id, int state)
{
    char filename[128];
    if (id < 0 || id > 3) {
        printf("Invalid LED id: %d\n", id);
        exit(2);
    }
    if (led_fd[id] == 0) {
        sprintf(filename, "/sys/class/leds/beaglebone::usr%d/brightness", id);
        led_fd[id] = tc_open(filename);
    }
    write(led_fd[id], state ? "1" : "0", 1);
}

int laser_fd[6] = {0,0,0,0,0,0};
char laser_gpios[6] = { 88, 89, 11, 81, 80, 79 };
void switch_laser(int id, int state)
{
    char filename[128];
    char tmp[8];
    int fd;

    if (id < 0 || id > 4) {
        printf("Invalid laser id: %d\n", id);
        exit(2);
    }
    if (laser_fd[id] == 0) {

        set_mode_7();

        fd = tc_open("/sys/class/gpio/export");
        sprintf(tmp, "%d", laser_gpios[id]);
        write(fd, tmp, 2);
        close(fd);

        sprintf(filename, "/sys/class/gpio/gpio%d/direction", laser_gpios[id]);
        fd = tc_open(filename);
        write(fd, "out", 3);
        close(fd);

        sprintf(filename, "/sys/class/gpio/gpio%d/value", laser_gpios[id]);
        laser_fd[id] = tc_open(filename);
    }
    write(laser_fd[id], state ? "1" : "0", 1);
}

int mode_7_set = 0;
void set_mode_7()
{
    const char *pins[15] = {
        "lcd_pclk", "lcd_ac_bias_en", "lcd_data15",
        "lcd_data11", "lcd_data10", "lcd_data9",
        "lcd_data7", "lcd_data2", "lcd_data3", "lcd_data1",
        "gpmc_csn1", "gpmc_ad4", "gpmc_ad0", "lcd_vsync", "lcd_hsync"
    };
    int fd;
    char filename[128];
    if (mode_7_set) {
        return;
    }
    mode_7_set = 1;

    for (int i=0; i<15; i++) {
        sprintf(filename, "/sys/kernel/debug/omap_mux/%s", pins[i]);
        fd = tc_open(filename);
        write(fd, "7", 1);
        close(fd);
    }
}

