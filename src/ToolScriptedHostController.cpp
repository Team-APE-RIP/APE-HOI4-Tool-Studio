//-------------------------------------------------------------------------------------
// ToolScriptedHostController.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "ToolScriptedHostController.h"

#include "Logger.h"
#include "ToolProxyInterface.h"
#include "ToolQmlHostController.h"

#include <QJsonObject>
#include <QMetaObject>
#include <QStringList>
#include <QVariantList>

namespace {
const char* kLogContext = "ToolScriptedHostController";
constexpr bool kVerboseToolScriptedHostLogging = false;

QString packetModelIds(const QVariantList& models) {
    QStringList ids;
    for (const QVariant& modelValue : models) {
        const QString id = modelValue.toMap().value(QStringLiteral("id")).toString().trimmed();
        if (!id.isEmpty()) {
            ids.append(id);
        }
    }
    return ids.join(QLatin1Char(','));
}

QString snapshotModelIds(const QHash<QString, ToolGuiCollectionModel>& models) {
    QStringList ids;
    for (auto it = models.constBegin(); it != models.constEnd(); ++it) {
        if (!it.key().trimmed().isEmpty()) {
            ids.append(it.key());
        }
    }
    ids.sort();
    return ids.join(QLatin1Char(','));
}

ToolUiStatePacket statePacketFromJsonObject(const QJsonObject& statePacket) {
    ToolUiStatePacket packet;
    packet.pageId = statePacket.value(QStringLiteral("pageId")).toString().trimmed();
    if (packet.pageId.isEmpty()) {
        packet.pageId = statePacket.value(QStringLiteral("currentPage")).toString().trimmed();
    }
    packet.modeId = statePacket.value(QStringLiteral("modeId")).toString().trimmed();
    packet.viewState = statePacket.value(QStringLiteral("viewState")).toObject().toVariantMap();
    if (packet.viewState.isEmpty()) {
        packet.viewState = statePacket.value(QStringLiteral("values")).toObject().toVariantMap();
    }
    packet.sidebarState = statePacket.value(QStringLiteral("sidebarState")).toObject().toVariantMap();
    packet.topbarState = statePacket.value(QStringLiteral("topbarState")).toObject().toVariantMap();
    packet.runtimeVariables = statePacket.value(QStringLiteral("runtimeVariables")).toObject().toVariantMap();

    if (statePacket.value(QStringLiteral("listModels")).isArray()) {
        packet.listModels = statePacket.value(QStringLiteral("listModels")).toArray().toVariantList();
    } else {
        const QJsonObject modelsObject = statePacket.value(QStringLiteral("models")).toObject();
        for (auto it = modelsObject.begin(); it != modelsObject.end(); ++it) {
            QVariantMap modelMap = it.value().toObject().toVariantMap();
            if (!modelMap.contains(QStringLiteral("id"))) {
                modelMap.insert(QStringLiteral("id"), it.key());
            }
            packet.listModels.append(modelMap);
        }
    }

    if (statePacket.value(QStringLiteral("patches")).isArray()) {
        packet.patches = statePacket.value(QStringLiteral("patches")).toArray().toVariantList();
    }

    return packet;
}
}

ToolScriptedHostController::ToolScriptedHostController(ToolProxyInterface* proxy, QObject* parent)
    : QObject(parent)
    , m_proxy(proxy) {
}

ToolScriptedHostController::~ToolScriptedHostController() = default;

bool ToolScriptedHostController::initialize(const QString& toolDirectoryPath, QString* errorMessage) {
    if (!m_proxy) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("ToolScriptedHostController has no bound tool proxy instance.");
        }
        return false;
    }

    m_toolDirectoryPath = toolDirectoryPath;

    if (!m_qmlHostController) {
        m_qmlHostController.reset(new ToolQmlHostController(m_proxy, this));

        connect(
            m_proxy,
            &ToolProxyInterface::statePacketUpdated,
            this,
            [this](const QJsonObject& statePacket) {
                mergeStatePacket(statePacket);
            },
            Qt::QueuedConnection
        );

        connect(
            m_qmlHostController.data(),
            &ToolQmlHostController::stateChanged,
            this,
            [this]() {
                if (!m_qmlHostController) {
                    return;
                }
                m_lastStatePacket = m_qmlHostController->lastStatePacket();
                m_currentState = m_qmlHostController->currentStateSnapshot();
                syncLegacySessionState();
                notifySessionStateChanged();
            }
        );

        connect(
            m_qmlHostController.data(),
            &ToolQmlHostController::pageChanged,
            this,
            [this](const QString& pageId) {
                if (m_qmlHostController) {
                    m_lastStatePacket = m_qmlHostController->lastStatePacket();
                    m_currentState = m_qmlHostController->currentStateSnapshot();
                    syncLegacySessionState();
                }
                emit pageChanged(pageId);
            }
        );
    }

    const ToolGuiResourceDescriptor descriptor = m_proxy->guiResourceDescriptor();
    if (!m_qmlHostController->initialize(toolDirectoryPath, descriptor, m_localizedStrings, errorMessage)) {
        return false;
    }

    m_guiFilePath = m_qmlHostController->guiFilePath();
    m_currentTheme = m_qmlHostController->currentTheme();
    m_lastStatePacket = m_qmlHostController->lastStatePacket();
    m_currentState = m_qmlHostController->currentStateSnapshot();
    syncLegacySessionState();
    return true;
}

QWidget* ToolScriptedHostController::buildUiV2(QWidget* parent, const QString& guiFilePath, const QString& pageId) {
    if (!m_qmlHostController) {
        Logger::instance().logError(kLogContext, QStringLiteral("Cannot build QML UI because the host controller is not initialized."));
        return nullptr;
    }

    if (!guiFilePath.trimmed().isEmpty() && guiFilePath.trimmed() != m_guiFilePath.trimmed()) {
        Logger::instance().logWarning(
            kLogContext,
            QStringLiteral("Runtime QML entry overrides are not supported after initialization. Requested: %1")
                .arg(guiFilePath)
        );
    }

    // Use persistent view instead of creating new ones
    // Pass parent to ensure correct parent is set at creation time
    if (!pageId.trimmed().isEmpty()) {
        m_qmlHostController->setCurrentPage(pageId, false);
    }
    return m_qmlHostController->persistentView(parent);
}

void ToolScriptedHostController::updateStateV2(const QJsonObject& statePacket) {
    m_lastStatePacket = statePacketFromJsonObject(statePacket);
    m_currentState = m_qmlHostController
        ? m_qmlHostController->stateSnapshotFromPacket(m_lastStatePacket)
        : ToolGuiStateSnapshot();
    syncLegacySessionState();
    notifySessionStateChanged();

    if (m_qmlHostController) {
        m_qmlHostController->applyStatePacket(m_lastStatePacket, true);
        return;
    }
}

void ToolScriptedHostController::updateThemeV2(const QString& theme) {
    m_currentTheme = theme.trimmed();
    if (m_qmlHostController) {
        m_qmlHostController->applyTheme(m_currentTheme);
        m_currentTheme = m_qmlHostController->currentTheme();
    }
}

ToolGuiRenderResult ToolScriptedHostController::buildUi(QWidget* parent) {
    ToolGuiRenderResult result;
    result.mainWidget = m_qmlHostController ? m_qmlHostController->persistentView(parent) : nullptr;
    return result;
}

void ToolScriptedHostController::applyAction(const QString& actionType,
                                             const QString& targetId,
                                             const QVariantMap& arguments) {
    if (!m_proxy || !m_qmlHostController || actionType.trimmed().isEmpty()) {
        return;
    }

    ToolUiActionRequest request;
    request.actionType = actionType;
    request.targetId = targetId;
    request.arguments = arguments;
    if (!request.targetId.trimmed().isEmpty() && !request.arguments.contains(QStringLiteral("targetId"))) {
        request.arguments.insert(QStringLiteral("targetId"), request.targetId);
    }

    m_lastStatePacket = m_proxy->handleUiAction(request);
    m_currentState = m_qmlHostController->stateSnapshotFromPacket(m_lastStatePacket);
    syncLegacySessionState();
    notifySessionStateChanged();
    m_qmlHostController->applyStatePacket(m_lastStatePacket, true);
}

bool ToolScriptedHostController::invokeTopbarShortcut(const QString& actionId) {
    return m_qmlHostController && m_qmlHostController->invokeTopbarShortcut(actionId);
}

void ToolScriptedHostController::mergeStatePacket(const QJsonObject& statePacket) {
    m_lastStatePacket = statePacketFromJsonObject(statePacket);
    m_currentState = m_qmlHostController
        ? m_qmlHostController->stateSnapshotFromPacket(m_lastStatePacket)
        : ToolGuiStateSnapshot();
    syncLegacySessionState();
    notifySessionStateChanged();

    if (m_qmlHostController) {
        m_qmlHostController->applyStatePacket(m_lastStatePacket, true);
    }
}

void ToolScriptedHostController::notifySessionStateChanged() {
    const int receiverCount = receivers(SIGNAL(sessionStateChanged()));
    emit sessionStateChanged();

    QObject* host = parent();
    if (!host) {
        return;
    }

    const bool invoked = QMetaObject::invokeMethod(
        host,
        "refreshActiveToolRightSidebarUi",
        Qt::QueuedConnection
    );

    if (m_proxy && m_proxy->name() == QStringLiteral("FlagManagerTool")) {
        Logger::instance().logInfo(
            kLogContext,
            QStringLiteral("[FLAG_SIDEBAR_NOTIFY] receivers=%1 host=%2 invoked=%3")
                .arg(receiverCount)
                .arg(QString::fromLatin1(host->metaObject()->className()))
                .arg(invoked ? QStringLiteral("true") : QStringLiteral("false"))
        );
    }
}

void ToolScriptedHostController::setLocalizedStrings(const QMap<QString, QString>& strings) {
    m_localizedStrings = strings;
    if (m_qmlHostController) {
        m_qmlHostController->setLocalizedStrings(strings);
    }
}

void ToolScriptedHostController::syncLegacySessionState() {
    m_sessionState.currentPageId = m_currentState.currentPage;
    m_sessionState = ToolGuiSessionState{};
    m_sessionState.currentPageId = m_currentState.currentPage;

    if (kVerboseToolScriptedHostLogging) {
        Logger::instance().logInfo(
            kLogContext,
            QStringLiteral("[STATE_CHAIN] Syncing legacy session: page=%1 models=%2 packetListModels=%3 sidebarVisible=%4 activeMode=%5")
                .arg(m_currentState.currentPage)
                .arg(m_currentState.models.size())
                .arg(m_lastStatePacket.listModels.size())
                .arg(m_lastStatePacket.sidebarState.value(QStringLiteral("visible")).toBool() ? QStringLiteral("true") : QStringLiteral("false"))
                .arg(m_lastStatePacket.sidebarState.value(QStringLiteral("activeMode")).toString())
        );
    }
    if (m_proxy && m_proxy->name() == QStringLiteral("FlagManagerTool")) {
        Logger::instance().logInfo(
            kLogContext,
            QStringLiteral("[FLAG_SIDEBAR_STATE] page=%1 mode=%2 sidebarModel=%3 sidebarActive=%4 packetModels=%5 snapshotModels=%6")
                .arg(m_currentState.currentPage)
                .arg(m_lastStatePacket.modeId)
                .arg(m_lastStatePacket.sidebarState.value(QStringLiteral("modelId")).toString())
                .arg(m_lastStatePacket.sidebarState.value(QStringLiteral("activeMode")).toString())
                .arg(packetModelIds(m_lastStatePacket.listModels))
                .arg(snapshotModelIds(m_currentState.models))
        );
    }

    m_sessionState.runtimeVariables = m_lastStatePacket.runtimeVariables;

    m_sessionState.topbar.visible = m_lastStatePacket.topbarState.value(QStringLiteral("visible"), false).toBool();
    m_sessionState.topbar.currentPageId = m_lastStatePacket.topbarState.value(
        QStringLiteral("currentPageId"),
        m_currentState.currentPage
    ).toString();
    m_sessionState.topbar.pageOrder = m_lastStatePacket.topbarState.value(QStringLiteral("pageOrder")).toStringList();
    m_sessionState.topbar.functionOrder = m_lastStatePacket.topbarState.value(QStringLiteral("functionOrder")).toStringList();

    m_sessionState.sidebar.visible = m_lastStatePacket.sidebarState.value(QStringLiteral("visible"), false).toBool();
    m_sessionState.sidebar.title = m_lastStatePacket.sidebarState.value(QStringLiteral("title")).toString();
    m_sessionState.sidebar.activeMode = m_lastStatePacket.sidebarState.value(QStringLiteral("modelId")).toString().trimmed();
    if (m_sessionState.sidebar.activeMode.isEmpty()) {
        m_sessionState.sidebar.activeMode = m_lastStatePacket.sidebarState.value(QStringLiteral("activeMode")).toString();
    }
    m_sessionState.sidebar.modeOrder = m_lastStatePacket.sidebarState.value(QStringLiteral("modeOrder")).toStringList();
    m_sessionState.sidebar.searchEnabled = m_lastStatePacket.sidebarState.value(QStringLiteral("searchEnabled"), false).toBool();
    m_sessionState.sidebar.selectAllEnabled = m_lastStatePacket.sidebarState.value(QStringLiteral("selectAllEnabled"), false).toBool();
    const QVariant searchableColumnsValue = m_lastStatePacket.sidebarState.value(QStringLiteral("searchableColumns"));
    const QVariantList searchableColumnList = searchableColumnsValue.toList();
    for (const QVariant& value : searchableColumnList) {
        bool ok = false;
        const int columnIndex = value.toInt(&ok);
        if (ok) {
            m_sessionState.sidebar.searchableColumns.append(columnIndex);
        }
    }
    m_sessionState.sidebar.searchableColumnLabels =
        m_lastStatePacket.sidebarState.value(QStringLiteral("searchableColumnLabels")).toStringList();

    m_sessionState.pages.clear();
    if (!m_sessionState.currentPageId.trimmed().isEmpty()) {
        ToolGuiPageRuntimeState pageState;
        pageState.pageId = m_sessionState.currentPageId;
        pageState.currentMode = m_lastStatePacket.modeId;
        pageState.activeFunction = m_lastStatePacket.topbarState.value(QStringLiteral("activeFunction")).toString();
        pageState.values = m_currentState.values;
        m_sessionState.pages.insert(pageState.pageId, pageState);
    }

    m_sessionState.lists.clear();
    for (auto it = m_currentState.models.constBegin(); it != m_currentState.models.constEnd(); ++it) {
        const ToolGuiCollectionModel& sourceModel = it.value();
        ToolGuiListModel legacyModel;
        legacyModel.id = sourceModel.id;
        legacyModel.title = sourceModel.title.trimmed().isEmpty() ? sourceModel.id : sourceModel.title.trimmed();
        legacyModel.headerHidden = sourceModel.headerHidden;
        legacyModel.listSearch = false;
        legacyModel.selectAll = false;
        legacyModel.selectionMode = sourceModel.selectionMode;
        legacyModel.contextActions = sourceModel.contextActions;

        for (const ToolGuiListColumn& sourceColumn : sourceModel.columns) {
            ToolGuiListColumnDefinition legacyColumn;
            legacyColumn.id = sourceColumn.key;
            legacyColumn.title.rawText = sourceColumn.text;
            legacyColumn.stretch = sourceColumn.stretch ? 1 : 0;
            legacyColumn.width = sourceColumn.width;
            legacyColumn.hidden = sourceColumn.hidden;
            legacyModel.columns.append(legacyColumn);
        }

        for (const ToolGuiListRow& sourceRow : sourceModel.rows) {
            ToolGuiListRow legacyRow;
            legacyRow.id = sourceRow.id;
            legacyRow.rowId = sourceRow.rowId;
            legacyRow.role = sourceRow.role;
            legacyRow.values = sourceRow.values;
            legacyRow.state = sourceRow.state;
            legacyRow.cells = sourceRow.cells;
            legacyModel.rows.append(legacyRow);
        }

        m_sessionState.lists.insert(legacyModel.id, legacyModel);
    }

    int fileListRows = -1;
    if (m_sessionState.lists.contains(QStringLiteral("file_list"))) {
        fileListRows = m_sessionState.lists.value(QStringLiteral("file_list")).rows.size();
    }
    if (kVerboseToolScriptedHostLogging) {
        Logger::instance().logInfo(
            kLogContext,
            QStringLiteral("[STATE_CHAIN] Synced legacy session complete: page=%1 activeMode=%2 sidebarVisible=%3 lists=%4 fileListRows=%5")
                .arg(m_sessionState.currentPageId)
                .arg(m_sessionState.sidebar.activeMode)
                .arg(m_sessionState.sidebar.visible ? QStringLiteral("true") : QStringLiteral("false"))
                .arg(m_sessionState.lists.size())
                .arg(fileListRows)
        );
    }
}
