#include "LogManagerTool.h"
#include "../../src/ToolRuntimeContext.h"

#include <QButtonGroup>
#include <QClipboard>
#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHeaderView>
#include <QJsonDocument>
#include <QMenu>
#include <QMetaObject>
#include <QPainter>
#include <QPointer>
#include <QProgressBar>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTableWidgetItem>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStringConverter>
#include <thread>

namespace {
struct ParsedLogFileData {
    bool isValid = false;
    QString displayName;
    QString sourcePath;
    LogMode mode = LogMode::ErrorLog;
    bool isLatest = false;
    QList<LogEntry> entries;
};

QString normalizeForSearch(const QString& value) {
    QString normalized = value;
    normalized.replace("\r\n", "\n");
    normalized.replace('\r', '\n');
    normalized = normalized.trimmed().toLower();
    return normalized;
}

QString buildCompareCellText(const LogEntry& entry) {
    QStringList lines;
    lines << entry.systemTime;
    lines << entry.gameTime;
    lines << entry.message.trimmed();
    return lines.join("\n");
}

QString latestLogInternalName() {
    return "__LATEST__";
}

int indexOfDisplayName(const QList<LogFileRecord>& files, const QString& displayName) {
    for (int i = 0; i < files.size(); ++i) {
        if (files[i].displayName == displayName) {
            return i;
        }
    }
    return -1;
}
}

class ToolLoadingOverlay : public QWidget {
public:
    explicit ToolLoadingOverlay(QWidget* parent = nullptr)
        : QWidget(parent) {
        setAttribute(Qt::WA_TransparentForMouseEvents, false);
        setAttribute(Qt::WA_StyledBackground, true);
        hide();

        m_container = new QWidget(this);
        m_container->setObjectName("ToolLoadingContainer");
        m_container->setFixedSize(320, 180);

        QVBoxLayout* layout = new QVBoxLayout(m_container);
        layout->setContentsMargins(28, 24, 28, 24);
        layout->setSpacing(14);
        layout->setAlignment(Qt::AlignCenter);

        m_messageLabel = new QLabel(m_container);
        m_messageLabel->setAlignment(Qt::AlignCenter);
        m_messageLabel->setWordWrap(true);

        m_progressBar = new QProgressBar(m_container);
        m_progressBar->setTextVisible(false);
        m_progressBar->setFixedHeight(6);
        m_progressBar->setRange(0, 0);

        layout->addStretch();
        layout->addWidget(m_messageLabel);
        layout->addWidget(m_progressBar);
        layout->addStretch();

        updateTheme();

        if (parent) {
            parent->installEventFilter(this);
            updatePosition();
        }
    }

    void setMessage(const QString& message) {
        m_messageLabel->setText(message);
    }

    void setIndeterminate() {
        m_progressBar->setRange(0, 0);
    }

    void showOverlay() {
        updateTheme();
        updatePosition();
        raise();
        show();
    }

    void hideOverlay() {
        hide();
    }

    void updateTheme() {
        const bool isDark = ConfigManager::instance().isCurrentThemeDark();
        const QString containerBg = isDark ? "#2C2C2E" : "#FFFFFF";
        const QString textColor = isDark ? "#F2F2F2" : "#1D1D1F";
        const QString borderColor = isDark ? "#3A3A3C" : "#D2D2D7";
        const QString progressBg = isDark ? "#3A3A3C" : "#E5E5EA";
        const QString progressChunk = "#007AFF";

        m_container->setStyleSheet(QString(
            "QWidget#ToolLoadingContainer {"
            " background-color: %1;"
            " border: 1px solid %2;"
            " border-radius: 12px;"
            "}"
        ).arg(containerBg, borderColor));

        m_messageLabel->setStyleSheet(QString(
            "QLabel { color: %1; font-size: 14px; font-weight: 500; }"
        ).arg(textColor));

        m_progressBar->setStyleSheet(QString(
            "QProgressBar { background-color: %1; border: none; border-radius: 3px; }"
            "QProgressBar::chunk { background-color: %2; border-radius: 3px; }"
        ).arg(progressBg, progressChunk));
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.fillRect(rect(), QColor(0, 0, 0, 120));
    }

    bool eventFilter(QObject* watched, QEvent* event) override {
        if (watched == parentWidget() && event->type() == QEvent::Resize) {
            updatePosition();
        }
        return QWidget::eventFilter(watched, event);
    }

private:
    void updatePosition() {
        if (!parentWidget()) {
            return;
        }

        setGeometry(parentWidget()->rect());
        m_container->move((width() - m_container->width()) / 2, (height() - m_container->height()) / 2);
    }

    QWidget* m_container = nullptr;
    QLabel* m_messageLabel = nullptr;
    QProgressBar* m_progressBar = nullptr;
};

// --- LogManagerMainWidget ---

LogManagerMainWidget::LogManagerMainWidget(LogManagerTool* tool, QWidget* parent)
    : QWidget(parent), m_tool(tool) {
    setObjectName("LogManagerMainWidget");
    setAttribute(Qt::WA_StyledBackground, true);

    QVBoxLayout* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    m_topBar = new QWidget(this);
    m_topBar->setObjectName("LogTopBar");
    m_topBar->setFixedHeight(40);

    QHBoxLayout* topLayout = new QHBoxLayout(m_topBar);
    topLayout->setContentsMargins(10, 0, 10, 0);
    topLayout->setSpacing(8);

    m_modeContainer = new QWidget(m_topBar);
    QHBoxLayout* modeLayout = new QHBoxLayout(m_modeContainer);
    modeLayout->setContentsMargins(0, 0, 0, 0);
    modeLayout->setSpacing(8);

    m_sortContainer = new QWidget(m_topBar);
    QHBoxLayout* sortLayout = new QHBoxLayout(m_sortContainer);
    sortLayout->setContentsMargins(0, 0, 0, 0);
    sortLayout->setSpacing(8);

    m_modeGroup = new QButtonGroup(this);
    m_sortGroup = new QButtonGroup(this);

    m_sortByTimeButton = new QPushButton(m_topBar);
    m_sortByCategoryButton = new QPushButton(m_topBar);
    m_sortByTimeButton->setCheckable(true);
    m_sortByCategoryButton->setCheckable(true);
    m_sortByTimeButton->setFixedSize(100, 28);
    m_sortByCategoryButton->setFixedSize(100, 28);
    m_sortGroup->addButton(m_sortByTimeButton, static_cast<int>(LogSortMode::ByTime));
    m_sortGroup->addButton(m_sortByCategoryButton, static_cast<int>(LogSortMode::ByCategory));

    sortLayout->addWidget(m_sortByTimeButton);
    sortLayout->addWidget(m_sortByCategoryButton);

    topLayout->addWidget(m_modeContainer);
    topLayout->addStretch();
    topLayout->addWidget(m_sortContainer);

    rootLayout->addWidget(m_topBar);

    m_searchEdit = new QLineEdit(this);
    rootLayout->addWidget(m_searchEdit);

    m_contentStack = new QStackedWidget(this);

    m_mainTable = new QTreeWidget(this);
    m_mainTable->setColumnCount(4);
    m_mainTable->setRootIsDecorated(false);
    m_mainTable->setIndentation(0);
    m_mainTable->setAllColumnsShowFocus(true);
    m_mainTable->setFrameShape(QFrame::NoFrame);
    m_mainTable->setContextMenuPolicy(Qt::CustomContextMenu);
    m_mainTable->header()->setStretchLastSection(false);
    m_mainTable->header()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_mainTable->header()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_mainTable->header()->setSectionResizeMode(2, QHeaderView::Fixed);
    m_mainTable->header()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_mainTable->setColumnWidth(0, 90);
    m_mainTable->setColumnWidth(1, 130);
    m_mainTable->setColumnWidth(2, 220);

    QWidget* comparePage = new QWidget(this);
    QVBoxLayout* compareLayout = new QVBoxLayout(comparePage);
    compareLayout->setContentsMargins(0, 0, 0, 0);
    compareLayout->setSpacing(0);

    m_compareHeaderLabel = new QLabel(comparePage);
    m_compareHeaderLabel->setAlignment(Qt::AlignCenter);
    m_compareHeaderLabel->setFixedHeight(40);
    compareLayout->addWidget(m_compareHeaderLabel);

    m_compareTable = new QTableWidget(comparePage);
    m_compareTable->setColumnCount(3);
    m_compareTable->verticalHeader()->setVisible(false);
    m_compareTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_compareTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_compareTable->setContextMenuPolicy(Qt::CustomContextMenu);
    m_compareTable->horizontalHeader()->setStretchLastSection(false);
    m_compareTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_compareTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_compareTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    m_compareTable->setColumnWidth(2, 220);
    compareLayout->addWidget(m_compareTable);

    m_contentStack->addWidget(m_mainTable);
    m_contentStack->addWidget(comparePage);
    rootLayout->addWidget(m_contentStack, 1);

    m_loadingOverlay = new ToolLoadingOverlay(this);
    m_loadingOverlay->setMessage("Loading...");
    m_loadingOverlay->hideOverlay();

    connect(m_modeGroup, &QButtonGroup::idClicked, this, [this](int id) {
        handleModeButtonClicked(id);
    });
    connect(m_sortGroup, &QButtonGroup::idClicked, this, [this](int id) {
        handleSortButtonClicked(id);
    });
    connect(m_searchEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
        handleSearchTextChanged(text);
    });
    connect(m_mainTable, &QTreeWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        handleMainListContextMenuRequested(pos);
    });
    connect(m_compareTable, &QTableWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        handleCompareTableContextMenuRequested(pos);
    });

    m_sortByTimeButton->setChecked(true);

    updateHeadersOnly();
    updateTexts();
    applyTheme();
}

void LogManagerMainWidget::updateTexts() {
    if (m_loadingOverlay) {
        m_loadingOverlay->setMessage(m_tool->getString("LoadingLogs"));
    }

    for (QPushButton* button : m_modeButtons) {
        const LogMode mode = static_cast<LogMode>(button->property("modeValue").toInt());
        if (mode == LogMode::ErrorLog) {
            button->setText(m_tool->getString("ErrorLog"));
        }
    }

    m_sortByTimeButton->setText(m_tool->getString("SortByTime"));
    m_sortByCategoryButton->setText(m_tool->getString("SortByCategory"));
    m_searchEdit->setPlaceholderText(m_tool->getString("SearchPlaceholder"));
    updateHeadersOnly();
    refreshView();
}

void LogManagerMainWidget::applyTheme() {
    const bool isDark = ConfigManager::instance().getTheme() == ConfigManager::Theme::Dark;
    const QString mainBg = isDark ? "#1E1E1E" : "#FFFFFF";
    const QString topBarBg = isDark ? "#252526" : "#F0F0F0";
    const QString borderColor = isDark ? "#3F3F46" : "#E0E0E0";
    const QString textColor = isDark ? "#F2F2F2" : "#333333";
    const QString mutedTextColor = isDark ? "#C9C9C9" : "#666666";
    const QString inputBg = isDark ? "#2C2C2E" : "#FFFFFF";
    const QString tableBg = isDark ? "#1F1F21" : "#FFFFFF";
    const QString hoverBg = isDark ? "#333337" : "#EAEAEA";
    const QString selectedBg = "#007AFF";
    const QString headerBg = isDark ? "#333337" : "#ECECEC";

    setStyleSheet(QString(
        "QWidget#LogManagerMainWidget {"
        " background-color: %1;"
        " border-top-right-radius: 10px;"
        " border-bottom-right-radius: 10px;"
        "}"
    ).arg(mainBg));

    m_topBar->setStyleSheet(QString(
        "QWidget#LogTopBar {"
        " background-color: %1;"
        " border-bottom: 1px solid %2;"
        " border-top-right-radius: 10px;"
        "}"
    ).arg(topBarBg, borderColor));

    m_searchEdit->setStyleSheet(QString(R"(
        QLineEdit {
            background-color: %1;
            color: %2;
            border: none;
            border-bottom: 1px solid %3;
            padding: 8px 12px;
        }
        QLineEdit:focus {
            border-bottom: 1px solid #007AFF;
        }
    )").arg(inputBg, textColor, borderColor));

    const QString tableStyle = QString(R"(
        QTreeWidget, QTableWidget {
            background-color: %1;
            color: %2;
            border: none;
            gridline-color: %3;
        }
        QTreeWidget::item, QTableWidget::item {
            padding: 6px 8px;
        }
        QTreeWidget::item:hover, QTableWidget::item:hover {
            background-color: %4;
        }
        QTreeWidget::item:selected, QTableWidget::item:selected {
            background-color: %5;
            color: white;
        }
        QHeaderView::section {
            background-color: %6;
            color: %2;
            border: none;
            padding: 8px;
            font-weight: bold;
        }
    )").arg(tableBg, textColor, borderColor, hoverBg, selectedBg, headerBg);

    m_mainTable->setStyleSheet(tableStyle);
    m_compareTable->setStyleSheet(tableStyle);
    m_compareHeaderLabel->setStyleSheet(QString("color: %1; font-weight: bold;").arg(mutedTextColor));

    updateButtonStyles();
    if (m_loadingOverlay) {
        m_loadingOverlay->updateTheme();
    }
    refreshView();
}

void LogManagerMainWidget::setAvailableModes(const QList<LogMode>& modes) {
    m_availableModes = modes;
    rebuildModeButtons();
}

void LogManagerMainWidget::setCurrentMode(LogMode mode) {
    m_currentMode = mode;
    updateButtonStyles();
}

void LogManagerMainWidget::setCurrentFile(const LogFileRecord& fileRecord) {
    m_currentFile = fileRecord;
    m_hasCurrentFile = true;
    refreshView();
}

void LogManagerMainWidget::setCompareFile(const LogFileRecord& compareRecord) {
    m_compareFile = compareRecord;
    m_isCompareMode = true;
    refreshView();
}

void LogManagerMainWidget::clearCompareMode() {
    m_compareFile = LogFileRecord();
    m_isCompareMode = false;
    refreshView();
}

void LogManagerMainWidget::setSortMode(LogSortMode sortMode) {
    if (m_sortMode == sortMode) {
        return;
    }
    m_sortMode = sortMode;
    updateButtonStyles();
    refreshView();
}

void LogManagerMainWidget::setSearchText(const QString& text) {
    if (m_searchEdit->text() == text) {
        return;
    }
    m_searchEdit->setText(text);
}

void LogManagerMainWidget::showLoadingOverlay(const QString& message) {
    if (!m_loadingOverlay) {
        return;
    }
    m_loadingOverlay->setMessage(message);
    m_loadingOverlay->setIndeterminate();
    m_loadingOverlay->showOverlay();
    QCoreApplication::processEvents();
}

void LogManagerMainWidget::hideLoadingOverlay() {
    if (!m_loadingOverlay) {
        return;
    }
    m_loadingOverlay->hideOverlay();
    QCoreApplication::processEvents();
}

void LogManagerMainWidget::handleModeButtonClicked(int id) {
    m_currentMode = static_cast<LogMode>(id);
    updateButtonStyles();
    if (m_tool) {
        m_tool->handleModeChanged(id);
    }
}

void LogManagerMainWidget::handleSortButtonClicked(int id) {
    const LogSortMode newMode = static_cast<LogSortMode>(id);
    if (m_sortMode == newMode) {
        return;
    }
    m_sortMode = newMode;
    updateButtonStyles();
    refreshView();
}

void LogManagerMainWidget::handleSearchTextChanged(const QString& text) {
    if (m_searchText == text) {
        return;
    }
    m_searchText = text;
    refreshView();
}

void LogManagerMainWidget::handleMainListContextMenuRequested(const QPoint& pos) {
    QTreeWidgetItem* item = m_mainTable->itemAt(pos);
    if (!item) {
        return;
    }

    QMenu menu(this);
    QAction* copyAction = menu.addAction(m_tool->getString("CopyFullEntry"));
    QAction* selectedAction = menu.exec(m_mainTable->viewport()->mapToGlobal(pos));
    if (selectedAction != copyAction) {
        return;
    }

    LogEntry entry;
    entry.systemTime = item->text(0);
    entry.gameTime = item->text(1);
    entry.category = item->text(2);
    entry.message = item->toolTip(3);
    QGuiApplication::clipboard()->setText(formatEntryForClipboard(entry));
}

void LogManagerMainWidget::handleCompareTableContextMenuRequested(const QPoint& pos) {
    QTableWidgetItem* item = m_compareTable->itemAt(pos);
    if (!item) {
        return;
    }

    if (item->column() > 1 || item->toolTip().trimmed().isEmpty()) {
        return;
    }

    QMenu menu(this);
    QAction* copyAction = menu.addAction(m_tool->getString("CopyFullEntry"));
    QAction* selectedAction = menu.exec(m_compareTable->viewport()->mapToGlobal(pos));
    if (selectedAction != copyAction) {
        return;
    }

    QGuiApplication::clipboard()->setText(item->toolTip());
}

void LogManagerMainWidget::rebuildModeButtons() {
    QLayout* layout = m_modeContainer->layout();
    while (!m_modeButtons.isEmpty()) {
        QPushButton* button = m_modeButtons.takeLast();
        m_modeGroup->removeButton(button);
        layout->removeWidget(button);
        button->deleteLater();
    }

    for (const LogMode mode : m_availableModes) {
        QPushButton* button = new QPushButton(m_modeContainer);
        button->setCheckable(true);
        button->setProperty("modeValue", static_cast<int>(mode));
        button->setFixedSize(100, 28);
        m_modeGroup->addButton(button, static_cast<int>(mode));
        layout->addWidget(button);
        m_modeButtons.append(button);
    }

    updateTexts();
    updateButtonStyles();
}

void LogManagerMainWidget::refreshView() {
    updateHeadersOnly();

    if (!m_hasCurrentFile) {
        m_mainTable->clear();
        updateHeadersOnly();
        m_compareTable->setRowCount(0);
        return;
    }

    if (m_isCompareMode) {
        m_contentStack->setCurrentIndex(1);
        populateCompareTable(buildCompareRows());
    } else {
        m_contentStack->setCurrentIndex(0);
        populateMainTable(buildSortedEntries(buildFilteredEntries()));
    }
}

QList<LogEntry> LogManagerMainWidget::buildFilteredEntries() const {
    QList<LogEntry> results;
    if (!m_hasCurrentFile) {
        return results;
    }

    for (const LogEntry& entry : m_currentFile.entries) {
        if (matchesSearch(entry)) {
            results.append(entry);
        }
    }
    return results;
}

QList<LogEntry> LogManagerMainWidget::buildSortedEntries(const QList<LogEntry>& entries) const {
    QList<LogEntry> sorted = entries;

    std::sort(sorted.begin(), sorted.end(), [this](const LogEntry& left, const LogEntry& right) {
        if (m_sortMode == LogSortMode::ByTime) {
            return left.originalIndex < right.originalIndex;
        }

        if (left.isHighPriority != right.isHighPriority) {
            return left.isHighPriority > right.isHighPriority;
        }

        const int categoryCompare = QString::compare(left.category, right.category, Qt::CaseInsensitive);
        if (categoryCompare != 0) {
            return categoryCompare < 0;
        }

        return left.originalIndex < right.originalIndex;
    });

    return sorted;
}

QList<CompareRow> LogManagerMainWidget::buildCompareRows() const {
    QList<CompareRow> rows;
    if (!m_hasCurrentFile || !m_isCompareMode) {
        return rows;
    }

    QMap<QString, CompareRow> rowMap;
    QList<QString> order;

    auto appendEntry = [&](const LogEntry& entry, bool isLeft) {
        if (!rowMap.contains(entry.normalizedKey)) {
            CompareRow row;
            row.normalizedKey = entry.normalizedKey;
            row.category = entry.category;
            row.isHighPriority = entry.isHighPriority;
            rowMap.insert(entry.normalizedKey, row);
            order.append(entry.normalizedKey);
        }

        CompareRow& row = rowMap[entry.normalizedKey];
        if (row.category.isEmpty()) {
            row.category = entry.category;
        }
        row.isHighPriority = row.isHighPriority || entry.isHighPriority;

        if (isLeft && !row.hasLeft) {
            row.hasLeft = true;
            row.leftEntry = entry;
        }
        if (!isLeft && !row.hasRight) {
            row.hasRight = true;
            row.rightEntry = entry;
        }
    };

    for (const LogEntry& entry : m_currentFile.entries) {
        appendEntry(entry, true);
    }
    for (const LogEntry& entry : m_compareFile.entries) {
        appendEntry(entry, false);
    }

    for (const QString& key : order) {
        const CompareRow row = rowMap.value(key);
        if (compareRowsMatchSearch(row)) {
            rows.append(row);
        }
    }

    std::sort(rows.begin(), rows.end(), [this](const CompareRow& left, const CompareRow& right) {
        if (m_sortMode == LogSortMode::ByCategory) {
            if (left.isHighPriority != right.isHighPriority) {
                return left.isHighPriority > right.isHighPriority;
            }

            const int categoryCompare = QString::compare(left.category, right.category, Qt::CaseInsensitive);
            if (categoryCompare != 0) {
                return categoryCompare < 0;
            }
        }

        const int leftIndex = left.hasLeft ? left.leftEntry.originalIndex : left.rightEntry.originalIndex;
        const int rightIndex = right.hasLeft ? right.leftEntry.originalIndex : right.rightEntry.originalIndex;
        return leftIndex < rightIndex;
    });

    return rows;
}

void LogManagerMainWidget::populateMainTable(const QList<LogEntry>& entries) {
    m_mainTable->clear();
    updateHeadersOnly();

    for (const LogEntry& entry : entries) {
        QTreeWidgetItem* item = new QTreeWidgetItem(m_mainTable);
        item->setText(0, entry.systemTime);
        item->setText(1, entry.gameTime);
        item->setText(2, entry.category);
        item->setText(3, buildPreviewText(entry.message));

        const QColor textColor = entry.isHighPriority ? priorityTextColor() : normalTextColor();
        item->setForeground(0, textColor);
        item->setForeground(1, textColor);
        item->setForeground(2, textColor);
        item->setForeground(3, textColor);
        item->setToolTip(3, entry.message);
    }
}

void LogManagerMainWidget::populateCompareTable(const QList<CompareRow>& rows) {
    m_compareHeaderLabel->setText(
        m_tool->getString("CompareMode") + ": " +
        m_tool->displayNameForUi(m_currentFile) + " ↔ " + m_tool->displayNameForUi(m_compareFile)
    );

    m_compareTable->setHorizontalHeaderLabels({
        m_tool->displayNameForUi(m_currentFile),
        m_tool->displayNameForUi(m_compareFile),
        m_tool->getString("ColCategory")
    });

    m_compareTable->setRowCount(0);

    const bool isDark = ConfigManager::instance().getTheme() == ConfigManager::Theme::Dark;
    const QColor darkCompareBackground(180, 95, 20);
    const QColor lightCompareBackground(255, 235, 210);

    for (const CompareRow& row : rows) {
        const int newRow = m_compareTable->rowCount();
        m_compareTable->insertRow(newRow);

        QTableWidgetItem* leftItem = new QTableWidgetItem(row.hasLeft ? buildPreviewText(buildCompareCellText(row.leftEntry)) : QString());
        QTableWidgetItem* rightItem = new QTableWidgetItem(row.hasRight ? buildPreviewText(buildCompareCellText(row.rightEntry)) : QString());
        QTableWidgetItem* categoryItem = new QTableWidgetItem(row.category);

        if (row.hasLeft) {
            leftItem->setToolTip(formatEntryForClipboard(row.leftEntry));
        }
        if (row.hasRight) {
            rightItem->setToolTip(formatEntryForClipboard(row.rightEntry));
        }

        const QColor textColor = row.isHighPriority ? priorityTextColor() : normalTextColor();
        leftItem->setForeground(textColor);
        rightItem->setForeground(textColor);
        categoryItem->setForeground(textColor);

        if (!row.hasLeft || !row.hasRight) {
            const QColor highlight = isDark ? darkCompareBackground : lightCompareBackground;
            if (!row.hasLeft) {
                leftItem->setBackground(highlight);
            }
            if (!row.hasRight) {
                rightItem->setBackground(highlight);
            }
        }

        m_compareTable->setItem(newRow, 0, leftItem);
        m_compareTable->setItem(newRow, 1, rightItem);
        m_compareTable->setItem(newRow, 2, categoryItem);
    }
}

QString LogManagerMainWidget::buildPreviewText(const QString& text) const {
    QString normalized = text;
    normalized.replace("\r\n", "\n");
    normalized.replace('\r', '\n');
    normalized = normalized.simplified();

    if (normalized.length() > 180) {
        normalized = normalized.left(180) + "...";
    }

    return normalized;
}

bool LogManagerMainWidget::matchesSearch(const LogEntry& entry) const {
    const QString needle = normalizeForSearch(m_searchText);
    if (needle.isEmpty()) {
        return true;
    }

    const QString haystack = normalizeForSearch(
        entry.systemTime + "\n" +
        entry.gameTime + "\n" +
        entry.category + "\n" +
        entry.message
    );

    return haystack.contains(needle);
}

bool LogManagerMainWidget::compareRowsMatchSearch(const CompareRow& row) const {
    const QString needle = normalizeForSearch(m_searchText);
    if (needle.isEmpty()) {
        return true;
    }

    QString haystack = row.category;
    if (row.hasLeft) {
        haystack += "\n" + row.leftEntry.systemTime + "\n" + row.leftEntry.gameTime + "\n" + row.leftEntry.message;
    }
    if (row.hasRight) {
        haystack += "\n" + row.rightEntry.systemTime + "\n" + row.rightEntry.gameTime + "\n" + row.rightEntry.message;
    }

    return normalizeForSearch(haystack).contains(needle);
}

void LogManagerMainWidget::updateButtonStyles() {
    const bool isDark = ConfigManager::instance().getTheme() == ConfigManager::Theme::Dark;
    const QString textColor = isDark ? "#CCCCCC" : "#444444";
    const QString buttonStyle = QString(
        "QPushButton { border: none; background: transparent; color: %1; border-radius: 5px; padding: 5px 15px; }"
        "QPushButton:hover { background: rgba(128,128,128,0.15); }"
        "QPushButton:checked { background: #007AFF; color: white; font-weight: bold; }"
    ).arg(textColor);

    for (QPushButton* button : m_modeButtons) {
        button->setStyleSheet(buttonStyle);
        const LogMode mode = static_cast<LogMode>(button->property("modeValue").toInt());
        button->setChecked(mode == m_currentMode);
    }

    m_sortByTimeButton->setStyleSheet(buttonStyle);
    m_sortByCategoryButton->setStyleSheet(buttonStyle);
    m_sortByTimeButton->setChecked(m_sortMode == LogSortMode::ByTime);
    m_sortByCategoryButton->setChecked(m_sortMode == LogSortMode::ByCategory);
}

QColor LogManagerMainWidget::priorityTextColor() const {
    return ConfigManager::instance().getTheme() == ConfigManager::Theme::Dark
        ? QColor(255, 120, 120)
        : QColor(220, 64, 64);
}

QColor LogManagerMainWidget::normalTextColor() const {
    return ConfigManager::instance().getTheme() == ConfigManager::Theme::Dark
        ? QColor("#F2F2F2")
        : QColor("#222222");
}

void LogManagerMainWidget::updateHeadersOnly() {
    m_mainTable->setHeaderLabels({
        m_tool->getString("ColSystemTime"),
        m_tool->getString("ColGameTime"),
        m_tool->getString("ColCategory"),
        m_tool->getString("ColMessage")
    });

    if (m_isCompareMode) {
        m_compareHeaderLabel->setText(
            m_tool->getString("CompareMode") + ": " +
            m_tool->displayNameForUi(m_currentFile) + " ↔ " + m_tool->displayNameForUi(m_compareFile)
        );
        m_compareTable->setHorizontalHeaderLabels({
            m_tool->displayNameForUi(m_currentFile),
            m_tool->displayNameForUi(m_compareFile),
            m_tool->getString("ColCategory")
        });
    } else {
        m_compareHeaderLabel->setText(m_tool->getString("CompareMode"));
        m_compareTable->setHorizontalHeaderLabels({
            m_tool->getString("Latest"),
            m_tool->getString("Latest"),
            m_tool->getString("ColCategory")
        });
    }
}

QString LogManagerMainWidget::formatEntryForClipboard(const LogEntry& entry) const {
    return QString("[%1][%2][%3]: %4")
        .arg(entry.systemTime, entry.gameTime, entry.category, entry.message);
}

// --- LogFileSidebarWidget ---

LogFileSidebarWidget::LogFileSidebarWidget(LogManagerTool* tool, QWidget* parent)
    : QWidget(parent), m_tool(tool) {
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_headerLabel = new QLabel(this);
    layout->addWidget(m_headerLabel);

    m_fileTable = new QTreeWidget(this);
    m_fileTable->setColumnCount(1);
    m_fileTable->setHeaderHidden(true);
    m_fileTable->setRootIsDecorated(false);
    m_fileTable->setIndentation(0);
    m_fileTable->setFrameShape(QFrame::NoFrame);
    m_fileTable->setContextMenuPolicy(Qt::CustomContextMenu);
    layout->addWidget(m_fileTable);

    connect(m_fileTable, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item, int column) {
        handleItemClicked(item, column);
    });
    connect(m_fileTable, &QTreeWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        handleContextMenuRequested(pos);
    });

    updateTexts();
    applyTheme();
}

void LogFileSidebarWidget::setState(const QList<LogFileRecord>& files, const QString& currentFileName, const QString& compareFileName) {
    m_files = files;
    m_currentFileName = currentFileName;
    m_compareFileName = compareFileName;
    refreshItems();
}

void LogFileSidebarWidget::updateTexts() {
    m_headerLabel->setText(m_tool->getString("LogFiles"));
}

void LogFileSidebarWidget::applyTheme() {
    const bool isDark = ConfigManager::instance().getTheme() == ConfigManager::Theme::Dark;
    const QString headerColor = isDark ? "#CCCCCC" : "#666666";
    const QString listBg = isDark ? "#2C2C2E" : "#F5F5F7";
    const QString listText = isDark ? "#FFFFFF" : "#1D1D1F";
    const QString hoverBg = isDark ? "#3A3A3C" : "#E8E8E8";
    const QString selectedBg = isDark ? "#0A84FF" : "#007AFF";

    m_headerLabel->setStyleSheet(QString("font-weight: bold; padding: 10px; color: %1;").arg(headerColor));
    m_fileTable->setStyleSheet(QString(R"(
        QTreeWidget {
            background-color: %1;
            border: none;
            color: %2;
            border-bottom-right-radius: 10px;
        }
        QTreeWidget::item {
            padding: 6px;
        }
        QTreeWidget::item:hover {
            background-color: %3;
        }
        QTreeWidget::item:selected {
            background-color: %4;
            color: white;
        }
    )").arg(listBg, listText, hoverBg, selectedBg));

    refreshItems();
}

void LogFileSidebarWidget::handleItemClicked(QTreeWidgetItem* item, int) {
    if (!item) {
        return;
    }

    const QString displayName = item->data(0, Qt::UserRole).toString();
    if (m_tool) {
        m_tool->handleFileActivated(displayName);
    }
}

void LogFileSidebarWidget::handleContextMenuRequested(const QPoint& pos) {
    QTreeWidgetItem* item = m_fileTable->itemAt(pos);
    if (!item) {
        return;
    }

    const QString displayName = item->data(0, Qt::UserRole).toString();
    if (displayName == m_currentFileName) {
        return;
    }

    QMenu menu(this);

    const bool isDark = ConfigManager::instance().getTheme() == ConfigManager::Theme::Dark;
    const QString menuStyle = isDark
        ? "QMenu { background-color: #3c3c3c; border: 1px solid #555; padding: 4px 0; }"
          "QMenu::item { color: #e0e0e0; padding: 6px 32px 6px 12px; }"
          "QMenu::item:selected { background-color: #0078d4; color: white; }"
        : "QMenu { background-color: #f8f8f8; border: 1px solid #ccc; padding: 4px 0; }"
          "QMenu::item { color: #333; padding: 6px 32px 6px 12px; }"
          "QMenu::item:selected { background-color: #0078d4; color: white; }";
    menu.setStyleSheet(menuStyle);

    if (displayName == m_compareFileName) {
        QAction* stopAction = menu.addAction(m_tool->getString("StopCompare"));
        connect(stopAction, &QAction::triggered, [this]() {
            if (m_tool) {
                m_tool->handleCompareCleared();
            }
        });
    } else {
        QAction* compareAction = menu.addAction(m_tool->getString("Compare"));
        connect(compareAction, &QAction::triggered, [this, displayName]() {
            if (m_tool) {
                m_tool->handleCompareRequested(displayName);
            }
        });
    }

    menu.exec(m_fileTable->viewport()->mapToGlobal(pos));
}

void LogFileSidebarWidget::refreshItems() {
    m_fileTable->clear();

    const bool isDark = ConfigManager::instance().getTheme() == ConfigManager::Theme::Dark;
    const QColor compareBackground = isDark ? QColor(180, 95, 20) : QColor(255, 235, 210);

    for (const LogFileRecord& file : m_files) {
        QTreeWidgetItem* item = new QTreeWidgetItem(m_fileTable);
        item->setText(0, m_tool->displayNameForUi(file));
        item->setData(0, Qt::UserRole, file.displayName);

        if (file.isLatest) {
            item->setForeground(0, QColor("#007AFF"));
        }

        if (file.displayName == m_compareFileName) {
            item->setBackground(0, compareBackground);
        }

        if (file.displayName == m_currentFileName) {
            m_fileTable->setCurrentItem(item);
            item->setSelected(true);
        }
    }
}

// --- LogManagerTool ---

QString LogManagerTool::name() const {
    return m_localizedNames.value(m_currentLang, "Log Manager");
}

QString LogManagerTool::description() const {
    return m_localizedDescs.value(m_currentLang, "View, filter, and compare game error logs");
}

void LogManagerTool::setMetaData(const QJsonObject& metaData) {
    m_id = metaData.value("id").toString();
    m_version = metaData.value("version").toString();
    m_compatibleVersion = metaData.value("compatibleVersion").toString();
    m_author = metaData.value("author").toString();
}

QIcon LogManagerTool::icon() const {
    QDir appDir(QCoreApplication::applicationDirPath());

    QString toolsPath;
    if (appDir.exists("tools")) {
        toolsPath = appDir.filePath("tools");
    } else if (QDir(appDir.filePath("../tools")).exists()) {
        toolsPath = appDir.filePath("../tools");
    }

    if (!toolsPath.isEmpty()) {
        const QString coverPath = toolsPath + "/LogManagerTool/cover.png";
        if (QFile::exists(coverPath)) {
            return QIcon(coverPath);
        }
    }

    return QIcon::fromTheme("document-text");
}

void LogManagerTool::initialize() {
    loadLanguage("English");
}

QWidget* LogManagerTool::createWidget(QWidget* parent) {
    m_mainWidget = new LogManagerMainWidget(this, parent);

    connect(m_mainWidget, &QObject::destroyed, this, [this]() {
        m_mainWidget = nullptr;
        releaseAllLoadedLogs();
    });

    m_currentFileName.clear();
    m_compareFileName.clear();
    m_discoveredFilesByMode.clear();

    m_mainWidget->setAvailableModes(availableModes());
    m_mainWidget->setCurrentMode(m_currentMode);

    QTimer::singleShot(0, this, [this]() {
        startInitialLoadAsync();
    });

    return m_mainWidget;
}

QWidget* LogManagerTool::createSidebarWidget(QWidget* parent) {
    m_sidebarWidget = new LogFileSidebarWidget(this, parent);

    connect(m_sidebarWidget, &QObject::destroyed, this, [this]() {
        m_sidebarWidget = nullptr;
    });

    syncSidebarState();
    return m_sidebarWidget;
}

void LogManagerTool::loadLanguage(const QString& lang) {
    m_currentLang = lang;

    QDir appDir(QCoreApplication::applicationDirPath());
    const QString locPath = appDir.filePath("tools/LogManagerTool/localization");
    const QString langFile = (lang == "English" ? "en_US" : (lang == "简体中文" ? "zh_CN" : "zh_TW"));

    QFile file(locPath + "/" + langFile + ".json");
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }

    m_localizedStrings = QJsonDocument::fromJson(file.readAll()).object();
    m_localizedNames[lang] = m_localizedStrings.value("Name").toString();
    m_localizedDescs[lang] = m_localizedStrings.value("Description").toString();

    if (m_mainWidget) {
        m_mainWidget->updateTexts();
    }
    if (m_sidebarWidget) {
        m_sidebarWidget->updateTexts();
    }

    syncSidebarState();
}

void LogManagerTool::applyTheme() {
    if (m_mainWidget) {
        m_mainWidget->applyTheme();
    }
    if (m_sidebarWidget) {
        m_sidebarWidget->applyTheme();
    }
}

QString LogManagerTool::getString(const QString& key) const {
    return m_localizedStrings.value(key).toString(key);
}

QString LogManagerTool::getGameDocsPath() const {
    const QString docsPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    return docsPath + "/Paradox Interactive/Hearts of Iron IV";
}

QString LogManagerTool::displayNameForUi(const LogFileRecord& fileRecord) const {
    return fileRecord.isLatest ? getString("Latest") : fileRecord.displayName;
}

QList<LogMode> LogManagerTool::availableModes() const {
    return {LogMode::ErrorLog};
}

QList<LogFileRecord> LogManagerTool::filesForMode(LogMode mode) const {
    return m_discoveredFilesByMode.value(static_cast<int>(mode));
}

bool LogManagerTool::hasCurrentFile() const {
    return !m_currentFileName.isEmpty();
}

QList<LogFileRecord> LogManagerTool::discoverLogFiles(LogMode mode) const {
    QList<LogFileRecord> files;
    if (mode != LogMode::ErrorLog) {
        return files;
    }

    const ToolRuntimeContext::DirectoryListResult logsResult =
        ToolRuntimeContext::instance().listDirectory(ToolRuntimeContext::FileRoot::Doc, "logs", false);

    if (logsResult.success) {
        for (const ToolRuntimeContext::DirectoryEntry& entry : logsResult.entries) {
            if (entry.isDirectory) {
                continue;
            }
            if (entry.name.compare("error.log", Qt::CaseInsensitive) != 0) {
                continue;
            }

            LogFileRecord latestRecord;
            latestRecord.displayName = latestLogInternalName();
            latestRecord.sourcePath = entry.relativePath;
            latestRecord.mode = mode;
            latestRecord.isLatest = true;
            latestRecord.isLoaded = false;
            files.append(latestRecord);
            break;
        }
    }

    QList<LogFileRecord> historyRecords;
    const ToolRuntimeContext::DirectoryListResult crashesResult =
        ToolRuntimeContext::instance().listDirectory(ToolRuntimeContext::FileRoot::Doc, "crashes", false);

    if (crashesResult.success) {
        for (const ToolRuntimeContext::DirectoryEntry& entry : crashesResult.entries) {
            if (!entry.isDirectory) {
                continue;
            }

            const QString logRelativePath = entry.relativePath + "/logs/error.log";
            const ToolRuntimeContext::TextReadResult probeResult =
                ToolRuntimeContext::instance().readTextFile(ToolRuntimeContext::FileRoot::Doc, logRelativePath);

            if (!probeResult.success) {
                continue;
            }

            LogFileRecord record;
            record.displayName = entry.name;
            record.sourcePath = logRelativePath;
            record.mode = mode;
            record.isLatest = false;
            record.isLoaded = false;
            historyRecords.append(record);
        }
    }

    std::sort(historyRecords.begin(), historyRecords.end(), [](const LogFileRecord& left, const LogFileRecord& right) {
        return QString::compare(left.displayName, right.displayName, Qt::CaseInsensitive) < 0;
    });

    files.append(historyRecords);
    return files;
}

QList<LogEntry> LogManagerTool::parseLogContent(const QString& content) const {
    QList<LogEntry> entries;
    const QStringList lines = content.split('\n');
    const QRegularExpression pattern(R"(^\[(\d{2}:\d{2}:\d{2})\]\[([^\]]+)\]\[([^\]]+)\]:\s?(.*)$)");

    LogEntry currentEntry;
    bool hasCurrentEntry = false;
    int currentIndex = 0;

    auto finalizeCurrentEntry = [&]() {
        if (!hasCurrentEntry) {
            return;
        }

        currentEntry.message.replace("\r\n", "\n");
        currentEntry.message.replace('\r', '\n');
        currentEntry.message = currentEntry.message.trimmed();
        currentEntry.isHighPriority = currentEntry.message.contains("This will likely crash the game", Qt::CaseInsensitive);
        currentEntry.normalizedKey = normalizeLogEntryKey(currentEntry.category, currentEntry.message);
        currentEntry.originalIndex = currentIndex++;
        entries.append(currentEntry);

        currentEntry = LogEntry();
        hasCurrentEntry = false;
    };

    for (const QString& rawLine : lines) {
        QString line = rawLine;
        if (line.endsWith('\r')) {
            line.chop(1);
        }

        const QRegularExpressionMatch match = pattern.match(line);
        if (match.hasMatch()) {
            finalizeCurrentEntry();

            currentEntry.systemTime = match.captured(1);
            currentEntry.gameTime = match.captured(2);
            currentEntry.category = match.captured(3);
            currentEntry.message = match.captured(4);
            hasCurrentEntry = true;
            continue;
        }

        if (!hasCurrentEntry) {
            continue;
        }

        currentEntry.message += "\n" + line;
    }

    finalizeCurrentEntry();
    return entries;
}

QString LogManagerTool::normalizeLogEntryKey(const QString& category, const QString& message) const {
    QString normalized = category.trimmed().toLower() + "\n" + message;
    normalized.replace("\r\n", "\n");
    normalized.replace('\r', '\n');

    QStringList lines = normalized.split('\n');
    for (QString& line : lines) {
        line = line.simplified();
    }

    return lines.join("\n").trimmed();
}

LogFileRecord LogManagerTool::findFileByDisplayName(LogMode mode, const QString& displayName, bool* ok) const {
    const QList<LogFileRecord> files = filesForMode(mode);
    for (const LogFileRecord& file : files) {
        if (file.displayName == displayName) {
            if (ok) {
                *ok = true;
            }
            return file;
        }
    }

    if (ok) {
        *ok = false;
    }
    return LogFileRecord();
}

void LogManagerTool::handleModeChanged(int mode) {
    if (m_isLoading) {
        return;
    }

    m_currentMode = static_cast<LogMode>(mode);
    m_currentFileName.clear();
    m_compareFileName.clear();
    syncSidebarState();
    startInitialLoadAsync();
}

void LogManagerTool::handleFileActivated(const QString& displayName) {
    if (m_isLoading || displayName.isEmpty()) {
        return;
    }

    m_compareFileName.clear();
    startCurrentFileLoadAsync(displayName);
}

void LogManagerTool::handleCompareRequested(const QString& displayName) {
    if (m_isLoading || displayName.isEmpty() || displayName == m_currentFileName) {
        return;
    }

    startCompareFileLoadAsync(displayName);
}

void LogManagerTool::handleCompareCleared() {
    if (m_isLoading) {
        return;
    }

    m_compareFileName.clear();
    if (m_mainWidget) {
        m_mainWidget->clearCompareMode();
    }
    syncSidebarState();
}

QString LogManagerTool::readTextFile(const QString& path) const {
    const ToolRuntimeContext::TextReadResult result =
        ToolRuntimeContext::instance().readTextFile(ToolRuntimeContext::FileRoot::Doc, path);

    if (!result.success) {
        Logger::instance().logWarning("LogManagerTool", "Failed to read log file: " + path + " - " + result.errorMessage);
        return QString();
    }

    return result.content;
}

void LogManagerTool::beginLoading(const QString& message) {
    emit loadingStarted();
    if (m_mainWidget) {
        m_mainWidget->showLoadingOverlay(message);
    }
}

void LogManagerTool::endLoading() {
    if (m_mainWidget) {
        m_mainWidget->hideLoadingOverlay();
    }
    emit loadingFinished();
}

void LogManagerTool::setLoadingState(bool loading, const QString& message) {
    m_isLoading = loading;
    if (loading) {
        beginLoading(message.isEmpty() ? getString("LoadingLogs") : message);
    } else {
        endLoading();
    }
}

void LogManagerTool::releaseAllLoadedLogs() {
    ++m_loadRequestSerial;
    m_isLoading = false;
    m_currentFileName.clear();
    m_compareFileName.clear();

    for (auto it = m_discoveredFilesByMode.begin(); it != m_discoveredFilesByMode.end(); ++it) {
        QList<LogFileRecord>& files = it.value();
        for (LogFileRecord& file : files) {
            QList<LogEntry>().swap(file.entries);
            file.isLoaded = false;
        }
    }

    m_discoveredFilesByMode.clear();
}

void LogManagerTool::refreshDiscoveredFiles() {
    m_discoveredFilesByMode.clear();
    for (const LogMode mode : availableModes()) {
        m_discoveredFilesByMode.insert(static_cast<int>(mode), discoverLogFiles(mode));
    }
}

void LogManagerTool::syncSidebarState() {
    if (!m_sidebarWidget) {
        return;
    }

    m_sidebarWidget->setState(filesForMode(m_currentMode), m_currentFileName, m_compareFileName);
}

bool LogManagerTool::ensureFileLoaded(LogMode mode, const QString& displayName, LogFileRecord* outRecord) {
    const int modeKey = static_cast<int>(mode);
    if (!m_discoveredFilesByMode.contains(modeKey)) {
        return false;
    }

    QList<LogFileRecord>& files = m_discoveredFilesByMode[modeKey];
    for (LogFileRecord& file : files) {
        if (file.displayName != displayName) {
            continue;
        }

        if (!file.isLoaded) {
            file.entries = parseLogContent(readTextFile(file.sourcePath));
            file.isLoaded = true;
        }

        if (outRecord) {
            *outRecord = file;
        }
        return true;
    }

    return false;
}

int LogManagerTool::indexOfFile(LogMode mode, const QString& displayName) const {
    const QList<LogFileRecord> files = filesForMode(mode);
    for (int i = 0; i < files.size(); ++i) {
        if (files[i].displayName == displayName) {
            return i;
        }
    }
    return -1;
}

void LogManagerTool::openInitialFileIfAvailable() {
    const QList<LogFileRecord> files = filesForMode(m_currentMode);

    m_currentFileName.clear();
    m_compareFileName.clear();

    if (files.isEmpty()) {
        if (m_mainWidget) {
            m_mainWidget->clearCompareMode();
        }
        syncSidebarState();
        return;
    }

    m_currentFileName = files.first().displayName;
    pushCurrentFileToView();
    if (m_mainWidget) {
        m_mainWidget->clearCompareMode();
    }
    syncSidebarState();
}

void LogManagerTool::pushCurrentFileToView() {
    if (!m_mainWidget || m_currentFileName.isEmpty()) {
        return;
    }

    LogFileRecord record;
    if (!ensureFileLoaded(m_currentMode, m_currentFileName, &record)) {
        return;
    }

    m_mainWidget->setCurrentMode(m_currentMode);
    m_mainWidget->setCurrentFile(record);
}

void LogManagerTool::pushCompareFileToView() {
    if (!m_mainWidget || m_compareFileName.isEmpty()) {
        return;
    }

    LogFileRecord record;
    if (!ensureFileLoaded(m_currentMode, m_compareFileName, &record)) {
        return;
    }

    m_mainWidget->setCompareFile(record);
}

void LogManagerTool::startInitialLoadAsync() {
    const LogMode mode = m_currentMode;
    const quint64 requestId = ++m_loadRequestSerial;

    m_currentFileName.clear();
    m_compareFileName.clear();
    syncSidebarState();
    setLoadingState(true, getString("LoadingLogs"));

    QPointer<LogManagerTool> self(this);
    std::thread([self, mode, requestId]() {
        if (!self) {
            return;
        }

        const QList<LogFileRecord> discoveredFiles = self->discoverLogFiles(mode);

        ParsedLogFileData initialFileData;
        if (!discoveredFiles.isEmpty()) {
            const LogFileRecord& firstFile = discoveredFiles.first();
            initialFileData.isValid = true;
            initialFileData.displayName = firstFile.displayName;
            initialFileData.sourcePath = firstFile.sourcePath;
            initialFileData.mode = firstFile.mode;
            initialFileData.isLatest = firstFile.isLatest;
            initialFileData.entries = self->parseLogContent(self->readTextFile(firstFile.sourcePath));
        }

        if (!self) {
            return;
        }

        QMetaObject::invokeMethod(self, [self, mode, requestId, discoveredFiles, initialFileData]() {
            if (!self) {
                return;
            }
            if (requestId != self->m_loadRequestSerial) {
                return;
            }

            self->m_discoveredFilesByMode.insert(static_cast<int>(mode), discoveredFiles);

            if (!initialFileData.isValid) {
                self->m_currentFileName.clear();
                self->m_compareFileName.clear();
                if (self->m_mainWidget) {
                    self->m_mainWidget->clearCompareMode();
                }
                self->syncSidebarState();
                self->setLoadingState(false);
                return;
            }

            QList<LogFileRecord>& files = self->m_discoveredFilesByMode[static_cast<int>(mode)];
            const int fileIndex = indexOfDisplayName(files, initialFileData.displayName);
            if (fileIndex >= 0) {
                files[fileIndex].entries = initialFileData.entries;
                files[fileIndex].isLoaded = true;
            }

            self->m_currentMode = mode;
            self->m_currentFileName = initialFileData.displayName;
            self->m_compareFileName.clear();
            self->pushCurrentFileToView();
            if (self->m_mainWidget) {
                self->m_mainWidget->clearCompareMode();
            }
            self->syncSidebarState();
            self->setLoadingState(false);
        }, Qt::QueuedConnection);
    }).detach();
}

void LogManagerTool::startCurrentFileLoadAsync(const QString& displayName) {
    const LogMode mode = m_currentMode;
    const QList<LogFileRecord> files = filesForMode(mode);
    const int fileIndex = indexOfDisplayName(files, displayName);
    if (fileIndex < 0) {
        return;
    }

    const LogFileRecord targetFile = files[fileIndex];
    const quint64 requestId = ++m_loadRequestSerial;

    setLoadingState(true, getString("LoadingLogs"));

    QPointer<LogManagerTool> self(this);
    std::thread([self, mode, targetFile, requestId]() {
        if (!self) {
            return;
        }

        ParsedLogFileData loadedFileData;
        loadedFileData.isValid = true;
        loadedFileData.displayName = targetFile.displayName;
        loadedFileData.sourcePath = targetFile.sourcePath;
        loadedFileData.mode = targetFile.mode;
        loadedFileData.isLatest = targetFile.isLatest;
        loadedFileData.entries = targetFile.isLoaded
            ? targetFile.entries
            : self->parseLogContent(self->readTextFile(targetFile.sourcePath));

        if (!self) {
            return;
        }

        QMetaObject::invokeMethod(self, [self, mode, requestId, loadedFileData]() {
            if (!self) {
                return;
            }
            if (requestId != self->m_loadRequestSerial) {
                return;
            }

            QList<LogFileRecord>& files = self->m_discoveredFilesByMode[static_cast<int>(mode)];
            const int targetIndex = indexOfDisplayName(files, loadedFileData.displayName);
            if (targetIndex < 0) {
                self->setLoadingState(false);
                return;
            }

            files[targetIndex].entries = loadedFileData.entries;
            files[targetIndex].isLoaded = true;

            self->m_currentMode = mode;
            self->m_currentFileName = loadedFileData.displayName;
            self->m_compareFileName.clear();
            self->pushCurrentFileToView();
            if (self->m_mainWidget) {
                self->m_mainWidget->clearCompareMode();
            }
            self->syncSidebarState();
            self->setLoadingState(false);
        }, Qt::QueuedConnection);
    }).detach();
}

void LogManagerTool::startCompareFileLoadAsync(const QString& displayName) {
    const LogMode mode = m_currentMode;
    const QString currentDisplayName = m_currentFileName;
    const QList<LogFileRecord> files = filesForMode(mode);
    const int currentIndex = indexOfDisplayName(files, currentDisplayName);
    const int compareIndex = indexOfDisplayName(files, displayName);
    if (currentIndex < 0 || compareIndex < 0) {
        return;
    }

    const LogFileRecord currentFile = files[currentIndex];
    const LogFileRecord compareFile = files[compareIndex];
    const quint64 requestId = ++m_loadRequestSerial;

    setLoadingState(true, getString("LoadingLogs"));

    QPointer<LogManagerTool> self(this);
    std::thread([self, mode, currentFile, compareFile, requestId]() {
        if (!self) {
            return;
        }

        ParsedLogFileData currentFileData;
        currentFileData.isValid = true;
        currentFileData.displayName = currentFile.displayName;
        currentFileData.sourcePath = currentFile.sourcePath;
        currentFileData.mode = currentFile.mode;
        currentFileData.isLatest = currentFile.isLatest;
        currentFileData.entries = currentFile.isLoaded
            ? currentFile.entries
            : self->parseLogContent(self->readTextFile(currentFile.sourcePath));

        ParsedLogFileData compareFileData;
        compareFileData.isValid = true;
        compareFileData.displayName = compareFile.displayName;
        compareFileData.sourcePath = compareFile.sourcePath;
        compareFileData.mode = compareFile.mode;
        compareFileData.isLatest = compareFile.isLatest;
        compareFileData.entries = compareFile.isLoaded
            ? compareFile.entries
            : self->parseLogContent(self->readTextFile(compareFile.sourcePath));

        if (!self) {
            return;
        }

        QMetaObject::invokeMethod(self, [self, mode, requestId, currentFileData, compareFileData]() {
            if (!self) {
                return;
            }
            if (requestId != self->m_loadRequestSerial) {
                return;
            }

            QList<LogFileRecord>& files = self->m_discoveredFilesByMode[static_cast<int>(mode)];
            const int currentIndex = indexOfDisplayName(files, currentFileData.displayName);
            const int compareIndex = indexOfDisplayName(files, compareFileData.displayName);
            if (currentIndex < 0 || compareIndex < 0) {
                self->setLoadingState(false);
                return;
            }

            files[currentIndex].entries = currentFileData.entries;
            files[currentIndex].isLoaded = true;
            files[compareIndex].entries = compareFileData.entries;
            files[compareIndex].isLoaded = true;

            self->m_currentMode = mode;
            self->m_currentFileName = currentFileData.displayName;
            self->m_compareFileName = compareFileData.displayName;
            self->pushCurrentFileToView();
            self->pushCompareFileToView();
            self->syncSidebarState();
            self->setLoadingState(false);
        }, Qt::QueuedConnection);
    }).detach();
}