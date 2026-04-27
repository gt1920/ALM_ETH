::srec_cat.exe boot.bin -Binary -offset 0x8000000 -o boot1.hex -Intel

@echo off

::将app.bin转为带地址的HEX
.\srecord\srec_cat.exe .\app.bin -Binary -offset 0x8004800 -o .\app1.hex -Intel

::将boot.hex与刚转换出来的app1.hex合并，生成新的he
.\srecord\srec_cat.exe .\Bootloader\BooLoader.hex -Intel .\app1.hex -Intel -o .\Full_Package.hex -Intel

del app1.hex