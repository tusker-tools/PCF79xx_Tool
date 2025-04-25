%1\Ext_Tools\srec_cat.exe  ..\Debug\PCF79xx_Prog_CubeIDE.hex -Intel ^
-fill 0xFF 0x08004000 0x0800FFFC ^
-crop 0x08004000 0x0800FFFC  ^
-CRC32_Big_Endian 0x800FFFC ^
-o ..\Debug\PCF79xx_Prog_CubeIDE_CRC32.hex -Intel

