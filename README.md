# Muse 桌面智能体终端（ESP32-C3 + 自建中继）

把 Muse 智能体装进一块 ESP32-C3 小屏幕里：一台巴掌大的桌面终端，实时显示智能体在线状态与本周 Token 用量，还能远程推图。

本地网络无法直连海外的智能体接口（连接会被快速重置），所以整套方案的核心是**自建中继**：设备所有云端流量先到新加坡节点，再由节点转发到 Muse 云端。全链路已实测打通：设备配对、Wi-Fi 连云、WebSocket 实时通道握手、状态注册、图片推送全部正常。

## 系统结构

```
┌──────────────┐   TLS/WSS    ┌────────────────┐   TLS/WSS    ┌──────────────┐
│  ESP32-C3    │ ───────────▶ │  自建中继       │ ───────────▶ │  Muse 云端    │
│  智能体终端   │  :10443      │  nginx 反向代理 │              │  api.muse.ai │
└──────────────┘              │  (新加坡节点)   │              │  hatch...    │
       ▲                      └────────────────┘              └──────────────┘
       │ 明文 HTTP 拉图（C3 内存有限，HTTPS 下图会被内存闸拒绝）
       └──────────  静态图片/JSON 托管（同一节点的 80 端口）
```

## 仓库内容

| 目录 | 内容 |
|---|---|
| `gadget-firmware/` | 对 Meta 官方 muse-gadget-sdk 的中继补丁（`firmware-relay.patch`）及逐项说明（`PATCHES.md`） |
| `relay/` | 中继节点实际在用的 nginx 配置 `muse-relay.conf` |
| `tools/` | `render_tokens.py`：把 Muse 用量渲染成 128×160 的仪表盘 JPEG（周用量%、进度条、重置时间、额外额度），供终端推图显示 |
| `token-ticker/` | 第二块 ESP32-C3 的独立固件（PlatformIO/Arduino）：每 60 秒拉取一次 `token.json`，在 240×240 ST7735 屏幕上显示时钟 + 周用量大字百分比 + 进度条 + 重置时间 |

## 终端固件要点

- 芯片 ESP32-C3（4MB Flash、无 PSRAM），固件基于官方 muse-gadget-sdk 本地移植
- 补丁只做一件事：把 API 基址与 Noise 通道强制指向自建中继，并拦截配对下发的端点覆盖（详见 `gadget-firmware/PATCHES.md`）
- 刷写时只刷 app 分区、保留 NVS，配对信息不丢
- 推图规格：128×160 baseline JPEG；HTTPS 下图会触发固件的内存闸，改走明文 HTTP 静态托管解决

## Token 用量屏（token-ticker）

```bash
cd token-ticker
# 先把 src/main.cpp 里的 Wi-Fi 与数据地址占位符改成你自己的
pio run -t upload
```

数据契约见 `token-ticker/token.json.example`：服务端把用量写成静态 JSON 放到任意 HTTP 目录，固件每 60 秒拉取并局部重绘（变了才重画，防闪烁）。

## 说明

- 本仓库只包含我自己的补丁、配置与工具代码；终端固件本体来自 Meta 的 [muse-gadget-sdk](https://github.com/facebookincubator/muse-gadget-sdk)，其许可与版权归原项目所有。
- 域名、IP 等均为我自有资产；Wi-Fi 凭据等敏感信息已在入库前脱敏为占位符。
