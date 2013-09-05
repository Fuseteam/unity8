/*
 * Copyright 2013 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *      Michael Zanetti <michael.zanetti@canonical.com>
 */

#include "launchermodel.h"
#include "launcheritem.h"
#include "backend/launcherbackend.h"

LauncherModel::LauncherModel(QObject *parent):
    LauncherModelInterface(parent),
    m_backend(new LauncherBackend(true, this))
{
    connect(m_backend, SIGNAL(countChanged(QString,int)), SLOT(countChanged(QString,int)));
    connect(m_backend, SIGNAL(progressChanged(QString,int)), SLOT(progressChanged(QString,int)));

    Q_FOREACH (const QString &entry, m_backend->storedApplications()) {
        LauncherItem *item = new LauncherItem(entry,
                                              m_backend->desktopFile(entry),
                                              m_backend->displayName(entry),
                                              m_backend->icon(entry),
                                              this);
        if (m_backend->isPinned(entry)) {
            item->setPinned(true);
        } else {
            item->setRecent(true);
        }
        m_list.append(item);
    }
}

LauncherModel::~LauncherModel()
{
    while (!m_list.empty()) {
        m_list.takeFirst()->deleteLater();
    }
}

int LauncherModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_list.count();
}

QVariant LauncherModel::data(const QModelIndex &index, int role) const
{
    LauncherItem *item = m_list.at(index.row());
    switch(role) {
        case RoleAppId:
            return item->desktopFile();
        case RoleDesktopFile:
            return item->desktopFile();
        case RoleName:
            return item->name();
        case RoleIcon:
            return item->icon();
        case RolePinned:
            return item->pinned();
        case RoleCount:
            return item->count();
        case RoleProgress:
            return item->progress();
        case RoleFocused:
            return item->focused();
    }

    return QVariant();
}

unity::shell::launcher::LauncherItemInterface *LauncherModel::get(int index) const
{
    if (index < 0 || index >= m_list.count()) {
        return 0;
    }
    return m_list.at(index);
}

void LauncherModel::move(int oldIndex, int newIndex)
{
    // Make sure its not moved outside the lists
    if (newIndex < 0) {
        newIndex = 0;
    }
    if (newIndex >= m_list.count()) {
        newIndex = m_list.count()-1;
    }

    // Nothing to do?
    if (oldIndex == newIndex) {
        return;
    }

    // QList's and QAbstractItemModel's move implementation differ when moving an item up the list :/
    // While QList needs the index in the resulting list, beginMoveRows expects it to be in the current list
    // adjust the model's index by +1 in case we're moving upwards
    int newModelIndex = newIndex > oldIndex ? newIndex+1 : newIndex;

    beginMoveRows(QModelIndex(), oldIndex, oldIndex, QModelIndex(), newModelIndex);
    m_list.move(oldIndex, newIndex);
    endMoveRows();

    storeAppList();

    pin(m_list.at(newIndex)->appId());
}

void LauncherModel::pin(const QString &appId, int index)
{
    int currentIndex = findApplication(appId);

    if (currentIndex >= 0) {
        if (index == -1 || index == currentIndex) {
            m_list.at(currentIndex)->setPinned(true);
            m_backend->setPinned(appId, true);
            QModelIndex modelIndex = this->index(currentIndex);
            Q_EMIT dataChanged(modelIndex, modelIndex);
        } else {
            move(currentIndex, index);
        }
    } else {
        if (index == -1) {
            index = m_list.count();
        }
        beginInsertRows(QModelIndex(), index, index);
        LauncherItem *item = new LauncherItem(appId,
                                              m_backend->desktopFile(appId),
                                              m_backend->displayName(appId),
                                              m_backend->icon(appId));
        item->setPinned(true);
        m_backend->setPinned(appId, true);
        m_list.insert(index, item);
        endInsertRows();
    }
}

void LauncherModel::requestRemove(const QString &appId)
{
    int index = findApplication(appId);
    if (index < 0) {
        return;
    }

    beginRemoveRows(QModelIndex(), index, index);
    m_list.takeAt(index)->deleteLater();
    endRemoveRows();

    storeAppList();
}

void LauncherModel::quickListActionInvoked(const QString &appId, int actionIndex)
{
    int index = findApplication(appId);
    if (index < 0) {
        return;
    }

    LauncherItem *item = m_list.at(index);
    QuickListModel *model = qobject_cast<QuickListModel*>(item->quickList());
    if (model) {
        QString actionId = model->get(actionIndex).actionId();

        // Check if this is one of the launcher actions we handle ourselves
        if (actionId == "pin_item") {
            if (item->pinned()) {
                requestRemove(appId);
            } else {
                pin(appId);
            }

        // Nope, we don't know this action, let the backend forward it to the application
        } else {
            m_backend->triggerQuickListAction(appId, actionId);
        }
    }
}

void LauncherModel::setUser(const QString &username)
{
    m_backend->setUser(username);
}

void LauncherModel::applicationFocused(const QString &appId)
{
    // Unfocus all apps
    for(int i = 0; i < m_list.count(); ++i) {
        LauncherItem *item = m_list.at(i);
        if (item->focused()) {
            item->setFocused(false);
            Q_EMIT dataChanged(this->index(i), this->index(i), QVector<int>() << RoleFocused);
        }
    }

    if (appId.isEmpty()) {
        return;
    }

    // FIXME: drop this once we get real appIds from the AppManager
    QString helper = appId.split('/').last();
    if (helper.endsWith(".desktop")) {
        helper.chop(8);
    }

    int index = findApplication(helper);
    if (index >= 0) {
        m_list.at(index)->setFocused(true);
        Q_EMIT dataChanged(this->index(index), this->index(index), QVector<int>() << RoleFocused);
    } else {
        // Add app to recent apps
        QString desktopFile = m_backend->desktopFile(helper);
        QString appName = m_backend->displayName(helper);
        QString icon = m_backend->icon(helper);

        LauncherItem *item = new LauncherItem(helper, desktopFile, appName, icon);
        item->setRecent(true);
        item->setFocused(true);
        beginInsertRows(QModelIndex(), m_list.count(), m_list.count());
        m_list.append(item);
        endInsertRows();

        // Clean up old recent apps
        QList<int> recentAppIndices;
        for (int i = 0; i < m_list.count(); ++i) {
            if (m_list.at(i)->recent()) {
                recentAppIndices << i;
            }
        }
        int run = 0;
        while (recentAppIndices.count() > 5) {
            beginRemoveRows(QModelIndex(), recentAppIndices.first() - run, recentAppIndices.first() - run);
            m_list.takeAt(recentAppIndices.first() - run)->deleteLater();
            endRemoveRows();
            recentAppIndices.takeFirst();
            ++run;
        }
        storeAppList();
    }
}

void LauncherModel::storeAppList()
{
    QStringList appIds;
    Q_FOREACH(LauncherItem *item, m_list) {
        if (item->pinned() || item->recent()) {
            appIds << item->appId();
        }
    }
    m_backend->setStoredApplications(appIds);
}

int LauncherModel::findApplication(const QString &appId)
{
    for (int i = 0; i < m_list.count(); ++i) {
        LauncherItem *item = m_list.at(i);
        if (item->appId() == appId) {
            return i;
        }
    }
    return -1;
}

void LauncherModel::progressChanged(const QString &appId, int progress)
{
    int idx = findApplication(appId);
    if (idx >= 0) {
        LauncherItem *item = m_list.at(idx);
        item->setProgress(progress);
        Q_EMIT dataChanged(index(idx), index(idx), QVector<int>() << RoleProgress);
    }
}


void LauncherModel::countChanged(const QString &appId, int count)
{
    int idx = findApplication(appId);
    if (idx >= 0) {
        LauncherItem *item = m_list.at(idx);
        item->setCount(count);
        Q_EMIT dataChanged(index(idx), index(idx), QVector<int>() << RoleCount);
    }
}
