*app需要在Keil中生成bin文件，不能使用Keil自带的Hex，然后通过外部指令转换为Hex，然后再合并才能一起下载

步骤：
1. Bootloader编译成Hex，放在Bootloader目录里面，文件名：BooLoader.hex
2. app编译成bin，放在根目录
3. 运行Run.bat合并工具，合并成新的HEX