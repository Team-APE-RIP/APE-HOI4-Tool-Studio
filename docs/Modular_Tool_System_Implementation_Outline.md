# 模块化工具系统实施提纲 (Modular Tool System Implementation Outline)

## 1. 文档目的

本文档用于定义 APE HOI4 Tool Studio 当前采用的模块化工具与插件体系标准，作为后续所有工具、插件、构建脚本与部署脚本的统一依据。

当前体系目标如下：

1. 将主程序基础能力、工具加载能力、插件加载能力拆分为独立动态库。
2. 允许工具声明前置插件依赖。
3. 允许插件以独立目录形式部署、热插拔式被主程序发现。
4. 缺失插件时不影响主程序启动，但会阻止依赖该插件的工具打开。
5. 插件原始代码与导出包装层严格分层，`main/` 目录只保留插件原始代码。
6. 发布时必须同步复制插件目录及其 `LICENSE` 文件。
7. 不得在可视化 UI 与日志中暴露敏感 API 接口信息。

---

## 2. 当前总体架构

当前桌面端采用以下八库拆分结构：

```text
APEHOI4ToolStudio.exe
├─ APEHOI4ToolStudio.logger.dll
├─ APEHOI4ToolStudio.config.dll
├─ APEHOI4ToolStudio.localization.dll
├─ APEHOI4ToolStudio.messageBox.dll
├─ APEHOI4ToolStudio.file.dll
├─ APEHOI4ToolStudio.core.dll
├─ APEHOI4ToolStudio.tool.dll
├─ APEHOI4ToolStudio.plugin.dll
├─ tools/
│  ├─ FileManagerTool/
│  ├─ FlagManagerTool/
│  └─ LogManagerTool/
└─ plugins/
   ├─ DirectXMath/
   ├─ DirectXTex/
   └─ TagList/
```

### 2.1 APEHTS.logger.dll

负责承载日志基础能力，主要包括：

- `Logger`

该库作为最底层基础库，供配置、文件、核心、工具、插件及主程序共享使用。

### 2.2 APEHTS.config.dll

负责承载本地配置基础能力，主要包括：

- `ConfigManager`

该库依赖 `APEHTS.logger.dll`，负责持久化主程序本地配置并向其他模块提供配置读取能力。

### 2.3 APEHTS.localization.dll

负责承载本地化基础能力，主要包括：

- `LocalizationManager`

该库负责读取资源系统中的三语 JSON 文本并向主程序、工具和 UI 组件提供文本访问能力。

### 2.4 APEHTS.messageBox.dll

负责承载通用消息框能力，主要包括：

- `CustomMessageBox`

该库依赖 `APEHTS.config.dll` 与 `APEHTS.localization.dll`，负责统一消息框主题与三语按钮文本。

### 2.5 APEHTS.file.dll

负责承载本地文件与路径监控能力，主要包括：

- `RecursiveFileSystemWatcher`
- `PathValidator`
- `FileManager`

该库依赖 `APEHTS.config.dll` 与 `APEHTS.logger.dll`，负责文件扫描、递归监控、路径验证以及文件索引共享能力。

### 2.6 APEHTS.core.dll

负责承载主程序联网与安全核心能力，主要包括：

- `AuthManager`
- `HttpClient`
- `SslConfig`

该库只保留联网、登录、传输、安全相关核心能力，不再承载本地配置、日志、本地化、消息框与文件扫描实现。

此外，桌面端联网基础地址配置统一由项目根目录 `baseurl.txt` 提供，并在构建阶段打包进 `APEHOI4ToolStudio.core.dll` 资源中。该文件内容只允许保存 host，例如：

```text
apehts.czxieddan.top
```

运行时由 core 层统一拼接 `http://` / `https://` 协议，不允许在桌面端源码中继续硬编码完整域名。

### 2.7 APEHTS.tool.dll

负责承载工具系统能力，主要包括：

- `ToolInterface`
- `ToolIpcProtocol`
- `ToolDescriptorParser`
- `ToolProxyInterface`
- `ToolManager`

该库负责工具发现、描述符解析、IPC 宿主代理与工具生命周期管理，并按需链接基础库与插件库。

### 2.8 APEHTS.plugin.dll

负责承载插件系统能力，主要包括：

- `PluginDescriptorParser`
- `PluginManager`

该库负责插件发现、插件描述符解析、插件元数据索引与工具依赖校验。

---

## 3. 工具与插件的职责边界

### 3.1 工具 (Tool)

工具是面向用户的功能模块，通常提供可视化界面，例如：

- FileManagerTool
- FlagManagerTool
- LogManagerTool

工具可以依赖插件，但不应内置第三方原始大库源码，也不应直接承担主程序基础文件加载职责。

### 3.2 插件 (Plugin)

插件是为工具提供底层能力或共享数据的模块，不直接出现在工具列表中，但会出现在配置页插件列表中。

典型插件：

- `DirectXMath`
- `DirectXTex`
- `TagList`

### 3.3 当前约束

1. 文件加载、路径验证、递归扫描等公共能力不得塞入单独工具库中。
2. `TagManager` 逻辑已迁移到 `TagList` 插件。
3. `DirectXMath` 与 `DirectXTex` 已从核心库拆分为独立插件。
4. 工具运行前应先校验插件依赖。
5. 工具缺依赖时必须拒绝打开，并给出卡片/消息提示。
6. 主程序本身不能因为插件缺失而崩溃或拒绝启动。
7. 工具运行时访问插件二进制或插件数据时，必须经过主程序授权校验。

---

## 4. 目录结构标准

## 4.1 工具目录结构

每个工具必须位于 `tools/<ToolName>/` 目录中，例如：

```text
tools/
  FlagManagerTool/
    FlagManagerTool.cpp
    FlagManagerTool.h
    descriptor.apehts
    localization/
      en_US.json
      zh_CN.json
      zh_TW.json
```

### 4.2 插件目录结构

每个插件必须位于 `plugins/<PluginName>/` 目录中，例如：

```text
plugins/
  DirectXTex/
    DirectXTexPluginExports.cpp
    descriptor.htsplugin
    LICENSE
    main/
      DirectXTexImage.cpp
      DirectXTexDDS.cpp
      ...
```

### 4.3 main 目录约束

`plugins/<PluginName>/main/` 目录只允许放置插件原始代码，不允许放置导出包装层、桥接层或项目自定义导出文件。

也就是说：

- `plugins/DirectXMath/main/` 只放 DirectXMath 原始代码
- `plugins/DirectXTex/main/` 只放 DirectXTex 原始代码
- `plugins/TagList/main/` 放 TagList 插件内部真实实现代码

以下文件必须位于插件根目录，而不是 `main/`：

- `plugins/DirectXMath/DirectXMathPluginExports.cpp`
- `plugins/DirectXTex/DirectXTexPluginExports.cpp`
- `plugins/TagList/TagListPluginExports.cpp`

---

## 5. 工具描述符标准

工具描述符文件名固定为：

```text
descriptor.apehts
```

当前不是 JSON 格式，而是自定义键值格式。

## 5.1 基本格式

示例：

```txt
name="FlagManagerTool"
version="3.0.0"
supported_version="2.0.*;2.1.*"
author="Team APE:RIP"
dependencies={
    "DirectXMath"
    "DirectXTex"
    "TagList"
}
```

## 5.2 支持字段

| 字段 | 必填 | 说明 |
|------|------|------|
| `name` | 是 | 工具唯一标识，同时作为工具 id |
| `version` | 是 | 工具版本 |
| `supported_version` | 是 | 兼容主程序版本 |
| `author` | 是 | 作者/组织 |
| `dependencies` | 否 | 插件依赖列表 |

## 5.3 dependencies 规则

- `dependencies` 使用块结构：
  ```txt
  dependencies={
      "PluginA"
      "PluginB"
  }
  ```
- 每行一个插件名称。
- 名称应与插件 `descriptor.htsplugin` 中的 `name` 一致。
- 若工具声明了依赖，但主程序未加载到对应插件，则工具不可打开。

---

## 6. 插件描述符标准

插件描述符文件名固定为：

```text
descriptor.htsplugin
```

同样采用自定义键值格式。

## 6.1 基本格式

示例：

```txt
id="20000002"
name="DirectXTex"
version="1.0.0"
supported_version="2.0.*;2.1.*"
author="Microsoft"
```

## 6.2 支持字段

| 字段 | 必填 | 说明 |
|------|------|------|
| `id` | 是 | 八位插件 id |
| `name` | 是 | 插件名称，供工具依赖引用 |
| `version` | 是 | 插件版本 |
| `supported_version` | 是 | 兼容主程序版本 |
| `author` | 是 | 作者/组织 |

## 6.3 当前预留插件 id

| 插件名 | id |
|--------|----|
| `DirectXMath` | `20000001` |
| `DirectXTex` | `20000002` |
| `TagList` | `20000003` |

---

## 7. 已实现插件

## 7.1 DirectXMath

- 目录：`plugins/DirectXMath/`
- 插件 id：`20000001`
- 用途：为图形相关处理提供 DirectXMath 能力
- 说明：原始 DirectXMath 代码位于 `plugins/DirectXMath/main/`
- 导出包装层位于：`plugins/DirectXMath/DirectXMathPluginExports.cpp`

## 7.2 DirectXTex

- 目录：`plugins/DirectXTex/`
- 插件 id：`20000002`
- 用途：为旗帜等图像处理提供 DDS 读取等能力
- 说明：原始 DirectXTex 代码位于 `plugins/DirectXTex/main/`
- 导出包装层位于：`plugins/DirectXTex/DirectXTexPluginExports.cpp`

### 当前导出约定

FlagManagerTool 当前通过运行时动态加载该插件导出函数，例如：

- `APE_DirectXTex_LoadDDSImage`
- `APE_DirectXTex_FreeImage`
- `APE_DirectXTex_GetLastError`

## 7.3 TagList

- 目录：`plugins/TagList/`
- 插件 id：`20000003`
- 用途：提供国家 TAG 列表数据
- 说明：`TagManager` 已迁移到该插件内部
- 插件根导出文件：`plugins/TagList/TagListPluginExports.cpp`
- 插件实现代码：`plugins/TagList/main/TagManager.cpp/.h`

### 当前导出约定

TagList 插件目前对外提供运行时导出接口，例如：

- `APE_TagList_GetTagsJson`
- `APE_TagList_GetTagCount`
- `APE_TagList_GetPluginName`

---

## 8. 工具依赖与运行时行为

## 8.1 插件扫描顺序

主程序文件扫描完成后，当前顺序为：

1. `PluginManager::loadPlugins()`
2. `ToolManager::loadTools()`

插件必须优先于工具加载，以便工具依赖校验生效。

## 8.2 工具打开前依赖检查

当用户从工具页面选择工具时：

1. 主程序获取该工具声明的依赖列表。
2. 使用 `PluginManager::getMissingDependencies()` 检查缺失项。
3. 如果有缺失插件：
   - 主程序弹出提示；
   - 拒绝创建工具界面；
   - 工具不启动。

## 8.3 主程序容错要求

- 插件目录为空时，主程序仍应正常运行。
- 某个插件损坏或缺失时，主程序仍应正常运行。
- 只有真正依赖该插件的工具会被限制打开。

---

## 9. 配置页展示规则

配置页必须展示当前已加载插件列表。

当前展示规则如下：

- 按插件 `id` 升序固定排序
- 标题仅显示插件名称
- 副标题显示插件作者
- 右侧信息区显示插件 `ID` 与版本号
- 当插件仅有元数据但未找到动态库时，右侧信息区显示元数据状态

配置页插件组的内外边距应与普通配置项视觉风格保持一致，不得出现内容直接贴边框的情况。

当无已加载插件时，应给出空状态提示。

---

## 10. IPC 与数据访问调整

## 10.1 当前 IPC 数据范围

当前工具宿主 IPC 只保留以下数据同步：

- `GetConfig` / `ConfigResponse`
- `GetFileIndex` / `FileIndexResponse`

## 10.2 已移除的旧链路

以下旧链路已从主程序侧移除：

- `GetTags`
- `TagsResponse`
- 主程序侧 `TagManager` 直连
- `ToolHostMode` 中对 `TagManager` 的依赖
- `ToolProxyInterface` 中对 `TagManager` 的依赖
- `MainWindow` 启动时对 `TagManager` 的初始化

### 原因

TAG 数据现在由 `TagList` 插件在运行时提供，工具应改为通过主程序授权后的插件访问链路获取，而不是继续通过主程序 IPC 获取或自行扫描插件目录。

## 10.3 新增插件授权访问链路

为防止工具绕过 `dependencies` 直接访问任意插件，当前工具系统新增以下运行时约束：

- 工具不得自行扫描 `plugins/` 目录查找插件动态库；
- 工具不得自行依据目录结构推断插件路径；
- 工具若需要访问插件动态库，必须通过主程序 IPC 请求授权路径；
- 主程序只会向当前工具返回其 `dependencies` 中声明过的插件动态库路径；
- 若插件未声明在 `dependencies` 中，则主程序必须拒绝返回插件路径或相关插件数据。

当前新增 IPC 消息如下：

- `GetPluginBinaryPath`
- `PluginBinaryPathResponse`

其行为规则如下：

1. tool host 向主程序发送 `GetPluginBinaryPath`，请求字段为 `pluginName`
2. 主程序代理层检查该 `pluginName` 是否存在于当前工具 `dependencies`
3. 若未声明依赖：
   - 返回失败；
   - 不暴露插件路径；
   - 记录抽象错误日志
4. 若已声明依赖但插件未加载或无有效动态库：
   - 返回失败；
   - 工具可自行决定降级行为
5. 若已声明且插件可用：
   - 返回授权后的 `libraryPath`

当前工具侧统一通过 `ToolRuntimeContext` 获取该授权路径，不应再在工具代码中保留私有插件目录扫描逻辑。

---


## 11. FlagManagerTool 的当前依赖规范

`FlagManagerTool` 当前必须声明以下插件依赖：

```txt
dependencies={
    "DirectXMath"
    "DirectXTex"
    "TagList"
}
```

其运行时行为为：

- 通过主程序授权后的 `TagList` 插件路径读取国家 TAG 列表；
- 通过主程序授权后的 `DirectXTex` 插件路径读取 DDS 图像；
- 不再直接依赖主程序侧 `TagManager`；
- 不再直接在工具代码中扫描 `plugins/` 目录或静态推断插件路径。

---

## 12. CMake 构建规范

## 12.1 当前核心目标

当前 CMake 目标包括：

- `APEHTSLogger`
- `APEHTSConfig`
- `APEHTSLocalization`
- `APEHTSMessageBox`
- `APEHTSFile`
- `APEHTSCore`
- `APEHTSPlugin`
- `APEHTSTool`
- `APEHOI4ToolStudio`
- `Updater`
- `SetupApp`
- `FileManagerTool`
- `FlagManagerTool`
- `LogManagerTool`
- `DirectXMath`
- `DirectXTex`
- `TagList`

## 12.2 输出命名

当前产物命名要求如下：

| 目标 | 输出名 |
|------|--------|
| `APEHTSLogger` | `APEHOI4ToolStudio.logger.dll` |
| `APEHTSConfig` | `APEHOI4ToolStudio.config.dll` |
| `APEHTSLocalization` | `APEHOI4ToolStudio.localization.dll` |
| `APEHTSMessageBox` | `APEHOI4ToolStudio.messageBox.dll` |
| `APEHTSFile` | `APEHOI4ToolStudio.file.dll` |
| `APEHTSCore` | `APEHOI4ToolStudio.core.dll` |
| `APEHTSPlugin` | `APEHOI4ToolStudio.plugin.dll` |
| `APEHTSTool` | `APEHOI4ToolStudio.tool.dll` |

## 12.3 版本资源与版权信息

所有主程序、工具和插件二进制都应通过统一版本资源模板写入：

- 公司名
- 产品名
- 版权信息
- 文件描述
- 文件版本
- 产品版本

当前模板文件为：

```text
resources/versioninfo.rc.in
```

当前版本资源规则如下：

1. 语言固定使用语言中性：
   - `BLOCK "000004b0"`
   - `Translation 0x0000, 1200`
2. 统一版权文本固定为：
   - `Copyright © 2026 Team APE:RIP`
3. 主程序 / Updater / Setup：
   - `ProductVersion` 字符串保留原文，例如 `2.1.alpha-0`
   - 数值版本按映射规则生成：
     - `stable -> 1`
     - `beta -> 2`
     - `alpha -> 3`
     - 例如 `2.1.alpha-0 -> 2,1,3,0`
4. 基础库与系统库：
   - 当前包含：
     - `APEHOI4ToolStudio.logger.dll`
     - `APEHOI4ToolStudio.config.dll`
     - `APEHOI4ToolStudio.localization.dll`
     - `APEHOI4ToolStudio.messageBox.dll`
     - `APEHOI4ToolStudio.file.dll`
     - `APEHOI4ToolStudio.core.dll`
     - `APEHOI4ToolStudio.plugin.dll`
     - `APEHOI4ToolStudio.tool.dll`
   - 前三位来自 `resources/library_versions.json`
   - 第四位来自对应 DLL 实际内容 SHA256 前缀计算结果
   - 当前通过构建后更新修订号并二次配置实现
5. 工具和插件自身版本：
   - 来自各自 `descriptor.apehts` / `descriptor.htsplugin`
   - 不跟随主程序版本

---

## 13. 部署规范

## 13.1 deploy_main.bat

`deploy_main.bat` 必须：

1. 首轮构建主程序、基础库、系统库、工具、插件；
2. 基于首轮构建出的以下 DLL：
   - `APEHOI4ToolStudio.logger.dll`
   - `APEHOI4ToolStudio.config.dll`
   - `APEHOI4ToolStudio.localization.dll`
   - `APEHOI4ToolStudio.messageBox.dll`
   - `APEHOI4ToolStudio.file.dll`
   - `APEHOI4ToolStudio.core.dll`
   - `APEHOI4ToolStudio.tool.dll`
   - `APEHOI4ToolStudio.plugin.dll`
   计算实际 DLL 内容哈希修订号；
3. 回写上述各库修订号并重新执行 CMake 配置；
4. 二次重编译上述各库与主程序，使版本资源写入最终哈希第四位；
5. 将以下文件复制到 `bin/main/`：
   - `APEHOI4ToolStudio.exe`
   - `Updater.exe`
   - `APEHOI4ToolStudio.logger.dll`
   - `APEHOI4ToolStudio.config.dll`
   - `APEHOI4ToolStudio.localization.dll`
   - `APEHOI4ToolStudio.messageBox.dll`
   - `APEHOI4ToolStudio.file.dll`
   - `APEHOI4ToolStudio.core.dll`
   - `APEHOI4ToolStudio.tool.dll`
   - `APEHOI4ToolStudio.plugin.dll`
6. 复制 `build/main/tools/` 到 `bin/main/tools/`
7. 复制 `build/main/plugins/` 到 `bin/main/plugins/`
8. 保证插件 `LICENSE` 文件随插件目录一起进入最终运行目录
9. 更新器清理旧文件时，必须保护不在官方 manifest 中的第三方 `tools/` 与第三方 `plugins/` 目录，不得误删用户自行安装的扩展

## 13.2 deploy.bat

`deploy.bat` 必须在完整打包流程中遵守与 `deploy_main.bat` 相同的主程序部署规则，并确保：

- `payload.7z` 中包含 `plugins/`
- `payload.7z` 中包含插件 `LICENSE`
- 安装包中的主程序目录结构与运行时结构一致

---

## 14. 安全要求

1. 不得把敏感 API 接口直接暴露到可视化 UI。
2. 不得把敏感 API 地址、密钥、完整接口路径直接打印到日志。
3. 若必须记录网络错误，只记录脱敏后的必要信息。
4. 插件或工具出现异常时，优先输出抽象错误信息，而不是泄露内部接口细节。

---

## 15. 新增工具流程

新增工具时必须遵守以下顺序：

1. 在 `tools/<ToolName>/` 创建工具目录。
2. 实现 `ToolInterface`。
3. 添加三语本地化文件：
   - `en_US.json`
   - `zh_CN.json`
   - `zh_TW.json`
4. 创建 `descriptor.apehts`
5. 若依赖插件，补充 `dependencies={...}`
6. 在 `CMakeLists.txt` 中注册新的 SHARED 目标
7. 确认工具 DLL 具备版本资源信息
8. 确认不把公共文件加载能力错误塞入工具库

---

## 16. 新增插件流程

新增插件时必须遵守以下顺序：

1. 在 `plugins/<PluginName>/` 创建插件目录。
2. 在插件根目录创建：
   - 导出包装层源码
   - `descriptor.htsplugin`
   - `LICENSE`
3. 在 `plugins/<PluginName>/main/` 放置插件原始实现代码。
4. 若插件被其他工具依赖，则依赖名必须使用 `descriptor.htsplugin` 中的 `name`。
5. 在 `CMakeLists.txt` 中注册插件 SHARED 目标。
6. 确认插件 DLL 具备版本资源信息。
7. 确认部署脚本会复制该插件目录与 `LICENSE`。

---

## 17. 当前状态结论

截至当前版本，模块化工具系统已具备以下能力：

- 八库拆分：
  - `APEHOI4ToolStudio.logger.dll`
  - `APEHOI4ToolStudio.config.dll`
  - `APEHOI4ToolStudio.localization.dll`
  - `APEHOI4ToolStudio.messageBox.dll`
  - `APEHOI4ToolStudio.file.dll`
  - `APEHOI4ToolStudio.core.dll`
  - `APEHOI4ToolStudio.tool.dll`
  - `APEHOI4ToolStudio.plugin.dll`
- 工具描述符支持 `dependencies`
- 插件描述符与插件扫描器已建立
- 配置页具备插件列表展示与显示时刷新机制
- 工具打开前会执行依赖校验
- 工具运行时访问插件路径时会再次基于 `dependencies` 做授权校验
- `DirectXMath` / `DirectXTex` 已拆为独立插件，并已从 `libs/` 目录移除
- `TagManager` 已迁移至 `TagList` 插件
- `RecursiveFileSystemWatcher` / `PathValidator` / `FileManager` 已统一归入 `APEHOI4ToolStudio.file.dll`
- 旧 `GetTags` / `TagsResponse` IPC 主程序链路已移除
- 部署脚本已要求复制基础库、`plugins/` 与插件 `LICENSE`
- `baseurl.txt` 已改为 host-only 配置，并构建进 `APEHOI4ToolStudio.core.dll` 资源
- 更新器清理逻辑已同时保护第三方 `tools/` 与第三方 `plugins/`
- 各基础库与系统库版本第四位已改为基于 DLL 实际内容哈希生成
- 主程序 `ProductVersion` 已保留原始版本文本

后续新增或修改工具/插件时，必须以本文档为准同步维护实现与部署链路。