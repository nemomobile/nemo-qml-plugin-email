/*
 * Copyright 2011 Intel Corporation.
 * Copyright (C) 2012-2013 Jolla Ltd.
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
#include "emailagent.h"


FolderListModel::FolderListModel(QObject *parent) :
    QAbstractListModel(parent)
  , m_currentFolderIdx(-1)
  , m_currentFolderUnreadCount(0)
  , m_currentFolderType(NormalFolder)
  , m_accountId(QMailAccountId())
{
    roles.insert(FolderName, "folderName");
    roles.insert(FolderId, "folderId");
    roles.insert(FolderUnreadCount, "folderUnreadCount");
    roles.insert(FolderServerCount, "folderServerCount");
    roles.insert(FolderNestingLevel, "folderNestingLevel");
    roles.insert(FolderMessageKey, "folderMessageKey");
    roles.insert(FolderType, "folderType");

    connect(QMailStore::instance(), SIGNAL(foldersAdded(const QMailFolderIdList &)), this,
                          SLOT(onFoldersChanged(const QMailFolderIdList &)));
    connect(QMailStore::instance(), SIGNAL(foldersRemoved(const QMailFolderIdList &)), this,
                          SLOT(onFoldersChanged(const QMailFolderIdList &)));
    connect(QMailStore::instance(), SIGNAL(folderContentsModified(const QMailFolderIdList&)), this,
                          SLOT(updateUnreadCount(const QMailFolderIdList&)));
}

FolderListModel::~FolderListModel()
{
}

QHash<int, QByteArray> FolderListModel::roleNames() const
{
    return roles;
}

int FolderListModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return m_folderList.count();
}

QVariant FolderListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() > m_folderList.count())
        return QVariant();

    const FolderItem *item = static_cast<const FolderItem *>(index.internalPointer());
    Q_ASSERT(item);
    
    QMailFolder folder(item->folderId);

    switch (role) {
    case FolderName:
    {
        if (item->folderId == QMailFolder::LocalStorageFolderId) {
            return localFolderName(item->folderType);
        } else {
            return folder.displayName();
        }
    }
    case FolderId:
        return item->folderId.toULongLong();
    case FolderUnreadCount:
    {
        return item->unreadCount;
    }
    case FolderServerCount:
        return (folder.serverCount());
    case FolderNestingLevel:
    {
        QMailFolder tempFolder = folder;
        int level = 0;
        while (tempFolder.parentFolderId().isValid()) {
            tempFolder = QMailFolder(tempFolder.parentFolderId());
            level++;
        }
        return level;
    }
    case FolderMessageKey:
        return item->messageKey;
    case FolderType:
        return item->folderType;
    default:
        return QVariant();
    }
}

int FolderListModel::currentFolderIdx() const
{
    return m_currentFolderIdx;
}

void FolderListModel::setCurrentFolderIdx(int folderIdx)
{
    if (folderIdx >= m_folderList.count()) {
        qCWarning(lcGeneral) << Q_FUNC_INFO << " Can't set Invalid Index: " << folderIdx;
    }

    if (folderIdx != m_currentFolderIdx) {
        m_currentFolderIdx = folderIdx;
        m_currentFolderType = static_cast<FolderListModel::FolderStandardType>(folderType(m_currentFolderIdx).toInt());
        m_currentFolderUnreadCount = folderUnreadCount(m_currentFolderIdx);
        m_currentFolderId = QMailFolderId(folderId(m_currentFolderIdx));
        emit currentFolderIdxChanged();
        emit currentFolderUnreadCountChanged();
    }
}

int FolderListModel::currentFolderUnreadCount() const
{
    return m_currentFolderUnreadCount;
}

QModelIndex FolderListModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED (column);
    Q_UNUSED (parent);

    if (-1 < row && row < m_folderList.count()) {
        return m_folderList[row]->index;
    }

    qCWarning(lcGeneral) << Q_FUNC_INFO << "Row " << row << "is not present in the model";
    return QModelIndex();
}

void FolderListModel::onFoldersChanged(const QMailFolderIdList &ids)
{
    // Don't reload the model if folders are not from current account or a local folder,
    // folders list can be long in some cases.
    foreach (QMailFolderId folderId, ids) {
        QMailFolder folder(folderId);
        if (folderId == QMailFolder::LocalStorageFolderId || folder.parentAccountId() == m_accountId) {
            resetModel();
            return;
        }
    }
}

void FolderListModel::updateUnreadCount(const QMailFolderIdList &folderIds)
{
    // Update unread count
    // all local folders in the model will be updated since they have same ID
    int count = rowCount();
    for (int i = 0; i < count; ++i) {
        QMailFolderId tmpFolderId(folderId(i));
        if (folderIds.contains(tmpFolderId)) {
            FolderItem *folderItem = m_folderList[i];
            if (folderItem->folderId == tmpFolderId) {
                folderItem->unreadCount = folderUnreadCount(folderItem->folderId, folderItem->folderType, folderItem->messageKey);
                dataChanged(index(i,0), index(i,0), QVector<int>() << FolderUnreadCount);
            } else {
                qCWarning(lcGeneral) << Q_FUNC_INFO << "Failed to update unread count for folderId " << tmpFolderId.toULongLong();
            }
        }
    }

    if (m_currentFolderId.isValid() && folderIds.contains(m_currentFolderId)) {
        if (m_currentFolderType == OutboxFolder || m_currentFolderType == DraftsFolder) {
            // read total number of messages again from database
            m_currentFolderUnreadCount = folderUnreadCount(m_currentFolderIdx);
            emit currentFolderUnreadCountChanged();
        } else if (m_currentFolderType == SentFolder) {
            m_currentFolderUnreadCount = 0;
            return;
        } else {
            int tmpUnreadCount = folderUnreadCount(m_currentFolderIdx);
            if (tmpUnreadCount != m_currentFolderUnreadCount) {
                m_currentFolderUnreadCount = tmpUnreadCount;
                emit currentFolderUnreadCountChanged();
            }
        }
    }
}

int FolderListModel::folderUnreadCount(const QMailFolderId &folderId, FolderStandardType folderType,
                                       QMailMessageKey folderMessageKey) const
{
    switch (folderType) {
    case InboxFolder:
    case NormalFolder:
    {
        // report actual unread count
        QMailMessageKey parentFolderKey(QMailMessageKey::parentFolderId(folderId));
        QMailMessageKey unreadKey(QMailMessageKey::status(QMailMessage::Read, QMailDataComparator::Excludes));
        return QMailStore::instance()->countMessages(parentFolderKey & unreadKey);
    }
    case TrashFolder:
    case JunkFolder:
    {
        // report actual unread count
        QMailMessageKey accountKey;
        // Local folders can have messages from several accounts.
        if (folderId == QMailFolder::LocalStorageFolderId) {
            accountKey = QMailMessageKey::parentAccountId(m_accountId);
        }
        QMailMessageKey parentFolderKey = accountKey & QMailMessageKey::parentFolderId(folderId);
        QMailMessageKey unreadKey = folderMessageKey & QMailMessageKey::status(QMailMessage::Read, QMailDataComparator::Excludes);
        return QMailStore::instance()->countMessages(parentFolderKey & unreadKey);
    }
    case OutboxFolder:
    case DraftsFolder:
    {
        // report all mails count, read and unread
        QMailMessageKey accountKey;
        // Local folders can have messages from several accounts.
        if (folderId == QMailFolder::LocalStorageFolderId) {
            accountKey = QMailMessageKey::parentAccountId(m_accountId);
        }
        QMailMessageKey parentFolderKey = accountKey & QMailMessageKey::parentFolderId(folderId);
        return QMailStore::instance()->countMessages(parentFolderKey & folderMessageKey);
    }
    case SentFolder:
        return 0;
    default:
    {
        qCWarning(lcGeneral) << "Folder type not recognized.";
        return 0;
    }
    }
}

// Note that local folders all have same id (QMailFolder::LocalStorageFolderId)
int FolderListModel::folderId(int idx)
{
    return data(index(idx,0), FolderId).toInt();
}

QVariant FolderListModel::folderMessageKey(int idx)
{
    return data(index(idx,0), FolderMessageKey);
}

QString FolderListModel::folderName(int idx)
{
    return data(index(idx,0), FolderName).toString();
}

QVariant FolderListModel::folderType(int idx)
{
    return data(index(idx,0), FolderType);
}

int FolderListModel::folderUnreadCount(int idx)
{
    return data(index(idx,0), FolderUnreadCount).toInt();
}

// Local folders will return always zero
int FolderListModel::folderServerCount(int folderId)
{
    QMailFolderId mailFolderId(folderId);
    if (!mailFolderId.isValid() || mailFolderId == QMailFolder::LocalStorageFolderId)
        return 0;

    QMailFolder folder (mailFolderId);
    return (folder.serverCount());
}

// For local folder first index found will be returned,
// since folderId is always the same (QMailFolder::LocalStorageFolderId)
int FolderListModel::indexFromFolderId(int folderId)
{
    QMailFolderId mailFolderId(folderId);
    foreach (const FolderItem *item, m_folderList) {
        if (item->folderId == mailFolderId) {
            return item->index.row();
        }
    }
    return -1;
}

// Returns true for sent, outbox and draft folders
bool FolderListModel::isOutgoingFolder(int idx)
{
    FolderStandardType folderStdType = static_cast<FolderListModel::FolderStandardType>(folderType(idx).toInt());
    if (folderStdType == SentFolder || folderStdType == DraftsFolder || folderStdType == OutboxFolder) {
        return true;
    }
    return false;
}

int FolderListModel::numberOfFolders()
{
    return m_folderList.count();
}

void FolderListModel::setAccountKey(int id)
{
  // Get all the folders belonging to this email account
    QMailAccountId accountId(id);
    if (accountId.isValid()) {
        m_accountId = accountId;
        m_currentFolderId = QMailFolderId();
        m_currentFolderIdx = -1;
        m_currentFolderUnreadCount = 0;
        resetModel();
    } else {
        qCDebug(lcGeneral) << "Can't create folder model for invalid account: " << id;
    }

}

int FolderListModel::standardFolderIndex(FolderStandardType folderType)
{
    foreach (const FolderItem *item, m_folderList) {
        if (item->folderType == folderType) {
            return item->index.row();
        }
    }
    return -1;
}

FolderListModel::FolderStandardType FolderListModel::folderTypeFromId(const QMailFolderId &id) const
{
    QMailFolder folder(id);
    if (!folder.parentAccountId().isValid() || id == QMailFolder::LocalStorageFolderId) {
        // Local folder
        return NormalFolder;
    }
    QMailAccount account(folder.parentAccountId());

    if (account.standardFolders().values().contains(id)) {
        QMailFolder::StandardFolder standardFolder = account.standardFolders().key(id);
        switch (standardFolder) {
        case QMailFolder::InboxFolder:
            return InboxFolder;
        case QMailFolder::OutboxFolder:
            return OutboxFolder;
        case QMailFolder::DraftsFolder:
            return DraftsFolder;
        case QMailFolder::SentFolder:
            return SentFolder;
        case QMailFolder::TrashFolder:
            return TrashFolder;
        case QMailFolder::JunkFolder:
            return JunkFolder;
        default:
            return NormalFolder;
        }
    }
    return NormalFolder;
}

QString FolderListModel::localFolderName(const FolderStandardType folderType) const
{
    switch (folderType) {
    case InboxFolder:
        return "Inbox";
    case OutboxFolder:
        return "Outbox";
    case DraftsFolder:
        return "Drafts";
    case SentFolder:
        return "Sent";
    case TrashFolder:
        return "Trash";
    case JunkFolder:
        return "Junk";
    default:
    {
        qCWarning(lcGeneral) << "Folder type not recognized.";
        return "Local Storage";
    }
    }
}

void FolderListModel::updateCurrentFolderIndex()
{
    foreach (const FolderItem *item, m_folderList) {
        if (item->folderId == m_currentFolderId && item->folderType == m_currentFolderType) {
            int index = item->index.row();
            if (index != m_currentFolderIdx) {
                setCurrentFolderIdx(index);
            }
            return;
        }
    }
    qCWarning(lcGeneral) << "Current folder not found in the model: " << m_currentFolderId.toULongLong();
    setCurrentFolderIdx(0);
}

void FolderListModel::resetModel()
{
    beginResetModel();
    qDeleteAll(m_folderList.begin(), m_folderList.end());
    m_folderList.clear();
    QMailFolderKey key = QMailFolderKey::parentAccountId(m_accountId);
    QMailMessageKey excludeRemovedKey = QMailMessageKey::status(QMailMessage::Removed,  QMailDataComparator::Excludes);
    QList<QMailFolderId> folders = QMailStore::instance()->queryFolders(key);
    int i=0;
    foreach (const QMailFolderId& folderId, folders) {
        FolderStandardType folderType = folderTypeFromId(folderId);
        QMailMessageKey messageKey(excludeRemovedKey);
        if (folderType != TrashFolder) {
            messageKey &= QMailMessageKey::status(QMailMessage::Trash, QMailDataComparator::Excludes);
        }
        FolderItem *item = new FolderItem(QModelIndex(), folderId, folderType, messageKey, 0);
        item->index = createIndex(i, 0, item);
        item->unreadCount = folderUnreadCount(item->folderId, item->folderType, item->messageKey);
        m_folderList.append(item);
        i++;
    }
    // Outbox
    FolderItem *item = new FolderItem(QModelIndex(), QMailFolder::LocalStorageFolderId, OutboxFolder,
                                      QMailMessageKey::status(QMailMessage::Outbox) &
                                      ~QMailMessageKey::status(QMailMessage::Trash) &
                                      excludeRemovedKey , 0);
    item->index = createIndex(i, 0, item);
    item->unreadCount = folderUnreadCount(item->folderId, item->folderType, item->messageKey);
    m_folderList.append(item);
    i++;

    // Check for the standard folders, if they don't exist assign local ones

    int trashFolderId = EmailAgent::instance()->trashFolderId(m_accountId.toULongLong());
    if (trashFolderId <= 1) {
        qCDebug(lcDebug) << "Creating local trash folder!";
        FolderItem *item = new FolderItem(QModelIndex(), QMailFolder::LocalStorageFolderId, TrashFolder,
                                          QMailMessageKey::status(QMailMessage::Trash) &
                                          excludeRemovedKey, 0);
        item->index = createIndex(i, 0, item);
        item->unreadCount = folderUnreadCount(item->folderId, item->folderType, item->messageKey);
        m_folderList.append(item);
        i++;
    }

    int sentFolderId = EmailAgent::instance()->sentFolderId(m_accountId.toULongLong());
    if (sentFolderId <= 1) {
        qCDebug(lcDebug) << "Creating local sent folder!";
        FolderItem *item = new FolderItem(QModelIndex(), QMailFolder::LocalStorageFolderId, SentFolder,
                                          QMailMessageKey::status(QMailMessage::Sent) &
                                          ~QMailMessageKey::status(QMailMessage::Trash) &
                                          excludeRemovedKey, 0);
        item->index = createIndex(i, 0, item);
        // Sent folder unread count is always zero
        m_folderList.append(item);
        i++;
    }

    int draftsFolderId = EmailAgent::instance()->draftsFolderId(m_accountId.toULongLong());
    if (draftsFolderId <= 1) {
        qCDebug(lcDebug) << "Creating local drafts folder!";
        FolderItem *item = new FolderItem(QModelIndex(), QMailFolder::LocalStorageFolderId, DraftsFolder,
                                          QMailMessageKey::status(QMailMessage::Draft) &
                                          ~QMailMessageKey::status(QMailMessage::Outbox) &
                                          ~QMailMessageKey::status(QMailMessage::Trash) &
                                          excludeRemovedKey, 0);
        item->index = createIndex(i, 0, item);
        item->unreadCount = folderUnreadCount(item->folderId, item->folderType, item->messageKey);
        m_folderList.append(item);
        i++;
    }
    endResetModel();

    if (m_currentFolderId.isValid()) {
        updateCurrentFolderIndex();
    }
}
