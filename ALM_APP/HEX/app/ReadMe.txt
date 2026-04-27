*app需要在Keil中生成bin文件，不能使用Keil自带的Hex，然后通过外部指令转换为Hex，然后再合并才能一起下载

内存布局：
  Bootloader: 0x08000000 ~ 0x08007FFF  (32 KB)
  App:        0x08008000 ~ 0x080FFFFF  (992 KB)

步骤：
1. Bootloader编译成Hex，放在Bootloader目录里面，文件名：BooLoader.hex
   (ALM_Bootloader 工程已配 Copy_HEX.bat 自动复制到此处)
2. app编译成bin，放在根目录
3. 运行Run.bat合并工具，按 0x08008000 偏移把 app.bin 转 Hex 并与 BooLoader.hex 拼接成 Full_Package.hex