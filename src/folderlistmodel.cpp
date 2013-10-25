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
#include "emailagent.h"


FolderListModel::FolderListModel(QObject *parent) :
    QAbstractListModel(parent)
  , m_accountId(QMailAccountId())
{
    roles.insert(FolderName, "folderName");
    roles.insert(FolderId, "folderId");
    roles.insert(FolderUnreadCount, "folderUnreadCount");
    roles.insert(FolderServerCount, "folderServerCount");
    roles.insert(FolderNestingLevel, "folderNestingLevel");
    roles.insert(FolderMessageKey, "folderMessageKey");
    roles.insert(FolderType, "folderType");
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    setRoleNames(roles);
#endif

    connect(QMailStore::instance(), SIGNAL(foldersAdded(const QMailFolderIdList &)), this,
                          SLOT(onFoldersChanged(const QMailFolderIdList &)));
    connect(QMailStore::instance(), SIGNAL(foldersRemoved(const QMailFolderIdList &)), this,
                          SLOT(onFoldersChanged(const QMailFolderIdList &)));
    connect(QMailStore::instance(), SIGNAL(foldersUpdated(const QMailFolderIdList &)), this,
                          SLOT(onFoldersChanged(const QMailFolderIdList &)));
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
        QMailMessageKey parentFolderKey(QMailMessageKey::parentFolderId(item->folderId));
        QMailMessageKey unreadKey(QMailMessageKey::status(QMailMessage::Read, QMailDataComparator::Excludes));
        return (QMailStore::instance()->countMessages(parentFolderKey & unreadKey));
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

QModelIndex FolderListModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED (column);
    Q_UNUSED (parent);

    if (-1 < row && row < m_folderList.count()) {
        return m_folderList[row]->index;
    }

    qWarning() << Q_FUNC_INFO << "Row " << row << "is not present in the model";
    return QModelIndex();
}

void FolderListModel::onFoldersChanged(const QMailFolderIdList &ids)
{
    // Don't reload the model if folders are not from current account
    // folders list can be long in some cases.
    foreach (QMailFolderId folderId, ids) {
        QMailFolder folder(folderId);
        if (folder.parentAccountId() == m_accountId) {
            resetModel();
            return;
        }
    }
}

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

QStringList FolderListModel::folderNames()
{
    QStringList folderNames;
    foreach (const FolderItem *item, m_folderList) {
        QMailFolder folder(item->folderId);
        QMailMessageKey parentFolderKey(QMailMessageKey::parentFolderId(item->folderId));
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

QVariant FolderListModel::folderType(int idx)
{
    return data(index(idx,0), FolderType);
}

int FolderListModel::folderServerCount(int folderId)
{
    QMailFolderId mailFolderId(folderId);
    if (!mailFolderId.isValid())
        return 0;

    QMailFolder folder (mailFolderId);
    return (folder.serverCount());
}

int FolderListModel::folderUnreadCount(int folderId)
{
    int folderIndex = indexFromFolderId(folderId);

    if (folderIndex < 0)
        return 0;

    return data(index(folderIndex,0), FolderUnreadCount).toInt();
}

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
        resetModel();
    } else {
        qDebug() << "Can't create folder model for invalid account: " << id;
    }

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
        qWarning() << "Folder type not recognized.";
        return "Local Storage";
    }
    }
}

void FolderListModel::resetModel()
{
    beginResetModel();
    qDeleteAll(m_folderList.begin(), m_folderList.end());
    m_folderList.clear();
    QMailFolderKey key = QMailFolderKey::parentAccountId(m_accountId);
    QList<QMailFolderId> folders = QMailStore::instance()->queryFolders(key);
    int i=0;
    foreach (const QMailFolderId& folderId, folders) {
        FolderItem *item = new FolderItem(QModelIndex(), folderId, folderTypeFromId(folderId),
                                          QMailMessageKey());
        item->index = createIndex(i, 0, item);
        m_folderList.append(item);
        i++;
    }
    // Outbox
    FolderItem *item = new FolderItem(QModelIndex(), QMailFolder::LocalStorageFolderId, OutboxFolder,
                                      QMailMessageKey::status(QMailMessage::Outbox));
    item->index = createIndex(i, 0, item);
    m_folderList.append(item);
    i++;

    // Check for the standard folders, if they don't exist assign local ones

    int trashFolderId = EmailAgent::instance()->trashFolderId(m_accountId.toULongLong());
    if (trashFolderId <= 1) {
        qDebug() << "Creating local trash folder!";
        FolderItem *item = new FolderItem(QModelIndex(), QMailFolder::LocalStorageFolderId, TrashFolder,
                                          QMailMessageKey::status(QMailMessage::Trash));
        item->index = createIndex(i, 0, item);
        m_folderList.append(item);
        i++;
    }

    int sentFolderId = EmailAgent::instance()->sentFolderId(m_accountId.toULongLong());
    if (sentFolderId <= 1) {
        qDebug() << "Creating local sent folder!";
        FolderItem *item = new FolderItem(QModelIndex(), QMailFolder::LocalStorageFolderId, SentFolder,
                                          QMailMessageKey::status(QMailMessage::Sent) &
                                          ~QMailMessageKey::status(QMailMessage::Trash));
        item->index = createIndex(i, 0, item);
        m_folderList.append(item);
        i++;
    }

    int draftsFolderId = EmailAgent::instance()->draftsFolderId(m_accountId.toULongLong());
    if (draftsFolderId <= 1) {
        qDebug() << "Creating local drafts folder!";
        FolderItem *item = new FolderItem(QModelIndex(), QMailFolder::LocalStorageFolderId, DraftsFolder,
                                          QMailMessageKey::status(QMailMessage::Draft) &
                                          ~QMailMessageKey::status(QMailMessage::Outbox) &
                                          ~QMailMessageKey::status(QMailMessage::Trash));
        item->index = createIndex(i, 0, item);
        m_folderList.append(item);
        i++;
    }
    endResetModel();
}
