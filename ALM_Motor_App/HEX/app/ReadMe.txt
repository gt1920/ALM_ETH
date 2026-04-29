*App 在 Keil 中必须勾选生成 .bin 文件；
 不能直接用 Keil 自带的 HEX —— 因为 BL hex 与 App bin 要走 srec_cat 拼接。

内存布局 (motor_partition.h)：
  Bootloader : 0x08000000 ~ 0x08002FFF  (12 KB,   6 pages)
  App        : 0x08003000 ~ 0x0800FFFF  (52 KB,  26 pages)
  Staging    : 0x08010000 ~ 0x0801F7FF  (62 KB,  31 pages, 装 .mot)
  Params     : 0x0801F800 ~ 0x0801FFFF  ( 2 KB,   1 page, node_id 等，OTA 不擦)

步骤（自动流程，推荐）：
1. 编译 ALM_Motor_Bootloader
   - After Build 自动跑 MDK-ARM\Copy_HEX.bat
   - 把 ALM_Motor_Bootloader.hex 复制到本目录 Bootloader\BooLoader.hex
     和 ALM_Motor_Bootloader\HEX\
   - 打印 BL ROM 占用百分比
2. 编译 ALM_Motor_App
   - After Build 自动跑 MDK-ARM\After_Build.bat：
       a) 把 Output\ALM_Motor_App.bin 复制到本目录 app.bin
       b) Copy_Bin.exe 加密出 ..\ALM_Motor_App_Unlock_<时间>.mot（OTA 用）
       c) srec_cat 合并 BooLoader.hex + app.bin -> 带时间戳的
          ..\ALM_Motor_App_Full_Package_<时间>.hex（SWD 烧机用）
       d) 打印 App ROM 占用百分比
3. 用 ST-Link / J-Link 烧 ..\ALM_Motor_App_Full_Package_<时间>.hex。

手动后备（仅当 After_Build.bat 因故没跑时使用）：
- 双击本目录 Run.bat
- 按 0x08003000 偏移把 app.bin 转 HEX，再与 Bootloader\BooLoader.hex 合并
- 输出本目录的 Full_Package.hex（无时间戳）
