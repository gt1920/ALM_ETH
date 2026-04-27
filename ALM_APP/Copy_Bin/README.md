# Copy_Bin 使用说明

打包工具：把 Keil 产出的 `.bin` 复制改名、调用 hex 合并、AES 加密为 `.gt`。配套 Bootloader/App 的 FW_ID + SN 绑定 + 头部全加密方案（`.gt` 前 16B 为 AES 密文头，含 CRC32）。

## 目录约定

Copy_Bin.exe 所在目录 = `App\Copy_Bin\`，相对路径按此基准：

| 名称 | 路径 | 说明 |
|---|---|---|
| 输入 bin | `..\Output\<name>.bin` | Keil Rebuild 产物 |
| 输出目录 | `..\HEX\` | 带时间戳的 `.bin` / `.hex` / `.gt` |
| 合包目录 | `..\HEX\app\` | 预放 `Run.bat` + `hex_merge` 工具，用于 Bootloader+App 合并 `Full_Package.hex` |

## 调用方式

```
Copy_Bin.exe <name>
```

`<name>` 是 `App\Output\` 里的 bin 基名（不带 `.bin` 后缀），例如：

```
cd App\Copy_Bin
Copy_Bin.exe B87_DT_30Wz
```

## 工具做了什么

1. 读取 `..\Output\<name>.bin`
2. 复制到 `..\HEX\<name>_yyMMdd_HHmm.bin`
3. 再复制一份到 `..\HEX\app\app.bin`（为合包的 `Run.bat` 使用）
4. 切到 `..\HEX\app\`，执行 `Run.bat`（生成 `Full_Package.hex`，即 Bootloader + App 合并 hex）
5. 把 `Full_Package.hex` 改名搬到 `..\HEX\<name>_yyMMdd_HHmm.hex`
6. 扫 bin 中 `FW_ID_MAGIC (0x47544657)`，取最后一个匹配，解出 `fw_sn`
7. 根据 `fw_sn` 拼文件名 Tag：
   - `0xA5C3F09E` → `Unlock`（通配 / 任何 MCU 可装）
   - 其它值 → 8 位大写 HEX（如 `3C41003F`，绑定 MCU SN）
8. AES-256-CBC 加密 bin → `..\HEX\<name>_<SnTag>_yyMMdd_HHmm.gt`
   - 前 16B：**独立 CBC 加密的密文头**，明文 = `magic(4)+board(2)+ver(2)+sn(4)+crc32(4)`
   - 其后：原有 Metadata + Payload（独立 CBC 链，初始 IV）
9. 删除第 2 步的 `.bin`（`.gt` 是发布产物，`.bin` 不下发）

## 典型流程

1. Keil 改 `App\BSP\FW_ID.h` 里的 `#define FW_SN` 为目标 SN（或 `FW_SN_WILDCARD` 通配）
2. Keil Rebuild → `..\Output\<name>.bin`
3. `cd App\Copy_Bin && Copy_Bin.exe <name>`
4. 产物到 `..\HEX\` 目录，`.gt` 发给 PC 工具 OTA，`.hex` 用 J-Link/ST-Link 整片烧

## 输出自检

控制台应打印类似：
```
FW_ID: magic=0x47544657, board=0x0B87, ver=0x0001, sn=0x3C41003F, crc=0xXXXXXXXX (绑定 MCU SN)
```
若显示 `警告: bin中未找到FW_ID magic(0x47544657)`：说明 `FW_HW_ID` 结构未被编进 bin（可能被优化掉或 `FW_ID.c` 未参与构建），**不要发布**这批 `.gt`。

## 依赖

- .NET 8 运行时。若目标机未装，运行本目录内的 `dotnet-runtime-8.0.25-win-x64.exe` 即可。
- `..\HEX\app\Run.bat` 与合包所需的工具（`hex_merge.exe` 等）需预先就位；与本工具独立维护。

## 注意事项

- **不要手工编辑 `.gt`**：前 16B 密文头有 CRC32，Bootloader / App 会拒绝任何一 bit 翻转。
- **老 12B 明文头 `.gt` 不兼容**：2026-04-20 起头部纳入 AES，旧版 `.gt` 在新 Bootloader 下会被拒。所有在库 `.gt` 必须用本工具重打。
- AES Key / IV 写死在 [Program.cs](../../Copy_Bin_Project/Copy_Bin/Program.cs) 中，必须与 [G0B1_SPI_Bootloader/BSP/AES.c](../../G0B1_SPI_Bootloader/BSP/AES.c) 和 [App/BSP/AES.c](../BSP/AES.c) 三处保持一致。
