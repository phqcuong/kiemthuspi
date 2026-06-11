#ifndef SPI_TEST_PINS_H
#define SPI_TEST_PINS_H

#include "driver/spi_master.h"

/* ============================================================
   CAU HINH CHAN GPIO THEO SCHEMATIC PCB
   Neu doi version PCB hoac doi mapping chan, chi sua file nay.
   ============================================================ */

/* SPI SD CARD */
#define SD_SPI_HOST             SPI3_HOST
#define SD_PIN_CS               5
#define SD_PIN_CLK              18
#define SD_PIN_MISO             19
#define SD_PIN_MOSI             23

/* SPI IDC / THIET BI SPI PHU */
#define IDC_SPI_HOST            SPI2_HOST
#define IDC_PIN_CS              25
#define IDC_PIN_CLK             14
#define IDC_PIN_MISO            12
#define IDC_PIN_MOSI            13

/* ============================================================
   BAT / TAT PHAN TEST SD CARD

   1 = Co cam SD card, test CMD0/CMD8 that.
   0 = Bo qua test phan hoi SD, SPI-004 se SKIP.

   Hien tai thay da co SD card nen de 1.
   ============================================================ */
#define SPI_TEST_ENABLE_SD_CARD 1

/* ============================================================
   THONG SO TEST
   ============================================================ */

#define SD_INIT_FREQ_HZ         400000      // SD init nen chay cham
#define IDC_TEST_FREQ_HZ        1000000
#define IDC_STRESS_FREQ_HZ      5000000

#define MEASURE_DELAY_MS        4000        // giu trang thai de do bang dong ho
#define SPI006_TOTAL_TEST       300
#define SPI006_PACKET_SIZE      128

#endif