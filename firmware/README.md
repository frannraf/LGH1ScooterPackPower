# Firmware Binary

`Jump_Start_BluePill_F103C8_USB_CDC_CAN_PB8_PB9.bin` is a prebuilt binary for:

- STM32F103C8 Blue Pill
- STM32duino `2.12.0`
- USB CDC serial enabled
- CAN remapped to `PB8` / `PB9`
- `1 second` startup delay before wake frames

## Flash Without Arduino IDE

The easiest path is STM32CubeProgrammer:

1. Connect ST-Link to the Blue Pill over SWD.
2. Open STM32CubeProgrammer.
3. Select `ST-LINK`, then connect.
4. Open the `.bin` file.
5. Set start address to `0x08000000`.
6. Program and verify.

Command-line example if `STM32_Programmer_CLI.exe` is installed:

```powershell
STM32_Programmer_CLI.exe -c port=SWD -w firmware\Jump_Start_BluePill_F103C8_USB_CDC_CAN_PB8_PB9.bin 0x08000000 -v -rst
```

You can also use OpenOCD:

```powershell
C:\Users\fchavez\AppData\Local\Arduino15\packages\STMicroelectronics\tools\xpack-openocd\0.12.0-6\bin\openocd.exe -s C:\Users\fchavez\AppData\Local\Arduino15\packages\STMicroelectronics\tools\xpack-openocd\0.12.0-6\openocd\scripts -f interface/stlink.cfg -f target/stm32f1x.cfg -c "program firmware/Jump_Start_BluePill_F103C8_USB_CDC_CAN_PB8_PB9.bin 0x08000000 verify reset exit"
```
