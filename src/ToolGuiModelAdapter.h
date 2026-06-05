//-------------------------------------------------------------------------------------
// ToolGuiModelAdapter.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
// V2 Architecture: Data model adapter
// - Adapts ToolGuiCollectionModel to QAbstractItemModel
// - Supports column definitions, row data, selection state
// - Supports row role and cell role for styling
// - Incremental data updates
//-------------------------------------------------------------------------------------
#ifndef TOOLGUIMODELADAPTER_H
#define TOOLGUIMODELADAPTER_H

#include "ToolGuiRuntime.h"
#include <QAbstractTableModel>
#include <QStringList>

// Model adapter for list controls
class ToolGuiModelAdapter : public QAbstractTableModel {
    Q_OBJECT
    
public:
    explicit ToolGuiModelAdapter(QObject* parent = nullptr);
    
    // Set model data from worker state
    void setModel(const ToolGuiCollectionModel& model);
    
    // Update selection
    void setSelection(const QStringList& selectedIds);
    
    // Get row/cell role for styling
    QString rowRole(int row) const;
    QString cellRole(int row, int column) const;
    
    // Get row ID
    QString rowId(int row) const;
    
    // Get selected row IDs
    QStringList selectedIds() const { return m_selectedIds; }
    
    // QAbstractItemModel interface
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

signals:
    // Emitted when selection changes
    void selectionChanged(const QStringList& selectedIds);

private:
    ToolGuiCollectionModel m_model;
    QStringList m_selectedIds;
};

#endif // TOOLGUIMODELADAPTER_H
