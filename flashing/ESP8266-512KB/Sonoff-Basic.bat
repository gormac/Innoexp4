c:\Python27\Scripts\esptool.py.exe --port COM5 --baud 115200 write_flash --flash_freq 80m --flash_mode dout --flash_size 512KB 0x0000 "boot_v1.6.bin" 0x1000 espruino_esp8266_user1.bin 0x7C000 esp_init_data_default.bin 0x7E000 blank.bin