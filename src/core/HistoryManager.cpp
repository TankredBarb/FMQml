#include "HistoryManager.h"

HistoryManager::HistoryManager(QObject *parent)
    : QObject(parent)
{
}

bool HistoryManager::canUndo() const
{
    return !m_undoStack.isEmpty();
}

bool HistoryManager::canRedo() const
{
    return !m_redoStack.isEmpty();
}

void HistoryManager::recordAction(HistoryAction action)
{
    m_undoStack.push(action);
    if (m_undoStack.size() > MaxHistory) {
        m_undoStack.removeFirst();
    }
    m_redoStack.clear();
    emit canUndoChanged();
    emit canRedoChanged();
}

HistoryAction HistoryManager::takeUndo()
{
    if (m_undoStack.isEmpty()) return {};
    HistoryAction action = m_undoStack.pop();
    m_redoStack.push(action);
    if (m_redoStack.size() > MaxHistory) {
        m_redoStack.removeFirst();
    }
    emit canUndoChanged();
    emit canRedoChanged();
    return action;
}

HistoryAction HistoryManager::takeRedo()
{
    if (m_redoStack.isEmpty()) return {};
    HistoryAction action = m_redoStack.pop();
    m_undoStack.push(action);
    if (m_undoStack.size() > MaxHistory) {
        m_undoStack.removeFirst();
    }
    emit canUndoChanged();
    emit canRedoChanged();
    return action;
}

void HistoryManager::clear()
{
    m_undoStack.clear();
    m_redoStack.clear();
    emit canUndoChanged();
    emit canRedoChanged();
}
