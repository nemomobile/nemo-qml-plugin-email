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
    roles.insert(Signature, "signature");
    roles.insert(IconPath, "iconPath");
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    setRoleNames(roles);
#endif
    connect(this, SIGNAL(rowsInserted(QModelIndex,int,int)),
            this,SLOT(onAccountsAdded(QModelIndex,int,int)));

    connect(this, SIGNAL(rowsRemoved(QModelIndex,int,int)),
            this,SLOT(onAccountsRemoved(QModelIndex,int,int)));
    connect(QMailStore::instance(), SIGNAL(accountContentsModified(const QMailAccountIdList&)),
            this, SLOT(onAccountContentsModified(const QMailAccountIdList&)));

    QMailAccountListModel::setSynchronizeEnabled(true);
    QMailAccountListModel::setKey(QMailAccountKey::status(QMailAccount::Enabled));
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
        return accountId.toULongLong();
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

    if (role == Signature) {
        return account.signature();
    }

    if (role == IconPath) {
        return account.iconPath();
    }

    return QVariant();
}

int EmailAccountListModel::rowCount(const QModelIndex &parent) const
{
    return QMailAccountListModel::rowCount(parent);
}

// ############ Slots ##############
void EmailAccountListModel::onAccountsAdded(const QModelIndex &parent, int start, int end)
{
    Q_UNUSED(parent);
    Q_UNUSED(start);
    Q_UNUSED(end);

    emit accountsAdded();
    emit numberOfAccountsChanged();
}

void EmailAccountListModel::onAccountsRemoved(const QModelIndex &parent, int start, int end)
{
    Q_UNUSED(parent);
    Q_UNUSED(start);
    Q_UNUSED(end);

    emit accountsRemoved();
    emit numberOfAccountsChanged();
}

void EmailAccountListModel::onAccountContentsModified(const QMailAccountIdList &ids)
{
    int count = numberOfAccounts();
    for (int i = 0; i < count; ++i) {
        QMailAccountId tmpAccountId(accountId(i));
        if (ids.contains(tmpAccountId)) {
            dataChanged(index(i), index(i), QVector<int>() << UnreadCount);
        }
    }
}

int EmailAccountListModel::numberOfAccounts() const
{
    return rowCount();
}

// ########### Invokable API ###################

int EmailAccountListModel::accountId(int idx)
{
    return data(index(idx), EmailAccountListModel::MailAccountId).toInt();
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

QString EmailAccountListModel::displayName(int idx)
{
    return data(index(idx), EmailAccountListModel::DisplayName).toString();
}

QString EmailAccountListModel::displayNameFromAccountId(int accountId)
{
    int accountIndex = indexFromAccountId(accountId);

    if (accountIndex < 0)
        return QString();

    return data(index(accountIndex), EmailAccountListModel::DisplayName).toString();
}


QString EmailAccountListModel::emailAddress(int idx)
{
    return data(index(idx), EmailAccountListModel::EmailAddress).toString();
}

QString EmailAccountListModel::emailAddressFromAccountId(int accountId)
{
    int accountIndex = indexFromAccountId(accountId);

    if (accountIndex < 0)
        return QString();

    return data(index(accountIndex), EmailAccountListModel::EmailAddress).toString();
}

int EmailAccountListModel::indexFromAccountId(int id)
{ 
    QMailAccountId accountId(id);
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

bool EmailAccountListModel::standardFoldersRetrieved(int idx)
{
    return data(index(idx), EmailAccountListModel::StandardFoldersRetrieved).toBool();
}
