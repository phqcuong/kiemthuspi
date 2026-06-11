#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_timer.h"

#include "spi_test_pins.h"

/* ============================================================
   BIEN TRANG THAI
   ============================================================ */

static bool sd_bus_initialized = false;
static bool idc_bus_initialized = false;

typedef enum {
    TEST_PASS = 0,
    TEST_FAIL,
    TEST_SKIP
} test_status_t;

/* ============================================================
   HAM IN KET QUA
   ============================================================ */

static void print_line(void)
{
    printf("------------------------------------------------------------\n");
}

static void print_result(const char *id, const char *name, test_status_t status)
{
    const char *text = "UNKNOWN";

    if (status == TEST_PASS) {
        text = "PASS";
    } else if (status == TEST_FAIL) {
        text = "FAILED";
    } else if (status == TEST_SKIP) {
        text = "SKIP";
    }

    printf("[%s] %-50s : %s\n", id, name, text);
}

static void wait_for_measure(const char *msg)
{
    printf("\n>>> %s\n", msg);
    printf(">>> Dang giu trang thai %d ms de do tren PCB...\n", MEASURE_DELAY_MS);
    vTaskDelay(pdMS_TO_TICKS(MEASURE_DELAY_MS));
}

/* ============================================================
   GPIO CS HELPER
   ============================================================ */

static void cs_high(int pin)
{
    gpio_set_level(pin, 1);
}

static void cs_low(int pin)
{
    gpio_set_level(pin, 0);
}

static int read_pin(int pin)
{
    return gpio_get_level(pin);
}

/*
   Cau hinh CS dang INPUT_OUTPUT_OD de doc duoc muc thuc te tren PCB.
   Khi ghi HIGH, chan duoc nha len qua pull-up.
   Neu PCB bi keo xuong GND, doc lai se thay LOW.
*/
static void config_cs_gpio_for_test(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << SD_PIN_CS) | (1ULL << IDC_PIN_CS),
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    gpio_config(&io_conf);

    cs_high(SD_PIN_CS);
    cs_high(IDC_PIN_CS);
}

/* ============================================================
   SPI TRANSFER HELPER
   ============================================================ */

static esp_err_t spi_transfer_bytes(
    spi_device_handle_t dev,
    const uint8_t *tx,
    uint8_t *rx,
    size_t len
)
{
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));

    t.length = len * 8;
    t.tx_buffer = tx;
    t.rx_buffer = rx;

    return spi_device_polling_transmit(dev, &t);
}

static uint8_t spi_transfer_byte(spi_device_handle_t dev, uint8_t data)
{
    uint8_t rx = 0xFF;
    spi_transfer_bytes(dev, &data, &rx, 1);
    return rx;
}

/* ============================================================
   SPI-001: KHOI TAO THIET BI SPI
   ============================================================ */

static bool init_spi_bus(
    spi_host_device_t host,
    int mosi,
    int miso,
    int clk,
    int max_transfer_sz
)
{
    spi_bus_config_t buscfg = {
        .mosi_io_num = mosi,
        .miso_io_num = miso,
        .sclk_io_num = clk,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = max_transfer_sz
    };

    esp_err_t ret = spi_bus_initialize(host, &buscfg, SPI_DMA_CH_AUTO);

    if (ret != ESP_OK) {
        printf("spi_bus_initialize failed: %s\n", esp_err_to_name(ret));
        return false;
    }

    return true;
}

static test_status_t test_spi_001_init(void)
{
    printf("\n[SPI-001] Khoi tao bus SPI tren PCB\n");

    config_cs_gpio_for_test();

    printf("SD SPI  : CS=%d CLK=%d MISO=%d MOSI=%d\n",
           SD_PIN_CS, SD_PIN_CLK, SD_PIN_MISO, SD_PIN_MOSI);

    printf("IDC SPI : CS=%d CLK=%d MISO=%d MOSI=%d\n",
           IDC_PIN_CS, IDC_PIN_CLK, IDC_PIN_MISO, IDC_PIN_MOSI);

    sd_bus_initialized = init_spi_bus(
        SD_SPI_HOST,
        SD_PIN_MOSI,
        SD_PIN_MISO,
        SD_PIN_CLK,
        4096
    );

    idc_bus_initialized = init_spi_bus(
        IDC_SPI_HOST,
        IDC_PIN_MOSI,
        IDC_PIN_MISO,
        IDC_PIN_CLK,
        4096
    );

    if (sd_bus_initialized && idc_bus_initialized) {
        return TEST_PASS;
    }

    return TEST_FAIL;
}

/* ============================================================
   SPI-002: CACH LY CHIP SELECT
   ============================================================ */

static test_status_t test_spi_002_chip_select(void)
{
    printf("\n[SPI-002] Kiem tra cach ly chip select tren PCB\n");

    bool pass = true;

    config_cs_gpio_for_test();

    /* STATE 1: IDLE */
    cs_high(SD_PIN_CS);
    cs_high(IDC_PIN_CS);
    wait_for_measure("STATE 1 - IDLE: SD_CS HIGH, IDC_CS HIGH");

    int sd_idle = read_pin(SD_PIN_CS);
    int idc_idle = read_pin(IDC_PIN_CS);

    printf("Readback IDLE: SD_CS=%d, IDC_CS=%d\n", sd_idle, idc_idle);

    if (sd_idle != 1 || idc_idle != 1) {
        printf("Loi: Trang thai IDLE khong giu duoc HIGH tren ca 2 CS.\n");
        pass = false;
    }

    /* STATE 2: SELECT SD */
    cs_low(SD_PIN_CS);
    cs_high(IDC_PIN_CS);
    wait_for_measure("STATE 2 - SELECT SD: SD_CS LOW, IDC_CS HIGH");

    int sd_select = read_pin(SD_PIN_CS);
    int idc_when_sd = read_pin(IDC_PIN_CS);

    printf("Readback SELECT SD: SD_CS=%d, IDC_CS=%d\n", sd_select, idc_when_sd);

    if (sd_select != 0) {
        printf("Loi: SD_CS khong xuong LOW khi chon SD.\n");
        pass = false;
    }

    if (idc_when_sd != 1) {
        printf("Loi: IDC_CS bi keo LOW khi chon SD.\n");
        pass = false;
    }

    /* STATE 3: SELECT IDC */
    cs_high(SD_PIN_CS);
    cs_low(IDC_PIN_CS);
    wait_for_measure("STATE 3 - SELECT IDC: SD_CS HIGH, IDC_CS LOW");

    int sd_when_idc = read_pin(SD_PIN_CS);
    int idc_select = read_pin(IDC_PIN_CS);

    printf("Readback SELECT IDC: SD_CS=%d, IDC_CS=%d\n", sd_when_idc, idc_select);

    if (sd_when_idc != 1) {
        printf("Loi: SD_CS bi keo LOW khi chon IDC.\n");
        pass = false;
    }

    if (idc_select != 0) {
        printf("Loi: IDC_CS khong xuong LOW khi chon IDC.\n");
        pass = false;
    }

    cs_high(SD_PIN_CS);
    cs_high(IDC_PIN_CS);

    return pass ? TEST_PASS : TEST_FAIL;
}

/* ============================================================
   IDC LOOPBACK HELPER
   Dung cho SPI-003, SPI-005, SPI-006
   Can noi: IDC_MOSI GPIO13 -> IDC_MISO GPIO12
   ============================================================ */

static bool add_idc_test_device(
    int clock_hz,
    spi_device_handle_t *out_dev
)
{
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = clock_hz,
        .mode = 0,
        .spics_io_num = -1,
        .queue_size = 1
    };

    esp_err_t ret = spi_bus_add_device(IDC_SPI_HOST, &devcfg, out_dev);

    if (ret != ESP_OK) {
        printf("add IDC device failed: %s\n", esp_err_to_name(ret));
        return false;
    }

    return true;
}

static bool idc_loopback_transfer(int clock_hz, int len)
{
    if (!idc_bus_initialized) {
        printf("IDC bus chua khoi tao.\n");
        return false;
    }

    spi_device_handle_t dev;

    if (!add_idc_test_device(clock_hz, &dev)) {
        return false;
    }

    uint8_t tx[1024];
    uint8_t rx[1024];

    if (len > sizeof(tx)) {
        len = sizeof(tx);
    }

    for (int i = 0; i < len; i++) {
        tx[i] = (uint8_t)((i * 37 + 0x5A) & 0xFF);
        rx[i] = 0x00;
    }

    cs_high(SD_PIN_CS);
    cs_low(IDC_PIN_CS);

    esp_err_t ret = spi_transfer_bytes(dev, tx, rx, len);

    cs_high(IDC_PIN_CS);

    spi_bus_remove_device(dev);

    if (ret != ESP_OK) {
        printf("IDC transfer failed: %s\n", esp_err_to_name(ret));
        return false;
    }

    for (int i = 0; i < len; i++) {
        if (rx[i] != tx[i]) {
            printf("Sai du lieu tai byte %d: TX=0x%02X RX=0x%02X\n",
                   i, tx[i], rx[i]);
            return false;
        }
    }

    return true;
}

/* ============================================================
   SPI-003: TOC DO CLOCK SPI
   ============================================================ */

static test_status_t test_spi_003_clock_speed(void)
{
    printf("\n[SPI-003] Kiem tra toc do clock SPI tren IDC bus\n");
    printf("Yeu cau: noi tam IDC_MOSI GPIO13 -> IDC_MISO GPIO12.\n");

    wait_for_measure("Chuan bi do IDC_CLK GPIO14 neu co oscilloscope/logic analyzer");

    int clock_list[] = {
        100000,
        400000,
        1000000,
        2000000,
        5000000,
        10000000
    };

    bool pass = true;

    for (int i = 0; i < sizeof(clock_list) / sizeof(clock_list[0]); i++) {
        int clk = clock_list[i];

        printf("Testing IDC clock = %d Hz...\n", clk);

        bool ok = idc_loopback_transfer(clk, 32);

        printf("  Clock %d Hz : %s\n", clk, ok ? "OK" : "ERROR");

        if (!ok) {
            pass = false;
        }

        vTaskDelay(pdMS_TO_TICKS(300));
    }

    return pass ? TEST_PASS : TEST_FAIL;
}

/* ============================================================
   SD RAW COMMAND HELPER
   Dung cho SPI-004
   ============================================================ */

static bool add_sd_raw_device(spi_device_handle_t *out_dev)
{
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = SD_INIT_FREQ_HZ,
        .mode = 0,
        .spics_io_num = -1,
        .queue_size = 1
    };

    esp_err_t ret = spi_bus_add_device(SD_SPI_HOST, &devcfg, out_dev);

    if (ret != ESP_OK) {
        printf("add SD raw device failed: %s\n", esp_err_to_name(ret));
        return false;
    }

    return true;
}

static bool sd_wait_r1(
    spi_device_handle_t dev,
    uint8_t *r1,
    int max_try
)
{
    for (int i = 0; i < max_try; i++) {
        uint8_t r = spi_transfer_byte(dev, 0xFF);

        if ((r & 0x80) == 0) {
            *r1 = r;
            return true;
        }
    }

    *r1 = 0xFF;
    return false;
}

static uint8_t sd_send_cmd(
    spi_device_handle_t dev,
    uint8_t cmd,
    uint32_t arg,
    uint8_t crc,
    uint8_t *extra,
    int extra_len
)
{
    uint8_t packet[6];

    packet[0] = 0x40 | cmd;
    packet[1] = (arg >> 24) & 0xFF;
    packet[2] = (arg >> 16) & 0xFF;
    packet[3] = (arg >> 8) & 0xFF;
    packet[4] = arg & 0xFF;
    packet[5] = crc;

    cs_high(IDC_PIN_CS);
    cs_low(SD_PIN_CS);

    spi_transfer_bytes(dev, packet, NULL, 6);

    uint8_t r1 = 0xFF;
    sd_wait_r1(dev, &r1, 20);

    for (int i = 0; i < extra_len; i++) {
        extra[i] = spi_transfer_byte(dev, 0xFF);
    }

    cs_high(SD_PIN_CS);

    spi_transfer_byte(dev, 0xFF);

    return r1;
}

static bool sd_cmd0_cmd8_test(void)
{
    if (!sd_bus_initialized) {
        printf("SD bus chua khoi tao.\n");
        return false;
    }

    spi_device_handle_t dev;

    if (!add_sd_raw_device(&dev)) {
        return false;
    }

    cs_high(SD_PIN_CS);
    cs_high(IDC_PIN_CS);

    /*
       SD card can toi thieu 74 clock voi CS HIGH.
       Gui 80 clock de dua SD vao SPI mode.
    */
    for (int i = 0; i < 10; i++) {
        spi_transfer_byte(dev, 0xFF);
    }

    /*
       CMD0: GO_IDLE_STATE
       Neu SD vao idle mode dung, R1 mong muon = 0x01.
    */
    uint8_t r1 = sd_send_cmd(dev, 0, 0x00000000, 0x95, NULL, 0);

    printf("CMD0 R1 = 0x%02X\n", r1);

    if (r1 != 0x01) {
        printf("CMD0 sai. Mong doi 0x01.\n");
        spi_bus_remove_device(dev);
        return false;
    }

    /*
       CMD8: SEND_IF_COND
       SD v2 mong muon:
       R1 = 0x01
       R7 byte cuoi = 0x01 0xAA
    */
    uint8_t r7[4] = {0};

    r1 = sd_send_cmd(dev, 8, 0x000001AA, 0x87, r7, 4);

    printf("CMD8 R1 = 0x%02X, R7 = %02X %02X %02X %02X\n",
           r1, r7[0], r7[1], r7[2], r7[3]);

    bool pass = true;

    if (r1 != 0x01) {
        printf("CMD8 R1 sai. Mong doi 0x01 voi SD card v2.\n");
        pass = false;
    }

    if (!(r7[2] == 0x01 && r7[3] == 0xAA)) {
        printf("CMD8 echo pattern sai. Mong doi ... 01 AA.\n");
        pass = false;
    }

    spi_bus_remove_device(dev);

    return pass;
}

/* ============================================================
   SPI-004: SD VA THIET BI SPI KHAC KHONG XUNG DOT
   ============================================================ */

static test_status_t test_spi_004_no_conflict(void)
{
    printf("\n[SPI-004] Kiem tra SD va thiet bi SPI khac khong xung dot\n");

    cs_high(SD_PIN_CS);
    cs_high(IDC_PIN_CS);

    wait_for_measure("Truoc khi test SD: SD_CS HIGH, IDC_CS HIGH");

    if (read_pin(IDC_PIN_CS) != 1) {
        printf("Loi: IDC_CS khong o muc HIGH truoc khi giao tiep SD.\n");
        return TEST_FAIL;
    }

#if SPI_TEST_ENABLE_SD_CARD

    printf("SPI_TEST_ENABLE_SD_CARD = 1, bat dau gui CMD0/CMD8 toi SD card.\n");

    bool sd_ok = sd_cmd0_cmd8_test();

    if (!sd_ok) {
        printf("Loi: SD card khong phan hoi dung CMD0/CMD8.\n");
        return TEST_FAIL;
    }

    if (read_pin(IDC_PIN_CS) != 1) {
        printf("Loi: IDC_CS bi thay doi sau khi giao tiep SD.\n");
        return TEST_FAIL;
    }

    return TEST_PASS;

#else

    printf("SPI_TEST_ENABLE_SD_CARD = 0, bo qua test phan hoi SD.\n");
    printf("Ket qua SPI-004 la SKIP, khong tinh PASS.\n");
    return TEST_SKIP;

#endif
}

/* ============================================================
   SPI-005: DOC/GHI SPI DAI
   ============================================================ */

static test_status_t test_spi_005_long_transfer(void)
{
    printf("\n[SPI-005] Kiem tra doc/ghi SPI dai tren IDC bus\n");
    printf("Yeu cau: noi tam IDC_MOSI GPIO13 -> IDC_MISO GPIO12.\n");

    bool pass = true;

    int len_list[] = {
        64,
        128,
        256,
        512,
        1024
    };

    for (int i = 0; i < sizeof(len_list) / sizeof(len_list[0]); i++) {
        int len = len_list[i];

        printf("Testing long transfer %d bytes...\n", len);

        bool ok = idc_loopback_transfer(IDC_TEST_FREQ_HZ, len);

        printf("  Long transfer %d bytes : %s\n", len, ok ? "OK" : "ERROR");

        if (!ok) {
            pass = false;
        }

        vTaskDelay(pdMS_TO_TICKS(300));
    }

    return pass ? TEST_PASS : TEST_FAIL;
}

/* ============================================================
   SPI-006: CHAT LUONG TIN HIEU / NHIEU
   ============================================================ */

static test_status_t test_spi_006_signal_quality(void)
{
    printf("\n[SPI-006] Kiem tra chat luong tin hieu / nhieu bang stress test\n");
    printf("Yeu cau: noi tam IDC_MOSI GPIO13 -> IDC_MISO GPIO12.\n");
    printf("Neu co oscilloscope, do IDC_CLK, IDC_MOSI, IDC_MISO, IDC_CS.\n");

    wait_for_measure("Chuan bi do song SPI bang oscilloscope/logic analyzer neu co");

    int error_count = 0;

    int64_t start_us = esp_timer_get_time();

    for (int i = 0; i < SPI006_TOTAL_TEST; i++) {
        bool ok = idc_loopback_transfer(IDC_STRESS_FREQ_HZ, SPI006_PACKET_SIZE);

        if (!ok) {
            error_count++;
        }

        if ((i + 1) % 50 == 0) {
            printf("  Stress %d/%d, error = %d\n",
                   i + 1, SPI006_TOTAL_TEST, error_count);
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }

    int64_t end_us = esp_timer_get_time();

    printf("Tong thoi gian stress: %lld us\n", end_us - start_us);
    printf("Tong loi du lieu: %d/%d\n", error_count, SPI006_TOTAL_TEST);

    return (error_count == 0) ? TEST_PASS : TEST_FAIL;
}

/* ============================================================
   APP MAIN
   ============================================================ */

void app_main(void)
{
    printf("\n\n");
    print_line();
    printf("BAT DAU KIEM THU SPI TREN PCB - ESP32\n");
    print_line();

    test_status_t result;

    result = test_spi_001_init();
    print_result("SPI-001", "Khoi tao thiet bi SPI", result);

    result = test_spi_002_chip_select();
    print_result("SPI-002", "Cach ly chip select", result);

    result = test_spi_003_clock_speed();
    print_result("SPI-003", "Toc do clock SPI", result);

    result = test_spi_004_no_conflict();
    print_result("SPI-004", "SD va thiet bi SPI khac khong xung dot", result);

    result = test_spi_005_long_transfer();
    print_result("SPI-005", "Doc/ghi SPI dai", result);

    result = test_spi_006_signal_quality();
    print_result("SPI-006", "Chat luong tin hieu / nhieu", result);

    print_line();
    printf("KET THUC KIEM THU SPI TREN PCB\n");
    print_line();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}