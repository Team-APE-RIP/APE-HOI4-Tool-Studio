//-------------------------------------------------------------------------------------
// ToolGuiRuntime.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
// V2 Architecture: Complete rewrite of the GUI runtime system
// - Anchor-based layout system (QML-like)
// - Preset reference with automatic theme adaptation
// - Simplified data binding (model = list_id)
// - Separation of static document, dynamic worker state, and host view state
//-------------------------------------------------------------------------------------
#ifndef TOOLGUIRUNTIME_H
#define TOOLGUIRUNTIME_H

#include <QHash>
#include <QMargins>
#include <QPoint>
#include <QMap>
#include <QSharedPointer>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>
#include <QVector>

// Forward declarations
class QWidget;
class QTreeWidget;
struct ToolGuiControlDefinition;
struct ToolGuiPageDefinitionV2;

//=============================================================================
// V2 STATIC DOCUMENT LAYER (Compiled .gui document)
//=============================================================================

// Anchor specification for layout positioning (QML-like)
struct ToolGuiAnchorSpec {
    // Anchor targets: "parent.left", "other_id.right", etc.
    QString left;
    QString right;
    QString top;
    QString bottom;
    QString centerX;
    QString centerY;
    
    // Shortcut anchors
    QString fill;      // "parent" - fills the parent
    QString centerIn;  // "parent" - centers in parent
    
    // Margins applied to anchors
    int marginLeft = 0;
    int marginRight = 0;
    int marginTop = 0;
    int marginBottom = 0;
};

// Size specification for controls
struct ToolGuiSizeSpec {
    // Size values: "280", "100%", "auto", "content", "1fr"
    QString width;
    QString height;
    QString minWidth;
    QString minHeight;
    QString maxWidth;
    QString maxHeight;
    QString preferred;  // For splitter panes
};

// Layout specification combining anchors and size
struct ToolGuiLayoutSpec {
    ToolGuiAnchorSpec anchors;
    ToolGuiSizeSpec size;
};

// Style specification with preset reference and overrides
struct ToolGuiStyleSpec {
    QString presetKey;           // Reference to preset definition
    QString presetBase;          // Base preset to inherit from
    QVariantMap overrides;       // Local style property overrides
};

// Data binding specification for dynamic properties
struct ToolGuiBindingSpec {
    QString property;    // "text", "visible", "hidden", "enabled", "checked", "placeholder"
    QString bindPath;    // "ui.sort_mode", "ui.is_loading", etc.
    QString bindOp;      // "", "not", "==", "!=", ">", "<", ">=", "<="
    QVariant bindValue;  // Value for comparison operations
};

// Row styling rule for list controls
struct ToolGuiRowStyleRule {
    QString role;        // "priority", "latest", "compare_target", etc.
    QString presetKey;   // Preset to apply for this role
};

// Cell styling rule for list controls
struct ToolGuiCellStyleRule {
    QString role;        // "missing", "error", "warning", etc.
    QString presetKey;   // Preset to apply for this role
};

//=============================================================================
// V2 HOST COMPONENT SYSTEM
//=============================================================================

// Host component type enumeration
enum class HostComponentType {
    None,        // Not a host component
    Topbar,      // Top toolbar area
    Sidebar,     // Left sidebar area
    RList,       // Right result list area
    Statusbar    // Bottom status bar area
};

// Base configuration for host-provided components
struct ToolGuiHostComponentConfig {
    HostComponentType type = HostComponentType::None;
    QString variant;             // Component variant: "default", "compact", "navigation", "data"
    QString preset;              // Style preset key
    QVariantMap customProperties;  // Additional custom properties
};

// Topbar host component configuration
struct ToolGuiTopbarConfig : ToolGuiHostComponentConfig {
    QString title;
    QString titleLocKey;
    
    // Button definition for topbar
    struct Button {
        QString name;              // Button identifier
        QString text;              // Button text
        QString textLocKey;        // Localization key for text
        QString icon;              // Icon path
        QString action;            // Action to dispatch on click
        QString checkedBinding;    // Binding path for checked state (for segment buttons)
        QString enabledBinding;    // Binding path for enabled state
        QString visibleBinding;    // Binding path for visibility
    };
    QVector<Button> leftButtons;   // Buttons on the left side
    QVector<Button> rightButtons;  // Buttons on the right side
    
    // Optional search box
    bool hasSearch = false;
    QString searchPlaceholder;
    QString searchPlaceholderLocKey;
    QString searchTextBinding;     // Binding path for search text
    QString searchChangeAction;    // Action to dispatch on search text change
};

// Sidebar host component configuration
struct ToolGuiSidebarConfig : ToolGuiHostComponentConfig {
    QString title;
    QString titleLocKey;
    QString subtitle;
    QString subtitleLocKey;
    
    // Main list configuration
    QString listModel;             // Collection model ID
    QString activateAction;        // Action on item activation (double-click/enter)
    QString contextAction;         // Action on context menu request
    
    // Row styling rules
    QVector<ToolGuiRowStyleRule> rowStyles;
    
    // Header/footer action definition
    struct HeaderAction {
        QString name;              // Action identifier
        QString text;              // Action text
        QString textLocKey;        // Localization key for text
        QString icon;              // Icon path
        QString action;            // Action to dispatch
    };
    QVector<HeaderAction> headerActions;  // Actions in header area
    QVector<HeaderAction> footerActions;  // Actions in footer area
    
    // Empty state
    QString emptyText;
    QString emptyTextLocKey;
};

// RList (Result List) host component configuration
struct ToolGuiRListConfig : ToolGuiHostComponentConfig {
    QString title;
    QString titleLocKey;
    
    // Search box configuration
    bool hasSearch = false;
    QString searchPlaceholder;
    QString searchPlaceholderLocKey;
    QString searchTextBinding;     // Binding path for search text
    QString searchChangeAction;    // Action to dispatch on search text change
    
    // Main list configuration
    QString mainListModel;         // Collection model ID for main list
    QString mainListVisibleBinding;  // Binding path for main list visibility
    QString mainListContextAction;   // Action on context menu request
    QVector<ToolGuiRowStyleRule> mainListRowStyles;
    QVector<ToolGuiCellStyleRule> mainListCellStyles;
    
    // Alternative list configuration (for comparison mode, etc.)
    QString altListModel;          // Collection model ID for alternative list
    QString altListVisibleBinding;   // Binding path for alt list visibility
    QString altListContextAction;    // Action on context menu request
    QVector<ToolGuiRowStyleRule> altListRowStyles;
    QVector<ToolGuiCellStyleRule> altListCellStyles;
    
    // Statistics display
    QString statsText;             // Static stats text
    QString statsTextBinding;      // Binding path for dynamic stats text
    
    // Empty state
    QString emptyText;
    QString emptyTextLocKey;
};

// Statusbar host component configuration
struct ToolGuiStatusbarConfig : ToolGuiHostComponentConfig {
    // Left status text
    QString leftText;              // Static left text
    QString leftTextBinding;       // Binding path for dynamic left text
    
    // Right statistics item definition
    struct StatItem {
        QString label;             // Stat label
        QString labelLocKey;       // Localization key for label
        QString valueBinding;      // Binding path for stat value
    };
    QVector<StatItem> rightStats;  // Statistics items on the right
    
    // Progress indicator
    bool hasProgress = false;
    QString progressBinding;       // Binding path for progress value (0-100, or -1 for indeterminate)
    QString progressTextBinding;   // Binding path for progress text
};

// Control definition (recursive tree structure)
struct ToolGuiControlDefinition {
    QString type;        // "window", "splitter", "pane", "button", "input", "list", "label", "icon", "overlay", "progress"
    QString name;        // Unique identifier within page
    
    // Layout and styling
    ToolGuiLayoutSpec layout;
    ToolGuiStyleSpec style;
    
    // Data binding
    QVector<ToolGuiBindingSpec> bindings;
    
    // Event actions: event_type -> action_name
    QHash<QString, QString> actions;
    
    // Child controls
    QVector<QSharedPointer<ToolGuiControlDefinition>> children;
    
    // Splitter-specific properties
    QString orientation;  // "horizontal", "vertical"
    QString persistKey;   // Key for persisting splitter state
    
    // List-specific properties
    QString modelId;      // Reference to collection model
    QVector<ToolGuiRowStyleRule> rowStyles;
    QVector<ToolGuiCellStyleRule> cellStyles;
    
    // Text content (for labels, buttons, etc.)
    QString textLiteral;
    QString textLocKey;
    
    // Icon properties
    QString iconPath;
    
    // Input properties
    QString placeholder;
    QString placeholderLocKey;
    
    // Host component specific properties
    bool isHostComponent = false;  // True if this control is a host-provided component
    QSharedPointer<ToolGuiHostComponentConfig> hostConfig;  // Configuration for host component
};

// Page definition in V2 document
struct ToolGuiPageDefinitionV2 {
    QString id;
    QString textLiteral;
    QString textLocKey;
    bool isDefault = false;
    QSharedPointer<ToolGuiControlDefinition> rootControl;  // Window root
};

// Compiled V2 GUI document
struct ToolGuiCompiledDocument {
    int version = 2;     // Version 2 for V2 architecture
    QString theme;       // "auto", "light", "dark"
    QVector<ToolGuiPageDefinitionV2> pages;
    QHash<QString, ToolGuiPageDefinitionV2*> pageIndex;  // Fast lookup by page id
};

//=============================================================================
// V2 PRESET SYSTEM
//=============================================================================

// Preset definition with theme support
struct ToolGuiPresetDefinition {
    QString key;         // Unique preset key
    QString role;        // "button", "input", "list", "surface", "row", "cell", etc.
    QString theme;       // "light", "dark", "default"
    QString base;        // Base preset key to inherit from
    
    // Style properties (extensible via QVariantMap for flexibility)
    QString background;
    QString backgroundHover;
    QString backgroundPressed;
    QString backgroundChecked;
    QString backgroundDisabled;
    QString textColor;
    QString textHover;
    QString textPressed;
    QString textChecked;
    QString textDisabled;
    QString border;
    QString borderHover;
    QString borderFocus;
    QString borderDisabled;
    int radius = 0;
    QMargins padding;
    QMargins margin;
    
    // Font properties
    int fontSize = 0;
    QString fontWeight;  // "normal", "bold"
    
    // Additional properties stored as variants
    QVariantMap extraProperties;

    QVariantMap values;
};

// Preset bundle with theme indexing
struct ToolGuiPresetBundle {
    QHash<QString, ToolGuiPresetDefinition> presets;  // key -> preset
    QHash<QString, QHash<QString, ToolGuiPresetDefinition*>> themeIndex;  // key -> theme -> preset
    
    // Resolve preset for current theme with fallback
    const ToolGuiPresetDefinition* resolve(const QString& key, const QString& theme) const {
        if (themeIndex.contains(key)) {
            const auto& themeMap = themeIndex[key];
            if (themeMap.contains(theme)) {
                return themeMap[theme];
            }
            if (themeMap.contains("default")) {
                return themeMap["default"];
            }
        }
        return nullptr;
    }
};

//=============================================================================
// V2 DYNAMIC WORKER STATE LAYER
//=============================================================================

// Cell value in a list row
struct ToolGuiCellValue {
    QVariant value;      // Cell content
    QString role;        // Role for cell styling (e.g., "missing", "error")
};

// Row in a list model
struct ToolGuiListRow {
    QString id;          // Unique row identifier
    QVector<ToolGuiCellValue> cells;
    QString role;        // Role for row styling (e.g., "priority", "latest")

    QString rowId;
    QVariantMap values;
    QVariantMap state;
};

// Column definition in a list model
struct ToolGuiListColumn {
    QString key;         // Column key
    QString text;        // Column header text
    int width = -1;      // Column width (-1 = auto)
    bool stretch = false;  // Whether column stretches to fill space
    bool hidden = false;    // Whether column is hidden in host-managed lists
};

// Collection model for list controls
struct ToolGuiCollectionModel {
    QString id;          // Model identifier
    QString title;       // Localized list title for host-managed sidebars
    QVector<ToolGuiListColumn> columns;
    QVector<ToolGuiListRow> rows;
    QStringList selection;  // Selected row ids
    bool headerHidden = false;
    QString selectionMode;
    QVariantList contextActions;
};

// Complete state snapshot from worker
struct ToolGuiStateSnapshot {
    QString currentPage;
    QVariantMap values;  // Nested values: ui.sort_mode, ui.search_text, etc.
    QHash<QString, ToolGuiCollectionModel> models;  // Collection models by id
};

//=============================================================================
// V2 HOST LOCAL VIEW STATE LAYER
//=============================================================================

// Splitter state for persistence
struct ToolGuiSplitterState {
    QString id;          // Splitter identifier
    QVector<int> sizes;  // Pane sizes in pixels
};

// View state managed by host (not synced to worker)
struct ToolGuiViewState {
    QHash<QString, ToolGuiSplitterState> splitters;
    QHash<QString, QPoint> scrollPositions;  // Control id -> scroll position
    QString focusedControl;
};

//=============================================================================
// V2 RUNTIME HOST INTERFACE
//=============================================================================

// Host interface for runtime operations
class ToolGuiRuntimeHost {
public:
    virtual ~ToolGuiRuntimeHost() = default;

    // Resolve localized text
    virtual QString resolveText(const QString& key, const QString& fallback) const = 0;
    
    // Get current theme ("light" or "dark")
    virtual QString currentTheme() const = 0;
    
    // Dispatch action to worker
    virtual void dispatchAction(const QString& actionName, const QVariantMap& payload) = 0;
    
    // Persist view state (splitter positions, scroll positions, etc.)
    virtual void persistViewState(const QString& key, const QVariant& value) = 0;
    
    // Load persisted view state
    virtual QVariant loadViewState(const QString& key, const QVariant& defaultValue) const = 0;
    
    // Request context menu at position
    virtual void requestContextMenu(const QString& controlId, const QPoint& pos, const QVariantMap& context) = 0;
};

struct ToolGuiTextValue {
    QString rawText;
    QString localizationKey;
};

struct ToolGuiListColumnDefinition {
    QString id;
    ToolGuiTextValue title;
    int stretch = 1;
    int width = -1;
    bool hidden = false;
};

struct ToolGuiPageRuntimeState {
    QString pageId;
    QString currentMode;
    QString activeFunction;
    QVariantMap values;
};

struct ToolGuiTopbarState {
    bool visible = true;
    QString currentPageId;
    QStringList pageOrder;
    QStringList functionOrder;
};

struct ToolGuiSidebarState {
    bool visible = true;
    QString title;
    QString activeMode;
    QStringList modeOrder;
    bool searchEnabled = false;
    bool selectAllEnabled = false;
    QList<int> searchableColumns;
    QStringList searchableColumnLabels;
};

struct ToolGuiListModel {
    QString id;
    QString title;
    QList<ToolGuiListColumnDefinition> columns;
    QList<ToolGuiListRow> rows;
    bool headerHidden = false;
    bool listSearch = false;
    bool selectAll = false;
    QString selectionMode;
    QVariantList contextActions;
};

struct ToolGuiSessionState {
    QString currentPageId;
    QMap<QString, ToolGuiPageRuntimeState> pages;
    ToolGuiTopbarState topbar;
    ToolGuiSidebarState sidebar;
    QMap<QString, ToolGuiListModel> lists;
    QVariantMap runtimeVariables;
};

struct ToolGuiRenderResult {
    QWidget* mainWidget = nullptr;
};

#endif // TOOLGUIRUNTIME_H
