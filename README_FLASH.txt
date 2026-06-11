ESP-IDF SPI PCB TEST - V2 CORRECTED

Cach nap:
1. Giai nen thu muc project.
2. Mo ESP-IDF terminal tai thu muc project.
3. Chay:
   idf.py set-target esp32
   idf.py build
   idf.py -p COMx flash monitor

Diem sua quan trong:
- SPI-002: CS duoc cau hinh GPIO_MODE_INPUT_OUTPUT_OD + pull-up de doc muc thuc tren PCB.
  Neu SD_CS/IDC_CS bi keo LOW, code se bao FAILED.
- SPI-004: Neu SPI_TEST_ENABLE_SD_CARD = 0 thi ket qua la SKIP, khong tinh PASS.
  Muon test SD that, cam the SD va sua trong main/spi_test_pins.h:
  #define SPI_TEST_ENABLE_SD_CARD 1
