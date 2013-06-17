/*
 * Copyright 2011 Intel Corporation.
 * Copyright (C) 2012 Jolla Ltd.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at 	
 * http://www.apache.org/licenses/LICENSE-2.0
 */


#include <qmailstore.h>
#include <qmailnamespace.h>

#include "emailaccountlistmodel.h"

EmailAccountListModel::EmailAccountListModel(QObject *parent) :
    QMailAccountListModel(parent)
{
    roles.insert(DisplayName, "displayName");
    roles.insert(EmailAddress, "emailAddress");
    roles.insert(MailServer, "mailServer");
    roles.insert(UnreadCount, "unreadCount");
    roles.insert(MailAccountId, "mailAccountId");
    roles.insert(LastSynchronized, "lastSynchronized");
    roles.insert(StandardFoldersRetrieved, "standardFoldersRetrieved");
    roles.insert(Preset, "preset");
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    setRoleNames(roles);
#endif

    connect (QMailStore::instance(), SIGNAL(accountsAdded(const QMailAccountIdList &)), this,
             SLOT(onAccountsAdded (const QMailAccountIdList &)));
    connect (QMailStore::instance(), SIGNAL(accountsRemoved(const QMailAccountIdList &)), this,
             SLOT(onAccountsRemoved(const QMailAccountIdList &)));
    connect (QMailStore::instance(), SIGNAL(accountsUpdated(const QMailAccountIdList &)), this,
             SLOT(onAccountsUpdated(const QMailAccountIdList &)));

    QMailAccountListModel::setSynchronizeEnabled(true);
    QMailAccountListModel::setKey(QMailAccountKey::messageType(QMailMessage::Email));
}

EmailAccountListModel::~EmailAccountListModel()
{
}

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
QHash<int, QByteArray> EmailAccountListModel::roleNames() const
{
    return roles;
}
#endif

QVariant EmailAccountListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (role == DisplayName) {
        return QMailAccountListModel::data(index, QMailAccountListModel::NameTextRole);
    }

    QMailAccountId accountId = QMailAccountListModel::idFromIndex(index);

    if (role == MailAccountId) {
        return accountId;
    }

    if (role == UnreadCount) {
        QMailFolderKey key = QMailFolderKey::parentAccountId(accountId);
        QMailFolderSortKey sortKey = QMailFolderSortKey::serverCount(Qt::DescendingOrder);
        QMailFolderIdList folderIds = QMailStore::instance()->queryFolders(key, sortKey);

        QMailMessageKey accountKey(QMailMessageKey::parentAccountId(accountId));
        QMailMessageKey folderKey(QMailMessageKey::parentFolderId(folderIds));
        QMailMessageKey unreadKey(QMailMessageKey::status(QMailMessage::Read, QMailDataComparator::Excludes));
        return (QMailStore::instance()->countMessages(accountKey & folderKey & unreadKey));
    }

    QMailAccount account(accountId);

    if (role == EmailAddress) {
        return account.fromAddress().address();
    }

    if (role == MailServer) {
        QString address = account.fromAddress().address();
        int index = address.indexOf("@");
        QString server = address.right(address.size() - index - 1);
        index = server.indexOf(".com", Qt::CaseInsensitive);
        return server.left(index);
    }

    if (role == LastSynchronized) {
        if (account.lastSynchronized().isValid()) {
            return account.lastSynchronized().toLocalTime();
        }
        else {
            //Account was never synced, return zero
            return 0;
        }
    }

    if (role == StandardFoldersRetrieved) {
        quint64 standardFoldersMask = QMailAccount::statusMask("StandardFoldersRetrieved");
        return account.status() & standardFoldersMask;
    }

    if (role == Preset) {
        return account.customField("preset").toInt();
    }

    return QVariant();
}

int EmailAccountListModel::rowCount(const QModelIndex &parent) const
{
    return QMailAccountListModel::rowCount(parent);
}
// ############ Slots ##############
void EmailAccountListModel::onAccountsAdded(const QMailAccountIdList &ids)
{
    QMailAccountListModel::beginResetModel();
    QMailAccountListModel::endResetModel();
    QVariantList accountIds;
    foreach (QMailAccountId accountId, ids) {
        accountIds.append(accountId.toULongLong());
    }
    emit accountsAdded(accountIds);
}

void EmailAccountListModel::onAccountsRemoved(const QMailAccountIdList &ids)
{
    QMailAccountListModel::beginResetModel();
    QMailAccountListModel::endResetModel();
    QVariantList accountIds;
    foreach (QMailAccountId accountId, ids) {
        accountIds.append(accountId.toULongLong());
    }
    emit accountsRemoved(accountIds);
}

void EmailAccountListModel::onAccountsUpdated(const QMailAccountIdList &ids)
{
    QMailAccountListModel::beginResetModel();
    QMailAccountListModel::endResetModel();
    QVariantList accountIds;
    foreach (QMailAccountId accountId, ids) {
        accountIds.append(accountId.toULongLong());
    }
    emit accountsUpdated(accountIds);
}

// ########### Invokable API ###################

QVariant EmailAccountListModel::accountId(int idx)
{
    return data(index(idx), EmailAccountListModel::MailAccountId);
}

QStringList EmailAccountListModel::allDisplayNames()
{
    QStringList displayNameList;
    for (int row = 0; row < rowCount(); row++) {
        QString displayName = data(index(row), EmailAccountListModel::DisplayName).toString();
        displayNameList << displayName;
    }
    return displayNameList;
}

QStringList EmailAccountListModel::allEmailAddresses()
{
    QStringList emailAddressList;
    for (int row = 0; row < rowCount(); row++) {
        QString emailAddress = data(index(row), EmailAccountListModel::EmailAddress).toString();
        emailAddressList << emailAddress;
    }
    return emailAddressList;
}

QVariant EmailAccountListModel::displayName(int idx)
{
    return data(index(idx), EmailAccountListModel::DisplayName);
}

QString EmailAccountListModel::displayNameFromAccountId(QVariant accountId)
{
    int accountIndex = indexFromAccountId(accountId);

    if (accountIndex < 0)
        return QString();

    return data(index(accountIndex), EmailAccountListModel::DisplayName).toString();
}


QVariant EmailAccountListModel::emailAddress(int idx)
{
    return data(index(idx), EmailAccountListModel::EmailAddress);
}

QString EmailAccountListModel::emailAddressFromAccountId(QVariant accountId)
{
    int accountIndex = indexFromAccountId(accountId);

    if (accountIndex < 0)
        return QString();

    return data(index(accountIndex), EmailAccountListModel::EmailAddress).toString();
}

int EmailAccountListModel::indexFromAccountId(QVariant id)
{ 
    QMailAccountId accountId = id.value<QMailAccountId>();
    if (!accountId.isValid())
        return -1;

    for (int row = 0; row < rowCount(); row++) {
        if (accountId == QMailAccountListModel::idFromIndex(index(row)))
            return row;
    }
    return -1;
}

QDateTime EmailAccountListModel::lastUpdatedAccountTime()
{
    QDateTime lastUpdatedAccTime;
    for (int row = 0; row < rowCount(); row++) {
        if ((data(index(row), EmailAccountListModel::LastSynchronized)).toDateTime() > lastUpdatedAccTime)
            lastUpdatedAccTime = (data(index(row), EmailAccountListModel::LastSynchronized)).toDateTime();
    }
    return lastUpdatedAccTime;
}

int EmailAccountListModel::numberOfAccounts()
{
    return rowCount();
}

QVariant EmailAccountListModel::standardFoldersRetrieved(int idx)
{
    return data(index(idx), EmailAccountListModel::StandardFoldersRetrieved);
}
