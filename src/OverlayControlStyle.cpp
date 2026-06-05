//-------------------------------------------------------------------------------------
// OverlayControlStyle.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "OverlayControlStyle.h"

#include "OverlayAcrylicMaterial.h"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QIcon>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QSizePolicy>
#include <QSpinBox>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QWidget>

namespace {
constexpr int kFormControlHeight = 30;
constexpr int kFormControlWidth = 88;

void updateComboPopupWidth(QComboBox *combo) {
    if (!combo || !combo->view())
        return;

    combo->view()->setTextElideMode(Qt::ElideNone);
    combo->view()->setFixedWidth(combo->width());
}

QString formControlDimensionStyle() {
    return QStringLiteral(
        "min-width: %1px;"
        "max-width: %1px;"
        "min-height: %2px;"
        "max-height: %2px;"
        "padding: 0px;")
        .arg(kFormControlWidth)
        .arg(kFormControlHeight);
}

class OverlayComboBoxItemDelegate : public QStyledItemDelegate {
public:
    explicit OverlayComboBoxItemDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent) {
    }

protected:
    void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const override {
        QStyledItemDelegate::initStyleOption(option, index);
        if (option)
            option->displayAlignment = Qt::AlignCenter;
    }
};

class OverlayComboBoxEventFilter : public QObject {
public:
    explicit OverlayComboBoxEventFilter(QComboBox *combo)
        : QObject(combo)
        , m_combo(combo) {
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (!m_combo || !event)
            return QObject::eventFilter(watched, event);

        if ((watched == m_combo || watched == m_combo->lineEdit())
            && event->type() == QEvent::MouseButtonPress) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                updateComboPopupWidth(m_combo);
                m_combo->showPopup();
                mouseEvent->accept();
                return true;
            }
        }

        return QObject::eventFilter(watched, event);
    }

private:
    QComboBox *m_combo = nullptr;
};

QString replaceTokens(QString style, bool isDark) {
    const QString text = isDark ? QStringLiteral("#F5F5F7") : QStringLiteral("#1D1D1F");
    const QString secondary = isDark ? QStringLiteral("#A1A1A6") : QStringLiteral("#6E6E73");
    const QString group = isDark ? QStringLiteral("rgba(44, 44, 46, 0.50)") : QStringLiteral("rgba(255, 255, 255, 0.58)");
    const QString groupBorder = isDark ? QStringLiteral("rgba(255, 255, 255, 0.14)") : QStringLiteral("rgba(60, 60, 67, 0.16)");
    const QString row = isDark ? QStringLiteral("rgba(54, 54, 58, 0.42)") : QStringLiteral("rgba(255, 255, 255, 0.46)");
    const QString rowHover = isDark ? QStringLiteral("rgba(68, 68, 72, 0.52)") : QStringLiteral("rgba(255, 255, 255, 0.62)");
    const QString border = isDark ? QStringLiteral("rgba(255, 255, 255, 0.08)") : QStringLiteral("rgba(60, 60, 67, 0.12)");
    const QString borderStrong = isDark ? QStringLiteral("rgba(255, 255, 255, 0.16)") : QStringLiteral("rgba(60, 60, 67, 0.22)");
    const QString control = isDark ? QStringLiteral("rgba(72, 72, 74, 0.58)") : QStringLiteral("rgba(242, 242, 247, 0.66)");
    const QString controlHover = isDark ? QStringLiteral("rgba(84, 84, 88, 0.68)") : QStringLiteral("rgba(229, 229, 234, 0.78)");
    const QString controlPressed = isDark ? QStringLiteral("rgba(94, 94, 98, 0.74)") : QStringLiteral("rgba(209, 209, 214, 0.84)");
    const QString controlFocus = isDark ? QStringLiteral("rgba(58, 58, 60, 0.74)") : QStringLiteral("rgba(255, 255, 255, 0.82)");
    const QString icon = isDark ? QStringLiteral("rgba(255, 255, 255, 0.07)") : QStringLiteral("rgba(118, 118, 128, 0.10)");
    const QString accent = isDark ? QStringLiteral("#0A84FF") : QStringLiteral("#007AFF");
    const QString accentGlass = OverlayAcrylicMaterial::accentGlassBrush(isDark);
    const QString accentGlassHover = OverlayAcrylicMaterial::accentGlassHoverBrush(isDark);
    const QString accentSoft = isDark ? QStringLiteral("rgba(10, 132, 255, 0.18)") : QStringLiteral("rgba(0, 122, 255, 0.12)");
    const QString disabled = isDark ? QStringLiteral("rgba(235, 235, 245, 0.34)") : QStringLiteral("rgba(60, 60, 67, 0.34)");
    const QString switchOff = isDark ? QStringLiteral("#48484A") : QStringLiteral("#E5E5EA");
    const QString danger = QStringLiteral("#FF3B30");
    const QString dangerSoft = isDark ? QStringLiteral("rgba(255, 69, 58, 0.16)") : QStringLiteral("rgba(255, 59, 48, 0.10)");
    const QString dangerHover = isDark ? QStringLiteral("rgba(255, 69, 58, 0.24)") : QStringLiteral("rgba(255, 59, 48, 0.16)");

    style.replace(QStringLiteral("@TEXT@"), text);
    style.replace(QStringLiteral("@SECONDARY@"), secondary);
    style.replace(QStringLiteral("@GROUP@"), group);
    style.replace(QStringLiteral("@GROUP_BORDER@"), groupBorder);
    style.replace(QStringLiteral("@ROW@"), row);
    style.replace(QStringLiteral("@ROW_HOVER@"), rowHover);
    style.replace(QStringLiteral("@BORDER@"), border);
    style.replace(QStringLiteral("@BORDER_STRONG@"), borderStrong);
    style.replace(QStringLiteral("@CONTROL@"), control);
    style.replace(QStringLiteral("@CONTROL_HOVER@"), controlHover);
    style.replace(QStringLiteral("@CONTROL_PRESSED@"), controlPressed);
    style.replace(QStringLiteral("@CONTROL_FOCUS@"), controlFocus);
    style.replace(QStringLiteral("@ICON@"), icon);
    style.replace(QStringLiteral("@ACCENT@"), accent);
    style.replace(QStringLiteral("@ACCENT_GLASS@"), accentGlass);
    style.replace(QStringLiteral("@ACCENT_GLASS_HOVER@"), accentGlassHover);
    style.replace(QStringLiteral("@ACCENT_SOFT@"), accentSoft);
    style.replace(QStringLiteral("@DISABLED@"), disabled);
    style.replace(QStringLiteral("@SWITCH_OFF@"), switchOff);
    style.replace(QStringLiteral("@DANGER@"), danger);
    style.replace(QStringLiteral("@DANGER_SOFT@"), dangerSoft);
    style.replace(QStringLiteral("@DANGER_HOVER@"), dangerHover);
    style.replace(QStringLiteral("@FORM_CONTROL_WIDTH@"), QString::number(kFormControlWidth));
    style.replace(QStringLiteral("@FORM_CONTROL_HEIGHT@"), QString::number(kFormControlHeight));
    return style;
}
}

QString OverlayControlStyle::pageStyleSheet(bool isDark) {
    QString style = QStringLiteral(R"(
        QWidget#SettingsContent,
        QWidget#ConfigContent,
        QWidget#AccountContent {
            background-color: transparent;
        }

        QLabel#SettingsTitle,
        QLabel#ConfigTitle,
        QLabel#AccountTitle {
            color: @TEXT@;
            font-size: 18px;
            font-weight: 700;
        }

        QGroupBox#SettingsGroup {
            background-color: @GROUP@;
            border: 1px solid @GROUP_BORDER@;
            border-radius: 13px;
            margin: 0px;
        }

        QWidget#GroupContainer {
            background-color: transparent;
            border: none;
        }

        QWidget#SettingRow {
            background-color: @ROW@;
            border: 1px solid @BORDER@;
            border-radius: 9px;
        }

        QWidget#SettingRow:hover {
            background-color: @ROW_HOVER@;
            border-color: @BORDER_STRONG@;
        }

        QLabel#SettingIcon {
            background-color: @ICON@;
            border-radius: 9px;
            color: @TEXT@;
        }

        QLabel[overlayRole="GroupTitle"] {
            color: @SECONDARY@;
            background-color: transparent;
            border: none;
            padding: 0px;
            margin-left: 10px;
            margin-bottom: 7px;
            font-size: 12px;
            font-weight: 700;
        }

        QLineEdit,
        QComboBox,
        QSpinBox {
            min-height: 24px;
            color: @TEXT@;
            background-color: @CONTROL@;
            border: 1px solid @BORDER@;
            border-radius: 7px;
            font-size: 12px;
            selection-background-color: @ACCENT_GLASS@;
            selection-color: #FFFFFF;
        }

        *[overlayFormControl="true"] {
            min-width: @FORM_CONTROL_WIDTH@px;
            max-width: @FORM_CONTROL_WIDTH@px;
            min-height: @FORM_CONTROL_HEIGHT@px;
            max-height: @FORM_CONTROL_HEIGHT@px;
        }

        QLineEdit[overlayFormControl="true"],
        QComboBox[overlayFormControl="true"],
        QSpinBox[overlayFormControl="true"],
        QPushButton[overlayFormControl="true"] {
            min-width: @FORM_CONTROL_WIDTH@px;
            max-width: @FORM_CONTROL_WIDTH@px;
            min-height: @FORM_CONTROL_HEIGHT@px;
            max-height: @FORM_CONTROL_HEIGHT@px;
            text-align: center;
            outline: none;
        }

        QLineEdit {
            padding: 2px 10px;
            min-width: 118px;
        }

        QLineEdit[overlayFormControl="true"] {
            min-width: @FORM_CONTROL_WIDTH@px;
            max-width: @FORM_CONTROL_WIDTH@px;
            padding: 0px 10px;
        }

        QLineEdit::placeholder {
            color: @SECONDARY@;
        }

        QComboBox {
            padding: 2px 24px 2px 10px;
            min-width: 118px;
        }

        QComboBox[overlayFormControl="true"] {
            min-width: @FORM_CONTROL_WIDTH@px;
            max-width: @FORM_CONTROL_WIDTH@px;
            padding: 0px;
        }

        QSpinBox {
            padding: 2px 22px 2px 10px;
            min-width: 64px;
        }

        QSpinBox[overlayFormControl="true"] {
            min-width: @FORM_CONTROL_WIDTH@px;
            max-width: @FORM_CONTROL_WIDTH@px;
            padding: 0px;
            qproperty-alignment: AlignCenter;
        }

        QLineEdit[overlayComboLineEdit="true"],
        QLineEdit[overlayComboLineEdit="true"]:hover,
        QLineEdit[overlayComboLineEdit="true"]:focus,
        QLineEdit[overlaySpinLineEdit="true"],
        QLineEdit[overlaySpinLineEdit="true"]:hover,
        QLineEdit[overlaySpinLineEdit="true"]:focus {
            min-width: 0px;
            max-width: 16777215px;
            padding: 0px;
            color: @TEXT@;
            background-color: transparent;
            border: none;
            selection-background-color: @ACCENT_GLASS@;
            selection-color: #FFFFFF;
        }

        QLineEdit:hover,
        QComboBox:hover,
        QSpinBox:hover {
            background-color: @CONTROL_HOVER@;
            border-color: @BORDER_STRONG@;
        }

        QLineEdit:focus,
        QComboBox:focus,
        QSpinBox:focus {
            background-color: @CONTROL_FOCUS@;
            border-color: @ACCENT@;
        }

        QComboBox[overlayFormControl="true"]::drop-down,
        QComboBox[overlayFormControl="true"]::drop-down:hover,
        QComboBox[overlayFormControl="true"]::drop-down:pressed {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 0px;
            border: none;
            background-color: transparent;
        }

        QComboBox::down-arrow {
            image: none;
            width: 0px;
            height: 0px;
        }

        QComboBox QAbstractItemView {
            color: @TEXT@;
            background-color: @ROW@;
            border: 1px solid @BORDER_STRONG@;
            border-radius: 9px;
            padding: 5px;
            outline: none;
            selection-background-color: @ACCENT_GLASS@;
            selection-color: #FFFFFF;
        }

        QComboBox QAbstractItemView::item {
            min-height: 22px;
            padding: 4px 0px;
            border-radius: 6px;
            color: @TEXT@;
            text-align: center;
        }

        QComboBox QAbstractItemView::item:hover {
            background-color: @ACCENT_SOFT@;
        }

        QSpinBox::up-button,
        QSpinBox::down-button {
            width: 19px;
            border: none;
            background-color: transparent;
            margin-right: 3px;
        }

        QSpinBox::up-button:hover,
        QSpinBox::down-button:hover {
            background-color: @CONTROL_HOVER@;
            border-radius: 5px;
        }

        QSpinBox[overlayFormControl="true"]::up-button,
        QSpinBox[overlayFormControl="true"]::down-button {
            width: 0px;
            height: 0px;
            margin: 0px;
            padding: 0px;
            border: none;
            background-color: transparent;
        }

        QCheckBox {
            background-color: transparent;
            min-width: 42px;
            max-width: 42px;
            min-height: 24px;
            max-height: 24px;
            spacing: 0px;
        }

        QCheckBox::indicator {
            width: 38px;
            height: 22px;
            border-radius: 11px;
            background-color: @SWITCH_OFF@;
            border: 1px solid @BORDER_STRONG@;
        }

        QCheckBox::indicator:hover {
            border-color: @ACCENT@;
        }

        QCheckBox::indicator:checked {
            background-color: #34C759;
            border-color: #34C759;
        }

        QCheckBox[settingsPillSwitch="true"] {
            min-width: 90px;
            max-width: 90px;
            min-height: 32px;
            max-height: 32px;
            padding: 0px;
            spacing: 0px;
        }

        QCheckBox[settingsPillSwitch="true"]::indicator {
            width: 0px;
            height: 0px;
            margin: 0px;
            padding: 0px;
            border: none;
            background-color: transparent;
        }

        QPushButton {
            min-height: 24px;
            padding: 3px 11px;
            color: @TEXT@;
            background-color: @CONTROL@;
            border: 1px solid @BORDER@;
            border-radius: 7px;
            font-size: 12px;
            font-weight: 600;
        }

        QPushButton[overlayFormControl="true"] {
            min-width: @FORM_CONTROL_WIDTH@px;
            max-width: @FORM_CONTROL_WIDTH@px;
            padding: 0px;
            text-align: center;
        }

        QPushButton[overlayFormControl="true"]::menu-indicator,
        QPushButton[overlayFormControl="true"]::menu-indicator:hover,
        QPushButton[overlayFormControl="true"]::menu-indicator:pressed {
            image: none;
            width: 0px;
            height: 0px;
        }

        QPushButton:hover {
            background-color: @CONTROL_HOVER@;
            border-color: @BORDER_STRONG@;
        }

        QPushButton:pressed {
            background-color: @CONTROL_PRESSED@;
        }

        QPushButton:checked {
            color: #FFFFFF;
            background-color: @ACCENT_GLASS@;
            border-color: rgba(120, 194, 255, 0.58);
        }

        QPushButton:checked:hover {
            background-color: @ACCENT_GLASS_HOVER@;
        }

        QPushButton:disabled {
            color: @DISABLED@;
            background-color: transparent;
            border-color: @BORDER@;
        }

        QPushButton#OverlayCloseButton {
            min-width: 30px;
            max-width: 30px;
            min-height: 30px;
            max-height: 30px;
            padding: 0px 0px 2px 0px;
            color: @SECONDARY@;
            background-color: transparent;
            border: none;
            border-radius: 15px;
            font-size: 20px;
            font-weight: 500;
        }

        QPushButton#OverlayCloseButton:hover {
            color: @TEXT@;
            background-color: @CONTROL_HOVER@;
        }

        QPushButton#OverlayCloseButton:pressed {
            background-color: @CONTROL_PRESSED@;
        }

        QPushButton#LogoutBtn {
            color: @DANGER@;
            background-color: @DANGER_SOFT@;
            border-color: rgba(255, 59, 48, 0.34);
        }

        QPushButton#LogoutBtn:hover {
            background-color: @DANGER_HOVER@;
            border-color: @DANGER@;
        }

        QPushButton#LogoutBtn:pressed {
            background-color: rgba(255, 59, 48, 0.28);
        }
    )");

    return replaceTokens(style, isDark);
}

void OverlayControlStyle::polishFormControl(QWidget *control) {
    if (!control || qobject_cast<QCheckBox *>(control))
        return;

    control->setProperty("overlayFormControl", true);
    control->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    control->setMinimumSize(kFormControlWidth, kFormControlHeight);
    control->setMaximumSize(kFormControlWidth, kFormControlHeight);

    if (auto *lineEdit = qobject_cast<QLineEdit *>(control)) {
        lineEdit->setAlignment(Qt::AlignCenter);
        lineEdit->setClearButtonEnabled(false);
        lineEdit->setFixedSize(kFormControlWidth, kFormControlHeight);
        return;
    }

    if (auto *combo = qobject_cast<QComboBox *>(control)) {
        combo->setEditable(true);
        combo->setInsertPolicy(QComboBox::NoInsert);
        if (QLineEdit *edit = combo->lineEdit()) {
            edit->setReadOnly(true);
            edit->setAlignment(Qt::AlignCenter);
            edit->setClearButtonEnabled(false);
            edit->setFrame(false);
            edit->setCursor(Qt::ArrowCursor);
            edit->setFocusPolicy(Qt::NoFocus);
            edit->setProperty("overlayComboLineEdit", true);
            edit->setTextMargins(0, 0, 0, 0);
            edit->setMinimumWidth(0);
            edit->setMaximumWidth(kFormControlWidth);
            if (!edit->property("overlayComboFilterInstalled").toBool()) {
                edit->installEventFilter(new OverlayComboBoxEventFilter(combo));
                edit->setProperty("overlayComboFilterInstalled", true);
            }
        }
        combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        combo->setMinimumContentsLength(0);
        combo->setMinimumWidth(kFormControlWidth);
        combo->setMaximumWidth(kFormControlWidth);
        combo->setFixedSize(kFormControlWidth, kFormControlHeight);
        if (combo->view()) {
            combo->view()->setTextElideMode(Qt::ElideNone);
            if (!combo->view()->property("overlayCenteredDelegateInstalled").toBool()) {
                combo->view()->setItemDelegate(new OverlayComboBoxItemDelegate(combo->view()));
                combo->view()->setProperty("overlayCenteredDelegateInstalled", true);
            }
        }
        updateComboPopupWidth(combo);
        if (!combo->property("overlayComboFilterInstalled").toBool()) {
            combo->installEventFilter(new OverlayComboBoxEventFilter(combo));
            combo->setProperty("overlayComboFilterInstalled", true);
        }
        return;
    }

    if (auto *spin = qobject_cast<QSpinBox *>(control)) {
        spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
        spin->setAlignment(Qt::AlignCenter);
        if (QLineEdit *edit = spin->findChild<QLineEdit *>()) {
            edit->setAlignment(Qt::AlignCenter);
            edit->setClearButtonEnabled(false);
            edit->setProperty("overlaySpinLineEdit", true);
            edit->setTextMargins(0, 0, 0, 0);
            edit->setMinimumWidth(0);
            edit->setMaximumWidth(kFormControlWidth);
        }
        spin->setMinimumWidth(kFormControlWidth);
        spin->setMaximumWidth(kFormControlWidth);
        spin->setStyleSheet(formControlDimensionStyle());
        spin->setFixedSize(kFormControlWidth, kFormControlHeight);
        return;
    }

    if (auto *pushButton = qobject_cast<QPushButton *>(control)) {
        pushButton->setAutoDefault(false);
        pushButton->setDefault(false);
        pushButton->setMenu(nullptr);
        pushButton->setMinimumWidth(kFormControlWidth);
        pushButton->setMaximumWidth(kFormControlWidth);
    }

    if (auto *button = qobject_cast<QAbstractButton *>(control)) {
        button->setIcon(QIcon());
        button->setIconSize(QSize(0, 0));
        button->setFocusPolicy(Qt::NoFocus);
        control->setFixedSize(kFormControlWidth, kFormControlHeight);
    }
}

QString OverlayControlStyle::linkButtonStyle(bool isDark) {
    QString style = QStringLiteral(R"(
        QPushButton {
            color: @ACCENT@;
            text-align: left;
            background-color: transparent;
            border: none;
            padding: 4px 0px;
            font-weight: 600;
        }

        QPushButton:hover {
            color: @TEXT@;
        }

        QPushButton:pressed {
            color: @SECONDARY@;
        }
    )");

    return replaceTokens(style, isDark);
}
