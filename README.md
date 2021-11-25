# PCF79xx_Tool
This project provides a STM32 based programmer for programming PCF79xx automotive key transponder chips over the custom monitor- and download interface (mdi).
The PCF79xx_Tool enumerates itself as serial port. It can either be controlled by later described commands over a serial terminal, or over a GUI offered by an other github project (which is yet under development).

The project base originates from the github project https://github.com/w01230/PCF7953.

## Supported PCF79xx chips:
- PCF7945C05 (BMW CAS3 key, 5WK49125) --> Tested ok!
- 26A0700 (BMW CAS3 china key)

## Hardware:
This project is designed for STM32F1 series µC's. It works perfectly on a "Blue Pill" Board which is using the STM32F103C8T6.
The "Debug" folder contains a .bin file which can be simply flashed on the STM32, e.g. with 2$ St-Link v2 clone via SWD.

**Connecting the Blue Pill to PCF79xx chip:**

STM32F1 Pin | PCF79xx Pin
-----| -----------
B12 | VCC / BAT
B13 | MSDA
B14 | MSCL
GND | GND

## Implemented Features:
- Memory Programming: EROM, EEROM
- Memory Reading: &nbsp; &nbsp; &nbsp; &nbsp;  EROM, EEROM, EEROM Special bytes (EEROM Page 127 byte 2 and 3)
- Erase (=unlock) chip
- Protect (=lock) chip
- Read checksum: EROM, EEROM, ROM (somehow command not working with PCF7945C05)

## Usage:
A detailed description of all supported commands can be found in the Word / PDF Documentation.
Nevertheless, the following paragraphs shall show the basic workflow used in the user interface protocol at the example of a PCF7945C05 (5WK49125 BMW key).

1. Connect the PCF79xx_Tool to the PC's USB Port. For Windows 8 and higher, no drivers are needed. For Windows 7 you need to install Nuvoton VCP driver (e.g. here: https://github.com/teamjft/NUVOTON-NUC200/blob/master/NUC200Series_sample/SampleCode/USB/VCOM/Windows%20Driver/NuvotonCDC.inf)
2. The device should enumerate as new COM Port. In the serial terminal (e.g. HTerm), connect to this COM Port. The settings (baud rate etc.) don't care.
3. Connect the PCF79xx to the Board according to the plan.
4. In the serial terminal, send 5 bytes "09 00 00 01 00".
5. If the tool responds with "91", the PCF is in protected mode and thus is locked. Execute the next step. Otherwise the tool responds with "06". In this case skip step 6.
6. Send 5 bytes "09 01 00 01 00". This will erase the chip's memory and unlock the chip. The tool should respond with "06"
We want to program a hex file with 8kB size to the EROM beginning at adress 0. In the first step, the file must be uploaded to the PCF79xx_Tool's buffer. In the second step, the buffer can be programmed to the PCF.
7. Open the file in a hex editor. At the beginning, add 5 bytes "2B 00 00 00 20".  Byte 1 and 2 (00 00) specify the start address of the PCF memory where the data shall be written. Byte 3 and 4 specify the (byte-swapped) file size 0x2000 = 8192 byte = 8kB. At the END of the hex file, add 4 bytes CRC32 checksum of the raw data (can be calculated with online checksum calculator). Then save the file.
8. With the serial terminal, send the prepared file to the tool. The tool shall confirm this with "06"
9. Send 5 bytes "4B 00 00 00 02" to the tool. It will take some seconds while the tool will programm hex data from buffer to the PCF. It should answer with "06", indicating that the write EROM operation was successful
10. Check if the data has correctly been written to PCF by reading the EROM. Send 5 bytes "0D 00 00 00 02" to the tool.
11. The tool reads the PCF and dumps its content (8192 bytes) to the serial console, followed by confirmation byte "06". This 8192 bytes should be equal to what has been programmed.

## Used License
- The project itself (all files which don't have a license specified in the header) is licensed under the MIT license
- Driver files from ST's Standard Peripheral- CMSIS- and HAL library are licensed under the BSD 3-Clause license by ST. The license text is given in the header of these files

## Further Development:
- [ ] Bootloader for updating the firmware via USB (USB Mass Storage Device)
- [ ] Test and add support for other PCF79xx type transponders
- [ ] Investigate the missing part for making a programmed PCF7945C05 (BMW CAS3 5WK49125 Key) working

## FAQ:
**Q:** &nbsp; I reprogrammed the EEROM and EROM of an used 5WK49125 Key for BMW CAS3, but afterwards the Key is not acessible with Hitag RFID Programmer!<br>
**A:** &nbsp; Unfortunately, it is not yet possible to produce a working key. It seems that something is missing in the programming procedure. If you have a commercial PCF flash tool, it would be great if you provide a logic analyzer capture of the programming procedure, so the missing part could be identified and implemented.


**Q:** &nbsp; When I read out the EEROM of the PCF after programming, there are still some bytes which are different from the binary I programmed!<br>
**A:** &nbsp;Yes, that's a normal behavior. The PCF has special EEROM pages which are read-only. E.g. Page 1 (contains the device ID), Page 126, 127 (ony byte 0 and 1 read-only).

**Q:**  &nbsp; I tryed to connect to PCF, but I always get response 0x90 or 0x91 <br>
**A:**  &nbsp; 0x90 -> Double-check if the connection wires are making contact. <br>
**A:**  &nbsp; 0x91 -> PCF is locked. Unlock it by issuing the command 09 01 00 01 00 (for PCF7945). Be aware that all data on the PCF will be erased!
