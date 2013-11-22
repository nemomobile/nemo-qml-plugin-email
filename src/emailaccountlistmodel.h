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

class Q_DECL_EXPORT EmailAccountListModel : public QMailAccountListModel
{
    Q_OBJECT
    Q_PROPERTY(int numberOfAccounts READ numberOfAccounts NOTIFY numberOfAccountsChanged)

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
        Signature = Qt::UserRole + 11,
        IconPath = Qt::UserRole + 12,
        Index = Qt::UserRole + 13
    };

    virtual QVariant data(const QModelIndex &index, int role) const;
    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const;

public:
    int numberOfAccounts() const;

public slots:
    Q_INVOKABLE int accountId(int idx);
    Q_INVOKABLE QStringList allDisplayNames();
    Q_INVOKABLE QStringList allEmailAddresses();
    Q_INVOKABLE QString displayName(int idx);
    Q_INVOKABLE QString displayNameFromAccountId(int accountId);
    Q_INVOKABLE QString emailAddress(int idx);
    Q_INVOKABLE QString emailAddressFromAccountId(int accountId);
    Q_INVOKABLE int indexFromAccountId(int accountId);
    Q_INVOKABLE QDateTime lastUpdatedAccountTime();
    Q_INVOKABLE bool standardFoldersRetrieved(int idx);

signals:
    void accountsAdded();
    void accountsRemoved();
    void accountsUpdated();
    void modelReset();
    void numberOfAccountsChanged();

private slots:
    void onAccountsAdded(const QModelIndex &parent, int start, int end);
    void onAccountsRemoved(const QModelIndex &parent, int start, int end);
    void onAccountContentsModified(const QMailAccountIdList& ids);

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
protected:
    virtual QHash<int, QByteArray> roleNames() const;
#endif

private:
    QHash<int, QByteArray> roles;
};

#endif
