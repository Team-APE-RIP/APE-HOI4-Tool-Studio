#ifndef FILETREEWIDGET_H
#define FILETREEWIDGET_H

#include <QWidget>
#include <QTreeWidget>
#include <QLineEdit>
#include <QLabel>

class FileTreeWidget : public QWidget {
    Q_OBJECT

public:
    explicit FileTreeWidget(QWidget *parent = nullptr);
    void updateTheme();

private slots:
    void refreshTree();
    void filterChanged(const QString &text);
    void onItemClicked(QTreeWidgetItem *item, int column);

private:
    void setupUi();
    void buildTree();

    QTreeWidget *m_tree;
    QLineEdit *m_searchBox;
    QLabel *m_pathLabel;
};

#endif // FILETREEWIDGET_H
