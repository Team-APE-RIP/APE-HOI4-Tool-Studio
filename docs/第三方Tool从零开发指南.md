# APE-HOI4-Tool-Studio 第三方 Tool 从零开发指南

## 1. 文档目的

本文档是一份面向第三方开发者的完整教程，目标是教你在**没有主程序源码参与开发流程**的前提下，从零制作一个可以被 **APE-HOI4-Tool-Studio** 识别、加载并运行的第三方 Tool。

本文档以：

- **Windows + Qt 6 + CMake**
- **桌面端 Tool 动态库形式交付**

为标准进行说明。

你读完并照做后，应该能够独立产出以下内容：

- 一个可加载的 Tool DLL
- 一份合法的 `descriptor.apehts`
- 一组三语本地化文件
- 一个可选的封面图 `cover.png`
- 一个可以直接放入主程序 `tools/` 目录中的最终交付目录

---

## 2. 本教程最终交付结果

一个第三方 Tool 的最终交付目录结构应当如下：

```text
tools/
  MyFirstTool/
    libMyFirstTool.dll
    descriptor.apehts
    cover.png
    localization/
      en_US.json
      zh_CN.json
      zh_TW.json
```

其中：

- `libMyFirstTool.dll` 是你的工具动态库
- `descriptor.apehts` 是工具元数据描述文件
- `cover.png` 是工具卡片图标，可选但强烈建议提供
- `localization/` 存放三语本地化文件，必须提供

> 注意：目录名、DLL 名、工具内部类名可以不完全相同，但**强烈建议统一命名**，避免后续排查问题时混乱。

---

## 3. 主程序如何识别一个第三方 Tool

主程序对 Tool 的识别规则可以概括为以下几条：

### 3.1 扫描位置

主程序会在运行目录下查找：

```text
tools/
```

然后继续扫描其下的每一个子目录，每个子目录都被视为一个候选 Tool。

例如：

```text
tools/
  MyFirstTool/
  MySecondTool/
```

### 3.2 每个 Tool 子目录必须包含描述文件

每个 Tool 子目录中，必须存在：

```text
descriptor.apehts
```

没有这个文件，主程序不会把该目录识别为有效 Tool。

### 3.3 每个 Tool 子目录中必须有可加载的动态库

主程序会在 Tool 子目录中寻找：

- `*.dll`
- `*.so`
- `*.dylib`

在 Windows 环境下，实际就是 `*.dll`。

### 3.4 一个 Tool 目录里不要放多个 DLL

虽然扫描器会枚举 DLL，但实际加载逻辑容易受到“目录下第一个 DLL”的影响。为了避免错误加载，请遵守下面的交付规范：

- **一个 Tool 子目录中只放一个真正的 Tool DLL**
- 不要把测试 DLL、辅助 DLL、旧版本 DLL 混放在同一个 Tool 目录下

推荐结构：

```text
tools/
  MyFirstTool/
    libMyFirstTool.dll
    descriptor.apehts
    localization/
```

不推荐结构：

```text
tools/
  MyFirstTool/
    libMyFirstTool.dll
    libMyFirstTool_old.dll
    helper.dll
    descriptor.apehts
```

---

## 4. 你必须遵守的 Tool 接口契约

APE-HOI4-Tool-Studio 通过 Qt 插件机制加载 Tool。你的动态库必须导出一个实现了 `ToolInterface` 的 Qt 插件对象。

Tool 接口的核心定义如下。

## 4.1 最小接口定义

下面这份代码是第三方开发时建议自备的最小接口头文件内容。你可以把它保存为：

```text
sdk/ToolInterface.h
```

代码如下：

```cpp
#ifndef TOOLINTERFACE_H
#define TOOLINTERFACE_H

#include <QtPlugin>
#include <QWidget>
#include <QString>
#include <QIcon>
#include <QJsonObject>
#include <QStringList>

class ToolInterface {
public:
    virtual ~ToolInterface() = default;

    virtual QString id() const = 0;
    virtual QString name() const = 0;
    virtual QString description() const = 0;
    virtual QString version() const = 0;
    virtual QString compatibleVersion() const = 0;
    virtual QString author() const = 0;
    virtual QStringList dependencies() const { return {}; }

    virtual void setMetaData(const QJsonObject& metaData) = 0;

    virtual QIcon icon() const = 0;

    virtual void initialize() = 0;

    virtual QWidget* createWidget(QWidget* parent = nullptr) = 0;
    virtual QWidget* createSidebarWidget(QWidget* parent = nullptr) { return nullptr; }

    virtual void loadLanguage(const QString& lang) = 0;

    virtual void applyTheme() {}
};

#define ToolInterface_iid "com.ape.hoi4toolstudio.ToolInterface"
Q_DECLARE_INTERFACE(ToolInterface, ToolInterface_iid)

#endif // TOOLINTERFACE_H
```

---

## 5. 每个接口函数的职责

你必须理解每个函数是干什么的，否则写出来的 Tool 很容易“能加载但不能正常工作”。

## 5.1 `id()`

返回工具唯一标识。

在当前系统中，这个值通常来自描述文件中的 `name` 字段，并由主程序通过 `setMetaData()` 注入。

**建议做法：**
不要把它写死为常量唯一来源，而是把它保存到成员变量中，由 `setMetaData()` 赋值。

---

## 5.2 `name()`

返回工具显示名称。

这个值通常应该来自当前语言的本地化内容，而不是直接写死在代码里。

---

## 5.3 `description()`

返回工具描述文本。

这个值会用于工具列表、卡片或相关 UI 展示，也建议从本地化文件读取。

---

## 5.4 `version()`

返回工具自身版本号。

通常来自 `descriptor.apehts` 的 `version` 字段。

---

## 5.5 `compatibleVersion()`

返回该 Tool 兼容的主程序版本要求。

通常来自 `descriptor.apehts` 的 `supported_version` 字段。

---

## 5.6 `author()`

返回作者或组织名。

通常来自 `descriptor.apehts` 的 `author` 字段。

---

## 5.7 `dependencies()`

返回该 Tool 声明依赖的插件列表。

如果你的工具不依赖任何插件，可以返回空列表：

```cpp
QStringList dependencies() const override { return {}; }
```

如果依赖插件，应该返回主程序注入后的依赖列表。

---

## 5.8 `setMetaData(const QJsonObject& metaData)`

这是**必须实现**的函数。

主程序会把 `descriptor.apehts` 解析成元数据对象，然后调用它，把数据注入到 Tool 中。

你应该在这个函数里提取并保存：

- id
- version
- compatibleVersion
- author
- dependencies

例如：

```cpp
void setMetaData(const QJsonObject& metaData) override {
    m_id = metaData.value("id").toString();
    m_version = metaData.value("version").toString();
    m_compatibleVersion = metaData.value("compatibleVersion").toString();
    m_author = metaData.value("author").toString();

    m_dependencies.clear();
    const QJsonArray deps = metaData.value("dependencies").toArray();
    for (const QJsonValue& value : deps) {
        const QString dep = value.toString().trimmed();
        if (!dep.isEmpty()) {
            m_dependencies.append(dep);
        }
    }
}
```

---

## 5.9 `icon()`

返回工具图标。

通常建议读取当前 Tool 目录下的 `cover.png`。

如果找不到文件，则返回默认图标。

---

## 5.10 `initialize()`

用于初始化 Tool 内部状态。

建议做以下事情：

- 加载默认语言
- 初始化缓存
- 准备内部数据结构

不要在这里做重型阻塞操作，尤其不要在这里直接执行长时间 I/O 扫描。

---

## 5.11 `createWidget(QWidget* parent)`

返回 Tool 的主界面 QWidget。

这是最核心的 UI 函数，主程序会把它嵌入到主区域中。

---

## 5.12 `createSidebarWidget(QWidget* parent)`

返回 Tool 的右侧边栏 QWidget。

这个函数不是必须有内容。如果你的工具不需要右侧边栏，直接返回 `nullptr` 即可。

---

## 5.13 `loadLanguage(const QString& lang)`

切换当前语言并刷新 Tool 文本。

主程序会在打开工具后调用这个函数，因此你的 Tool 需要能根据传入值加载对应本地化文件。

---

## 5.14 `applyTheme()`

用于响应主题切换。

如果你的 Tool 有深浅色风格切换需求，可以在这里刷新样式表。没有需求时可以保持空实现。

---

## 6. 第三方开发时建议自备的最小 SDK

如果你没有主程序源码，最少要自备两份接口声明：

- `ToolInterface.h`
- `ToolRuntimeContext.h`

其中 `ToolRuntimeContext` 是插件授权访问链路的入口。

## 6.1 `ToolRuntimeContext.h`

建议自备如下最小头文件：

```cpp
#ifndef TOOLRUNTIMECONTEXT_H
#define TOOLRUNTIMECONTEXT_H

#include <QString>
#include <functional>

class ToolRuntimeContext {
public:
    using PluginBinaryPathResolver = std::function<bool(const QString&, QString*, QString*)>;

    static ToolRuntimeContext& instance();

    void setPluginBinaryPathResolver(PluginBinaryPathResolver resolver);
    bool requestAuthorizedPluginBinaryPath(const QString& pluginName, QString* outPath, QString* errorMessage = nullptr) const;

private:
    ToolRuntimeContext() = default;
    PluginBinaryPathResolver m_pluginBinaryPathResolver;
};

#endif // TOOLRUNTIMECONTEXT_H
```

## 6.2 `ToolRuntimeContext.cpp`

如果你是独立第三方工程，并且需要本地编译通过，可以提供如下实现：

```cpp
#include "ToolRuntimeContext.h"

ToolRuntimeContext& ToolRuntimeContext::instance() {
    static ToolRuntimeContext ctx;
    return ctx;
}

void ToolRuntimeContext::setPluginBinaryPathResolver(PluginBinaryPathResolver resolver) {
    m_pluginBinaryPathResolver = std::move(resolver);
}

bool ToolRuntimeContext::requestAuthorizedPluginBinaryPath(const QString& pluginName, QString* outPath, QString* errorMessage) const {
    if (!m_pluginBinaryPathResolver) {
        if (errorMessage) {
            *errorMessage = "Plugin path resolver is not available.";
        }
        return false;
    }
    return m_pluginBinaryPathResolver(pluginName, outPath, errorMessage);
}
```

> 注意：在主程序真实运行时，这个解析器会由主程序注入。  
> 你作为第三方作者的任务不是自己设置 resolver，而是**调用** `requestAuthorizedPluginBinaryPath()`。

---

## 7. 目录结构规范

推荐按下面的工程结构开发你的第三方 Tool：

```text
MyFirstToolProject/
  CMakeLists.txt
  sdk/
    ToolInterface.h
    ToolRuntimeContext.h
    ToolRuntimeContext.cpp
  src/
    MyFirstTool.h
    MyFirstTool.cpp
  package/
    descriptor.apehts
    cover.png
    localization/
      en_US.json
      zh_CN.json
      zh_TW.json
```

构建完成后，再把产物整理成最终交付结构：

```text
MyFirstTool/
  libMyFirstTool.dll
  descriptor.apehts
  cover.png
  localization/
    en_US.json
    zh_CN.json
    zh_TW.json
```

---

## 8. `descriptor.apehts` 规范

Tool 描述文件文件名必须固定为：

```text
descriptor.apehts
```

当前格式不是 JSON，而是自定义键值文本格式。

## 8.1 最小示例

```txt
name="MyFirstTool"
version="1.0.0"
supported_version="2.0.*;2.1.*"
author="Your Name"
```

## 8.2 带插件依赖的示例

```txt
name="MyFirstTool"
version="1.0.0"
supported_version="2.0.*;2.1.*"
author="Your Name"
dependencies={
    "TagList"
}
```

## 8.3 字段说明

| 字段 | 是否必填 | 说明 |
|------|----------|------|
| `name` | 是 | 工具唯一标识，同时会作为工具 id |
| `version` | 是 | 工具自身版本 |
| `supported_version` | 是 | 主程序兼容版本要求 |
| `author` | 是 | 作者或组织名 |
| `dependencies` | 否 | 依赖的插件列表 |

## 8.4 `supported_version` 的写法

支持多个模式，用 `;` 分隔，例如：

```txt
supported_version="2.0.*;2.1.*"
```

含义是：

- 兼容 `2.0.x`
- 兼容 `2.1.x`

建议第三方工具至少声明与你实际测试过的主程序版本范围一致，不要随意写得过宽。

## 8.5 `dependencies` 的写法规则

必须写成块结构：

```txt
dependencies={
    "PluginA"
    "PluginB"
}
```

规则如下：

- 每行一个插件名
- 名称必须与插件 `descriptor.htsplugin` 里的 `name` 一致
- 名称区分大小写处理时应保持一致
- 如果声明了依赖，但主程序未加载到对应插件，Tool 将无法打开

---

## 9. 本地化文件规范

每个第三方 Tool 都必须提供三语本地化文件：

```text
localization/
  en_US.json
  zh_CN.json
  zh_TW.json
```

## 9.1 最小本地化文件要求

每个语言文件至少必须包含：

- `Name`
- `Description`

例如：

### `zh_CN.json`

```json
{
    "Name": "我的第一个工具",
    "Description": "一个用于演示第三方 Tool 接入流程的示例工具",
    "HelloTitle": "欢迎",
    "HelloMessage": "第三方 Tool 已成功加载。"
}
```

### `zh_TW.json`

```json
{
    "Name": "我的第一個工具",
    "Description": "一個用於示範第三方 Tool 接入流程的範例工具",
    "HelloTitle": "歡迎",
    "HelloMessage": "第三方 Tool 已成功載入。"
}
```

### `en_US.json`

```json
{
    "Name": "My First Tool",
    "Description": "A sample tool for demonstrating third-party tool integration",
    "HelloTitle": "Welcome",
    "HelloMessage": "The third-party tool has been loaded successfully."
}
```

## 9.2 为什么 `Name` 和 `Description` 很重要

主程序在预加载 Tool 信息时，会优先从本地化文件中读取：

- `Name`
- `Description`

用于工具卡片、工具列表等 UI 展示。

如果你缺少这两个字段，工具虽然可能仍能加载，但展示层会不完整或退化。

## 9.3 语言回退建议

建议你的 Tool 实现以下逻辑：

- 优先读取当前语言文件
- 如果不存在，则回退到 `en_US.json`

这样可以在个别语言文件缺失时保持可用。

---

## 10. 封面图 `cover.png`

这是可选文件，但建议提供。

主程序会尝试读取 Tool 目录下的：

```text
cover.png
```

并将其作为 Tool 图标显示。

建议：

- 使用 PNG
- 方形图，如 256x256
- 图像简洁清晰

---

## 11. 最小可运行示例：完整代码

下面给出一套最小可运行示例。你可以直接改名后作为自己的第一个第三方 Tool 模板。

## 11.1 `src/MyFirstTool.h`

```cpp
#ifndef MYFIRSTTOOL_H
#define MYFIRSTTOOL_H

#include <QObject>
#include <QWidget>
#include <QMap>
#include <QJsonObject>
#include <QIcon>
#include <QStringList>

#include "../sdk/ToolInterface.h"

class QLabel;
class QPushButton;

class MyFirstTool : public QObject, public ToolInterface {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "com.ape.hoi4toolstudio.ToolInterface")
    Q_INTERFACES(ToolInterface)

public:
    QString id() const override { return m_id; }
    QString name() const override;
    QString description() const override;
    QString version() const override { return m_version; }
    QString compatibleVersion() const override { return m_compatibleVersion; }
    QString author() const override { return m_author; }
    QStringList dependencies() const override { return m_dependencies; }

    void setMetaData(const QJsonObject& metaData) override;

    QIcon icon() const override;

    void initialize() override;
    QWidget* createWidget(QWidget* parent = nullptr) override;
    QWidget* createSidebarWidget(QWidget* parent = nullptr) override;
    void loadLanguage(const QString& lang) override;
    void applyTheme() override;

private:
    QString toolDirectoryPath() const;
    QString languageNameToCode(const QString& lang) const;
    QString getString(const QString& key) const;
    void loadLocalizationFile(const QString& languageCode);

    QString m_id;
    QString m_version;
    QString m_compatibleVersion;
    QString m_author;
    QStringList m_dependencies;

    QString m_currentLangCode = "en_US";
    QJsonObject m_strings;

    QWidget* m_mainWidget = nullptr;
    QWidget* m_sidebarWidget = nullptr;
    QLabel* m_titleLabel = nullptr;
    QLabel* m_descLabel = nullptr;
    QLabel* m_sidebarLabel = nullptr;
};

#endif // MYFIRSTTOOL_H
```

## 11.2 `src/MyFirstTool.cpp`

```cpp
#include "MyFirstTool.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

QString MyFirstTool::name() const {
    return getString("Name");
}

QString MyFirstTool::description() const {
    return getString("Description");
}

void MyFirstTool::setMetaData(const QJsonObject& metaData) {
    m_id = metaData.value("id").toString();
    m_version = metaData.value("version").toString();
    m_compatibleVersion = metaData.value("compatibleVersion").toString();
    m_author = metaData.value("author").toString();

    m_dependencies.clear();
    const QJsonArray deps = metaData.value("dependencies").toArray();
    for (const QJsonValue& value : deps) {
        const QString dep = value.toString().trimmed();
        if (!dep.isEmpty()) {
            m_dependencies.append(dep);
        }
    }
}

QIcon MyFirstTool::icon() const {
    const QString coverPath = toolDirectoryPath() + "/cover.png";
    if (QFile::exists(coverPath)) {
        return QIcon(coverPath);
    }
    return QIcon::fromTheme("applications-system");
}

void MyFirstTool::initialize() {
    loadLanguage("English");
}

QWidget* MyFirstTool::createWidget(QWidget* parent) {
    if (m_mainWidget) {
        delete m_mainWidget;
        m_mainWidget = nullptr;
    }

    QWidget* root = new QWidget(parent);
    QVBoxLayout* layout = new QVBoxLayout(root);

    m_titleLabel = new QLabel(root);
    m_descLabel = new QLabel(root);
    QPushButton* button = new QPushButton(root);

    m_titleLabel->setStyleSheet("font-size: 24px; font-weight: 700;");
    m_descLabel->setWordWrap(true);

    layout->addWidget(m_titleLabel);
    layout->addWidget(m_descLabel);
    layout->addSpacing(12);
    layout->addWidget(button);
    layout->addStretch();

    QObject::connect(button, &QPushButton::clicked, root, [this, root]() {
        QMessageBox::information(root, getString("HelloTitle"), getString("HelloMessage"));
    });

    m_mainWidget = root;
    loadLanguage(m_currentLangCode);
    return m_mainWidget;
}

QWidget* MyFirstTool::createSidebarWidget(QWidget* parent) {
    if (m_sidebarWidget) {
        delete m_sidebarWidget;
        m_sidebarWidget = nullptr;
    }

    QWidget* sidebar = new QWidget(parent);
    QVBoxLayout* layout = new QVBoxLayout(sidebar);

    m_sidebarLabel = new QLabel(sidebar);
    m_sidebarLabel->setWordWrap(true);

    layout->addWidget(m_sidebarLabel);
    layout->addStretch();

    m_sidebarWidget = sidebar;
    loadLanguage(m_currentLangCode);
    return m_sidebarWidget;
}

void MyFirstTool::loadLanguage(const QString& lang) {
    const QString langCode = languageNameToCode(lang);
    m_currentLangCode = langCode;
    loadLocalizationFile(langCode);

    if (m_titleLabel) {
        m_titleLabel->setText(getString("Name"));
    }
    if (m_descLabel) {
        m_descLabel->setText(getString("Description"));
    }
    if (m_sidebarLabel) {
        m_sidebarLabel->setText(getString("SidebarMessage"));
    }

    if (m_mainWidget) {
        const QList<QPushButton*> buttons = m_mainWidget->findChildren<QPushButton*>();
        for (QPushButton* button : buttons) {
            button->setText(getString("HelloButton"));
        }
    }
}

void MyFirstTool::applyTheme() {
    if (m_mainWidget) {
        m_mainWidget->setStyleSheet(
            "QWidget { background-color: transparent; }"
            "QLabel { color: palette(window-text); }"
            "QPushButton { padding: 8px 14px; }"
        );
    }

    if (m_sidebarWidget) {
        m_sidebarWidget->setStyleSheet(
            "QWidget { background-color: transparent; }"
            "QLabel { color: palette(window-text); }"
        );
    }
}

QString MyFirstTool::toolDirectoryPath() const {
    const QString appDir = QCoreApplication::applicationDirPath();

    QDir dir(appDir);
    if (dir.exists("tools/" + m_id)) {
        return dir.filePath("tools/" + m_id);
    }

    if (dir.cdUp() && dir.exists("tools/" + m_id)) {
        return dir.filePath("tools/" + m_id);
    }

    return appDir;
}

QString MyFirstTool::languageNameToCode(const QString& lang) const {
    if (lang == "简体中文" || lang == "zh_CN") {
        return "zh_CN";
    }
    if (lang == "繁體中文" || lang == "zh_TW") {
        return "zh_TW";
    }
    if (lang == "English" || lang == "en_US") {
        return "en_US";
    }
    return "en_US";
}

QString MyFirstTool::getString(const QString& key) const {
    return m_strings.value(key).toString(key);
}

void MyFirstTool::loadLocalizationFile(const QString& languageCode) {
    QString path = toolDirectoryPath() + "/localization/" + languageCode + ".json";

    QFile file(path);
    if (!file.exists()) {
        path = toolDirectoryPath() + "/localization/en_US.json";
        file.setFileName(path);
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_strings = QJsonObject();
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject()) {
        m_strings = QJsonObject();
        return;
    }

    m_strings = doc.object();
}
```

---

## 12. 对应本地化文件示例

## 12.1 `localization/zh_CN.json`

```json
{
    "Name": "我的第一个工具",
    "Description": "一个用于演示第三方 Tool 接入流程的最小示例工具。",
    "HelloTitle": "欢迎",
    "HelloMessage": "第三方 Tool 已成功加载。",
    "HelloButton": "点击测试",
    "SidebarMessage": "这是一个可选的右侧边栏。"
}
```

## 12.2 `localization/zh_TW.json`

```json
{
    "Name": "我的第一個工具",
    "Description": "一個用於示範第三方 Tool 接入流程的最小範例工具。",
    "HelloTitle": "歡迎",
    "HelloMessage": "第三方 Tool 已成功載入。",
    "HelloButton": "點擊測試",
    "SidebarMessage": "這是一個可選的右側側欄。"
}
```

## 12.3 `localization/en_US.json`

```json
{
    "Name": "My First Tool",
    "Description": "A minimal sample tool for demonstrating third-party tool integration.",
    "HelloTitle": "Welcome",
    "HelloMessage": "The third-party tool has been loaded successfully.",
    "HelloButton": "Click to Test",
    "SidebarMessage": "This is an optional right sidebar."
}
```

---

## 13. 对应 `descriptor.apehts` 示例

```txt
name="MyFirstTool"
version="1.0.0"
supported_version="2.0.*;2.1.*"
author="Your Name"
```

如果需要依赖插件，例如 `TagList`，则写成：

```txt
name="MyFirstTool"
version="1.0.0"
supported_version="2.0.*;2.1.*"
author="Your Name"
dependencies={
    "TagList"
}
```

---

## 14. 第三方 Tool 的插件依赖访问规范

这一节非常重要。

即使本文档以“**进程隔离未启用**”为标准，**你依然必须按统一授权链路访问插件**，不能自行扫描 `plugins/` 目录。

## 14.1 禁止做的事情

以下做法都不符合规范：

### 错误做法 1：自己扫描插件目录

```cpp
QDir dir("plugins");
```

### 错误做法 2：自己拼接插件 DLL 路径

```cpp
QString path = appDir + "/plugins/TagList/TagList.dll";
```

### 错误做法 3：根据目录结构硬编码插件路径

```cpp
QString path = "../plugins/DirectXTex/DirectXTex.dll";
```

这些写法都不允许使用。

---

## 14.2 正确做法：通过 `ToolRuntimeContext` 请求授权路径

如果你的 Tool 依赖某个插件，必须：

1. 在 `descriptor.apehts` 中声明依赖
2. 运行时调用 `ToolRuntimeContext::instance().requestAuthorizedPluginBinaryPath(...)`

示例：

```cpp
#include "../sdk/ToolRuntimeContext.h"

QString pluginPath;
QString errorMessage;

if (!ToolRuntimeContext::instance().requestAuthorizedPluginBinaryPath("TagList", &pluginPath, &errorMessage)) {
    QMessageBox::warning(nullptr, "Plugin Error", errorMessage);
    return;
}

// pluginPath 即为主程序授权后的插件动态库路径
```

## 14.3 为什么必须这样写

因为主程序会根据当前 Tool 的 `dependencies` 做授权校验：

- 你声明了依赖，主程序才会给你路径
- 你没声明依赖，主程序必须拒绝
- 插件缺失或不可用时，主程序也会拒绝

这条规则在当前系统中是明确存在的，因此第三方 Tool 必须从一开始就按这个标准实现。

---

## 15. 如何在 Tool 中保存依赖列表

如果你的 Tool 需要依赖插件，建议像下面这样实现：

### 头文件中

```cpp
QStringList dependencies() const override { return m_dependencies; }

private:
    QStringList m_dependencies;
```

### `setMetaData()` 中

```cpp
void setMetaData(const QJsonObject& metaData) override {
    m_dependencies.clear();
    const QJsonArray deps = metaData.value("dependencies").toArray();
    for (const QJsonValue& value : deps) {
        const QString dep = value.toString().trimmed();
        if (!dep.isEmpty()) {
            m_dependencies.append(dep);
        }
    }
}
```

这样你的 Tool 与主程序的依赖声明就保持一致。

---

## 16. 第三方 Tool 的 CMake 构建模板

下面给出一个适合第三方独立工程使用的最小 `CMakeLists.txt`。

```cmake
cmake_minimum_required(VERSION 3.16)

project(MyFirstTool LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

add_library(MyFirstTool SHARED
    sdk/ToolRuntimeContext.cpp
    src/MyFirstTool.cpp
    src/MyFirstTool.h
    sdk/ToolInterface.h
    sdk/ToolRuntimeContext.h
)

target_link_libraries(MyFirstTool PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

target_include_directories(MyFirstTool PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

set_target_properties(MyFirstTool PROPERTIES
    PREFIX ""
    OUTPUT_NAME "MyFirstTool"
)
```

---

## 17. 如何构建

假设你的工程目录为：

```text
D:\Dev\MyFirstToolProject
```

则可使用以下命令：

```bat
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
```

或者如果你使用 Ninja：

```bat
cmake -S . -B build -G Ninja
cmake --build build
```

构建完成后，你会得到类似：

```text
build/libMyFirstTool.dll
```

---

## 18. 如何整理最终交付目录

把构建产物和资源整理成如下形式：

```text
MyFirstTool/
  libMyFirstTool.dll
  descriptor.apehts
  cover.png
  localization/
    en_US.json
    zh_CN.json
    zh_TW.json
```

然后把整个目录复制到主程序运行目录下的：

```text
tools/
```

最终效果应为：

```text
APEHOI4ToolStudio.exe
tools/
  MyFirstTool/
    libMyFirstTool.dll
    descriptor.apehts
    cover.png
    localization/
      en_US.json
      zh_CN.json
      zh_TW.json
```

---

## 19. 如何确认主程序已识别你的 Tool

如果你的 Tool 被成功识别，一般会表现为：

1. 主程序启动后，工具列表中出现你的工具卡片
2. 工具名称和描述能按当前语言正常显示
3. 点击工具后能打开主界面
4. 如果你实现了侧栏，右侧边栏也会显示
5. 如果声明了插件依赖，缺失时主程序会阻止打开并提示

---

## 20. 常见错误与排查方法

## 20.1 没有显示在工具列表里

检查：

- `tools/<ToolName>/` 目录是否放对
- 是否存在 `descriptor.apehts`
- 是否存在 DLL
- DLL 是否真的是 Qt 插件 DLL
- 是否实现了 `Q_PLUGIN_METADATA`
- 是否实现了 `Q_INTERFACES(ToolInterface)`

---

## 20.2 工具卡片出现但名称或描述为空

检查：

- `localization/` 是否存在
- 当前语言对应的 JSON 是否存在
- JSON 中是否包含 `Name` 与 `Description`
- JSON 是否是合法格式

---

## 20.3 工具无法打开

检查：

- `createWidget()` 是否返回了有效 QWidget
- Tool 是否在 `initialize()` 中抛出异常或发生致命错误
- 是否依赖了缺失插件
- `supported_version` 是否与当前主程序版本兼容

---

## 20.4 工具打开时报依赖缺失

检查：

- `descriptor.apehts` 中 `dependencies` 是否正确
- 插件名是否与插件描述符中的 `name` 完全一致
- 主程序 `plugins/` 目录中是否真的存在并加载了插件

---

## 20.5 插件访问失败

检查：

- 你是否真的在 `descriptor.apehts` 中声明了依赖
- 是否使用了 `ToolRuntimeContext::instance().requestAuthorizedPluginBinaryPath(...)`
- 是否错误地自己扫描 `plugins/`

---

## 20.6 多语言切换无效

检查：

- `loadLanguage()` 是否真正重新加载了 JSON
- 语言名是否做了 `"简体中文" -> "zh_CN"`、`"繁體中文" -> "zh_TW"`、`"English" -> "en_US"` 的映射
- 切换语言后是否刷新了已有控件文本

---

## 20.7 Tool 目录下有多个 DLL 导致加载错对象

处理方法：

- 每个 Tool 子目录只保留一个主 DLL
- 删除旧版 DLL、测试 DLL、辅助 DLL
- 第三方依赖若必须存在，应尽量静态链接，或采用不会干扰主扫描逻辑的发布方式

---

## 21. 推荐的首次开发流程

如果你是第一次做第三方 Tool，建议按下面顺序进行。

### 第 1 步：创建独立第三方工程

建立如下目录：

```text
MyFirstToolProject/
  CMakeLists.txt
  sdk/
  src/
  package/
```

### 第 2 步：加入最小 SDK 头文件

至少加入：

- `ToolInterface.h`
- `ToolRuntimeContext.h`
- `ToolRuntimeContext.cpp`

### 第 3 步：写最小 Tool 类

先实现：

- `setMetaData()`
- `loadLanguage()`
- `createWidget()`
- `createSidebarWidget()`
- `icon()`

### 第 4 步：准备三语 JSON

先保证：

- `Name`
- `Description`

存在，再补充你自己的业务键。

### 第 5 步：写 `descriptor.apehts`

先做无插件依赖版本，确认基础加载通路没问题。

### 第 6 步：构建 DLL

生成 `libMyFirstTool.dll`。

### 第 7 步：整理交付目录

放好：

- DLL
- descriptor
- localization
- cover.png

### 第 8 步：复制到主程序 `tools/`

启动主程序测试识别与打开。

### 第 9 步：如需插件再接入 `ToolRuntimeContext`

在确认无依赖版本能跑之后，再接入插件授权路径请求逻辑。

---

## 22. 一份可直接照抄的最终发布模板

假设你的工具名为 `MyFirstTool`，则最终建议如下：

### 22.1 发布目录

```text
MyFirstTool/
  libMyFirstTool.dll
  descriptor.apehts
  cover.png
  localization/
    en_US.json
    zh_CN.json
    zh_TW.json
```

### 22.2 `descriptor.apehts`

```txt
name="MyFirstTool"
version="1.0.0"
supported_version="2.0.*;2.1.*"
author="Your Name"
```

### 22.3 `en_US.json`

```json
{
    "Name": "My First Tool",
    "Description": "A minimal sample third-party tool."
}
```

### 22.4 `zh_CN.json`

```json
{
    "Name": "我的第一个工具",
    "Description": "一个最小可用的第三方工具示例。"
}
```

### 22.5 `zh_TW.json`

```json
{
    "Name": "我的第一個工具",
    "Description": "一個最小可用的第三方工具範例。"
}
```

---

## 23. 关于版本资源与额外元数据

主程序内置工具和插件会统一写入 Windows 版本资源，但对于第三方 Tool 来说：

- **能否被主程序识别和加载，不以版本资源为前提**
- 真正必须的是：
  - Qt 插件导出
  - `ToolInterface` 实现
  - 合法的 `descriptor.apehts`
  - 合法的 `localization/`

因此，如果你是首次开发第三方 Tool，可以先不做版本资源，等工具稳定后再补充。

---

## 24. 结论

一个合格的 APE-HOI4-Tool-Studio 第三方 Tool，至少要满足以下条件：

1. 位于 `tools/<ToolName>/`
2. 目录中有且仅有一个 Tool DLL
3. 存在 `descriptor.apehts`
4. 实现 `ToolInterface`
5. 使用 `Q_PLUGIN_METADATA` 与 `Q_INTERFACES`
6. 提供 `localization/en_US.json`、`zh_CN.json`、`zh_TW.json`
7. 本地化文件包含 `Name` 与 `Description`
8. 如依赖插件，必须在 `descriptor.apehts` 中声明
9. 如访问插件，必须通过 `ToolRuntimeContext` 请求授权路径
10. 不得自行扫描 `plugins/` 或硬编码插件 DLL 路径

只要遵守本文档，你就可以在不依赖主程序源码参与构建的情况下，独立制作并交付一个可被 APE-HOI4-Tool-Studio 使用的第三方 Tool。