*App 在 Keil 中必须勾选生成 .bin 文件；
 不能直接用 Keil 自带的 HEX —— 因为 BL hex 与 App bin 要走 srec_cat 拼接。

内存布局 (motor_partition.h)：
  Bootloader : 0x08000000 ~ 0x080027FF  (10 KB,   5 pages)
  App        : 0x08002800 ~ 0x0800FFFF  (54 KB,  27 pages)
  Staging    : 0x08010000 ~ 0x0801F7FF  (62 KB,  31 pages, 装 .mot)
  Params     : 0x0801F800 ~ 0x0801FFFF  ( 2 KB,   1 page, node_id 等，OTA 不擦)

步骤：
1. 编译 ALM_Motor_Bootloader
   它的 After Build 里挂了 MDK-ARM\Copy_HEX.bat，会把 ALM_Motor_Bootloader.hex
   自动复制到本目录的 Bootloader\BooLoader.hex（同时也存一份到
   ALM_Motor_Bootloader\HEX\）。
2. 编译 ALM_Motor_App，生成 app.bin，放到本目录根。
3. 运行 Run.bat：
     - 按 0x08002800 偏移把 app.bin 转 Intel HEX
     - 与 Bootloader\BooLoader.hex 拼成 Full_Package.hex
4. 用 ST-Link / J-Link 把 Full_Package.hex 一次烧入 MCU。
