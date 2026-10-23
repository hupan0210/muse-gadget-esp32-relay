# 固件中继补丁说明

本目录的 `firmware-relay.patch` 是对 Meta 官方 [muse-gadget-sdk](https://github.com/facebookincubator/muse-gadget-sdk)（ESP32 固件）的本地修改，共 4 处。目的只有一个：让位于受限网络后的 ESP32-C3 把全部云端流量改走自建中继（新加坡节点，`xjp.112583.xyz:10443`），而不是直连 `api.muse.ai` / `hatch.metaaivm.com`（直连会被快速重置，设备永远连不上云）。

## 改动明细

| 文件 | 改动 | 原因 |
|---|---|---|
| `esp32/main/vm_api.h` | 默认 API 基址 `https://api.muse.ai` → `https://xjp.112583.xyz:10443` | REST 接口改走中继 |
| `esp32/main/vm_api.c` | `vm_api_set_base_url()` 忽略入参，强制使用默认（中继）基址 | 配对流程会把官方端点下发并覆盖基址，不拦截就会被改回去 |
| `esp32/components/muse/muse_account_api.c` | `FETCH_URL` 指向中继的 `/fetch_vms` | 账号/虚拟机列表拉取同样走中继 |
| `esp32/main/noise_control.cpp` | Noise 默认主机改为中继域名、端口 443 → 10443；`noise_ctrl_set_host()` 忽略入参 | 实时 WebSocket 通道（Noise 协议）也必须走中继，且防止配对下发覆盖 |

## 应用方法

在 muse-gadget-sdk 的本地检出里执行：

```bash
git apply firmware-relay.patch
```

然后按官方说明编译、只刷写 app 分区（保留 NVS 里的配对信息）：

```bash
# 参数示意，偏移以你的分区表为准
esptool.py write_flash <app 分区偏移> build/muse_gadget.bin
```

## 中继端要求

中继就是一台带公网 IP 的小服务器 + nginx 反向代理（配置见仓库 `relay/muse-relay.conf`）：

- `https://<中继>:10443/` → 反代 `https://api.muse.ai`
- `wss://<中继>:10443/v1/noise` → 反代 `wss://hatch.metaaivm.com`（开 WebSocket 升级与长连接超时）
- 证书用任意受信任 CA 签发的域名证书（设备端会做 TLS 校验）
