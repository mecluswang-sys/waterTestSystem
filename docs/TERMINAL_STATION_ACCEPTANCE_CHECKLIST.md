# Terminal-Station 架构验收清单

适用目标架构：1 台工控机 + 1 台 PLC + 4 台工业平板（操作台）。

## 1. 部署前检查

- 工控机与 PLC 网络互通（ping PLC IP 成功）。
- 4 台平板与工控机网络互通（可 ping 工控机 IP）。
- 工控机 `config/system.conf` 已确认如下关键项：
  - `plc.ip`, `plc.rack`, `plc.slot`
  - `data.collection_interval`
  - `station.strict_remote_mode = true`
- 工控机防火墙允许主程序监听端口（默认 `5555`）。

## 2. 启动顺序

1. 在工控机启动 Terminal：
   - `WaterTestSystem.exe --mode terminal --port 5555`
2. 等待 Terminal 日志显示 PLC 连接成功和采集启动。
3. 在 4 台平板分别启动 Station：
   - 平板1：`WaterTestSystem.exe --mode station --id 1 --name "Station 1" --host <工控机IP> --port 5555`
   - 平板2：`WaterTestSystem.exe --mode station --id 2 --name "Station 2" --host <工控机IP> --port 5555`
   - 平板3：`WaterTestSystem.exe --mode station --id 3 --name "Station 3" --host <工控机IP> --port 5555`
   - 平板4：`WaterTestSystem.exe --mode station --id 4 --name "Station 4" --host <工控机IP> --port 5555`

## 3. 连接状态验收

- 工控机 Terminal 可看到 4 个 Station 已连接。
- 每台平板状态栏显示“已连接到主控台”。
- 在严格远程模式下，平板端本地 PLC 连接按钮应不可用或不触发本地连接。

## 4. 数据链路验收

- 任一平板实时监控页可看到压力/流量刷新。
- 断开工控机与 PLC 网线后：
  - 工控机显示 PLC 异常或数据停滞。
  - 平板显示数据异常/停滞，但仍保持与工控机 TCP 连接（视网络是否中断）。
- 恢复网线后，数据恢复更新。

## 5. 控制链路验收（关键）

在任意一台平板依次执行，工控机侧观察命令日志与设备状态变化：

1. 准备区：开始加水、停止、放水、紧急停止。
2. 测试区：电磁阀切换、开始测试、停止测试、紧急停止。
3. 1号操作台：阀门弹窗开/关、继电器按钮切换。
4. 自动测试区：待测阀开/关、自动测试启动/停止。

验收标准：
- 平板操作后，命令由 Terminal 执行；平板不依赖本地 PLC 直连。
- 同一设备状态在其他平板可观测到一致变化（允许存在 1-2 个刷新周期延迟）。

## 6. 异常场景验收

### 6.1 平板断线重连

- 关闭任意平板进程后重启，能重新注册并恢复数据与控制。

### 6.2 主控重启

- 重启工控机 Terminal 后，平板可重新连接并恢复数据。

### 6.3 命令失败处理

- 工控机断网或停止服务时，平板点击控制应提示“主控通信异常/失败”。

## 7. 退出顺序

1. 先停止 4 台平板。
2. 再停止工控机 Terminal。
3. 检查工控机日志：采集线程已退出、PLC 已断开。

## 8. 交付结论模板

- 架构模式：Terminal-Station（严格远程）
- 设备规模：1 工控机 + 1 PLC + 4 平板
- 连接结果：通过/不通过
- 数据链路：通过/不通过
- 控制链路：通过/不通过
- 异常场景：通过/不通过
- 遗留问题：
  - 问题1：
  - 问题2：
