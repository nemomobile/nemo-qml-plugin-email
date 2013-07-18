/*
 * Copyright 2011 Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at 	
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#ifndef FOLDERLISTMODEL_H
#define FOLDERLISTMODEL_H

#include <qmailfolder.h>
#include <qmailaccount.h>

#include <QAbstractListModel>

class FolderListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit FolderListModel(QObject *parent = 0);
    ~FolderListModel();

    enum Role {
        FolderName = Qt::UserRole + 1,
        FolderId = Qt::UserRole + 2,
        FolderUnreadCount = Qt::UserRole + 3,
        FolderServerCount = Qt::UserRole + 4,
        Index = Qt::UserRole + 5,
        FolderNestingLevel = Qt::UserRole + 6
    };

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role) const;


    Q_INVOKABLE int folderId(int index);
    Q_INVOKABLE QStringList folderNames();
    Q_INVOKABLE int folderServerCount(int folderId);
    Q_INVOKABLE int folderUnreadCount(int folderId);
    Q_INVOKABLE int indexFromFolderId(int folderId);
    Q_INVOKABLE int numberOfFolders();
    Q_INVOKABLE void setAccountKey(int id);

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
protected:
    virtual QHash<int, QByteArray> roleNames() const;
#endif

private slots:
    void onFoldersChanged(const QMailFolderIdList &);

private:
    QHash<int, QByteArray> roles;
    QMailFolderIdList m_mailFolderIds;
    QMailAccountId m_accountId;

    void resetModel();
};

#endif
