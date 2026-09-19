#include "assemblymodel.h"

#include <QDataStream>
#include <QFont>
#include <QIODevice>
#include <QMimeData>
#include <QUndoCommand>
#include <algorithm>

namespace {
constexpr auto itemMime = "application/x-pdfmerge-item";
}

class AssemblyEdit : public QUndoCommand {
public:
    AssemblyEdit(AssemblyModel* model, std::vector<Item> before,
                 std::vector<Item> after, const QString& description)
        : QUndoCommand(description), model_(model), before_(std::move(before)),
          after_(std::move(after)) {}
    void undo() override { model_->restore(before_); }
    void redo() override { model_->restore(after_); }

private:
    AssemblyModel* model_;
    std::vector<Item> before_;
    std::vector<Item> after_;
};

AssemblyModel::AssemblyModel(QObject* parent) : QAbstractItemModel(parent)
{
    undo_.setUndoLimit(100);
}

const Item* AssemblyModel::item(const QModelIndex& index) const
{
    return index.isValid() ? static_cast<const Item*>(index.internalPointer()) : nullptr;
}

QModelIndex AssemblyModel::index(int row, int column, const QModelIndex& parent) const
{
    if (row < 0 || column != 0 || (parent.isValid() && parent.column() != 0))
        return {};
    const auto* container = item(parent);
    if (container && !container->group)
        return {};
    const auto& items = container ? container->children : roots_;
    if (row >= static_cast<int>(items.size()))
        return {};
    return createIndex(row, column, const_cast<Item*>(&items[row]));
}

QModelIndex AssemblyModel::parent(const QModelIndex& index) const
{
    const auto* child = item(index);
    if (!child || child->parentRow < 0)
        return {};
    return createIndex(child->parentRow, 0, const_cast<Item*>(&roots_[child->parentRow]));
}

int AssemblyModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid() && parent.column() != 0)
        return 0;
    const auto* container = item(parent);
    return static_cast<int>(container ? container->children.size() : roots_.size());
}

QVariant AssemblyModel::data(const QModelIndex& index, int role) const
{
    const auto* entry = item(index);
    if (!entry)
        return {};
    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        if (entry->group)
            return role == Qt::EditRole ? entry->title
                : tr("%1  ·  %2 pages").arg(entry->title).arg(entry->children.size());
        return tr("%1  ·  Page %2").arg(entry->page.source->name).arg(entry->page.number + 1);
    }
    if (role == Qt::ToolTipRole) {
        if (entry->group)
            return tr("%1\nDrag to move the group. Double-click to rename.").arg(entry->title);
        return tr("%1\nOriginal page %2").arg(entry->page.source->originalPath)
            .arg(entry->page.number + 1);
    }
    if (role == Qt::FontRole && entry->group) {
        QFont font;
        font.setWeight(QFont::DemiBold);
        return font;
    }
    if (role == Qt::UserRole)
        return entry->id;
    return {};
}

Qt::ItemFlags AssemblyModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return Qt::ItemIsDropEnabled;
    auto result = Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled;
    if (item(index)->group)
        result |= Qt::ItemIsDropEnabled | Qt::ItemIsEditable;
    return result;
}

bool AssemblyModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (role != Qt::EditRole || !item(index) || !item(index)->group)
        return false;
    const auto title = value.toString().trimmed();
    if (title.isEmpty() || title == item(index)->title)
        return false;
    auto next = roots_;
    next[index.row()].title = title;
    commit(std::move(next), tr("Rename group"));
    return true;
}

QModelIndex AssemblyModel::find(const QUuid& id) const
{
    return indexes_.value(id);
}

std::vector<Page> AssemblyModel::pages() const
{
    std::vector<Page> result;
    for (const auto& entry : roots_) {
        if (entry.group) {
            for (const auto& child : entry.children)
                result.push_back(child.page);
        } else {
            result.push_back(entry.page);
        }
    }
    return result;
}

QUuid AssemblyModel::firstPageId(const QModelIndex& index) const
{
    const auto* entry = item(index);
    if (!entry)
        return {};
    if (!entry->group)
        return entry->page.id;
    return entry->children.empty() ? QUuid{} : entry->children.front().page.id;
}

void AssemblyModel::restore(const State& state)
{
    beginResetModel();
    roots_ = state;
    indexes_.clear();
    for (int row = 0; row < static_cast<int>(roots_.size()); ++row) {
        auto& root = roots_[row];
        root.parentRow = -1;
        indexes_.insert(root.id, createIndex(row, 0, &root));
        for (int child = 0; child < static_cast<int>(root.children.size()); ++child) {
            auto& page = root.children[child];
            page.parentRow = row;
            indexes_.insert(page.id, createIndex(child, 0, &page));
        }
    }
    endResetModel();
    emit assemblyChanged();
}

void AssemblyModel::commit(State state, const QString& description)
{
    undo_.push(new AssemblyEdit(this, roots_, std::move(state), description));
}

void AssemblyModel::addSources(const std::vector<std::shared_ptr<Source>>& sources)
{
    if (sources.empty())
        return;
    auto next = roots_;
    for (const auto& source : sources) {
        Item group;
        group.group = true;
        group.title = source->name;
        for (int number = 0; number < source->document.pageCount(); ++number) {
            Item child;
            child.page.source = source;
            child.page.number = number;
            child.page.id = child.id;
            group.children.push_back(std::move(child));
        }
        next.push_back(std::move(group));
    }
    commit(std::move(next), tr("Add PDFs"));
}

void AssemblyModel::remove(const QModelIndex& index)
{
    if (!index.isValid())
        return;
    auto next = roots_;
    const auto parentIndex = parent(index);
    auto& siblings = parentIndex.isValid() ? next[parentIndex.row()].children : next;
    const auto description = item(index)->group ? tr("Remove group") : tr("Remove page");
    siblings.erase(siblings.begin() + index.row());
    commit(std::move(next), description);
}

QStringList AssemblyModel::mimeTypes() const
{
    return {QString::fromLatin1(itemMime)};
}

QMimeData* AssemblyModel::mimeData(const QModelIndexList& indexes) const
{
    auto* mime = new QMimeData;
    if (!indexes.isEmpty()) {
        QByteArray bytes;
        QDataStream stream(&bytes, QIODevice::WriteOnly);
        stream << token_ << item(indexes.front())->id;
        mime->setData(itemMime, bytes);
    }
    return mime;
}

QUuid AssemblyModel::draggedId(const QMimeData* data) const
{
    if (!data->hasFormat(itemMime))
        return {};
    QDataStream stream(data->data(itemMime));
    QUuid token, id;
    stream >> token >> id;
    return stream.status() == QDataStream::Ok && token == token_ ? id : QUuid{};
}

bool AssemblyModel::canDropMimeData(const QMimeData* data, Qt::DropAction action,
                                  int row, int column, const QModelIndex& parent) const
{
    if (action != Qt::MoveAction || column > 0 || row < -1 || row > rowCount(parent))
        return false;
    const auto source = find(draggedId(data));
    if (!source.isValid())
        return false;
    const auto* target = item(parent);
    return !target || (target->group && !item(source)->group);
}

bool AssemblyModel::dropMimeData(const QMimeData* data, Qt::DropAction action,
                               int row, int column, const QModelIndex& parent)
{
    if (!canDropMimeData(data, action, row, column, parent))
        return false;
    return moveItem(draggedId(data), parent.isValid() ? item(parent)->id : QUuid{},
                    row < 0 ? rowCount(parent) : row);
}

bool AssemblyModel::moveItem(const QUuid& id, const QUuid& parentId, int row)
{
    const auto source = find(id);
    const auto target = find(parentId);
    if (!source.isValid() || (!parentId.isNull() && !target.isValid()))
        return false;
    if (target.isValid() && (!item(target)->group || item(source)->group))
        return false;
    const auto oldParent = parent(source);
    if (oldParent == target && (row == source.row() || row == source.row() + 1))
        return false;
    auto next = roots_;
    auto& oldItems = oldParent.isValid() ? next[oldParent.row()].children : next;
    Item moving = oldItems[source.row()];
    oldItems.erase(oldItems.begin() + source.row());
    if (oldParent == target && row > source.row())
        --row;
    // Locate the target by ID again: removing a root can shift its row.
    auto destination = std::find_if(next.begin(), next.end(), [&](const Item& entry) {
        return entry.id == parentId;
    });
    auto& newItems = parentId.isNull() ? next : destination->children;
    row = std::clamp(row, 0, static_cast<int>(newItems.size()));
    newItems.insert(newItems.begin() + row, std::move(moving));
    commit(std::move(next), tr("Move item"));
    return true;
}

void AssemblyModel::moveBy(const QModelIndex& index, int offset)
{
    if (!index.isValid())
        return;
    const auto owner = parent(index);
    const int row = index.row() + offset;
    if (row < 0 || row >= rowCount(owner))
        return;
    moveItem(item(index)->id, owner.isValid() ? item(owner)->id : QUuid{},
             row + (offset > 0 ? 1 : 0));
}

void AssemblyModel::makeStandalone(const QModelIndex& index)
{
    const auto owner = parent(index);
    if (owner.isValid())
        moveItem(item(index)->id, {}, owner.row() + 1);
}
