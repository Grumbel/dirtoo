// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "directory_tree_model.hpp"

#include <QFileIconProvider>
#include <QSet>
#include <QFileInfo>
#include <QtConcurrent>

#include <filesystem>
#include <functional>
#include <system_error>

namespace dirtoo::app {
namespace {

QStringList list_subdirectories(const QString& path, bool show_hidden)
{
  QStringList out;
  namespace fs = std::filesystem;
  std::error_code ec;
  const fs::path dir{path.toStdString()};
  if (!fs::is_directory(dir, ec) || ec) {
    return out;
  }
  for (fs::directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec), end;
       !ec && it != end; it.increment(ec)) {
    std::error_code st;
    if (!it->is_directory(st) || st) {
      continue;
    }
    const auto name = it->path().filename().string();
    if (name.empty() || name == "." || name == "..") {
      continue;
    }
    if (!show_hidden && name[0] == '.') {
      continue;
    }
    out.append(QString::fromStdString(it->path().string()));
  }
  out.sort(Qt::CaseInsensitive);
  return out;
}

} // namespace

DirectoryTreeModel::DirectoryTreeModel(QObject* parent)
    : QAbstractItemModel(parent)
{
  QFileIconProvider icons;
  folder_icon_ = icons.icon(QFileIconProvider::Folder);
  root_.path.clear();
  root_.display = QStringLiteral("root");
  root_.loaded = true;
}

DirectoryTreeModel::~DirectoryTreeModel()
{
  clear_tree();
}

void DirectoryTreeModel::set_show_hidden(bool show)
{
  if (show_hidden_ == show) {
    return;
  }
  show_hidden_ = show;
  QStringList paths;
  QStringList labels;
  for (Node* n : root_.children) {
    paths.append(n->path);
    labels.append(n->display);
  }
  reset_roots(paths, labels);
}

void DirectoryTreeModel::clear_tree()
{
  std::function<void(Node*)> wipe = [&](Node* n) {
    for (Node* c : n->children) {
      wipe(c);
      delete c;
    }
    n->children.clear();
  };
  wipe(&root_);
  root_.loaded = true;
  root_.loading = false;
}

void DirectoryTreeModel::reset_roots(const QStringList& root_paths, const QStringList& root_labels)
{
  beginResetModel();
  clear_tree();
  const int n = root_paths.size();
  for (int i = 0; i < n; ++i) {
    auto* node = new Node;
    node->path = root_paths[i];
    node->display = (i < root_labels.size() && !root_labels[i].isEmpty())
                        ? root_labels[i]
                        : QFileInfo(root_paths[i]).fileName();
    if (node->display.isEmpty()) {
      node->display = root_paths[i];
    }
    node->parent = &root_;
    node->loaded = false;
    node->is_dir = true;
    root_.children.append(node);
  }
  endResetModel();
}

DirectoryTreeModel::Node* DirectoryTreeModel::node_from_index(const QModelIndex& index) const
{
  if (!index.isValid()) {
    return const_cast<Node*>(&root_);
  }
  return static_cast<Node*>(index.internalPointer());
}

QModelIndex DirectoryTreeModel::index_from_node(Node* node, int column) const
{
  if (node == nullptr || node == &root_) {
    return {};
  }
  Node* parent = node->parent != nullptr ? node->parent : const_cast<Node*>(&root_);
  const int row = parent->children.indexOf(node);
  if (row < 0) {
    return {};
  }
  return createIndex(row, column, node);
}

QModelIndex DirectoryTreeModel::index(int row, int column, const QModelIndex& parent) const
{
  if (row < 0 || column != 0) {
    return {};
  }
  Node* p = node_from_index(parent);
  if (p == nullptr || row >= p->children.size()) {
    return {};
  }
  return createIndex(row, column, p->children.at(row));
}

QModelIndex DirectoryTreeModel::parent(const QModelIndex& index) const
{
  if (!index.isValid()) {
    return {};
  }
  Node* n = node_from_index(index);
  if (n == nullptr || n->parent == nullptr || n->parent == &root_) {
    return {};
  }
  return index_from_node(n->parent);
}

int DirectoryTreeModel::rowCount(const QModelIndex& parent) const
{
  Node* n = node_from_index(parent);
  return n != nullptr ? n->children.size() : 0;
}

int DirectoryTreeModel::columnCount(const QModelIndex& parent) const
{
  Q_UNUSED(parent);
  return 1;
}

QVariant DirectoryTreeModel::data(const QModelIndex& index, int role) const
{
  if (!index.isValid()) {
    return {};
  }
  Node* n = node_from_index(index);
  if (n == nullptr) {
    return {};
  }
  switch (role) {
  case Qt::DisplayRole:
  case Qt::EditRole:
    return n->display;
  case Qt::DecorationRole:
    return folder_icon_;
  case Qt::ToolTipRole:
  case Qt::UserRole:
    return n->path;
  default:
    return {};
  }
}

Qt::ItemFlags DirectoryTreeModel::flags(const QModelIndex& index) const
{
  if (!index.isValid()) {
    return Qt::NoItemFlags;
  }
  return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

bool DirectoryTreeModel::hasChildren(const QModelIndex& parent) const
{
  Node* n = node_from_index(parent);
  if (n == nullptr) {
    return false;
  }
  if (!n->loaded) {
    return n->is_dir;
  }
  return !n->children.isEmpty();
}

bool DirectoryTreeModel::canFetchMore(const QModelIndex& parent) const
{
  Node* n = node_from_index(parent);
  if (n == nullptr || n == &root_) {
    return false;
  }
  return n->is_dir && !n->loaded && !n->loading;
}

void DirectoryTreeModel::fetchMore(const QModelIndex& parent)
{
  Node* n = node_from_index(parent);
  if (n == nullptr || n == &root_ || n->loaded || n->loading) {
    return;
  }
  n->loading = true;
  const std::uint64_t gen = next_fetch_generation_++;
  n->fetch_generation = gen;
  const QString path = n->path;
  const bool hidden = show_hidden_;

  (void)QtConcurrent::run([this, n, path, hidden, gen]() {
    const QStringList children = list_subdirectories(path, hidden);
    QMetaObject::invokeMethod(
        this,
        [this, n, children, gen]() { apply_children(n, children, gen); },
        Qt::QueuedConnection);
  });
}

void DirectoryTreeModel::apply_children(Node* parent, const QStringList& child_paths,
                                        std::uint64_t generation)
{
  if (parent == nullptr || parent->fetch_generation != generation) {
    return;
  }
  parent->loading = false;
  if (parent->loaded) {
    return;
  }

  const QModelIndex parent_index = (parent == &root_) ? QModelIndex() : index_from_node(parent);

  if (child_paths.isEmpty()) {
    parent->loaded = true;
    // Notify views that hasChildren may have changed.
    if (parent_index.isValid()) {
      emit dataChanged(parent_index, parent_index);
    }
    return;
  }

  beginInsertRows(parent_index, 0, child_paths.size() - 1);
  for (const QString& p : child_paths) {
    auto* child = new Node;
    child->path = p;
    child->display = QFileInfo(p).fileName();
    if (child->display.isEmpty()) {
      child->display = p;
    }
    child->parent = parent;
    child->loaded = false;
    child->is_dir = true;
    parent->children.append(child);
  }
  parent->loaded = true;
  endInsertRows();
}

QString DirectoryTreeModel::path_for_index(const QModelIndex& index) const
{
  Node* n = node_from_index(index);
  if (n == nullptr || n == &root_) {
    return {};
  }
  return n->path;
}

QModelIndex DirectoryTreeModel::index_for_path(const QString& path) const
{
  if (path.isEmpty()) {
    return {};
  }
  // Breadth-first among currently loaded nodes only.
  std::function<QModelIndex(Node*)> find = [&](Node* n) -> QModelIndex {
    if (n != &root_ && n->path == path) {
      return index_from_node(n);
    }
    for (Node* c : n->children) {
      if (QModelIndex ix = find(c); ix.isValid()) {
        return ix;
      }
    }
    return {};
  };
  return find(const_cast<Node*>(&root_));
}


QModelIndex DirectoryTreeModel::ensure_path_visible(const QString& path)
{
  if (path.isEmpty()) {
    return {};
  }
  namespace fs = std::filesystem;
  std::error_code ec;
  const fs::path target = fs::weakly_canonical(fs::path(path.toStdString()), ec);
  const QString want = QString::fromStdString(ec ? path.toStdString() : target.string());

  // Find the deepest root that is a prefix of want.
  Node* root_match = nullptr;
  for (Node* r : root_.children) {
    if (want == r->path || want.startsWith(r->path + QLatin1Char('/'))
        || (r->path == QLatin1String("/") && want.startsWith(QLatin1Char('/')))) {
      if (root_match == nullptr || r->path.size() > root_match->path.size()) {
        root_match = r;
      }
    }
  }
  if (root_match == nullptr) {
    return index_for_path(want);
  }

  // Walk components from root_match down.
  Node* cur = root_match;
  if (!cur->loaded && !cur->loading) {
    fetchMore(index_from_node(cur));
  }
  QString remaining = want;
  // Normalize: strip root path prefix
  if (cur->path == QLatin1String("/")) {
    remaining = want;
  } else if (want.startsWith(cur->path)) {
    remaining = want.mid(cur->path.size());
  }
  while (remaining.startsWith(QLatin1Char('/'))) {
    remaining.remove(0, 1);
  }
  if (remaining.isEmpty() || want == cur->path) {
    return index_from_node(cur);
  }

  const QStringList parts = remaining.split(QLatin1Char('/'), Qt::SkipEmptyParts);
  QString built = cur->path;
  if (built.endsWith(QLatin1Char('/'))) {
    built.chop(1);
  }
  for (const QString& part : parts) {
    if (built == QLatin1String("/")) {
      built = QLatin1Char('/') + part;
    } else {
      built += QLatin1Char('/') + part;
    }
    if (!cur->loaded) {
      // Not listed yet — trigger fetch; caller may retry after volumes/list settle.
      if (!cur->loading) {
        fetchMore(index_from_node(cur));
      }
      return index_from_node(cur);
    }
    Node* next = nullptr;
    for (Node* c : cur->children) {
      if (c->path == built || QFileInfo(c->path).fileName() == part) {
        next = c;
        break;
      }
    }
    if (next == nullptr) {
      return index_from_node(cur);
    }
    cur = next;
    if (!cur->loaded && !cur->loading) {
      fetchMore(index_from_node(cur));
    }
  }
  return index_from_node(cur);
}

void DirectoryTreeModel::refresh_if_loaded(const QString& path)
{
  if (path.isEmpty()) {
    return;
  }
  const QModelIndex idx = index_for_path(path);
  if (!idx.isValid()) {
    return;
  }
  Node* n = node_from_index(idx);
  if (n == nullptr || n == &root_ || !n->loaded || n->loading) {
    return;
  }
  n->loading = true;
  const std::uint64_t gen = next_fetch_generation_++;
  n->fetch_generation = gen;
  const QString p = n->path;
  const bool hidden = show_hidden_;

  (void)QtConcurrent::run([this, n, p, hidden, gen]() {
    const QStringList children = list_subdirectories(p, hidden);
    QMetaObject::invokeMethod(
        this,
        [this, n, children, gen]() { apply_refreshed_children(n, children, gen); },
        Qt::QueuedConnection);
  });
}

void DirectoryTreeModel::apply_refreshed_children(Node* parent, const QStringList& child_paths,
                                                  std::uint64_t generation)
{
  if (parent == nullptr || parent->fetch_generation != generation) {
    return;
  }
  parent->loading = false;
  if (!parent->loaded) {
    apply_children(parent, child_paths, generation);
    return;
  }

  const QModelIndex parent_index = index_from_node(parent);

  QSet<QString> desired;
  desired.reserve(child_paths.size());
  for (const QString& cp : child_paths) {
    desired.insert(cp);
  }

  for (int i = parent->children.size() - 1; i >= 0; --i) {
    Node* c = parent->children[i];
    if (!desired.contains(c->path)) {
      beginRemoveRows(parent_index, i, i);
      parent->children.removeAt(i);
      std::function<void(Node*)> wipe = [&](Node* node) {
        for (Node* ch : node->children) {
          wipe(ch);
          delete ch;
        }
        node->children.clear();
        delete node;
      };
      wipe(c);
      endRemoveRows();
    }
  }

  QSet<QString> existing;
  for (Node* c : parent->children) {
    existing.insert(c->path);
  }

  int insert_at = 0;
  for (const QString& cp : child_paths) {
    if (existing.contains(cp)) {
      while (insert_at < parent->children.size() && parent->children[insert_at]->path != cp) {
        ++insert_at;
      }
      if (insert_at < parent->children.size()) {
        ++insert_at;
      }
      continue;
    }
    beginInsertRows(parent_index, insert_at, insert_at);
    auto* child = new Node;
    child->path = cp;
    child->display = QFileInfo(cp).fileName();
    if (child->display.isEmpty()) {
      child->display = cp;
    }
    child->parent = parent;
    child->loaded = false;
    child->is_dir = true;
    parent->children.insert(insert_at, child);
    endInsertRows();
    ++insert_at;
  }
}

} // namespace dirtoo::app
