/*
 * Copyright 2011 Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at 	
 * http://www.apache.org/licenses/LICENSE-2.0
 */


#include <QDateTime>
#include <QTimer>
#include <QProcess>

#include <qmailnamespace.h>
#include <qmailaccount.h>
#include <qmailfolder.h>
#include <qmailmessage.h>
#include <qmailmessagekey.h>
#include <qmailstore.h>

#include "folderlistmodel.h"


FolderListModel::FolderListModel(QObject *parent) :
    QAbstractListModel(parent)
{
    roles.insert(FolderName, "folderName");
    roles.insert(FolderId, "folderId");
    roles.insert(FolderUnreadCount, "folderUnreadCount");
    roles.insert(FolderServerCount, "folderServerCount");
    roles.insert(FolderNestingLevel, "folderNestingLevel");
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    setRoleNames(roles);
#endif
}

FolderListModel::~FolderListModel()
{
}

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
QHash<int, QByteArray> FolderListModel::roleNames() const
{
    return roles;
}
#endif

int FolderListModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return m_mailFolderIds.count();
}

QVariant FolderListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() > m_mailFolderIds.count())
        return QVariant();

    
    QMailFolder folder(m_mailFolderIds[index.row()]);
    if (role == FolderName) {
        return folder.displayName();
    }
    else if (role == FolderId) {
        return m_mailFolderIds[index.row()].toULongLong();
    } 
    else if (role == FolderUnreadCount) {
        QMailMessageKey parentFolderKey(QMailMessageKey::parentFolderId(m_mailFolderIds[index.row()]));
        QMailMessageKey unreadKey(QMailMessageKey::status(QMailMessage::Read, QMailDataComparator::Excludes));
        return (QMailStore::instance()->countMessages(parentFolderKey & unreadKey));
    }
    else if (role == FolderServerCount) {
        return (folder.serverCount());
    }
    else if (role == FolderNestingLevel) {
        QMailFolder tempFolder = folder;
        int level = 0;
        while (tempFolder.parentFolderId().isValid()) {
            tempFolder = QMailFolder(tempFolder.parentFolderId());
            level++;
        }
        return level;
    }

    return QVariant();
}

int FolderListModel::folderId(int index)
{
    if (index < 0 || index >= m_mailFolderIds.count())
        return -1;

    return m_mailFolderIds[index].toULongLong();
}

QStringList FolderListModel::folderNames()
{
    QStringList folderNames;
    foreach (QMailFolderId fId, m_mailFolderIds) {
        QMailFolder folder(fId);
        QMailMessageKey parentFolderKey(QMailMessageKey::parentFolderId(fId));
        QMailMessageKey unreadKey(QMailMessageKey::status(QMailMessage::Read, QMailDataComparator::Excludes));
        int numberOfMessages = QMailStore::instance()->countMessages(parentFolderKey & unreadKey);
        QString displayName = folder.displayName();
        if (numberOfMessages > 0) {
            displayName = displayName + " (" + QString::number(numberOfMessages) + ")";
        }
        folderNames << displayName;
    }
    return folderNames;
}

int FolderListModel::folderServerCount(int vFolderId)
{
    QMailFolderId folderId(vFolderId);
    if (!folderId.isValid())
        return 0;

    QMailFolder folder (folderId);
    return (folder.serverCount());
}

int FolderListModel::folderUnreadCount(int folderId)
{
    int folderIndex = indexFromFolderId(folderId);

    if (folderIndex < 0)
        return 0;

    return data(index(folderIndex), FolderUnreadCount).toInt();
}

int FolderListModel::indexFromFolderId(int vFolderId)
{
    QMailFolderId folderId(vFolderId);
    for (int i = 0; i < m_mailFolderIds.size(); i ++) {
        if (folderId == m_mailFolderIds[i])
            return i;
    }
    return -1;
}

int FolderListModel::numberOfFolders()
{
    return m_mailFolderIds.count();
}

void FolderListModel::setAccountKey(int id)
{
  // Get all the folders belonging to this email account
    beginResetModel();
    QMailAccountId accountId(id);
    QMailFolderKey key = QMailFolderKey::parentAccountId(accountId);
    m_mailFolderIds = QMailStore::instance()->queryFolders(key);
    endResetModel();
}
