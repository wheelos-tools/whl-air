# 本地测试计划：图像传输（真实相机）

## 1. 目标
验证车端真实相机视频通过 WebRTC 稳定传输到座舱页面，满足以下验收指标：

- 首帧时间（TTFF） `< 1s`
- 平均 FPS `>= 15`（最低可用 `>= 10`）
- 黑屏/冻结次数可控（建议 30 分钟内 `<= 2` 次，单次冻结 `< 2s`）
- 连续 30 分钟不断流

## 2. 环境准备

- 代码根目录：`/home/wfh/01code/whl-air`
- 已可执行：`./vehicle_client/vehicle_client_app`、`./cockpit_client/cockpit_client_app`
- 配置文件：
  - `./configs/vehicle_client_conf.txt`
  - `./configs/cockpit_client_conf.txt`
- 建议先执行预检：

```bash
./scripts/preflight_runtime_check.sh
```

## 3. 执行步骤（多进程）

### 3.1 启动信令服务

```bash
cd signaling_server
npm install
PORT=8898 npm start
```

### 3.2 启动车端

```bash
cd /home/wfh/01code/whl-air
./vehicle_client/vehicle_client_app --config ./configs/vehicle_client_conf.txt
```

### 3.3 启动座舱端

```bash
cd /home/wfh/01code/whl-air
./cockpit_client/cockpit_client_app --config ./configs/cockpit_client_conf.txt
```

### 3.4 打开浏览器页面

访问：

- `http://<cockpit-host>:8899/`
- 或项目集成页（如 `rtc_integration_test.html` / `rtc_remote_driving_validation_test.html`）

## 4. 指标测量方法

### 4.1 首帧时间（TTFF）

- 起点：点击连接/页面触发会话建立时刻（t0）
- 终点：视频标签首次出现有效画面时刻（t1）
- 结果：`TTFF = t1 - t0`
- 工具建议：浏览器开发者工具 + 页面日志时间戳

### 4.2 FPS

- 优先使用页面内统计（若页面已有 getStats 指标）
- 无页面指标时，使用浏览器性能面板或自定义采样 60s
- 记录平均 FPS 与最低 FPS

### 4.3 黑屏/冻结

- 黑屏：视频画面不可见或仅黑帧
- 冻结：画面静止且持续超过 500ms
- 记录每次发生时间、持续时长、是否自动恢复

### 4.4 30 分钟稳定性

- 连续运行 30 分钟
- 每 5 分钟记录一次：FPS、是否黑屏、是否冻结、连接状态
- 若中断，记录中断时间点与日志片段

## 5. 结果记录模板（建议）

| 检查项 | 目标 | 实测 | 结论 |
|---|---|---|---|
| 首帧时间 | `< 1s` |  |  |
| 平均 FPS | `>= 15` |  |  |
| 最低 FPS | `>= 10` |  |  |
| 黑屏次数（30min） | `<= 2` |  |  |
| 冻结次数（30min） | 可控 |  |  |
| 连续不断流（30min） | 通过 |  |  |

## 6. 自动化可验证项（当前仓库）

以下项可在当前环境先行验证（不依赖真实相机画面质量）：

```bash
# 1) 运行时前置检查
./scripts/preflight_runtime_check.sh

# 2) C++/协议基础回归
bazel test //:rtc_sanity_smoke_test //:rtc_headers_compile_test //:signaling_message_test //:signaling_proto_contract_test //:webrtc_manager_compile_test

# 3) 信令服务路由+集成验证
cd signaling_server && npm run test:all
```

## 7. 注意事项

- `8898`（信令）与 `8899`（座舱本地页面）端口必须可用。
- 真实相机测试受摄像头驱动、权限、网络抖动影响，建议保留完整日志。
- 若涉及跨机访问，`<cockpit-host>` 需替换为座舱机器可达地址。
