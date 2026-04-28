# ALM ETH 控制器网络设计决策

## 架构概述

ALM ETH 是演出行业的网络中转控制器，一端连接局域网（PC via Ethernet），另一端连接 CAN FD 设备。局域网中可能存在几十个 ALM ETH，每个 ALM ETH 下挂多个 CAN FD 设备。

```
PC (Client)
  ├── TCP connect → ALM ETH #1 (Server)
  │                    ├── CAN FD device A
  │                    └── CAN FD device B
  ├── TCP connect → ALM ETH #2 (Server)
  └── TCP connect → ALM ETH #N ...
```

---

## 协议选择：TCP

选用 **TCP**，不用 UDP，原因：

- CAN FD 是可靠总线，PC 侧同样需要可靠传输，TCP 特性与场景匹配
- 几十个节点的局域网，TCP 开销完全可接受
- 工业/演出控制场景不可接受丢包，UDP 需自行实现重传逻辑

---

## 角色选择：ALM ETH 做 Server

- ALM ETH 固定部署，IP 固定，天然适合监听
- PC 软件可能重启、多开、切换，做 Client 更灵活
- 一台 PC 可同时连接多个 ALM ETH（多条 TCP 连接），管理清晰
- 避免 ALM ETH 做 Client 时因 PC 重启导致的重连逻辑复杂性

---

## 端口选择：40000

### 演出行业主流协议端口占用

| 协议 | 端口 |
|------|------|
| Art-Net | UDP 6454 |
| sACN (E1.31) | UDP 5568 |
| NetLase | UDP/TCP 7000 附近 |
| Pangolin FB4 | TCP/UDP 7001, 7002 附近 |
| AES67 | UDP 5004, 5005 (RTP/RTCP) |
| Dante | UDP/TCP 4440, 4444, 8700–8800 |

### 选用 **TCP `40000`**，理由：

- 演出行业所有主流协议均未占用此端口
- 整数，易记，便于现场口述
- 远离系统保留端口（0–1023）和常用注册端口
- 属于私有端口安全范围（49152 以下但远离冲突区）

> 建议端口在设备配置中**可修改**，默认值为 40000，避免现场冲突时需要重新刷固件。

---

## 其他实施建议

- **心跳机制**：实现应用层心跳或启用 TCP Keepalive，及时感知断线
- **长连接**：PC 对每个 ALM ETH 保持一条长连接，避免频繁建连开销
- **粘包处理**：TCP 是流协议，帧头需包含长度字段以正确分包
- **多设备寻址**：帧内加设备 ID 字段，区分同一 ALM ETH 下的不同 CAN FD 设备
- **广播过滤**：Art-Net 广播流量较大，ALM ETH 应过滤无关广播包，保障 TCP 连接稳定性
