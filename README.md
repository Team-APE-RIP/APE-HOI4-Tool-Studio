<div align="center">
<a href="https://apehts.czxieddan.top/">
 <img src="https://apehts.czxieddan.top/apehts.ico" alt="APE HOI4 Tool Studio Logo" width="256" height="256" />
</a>
<h1>APE HOI4 Tool Studio</a></h1>
<p><strong>面向《Hearts of Iron IV》创作生态的模块化桌面工具工作台</strong><br>
 一个围绕 <b>HOI4 工具链</b>、<b>模块化 Tool / Plugin 架构</b>、<b>桌面端扩展生态</b> 打造的现代化工作室式项目。</p>
</div>
<div align="center">
<p>
  <img src="https://img.shields.io/github/stars/Team-APE-RIP/APE-HOI4-Tool-Studio?style=for-the-badge&logo=github&color=7C3AED" alt="stars" />
  <img src="https://img.shields.io/github/issues/Team-APE-RIP/APE-HOI4-Tool-Studio?style=for-the-badge&logo=github&color=DC2626" alt="issues" />
  <img src="https://img.shields.io/badge/License-APE--HTS--SCL-ff3a68?style=for-the-badge" alt="License" />
  <a href="https://apehts.czxieddan.top/d-l/">
    <img src="https://img.shields.io/badge/立即下载-最新版安装包-22C55E?style=for-the-badge&logo=windows&logoColor=white" alt="download latest installer" />
  </a>
</p>
</div>
<div align="center">
<p>
  <img src="https://img.shields.io/github/repo-size/Team-APE-RIP/APE-HOI4-Tool-Studio?style=flat-square&color=0EA5E9" alt="repo size" />
  <img src="https://img.shields.io/badge/C%2B%2B-17-00599C?style=flat-square&logo=c%2B%2B" alt="C++17" />
  <img src="https://img.shields.io/badge/Qt-6-41CD52?style=flat-square&logo=qt" alt="Qt6" />
  <img src="https://img.shields.io/badge/Platform-Windows-0078D6?style=flat-square&logo=windows" alt="Windows" />
  <img src="https://img.shields.io/badge/Status-Active%20Development-F59E0B?style=flat-square" alt="status" />
</p>
</div>

---


[![FOSSA Status](https://app.fossa.com/api/projects/git%2Bgithub.com%2FTeam-APE-RIP%2FAPE-HOI4-Tool-Studio.svg?type=large)](https://app.fossa.com/projects/git%2Bgithub.com%2FTeam-APE-RIP%2FAPE-HOI4-Tool-Studio?ref=badge_large)

## ✨ 项目特色
[![FOSSA Status](https://app.fossa.com/api/projects/git%2Bgithub.com%2FTeam-APE-RIP%2FAPE-HOI4-Tool-Studio.svg?type=shield)](https://app.fossa.com/projects/git%2Bgithub.com%2FTeam-APE-RIP%2FAPE-HOI4-Tool-Studio?ref=badge_shield)


> 不只是一个单体工具，而是一个可持续扩展的 **HOI4 桌面工具平台**。

<table>
  <tr>
    <td width="50%">
      <h3>🧩 模块化 Tool 系统</h3>
      <p>主程序、工具、插件解耦，支持独立加载、独立演进、独立交付，便于功能扩展与生态建设。</p>
    </td>
    <td width="50%">
      <h3>🔌 Plugin 扩展架构</h3>
      <p>支持插件依赖声明、授权访问链路与运行时校验，让复杂能力可以沉淀为可复用基础模块。</p>
    </td>
  </tr>
  <tr>
    <td width="50%">
      <h3>🖥️ 原生桌面体验</h3>
      <p>基于 C++ / Qt 构建，拥有桌面级 UI、配置管理、主题适配、多窗口组件与本地文件工作流。</p>
    </td>
    <td width="50%">
      <h3>🌐 在线能力整合</h3>
      <p>内置账号、更新、验证、广告位、远程服务接入等能力，兼顾本地生产力与在线协同基础。</p>
    </td>
  </tr>
  <tr>
    <td width="50%">
      <h3>🌍 三语本地化</h3>
      <p>项目内多处内容支持简体中文、繁體中文、English，多语言 UI 与资源结构清晰可扩展。</p>
    </td>
    <td width="50%">
      <h3>🛠️ 第三方生态友好</h3>
      <p>已提供第三方 Tool 规范思路，可围绕主程序能力继续构建更多 HOI4 创作工具与辅助模块。</p>
    </td>
  </tr>
</table>

---

## 🚀 为什么它很特别

<div align="center">

| 维度   | APE-HOI4-Tool-Studio    |
| ---- | ----------------------- |
| 核心定位 | HOI4 创作与处理的桌面工作室        |
| 技术路线 | C++17 + Qt 6 + 模块化动态库体系 |
| 工具系统 | Tool / Plugin 分层加载      |
| 扩展方式 | 内置工具 + 内置插件 + 第三方扩展     |
| 产品形态 | 原生桌面应用 + 在线服务支撑         |
| 目标风格 | 高集成、可扩展、面向生态            |

</div>

---

## 🔥 当前可见能力组成

### 内置 Tool

- `FileManagerTool`
- `FlagManagerTool`
- `LogManagerTool`

### 内置 Plugin

- `TagList`

### 核心系统能力

- 配置管理
- 本地化管理
- 文件索引与扫描
- 插件发现与依赖校验
- 工具加载与代理
- 更新 / 在线连接能力

---

## 🧠 项目设计关键词

```text
Modular / Desktop Native / Tool Ecosystem / Plugin Dependency /
HOI4 Tools / Qt UI / Update Service / Auth System / Localization
```

---

## 📦 仓库结构一览

```text
APE-HOI4-Tool-Studio/
├─ src/                 # 主程序源码
├─ tools/               # 内置工具
├─ plugins/             # 内置插件
├─ docs/                # 开发文档
├─ setup/               # 安装程序
├─ updater/             # 更新器
```

---

## 🌌 项目观感

<div align="center">
  <img src="https://img.shields.io/badge/HOI4-Tool%20Studio-111827?style=for-the-badge" alt="hoi4 tool studio" />
  <img src="https://img.shields.io/badge/Desktop-Native-0F172A?style=for-the-badge" alt="desktop native" />
  <img src="https://img.shields.io/badge/Plugin-Ecosystem-4C1D95?style=for-the-badge" alt="plugin ecosystem" />
  <img src="https://img.shields.io/badge/Future-Extensible-0F766E?style=for-the-badge" alt="future extensible" />
</div>

<p align="center">
  这个项目的重点不是“单个功能页面”，而是构建一个能够持续生长的 <b>HOI4 工具生态底座</b>。
</p>

---

## 🔗 官方入口

<div align="center">
<a href="https://apehts.czxieddan.top/">
  <img src="https://img.shields.io/badge/Official%20Website-apehts.czxieddan.top-0EA5E9?style=for-the-badge&logo=googlechrome&logoColor=white" alt="official website" />
</a>
</div>
<div align="center">
<a href="https://apehts.czxieddan.top/?view=ape-hoi4-tool-studio#ape-hoi4-tool-studio">
  <img src="https://img.shields.io/badge/Product%20Page-APE%20HOI4%20Tool%20Studio-8B5CF6?style=for-the-badge&logo=windows-terminal&logoColor=white" alt="product page" />
</a>
</div>

---

## 👥 Contributors

<div align="center">
  <a href="https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/graphs/contributors">
    <img src="https://contrib.rocks/image?repo=Team-APE-RIP/APE-HOI4-Tool-Studio" alt="contributors" />
  </a>
</div>

---

## ⭐ Star History

<div align="center">
  <a href="https://star-history.com/#Team-APE-RIP/APE-HOI4-Tool-Studio&Date">
    <img src="https://api.star-history.com/svg?repos=Team-APE-RIP/APE-HOI4-Tool-Studio&type=Date" alt="Star History Chart" />
  </a>
</div>