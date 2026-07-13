#pragma once

#include <QObject>
#include <QStringList>
#include <QStack>

struct HistoryAction {
    enum class Type {
        Move,
        Rename,
        Copy,
        Delete
    };

    Type type;
    QStringList sources;
    QString destination; // For Move/Copy/Rename, this is the resulting path(s) or folder
    QStringList originalPaths; // For Rename/Move, the source paths before operation
};

class HistoryManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(int undoCount READ undoCount NOTIFY canUndoChanged)
    Q_PROPERTY(int redoCount READ redoCount NOTIFY canRedoChanged)

public:
    explicit HistoryManager(QObject *parent = nullptr);

    bool canUndo() const;
    bool canRedo() const;
    int undoCount() const;
    int redoCount() const;

    void recordAction(HistoryAction action);
    
    HistoryAction takeUndo();
    HistoryAction takeRedo();

    void clear();

signals:
    void canUndoChanged();
    void canRedoChanged();

private:
    QStack<HistoryAction> m_undoStack;
    QStack<HistoryAction> m_redoStack;
    static constexpr int MaxHistory = 50;
};
