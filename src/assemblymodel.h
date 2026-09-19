#pragma once

#include "source.h"
#include <QAbstractItemModel>
#include <QHash>
#include <QUndoStack>
#include <vector>

struct Item {
    QUuid id = QUuid::createUuid();
    bool group = false;
    QString title;
    Page page;
    std::vector<Item> children;
    int parentRow = -1;
};

class AssemblyModel : public QAbstractItemModel {
    Q_OBJECT
public:
    explicit AssemblyModel(QObject* parent = nullptr);

    QModelIndex index(int row, int column, const QModelIndex& parent = {}) const override;
    QModelIndex parent(const QModelIndex& index) const override;
    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& = {}) const override { return 1; }
    QVariant data(const QModelIndex& index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    QStringList mimeTypes() const override;
    QMimeData* mimeData(const QModelIndexList& indexes) const override;
    bool canDropMimeData(const QMimeData* data, Qt::DropAction action, int row,
                        int column, const QModelIndex& parent) const override;
    bool dropMimeData(const QMimeData* data, Qt::DropAction action, int row,
                     int column, const QModelIndex& parent) override;
    Qt::DropActions supportedDropActions() const override { return Qt::MoveAction; }
    Qt::DropActions supportedDragActions() const override { return Qt::MoveAction; }

    const Item* item(const QModelIndex& index) const;
    QModelIndex find(const QUuid& id) const;
    std::vector<Page> pages() const;
    QUuid firstPageId(const QModelIndex& index) const;
    void addSources(const std::vector<std::shared_ptr<Source>>& sources);
    void remove(const QModelIndex& index);
    void moveBy(const QModelIndex& index, int offset);
    void makeStandalone(const QModelIndex& index);
    QUndoStack* undoStack() { return &undo_; }

signals:
    void assemblyChanged();

private:
    friend class AssemblyEdit;
    using State = std::vector<Item>;
    void restore(const State& state);
    void commit(State state, const QString& description);
    bool moveItem(const QUuid& id, const QUuid& parentId, int row);
    QUuid draggedId(const QMimeData* data) const;

    State roots_;
    QHash<QUuid, QModelIndex> indexes_;
    QUndoStack undo_;
    QUuid token_ = QUuid::createUuid();
};
