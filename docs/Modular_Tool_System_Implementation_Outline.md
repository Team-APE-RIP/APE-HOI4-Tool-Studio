# 模块化工具系统实施提纲 (Modular Tool System Implementation Outline)

## 1. 概述 (Overview)

本提纲详细描述了 APE HOI4 Tool Studio 的模块化工具系统架构。该系统旨在实现功能的解耦，允许开发者通过独立的插件形式扩展应用程序，同时保持主程序的轻量和稳定。

系统基于 Qt 的 `QPluginLoader` 机制，通过定义的 `ToolInterface` 接口实现主程序与插件的通信。

## 2. 核心架构 (Core Architecture)

### 2.1 核心库 (APEHOI4Core)
*   **角色**: 提供所有工具和主程序共用的基础服务。
*   **包含组件**:
    *   `ConfigManager`: 配置管理（单例），负责读写应用程序配置。
    *   `LocalizationManager`: 本地化管理（单例），处理多语言支持。
    *   `Logger`: 日志系统（单例），提供统一的日志记录功能。
    *   `FileManager`: 文件索引与扫描（单例），管理 MOD 文件结构。
    *   `TagManager`: 标签管理（单例），处理 HOI4 标签系统。
    *   `RecursiveFileSystemWatcher`: 递归文件监视器，监控文件系统变化。
    *   `PathValidator`: 路径验证工具，验证游戏和 MOD 路径有效性。
    *   `ToolInterface`: 插件接口定义。
    *   `ToolManager`: 工具管理器（单例），负责加载和管理插件。
*   **实现方式**: 编译为动态链接库 (`libAPEHOI4Core.dll` / `libAPEHOI4Core.so`)，主程序和所有工具插件都链接此库。
*   **版本定义**: 通过 CMake 的 `target_compile_definitions` 传递 `APP_VERSION` 宏。

### 2.2 插件接口 (ToolInterface)
*   **定义文件**: `src/ToolInterface.h`
*   **接口 ID**: `com.ape.hoi4toolstudio.ToolInterface`
*   **关键方法**:

| 方法 | 返回类型 | 说明 |
|------|----------|------|
| `id()` | `QString` | 唯一标识符，用于内部识别工具 |
| `name()` | `QString` | 显示名称（支持本地化） |
| `description()` | `QString` | 工具描述（支持本地化） |
| `version()` | `QString` | 工具版本号 |
| `compatibleVersion()` | `QString` | 兼容的主程序版本号 |
| `author()` | `QString` | 作者信息 |
| `setMetaData(const QJsonObject&)` | `void` | 注入元数据（从 metadata.json 读取） |
| `icon()` | `QIcon` | 工具图标 |
| `initialize()` | `void` | 初始化逻辑 |
| `createWidget(QWidget*)` | `QWidget*` | 创建工具的主 UI 组件 |
| `createSidebarWidget(QWidget*)` | `QWidget*` | 创建侧边栏组件（可选，默认返回 nullptr） |
| `loadLanguage(const QString&)` | `void` | 加载本地化字符串 |

### 2.3 工具管理器 (ToolManager)
*   **角色**: 负责扫描、加载和管理工具插件。
*   **实现文件**: `src/ToolManager.h`, `src/ToolManager.cpp`
*   **功能**:
    *   启动时扫描 `tools/` 目录（支持多级目录查找）。
    *   遍历子目录，查找并加载动态库 (`.dll`, `.so`, `.dylib`)。
    *   验证插件是否实现了 `ToolInterface`。
    *   执行版本兼容性检查（比较 `compatibleVersion` 与 `APP_VERSION`）。
    *   检测并跳过重复的工具 ID。
    *   维护已加载工具的列表 (`m_tools`) 和映射 (`m_toolMap`)。
    *   管理当前激活的工具状态 (`isToolActive()`, `setToolActive()`)。
*   **信号**: `toolsLoaded()` - 工具加载完成时发出。

## 3. 目录结构与资源隔离 (Directory Structure & Resource Isolation)

每个工具都应封装在 `tools/` 目录下的独立子目录中。

```
bin/
  APEHOI4ToolStudio.exe       <-- 主程序
  libAPEHOI4Core.dll          <-- 核心库
  tools/
    FileManagerTool/          <-- 工具根目录
      libFileManagerTool.dll  <-- 工具二进制
      metadata.json           <-- 元数据
      cover.png               <-- 封面图 (用于工具列表展示)
      localization/           <-- 独立本地化文件
        en_US.json
        zh_CN.json
        zh_TW.json
    FlagManagerTool/          <-- 另一个工具
      libFlagManagerTool.dll
      metadata.json
      localization/
        en_US.json
        zh_CN.json
        zh_TW.json
```

### 3.1 元数据文件 (metadata.json)
每个工具必须包含 `metadata.json` 文件，格式如下：
```json
{
    "id": "FileManagerTool",
    "version": "0.0.1",
    "compatibleVersion": "1.0.0",
    "author": "Team APE:RIP"
}
```

| 字段 | 说明 |
|------|------|
| `id` | 工具唯一标识符，必须与代码中 `id()` 返回值一致 |
| `version` | 工具版本号 |
| `compatibleVersion` | 兼容的主程序版本号 |
| `author` | 作者/团队名称 |

### 3.2 构建系统 (CMake)
每个工具在 `CMakeLists.txt` 中定义为一个 `SHARED` 库。

**完整示例**:
```cmake
# --- MyTool ---
set(MYTOOL_SOURCES
    tools/MyTool/MyTool.cpp
    tools/MyTool/MyTool.h
)

add_library(MyTool SHARED ${MYTOOL_SOURCES})
target_link_libraries(MyTool PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets APEHOI4Core)

# 设置输出目录
set_target_properties(MyTool PROPERTIES 
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools/MyTool"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools/MyTool"
)

# 复制 metadata.json
add_custom_command(TARGET MyTool POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/MyTool/metadata.json"
    "$<TARGET_FILE_DIR:MyTool>/metadata.json"
)

# 复制本地化目录
add_custom_command(TARGET MyTool POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/MyTool/localization"
    "$<TARGET_FILE_DIR:MyTool>/localization"
)

# 复制封面图（如果存在）
add_custom_command(TARGET MyTool POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/MyTool/cover.png"
    "$<TARGET_FILE_DIR:MyTool>/cover.png"
)
```

## 4. UI 集成 (UI Integration)

### 4.1 主窗口结构 (MainWindow)
*   **容器**: `MainWindow` 使用 `QStackedWidget` (`m_mainStack`) 管理视图。
    *   Index 0: Dashboard (工具运行区域，包含 `QSplitter` 支持侧边栏)
    *   Index 1: SettingsPage
    *   Index 2: ConfigPage
    *   Index 3: ToolsPage
*   **Dashboard 结构**:
    *   `m_dashboardSplitter`: 分割器
        *   `m_dashboardContent`: 主内容区域（工具 Widget）
        *   `m_rightSidebar`: 右侧边栏（工具侧边栏 Widget）

### 4.2 工具列表 (ToolsPage)
*   **展示**: 在主程序的 "Tools" 页面以卡片网格形式展示所有已加载的工具。
*   **交互**: 点击卡片触发 `toolSelected(const QString& toolId)` 信号。
*   **数据源**: 通过 `ToolManager::instance().getTools()` 获取工具列表。

### 4.3 工具激活流程
1. 用户在 `ToolsPage` 点击工具卡片。
2. `MainWindow` 接收 `toolSelected` 信号。
3. 检查是否有正在运行的工具（`ToolManager::isToolActive()`）。
4. 如有活动工具，弹出确认对话框。
5. 调用工具的 `createWidget()` 创建主 UI。
6. 调用工具的 `createSidebarWidget()` 创建侧边栏（如果有）。
7. 清空 Dashboard 并添加新工具的 Widget。
8. 切换到 Dashboard 视图。
9. 标记 `ToolManager` 状态为 Active。

### 4.4 关闭保护
*   当工具处于激活状态时，尝试关闭窗口或切换工具会触发确认对话框 (`CustomMessageBox`)，防止数据丢失。

## 5. 本地化策略 (Localization Strategy)

### 5.1 主程序本地化
*   使用 JSON 文件格式，位于 `resources/localization/<lang>/` 目录。
*   通过 `LocalizationManager` 单例管理。

### 5.2 工具插件本地化
*   工具内部维护自己的本地化文件（JSON 格式）。
*   文件位于工具目录的 `localization/` 子目录。
*   `ToolInterface::loadLanguage(lang)` 被主程序调用时，工具负责：
    1. 根据语言代码加载对应的 JSON 文件。
    2. 更新内部字符串缓存。
    3. 刷新 UI 显示（如果已创建）。

**本地化文件示例** (`localization/zh_CN.json`):
```json
{
    "name": "文件管理器",
    "description": "管理 MOD 文件结构"
}
```

## 6. 数据访问与安全性 (Data Access & Security)

### 6.1 数据访问
*   **只读访问**: 工具通过 `APEHOI4Core` 提供的单例读取全局配置和文件索引：
    *   `ConfigManager::instance()` - 配置信息
    *   `FileManager::instance()` - 文件索引
    *   `TagManager::instance()` - 标签数据
    *   `LocalizationManager::instance()` - 本地化字符串
*   **文件操作**: 建议工具使用 `FileManager` 或标准 Qt 文件类进行操作。

### 6.2 安全性考虑
*   **隔离性**: 工具运行在主进程中，因此没有进程级隔离。崩溃会影响主程序。
*   **缓解措施**:
    *   严格的代码审查
    *   防御性编程
    *   版本兼容性检查
    *   重复 ID 检测

## 7. 开发新工具流程 (Workflow for New Tools)

### 7.1 创建步骤
1. 在 `tools/` 下创建新目录（例如 `CharacterEditor`）。
2. 创建头文件和实现文件：
   ```cpp
   // CharacterEditor.h
   #ifndef CHARACTEREDITOR_H
   #define CHARACTEREDITOR_H
   
   #include <QObject>
   #include "../../src/ToolInterface.h"
   
   class CharacterEditor : public QObject, public ToolInterface {
       Q_OBJECT
       Q_PLUGIN_METADATA(IID "com.ape.hoi4toolstudio.ToolInterface" FILE "metadata.json")
       Q_INTERFACES(ToolInterface)
   
   public:
       QString id() const override { return m_id; }
       QString name() const override;
       QString description() const override;
       QString version() const override { return m_version; }
       QString compatibleVersion() const override { return m_compatibleVersion; }
       QString author() const override { return m_author; }
       
       void setMetaData(const QJsonObject& metaData) override;
       QIcon icon() const override;
       void initialize() override;
       QWidget* createWidget(QWidget* parent = nullptr) override;
       void loadLanguage(const QString& lang) override;
   
   private:
       QString m_id;
       QString m_version;
       QString m_compatibleVersion;
       QString m_author;
       // ... 其他成员
   };
   
   #endif
   ```
3. 创建 `metadata.json` 文件。
4. 创建 `localization/` 目录及语言文件。
5. 在 `CMakeLists.txt` 中添加新的库目标。
6. 实现 UI 和逻辑。
7. 编译并运行，系统将自动发现新工具。

### 7.2 检查清单
- [ ] 实现所有 `ToolInterface` 纯虚方法
- [ ] 创建 `metadata.json` 并填写正确的 `compatibleVersion`
- [ ] 创建本地化文件（至少 `en_US.json`）
- [ ] 在 CMakeLists.txt 中配置构建目标
- [ ] 测试工具加载和卸载
- [ ] 测试多语言切换

## 8. 现有实现状态 (Current Status)

### 8.1 核心系统
- [x] `ToolInterface` 定义完成
- [x] `ToolManager` 扫描与加载逻辑完成
- [x] `APEHOI4Core` 库分离完成
- [x] `MainWindow` UI 覆盖层与切换逻辑完成
- [x] CMake 构建配置已支持自动复制资源
- [x] 版本兼容性检查
- [x] 重复工具 ID 检测

### 8.2 已实现工具
| 工具 | 状态 | 说明 |
|------|------|------|
| FileManagerTool | ✅ 完成 | 文件管理器，作为参考实现 |
| FlagManagerTool | ✅ 完成 | 旗帜管理器 |

### 8.3 待实现功能
- [ ] 工具热重载支持
- [ ] 工具依赖管理
- [ ] 工具设置持久化
- [ ] 工具间通信机制
