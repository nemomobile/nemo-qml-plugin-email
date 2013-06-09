/*
 * Copyright 2011 Intel Corporation.
 * Copyright (C) 2012 Jolla Ltd.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at 	
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#ifndef EMAILACCOUNTLISTMODEL_H
#define EMAILACCOUNTLISTMODEL_H

#include <QAbstractListModel>

#include <qmailaccountlistmodel.h>
#include <qmailaccount.h>

class EmailAccountListModel : public QMailAccountListModel
{
    Q_OBJECT

public:
    explicit EmailAccountListModel(QObject *parent = 0);
    ~EmailAccountListModel();

    enum Role {
        DisplayName = Qt::UserRole + 4,
        EmailAddress = Qt::UserRole + 5,
        MailServer = Qt::UserRole + 6,
        UnreadCount = Qt::UserRole + 7,
        MailAccountId  = Qt::UserRole + 8,
        LastSynchronized = Qt::UserRole + 9,
        StandardFoldersRetrieved = Qt::UserRole + 10,
        Index = Qt::UserRole + 11
    };

    virtual QVariant data(const QModelIndex &index, int role) const;
    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const;

public slots:
    Q_INVOKABLE QVariant accountId(int idx);
    Q_INVOKABLE QStringList allDisplayNames();
    Q_INVOKABLE QStringList allEmailAddresses();
    Q_INVOKABLE QVariant displayName(int idx);
    Q_INVOKABLE QString displayNameFromAccountId(QVariant accountId);
    Q_INVOKABLE QVariant emailAddress(int idx);
    Q_INVOKABLE QString emailAddressFromAccountId(QVariant accountId);
    Q_INVOKABLE int indexFromAccountId(QVariant id);
    Q_INVOKABLE QDateTime lastUpdatedAccountTime();
    Q_INVOKABLE int numberOfAccounts();
    Q_INVOKABLE QVariant standardFoldersRetrieved(int idx);

signals:
    void accountsAdded(QVariantList accountIds);
    void accountsRemoved(QVariantList accountIds);
    void accountsUpdated(QVariantList accountIds);
    void modelReset();

private slots:
    void onAccountsAdded(const QMailAccountIdList &);
    void onAccountsRemoved(const QMailAccountIdList &);
    void onAccountsUpdated(const QMailAccountIdList &);

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
protected:
    virtual QHash<int, QByteArray> roleNames() const;
#endif

private:
    QHash<int, QByteArray> roles;
};

#endif
