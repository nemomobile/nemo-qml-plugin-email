/*
 * Copyright (C) 2013 Jolla Ltd.
 * Contact: Valerio Valerio <valerio.valerio@jollamobile.com>
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#include "emailfolder.h"
#include "emailagent.h"

#include <qmailstore.h>

EmailFolder::EmailFolder(QObject *parent) :
    QObject(parent)
  , m_folder(QMailFolder())
{
    connect(QMailStore::instance(), SIGNAL(foldersUpdated(const QMailFolderIdList &)), this,
                          SLOT(onFoldersUpdated(const QMailFolderIdList &)));
}

EmailFolder::~EmailFolder()
{
}

QString EmailFolder::displayName() const
{
    return m_folder.displayName();
}

int EmailFolder::folderId() const
{
    return m_folder.id().toULongLong();
}

int EmailFolder::parentAccountId() const
{
    return m_folder.parentAccountId().toULongLong();
}

int EmailFolder::parentFolderId() const
{
    return m_folder.parentFolderId().toULongLong();
}

QString EmailFolder::path() const
{
    return m_folder.path();
}

int EmailFolder::serverCount() const
{
    return m_folder.serverCount();
}

int EmailFolder::serverUndiscoveredCount() const
{
    return m_folder.serverUndiscoveredCount();
}

int EmailFolder::serverUnreadCount() const
{
    return m_folder.serverUnreadCount();
}

void EmailFolder::setDisplayName(const QString &displayName)
{
    m_folder.setDisplayName(displayName);
    emit displayNameChanged();
}

void EmailFolder::setFolderId(int folderId)
{
    QMailFolderId foldId(folderId);
    if (foldId != m_folder.id()) {
        if (foldId.isValid()) {
            m_folder = QMailFolder(foldId);
        } else {
            m_folder = QMailFolder();
            qCWarning(lcGeneral) << "Invalid folder id " << foldId.toULongLong();
        }

        // Folder loaded from the store (or a empty folder), all properties changes
        emit folderIdChanged();
    }
}

void EmailFolder::onFoldersUpdated(const QMailFolderIdList &ids)
{
    foreach (QMailFolderId folderId, ids) {
        if (folderId == m_folder.id()) {
            emit displayNameChanged();
            return;
        }
    }
}
