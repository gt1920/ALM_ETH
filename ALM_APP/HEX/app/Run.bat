::srec_cat.exe boot.bin -Binary -offset 0x8000000 -o boot1.hex -Intel

@echo off

::将app.bin转为带地址的HEX (App 起始地址 0x08008000，Bootloader 占 0x08000000~0x08007FFF)
.\srecord\srec_cat.exe .\app.bin -Binary -offset 0x8008000 -o .\app1.hex -Intel

::将boot.hex与刚转换出来的app1.hex合并，生成新的hex
.\srecord\srec_cat.exe .\Bootloader\BooLoader.hex -Intel .\app1.hex -Intel -o .\Full_Package.hex -Intel

del app1.hex