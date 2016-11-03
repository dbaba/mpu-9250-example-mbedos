PROJECT=mpu-9250-example-mbedos
# TARGET=NUCLEO_F401RE/GCC_ARM
TARGET=NUCLEO_F411RE/GCC_ARM

.PHONY: all

all:
	mbed compile --profile profiles/default.json

debug:
	mbed -c compile --options debug-info --profile profiles/debug.json

clean-build:
	mbed -c compile

deploy:
	st-flash write BUILD/$(TARGET)/$(PROJECT).bin 0x8000000

gdb:
	# -> Run openocd then run this, and:
	# (gdb) target remote :3333
	# (gdb) load
	# (gdb) monitor reset init
	arm-none-eabi-gdb BUILD/$(TARGET)/$(PROJECT).elf

openocd:
	openocd -f interface/stlink-v2.cfg -f target/stm32f4x.cfg

openocd_nucleo:
	openocd -f interface/stlink-v2-1.cfg -f target/stm32f4x.cfg
