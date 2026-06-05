//-------------------------------------------------------------------------------------
// ToolGuiModelAdapter.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "ToolGuiModelAdapter.h"

ToolGuiModelAdapter::ToolGuiModelAdapter(QObject* parent)
    : QAbstractTableModel(parent)
{
}

void ToolGuiModelAdapter::setModel(const ToolGuiCollectionModel& model) {
    beginResetModel();
    m_model = model;
    endResetModel();
}

void ToolGuiModelAdapter::setSelection(const QStringList& selectedIds) {
    if (m_selectedIds != selectedIds) {
        m_selectedIds = selectedIds;
        emit selectionChanged(m_selectedIds);
        
        // Emit data changed for all rows to update selection visual state
        if (rowCount() > 0) {
            emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));
        }
    }
}

QString ToolGuiModelAdapter::rowRole(int row) const {
    if (row < 0 || row >= m_model.rows.size()) {
        return QString();
    }
    return m_model.rows[row].role;
}

QString ToolGuiModelAdapter::cellRole(int row, int column) const {
    if (row < 0 || row >= m_model.rows.size()) {
        return QString();
    }
    
    const auto& rowData = m_model.rows[row];
    if (column < 0 || column >= rowData.cells.size()) {
        return QString();
    }
    
    return rowData.cells[column].role;
}

QString ToolGuiModelAdapter::rowId(int row) const {
    if (row < 0 || row >= m_model.rows.size()) {
        return QString();
    }
    return m_model.rows[row].id;
}

int ToolGuiModelAdapter::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return m_model.rows.size();
}

int ToolGuiModelAdapter::columnCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return m_model.columns.size();
}

QVariant ToolGuiModelAdapter::data(const QModelIndex& index, int role) const {
    if (!index.isValid()) {
        return QVariant();
    }
    
    int row = index.row();
    int column = index.column();
    
    if (row < 0 || row >= m_model.rows.size()) {
        return QVariant();
    }
    
    const auto& rowData = m_model.rows[row];
    
    if (column < 0 || column >= rowData.cells.size()) {
        return QVariant();
    }
    
    const auto& cell = rowData.cells[column];
    
    switch (role) {
        case Qt::DisplayRole:
        case Qt::EditRole:
            return cell.value;
            
        case Qt::UserRole: // Row ID
            return rowData.id;
            
        case Qt::UserRole + 1: // Row role
            return rowData.role;
            
        case Qt::UserRole + 2: // Cell role
            return cell.role;
            
        case Qt::UserRole + 3: // Is selected
            return m_selectedIds.contains(rowData.id);
            
        default:
            return QVariant();
    }
}

QVariant ToolGuiModelAdapter::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal) {
        return QVariant();
    }
    
    if (section < 0 || section >= m_model.columns.size()) {
        return QVariant();
    }
    
    const auto& column = m_model.columns[section];
    
    switch (role) {
        case Qt::DisplayRole:
            return column.text;
            
        case Qt::UserRole: // Column key
            return column.key;
            
        case Qt::UserRole + 1: // Column width
            return column.width;
            
        case Qt::UserRole + 2: // Column stretch
            return column.stretch;
            
        default:
            return QVariant();
    }
}

Qt::ItemFlags ToolGuiModelAdapter::flags(const QModelIndex& index) const {
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }
    
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}
