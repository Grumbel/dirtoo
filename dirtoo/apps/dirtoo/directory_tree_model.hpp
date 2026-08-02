// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QAbstractItemModel>
#include <QHash>
#include <QIcon>
#include <QString>
#include <QVector>
#include <memory>
#include <vector>

namespace dirtoo::app {

/// Lazy directory-only tree for the sidebar. Children are listed off the GUI
/// thread via QtConcurrent and applied with a generation guard.
class DirectoryTreeModel : public QAbstractItemModel {
  Q_OBJECT

public:
  explicit DirectoryTreeModel(QObject* parent = nullptr);
  ~DirectoryTreeModel() override;

  void set_show_hidden(bool show);
  [[nodiscard]] bool show_hidden() const { return show_hidden_; }

  /// Rebuild top-level places (Home, /, optional extra roots).
  void reset_roots(const QStringList& root_paths, const QStringList& root_labels);

  [[nodiscard]] QString path_for_index(const QModelIndex& index) const;
  [[nodiscard]] QModelIndex index_for_path(const QString& path) const;

  // QAbstractItemModel
  [[nodiscard]] QModelIndex index(int row, int column,
                                  const QModelIndex& parent = {}) const override;
  [[nodiscard]] QModelIndex parent(const QModelIndex& index) const override;
  [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
  [[nodiscard]] int columnCount(const QModelIndex& parent = {}) const override;
  [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;
  [[nodiscard]] bool hasChildren(const QModelIndex& parent = {}) const override;
  [[nodiscard]] bool canFetchMore(const QModelIndex& parent) const override;
  void fetchMore(const QModelIndex& parent) override;

private:
  struct Node {
    QString path;
    QString display;
    Node* parent = nullptr;
    QVector<Node*> children;
    bool loaded = false;
    bool loading = false;
    bool is_dir = true;
    std::uint64_t fetch_generation = 0;
  };

  Node* node_from_index(const QModelIndex& index) const;
  QModelIndex index_from_node(Node* node, int column = 0) const;
  void clear_tree();
  void apply_children(Node* parent, const QStringList& child_paths, std::uint64_t generation);

  Node root_; // invisible root
  bool show_hidden_ = false;
  QIcon folder_icon_;
  std::uint64_t next_fetch_generation_ = 1;
};

} // namespace dirtoo::app
