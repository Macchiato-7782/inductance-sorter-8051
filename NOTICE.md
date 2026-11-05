# 来源与第三方说明

本仓库中的原创代码、Proteus 工程整理和文档采用 MIT 许可证。

- `firmware/keil/STARTUP.A51` 是 Keil C51 的标准启动文件，版权归 Keil/ARM；保留它是为了让 Keil 工程可以直接编译。若你不需要，可以删除并在 Keil 中重新添加标准启动文件。
- Proteus 是商业软件，仓库只提供 `.pdsprj` 工程文件，不包含 Proteus 安装包、破解文件或厂商模型。
- `firmware/prebuilt/exp6_8051.hex` 由本仓库的 8051 源码编译得到，仅用于快速仿真。

## 安全与隐私

仓库已移除个人姓名、学号、邮箱、电话、Windows 用户名、机器名、绝对路径和编辑器用户配置。`.pdsprj` 内如仍出现旧固件路径，打开工程时按需重新指定 Program File 即可。
