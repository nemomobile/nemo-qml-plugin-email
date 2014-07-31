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

class Q_DECL_EXPORT FolderListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_ENUMS(FolderStandardType)
    Q_PROPERTY(quint64 currentFolderIdx READ currentFolderIdx WRITE setCurrentFolderIdx NOTIFY currentFolderIdxChanged FINAL)
    Q_PROPERTY(int currentFolderUnreadCount READ currentFolderUnreadCount NOTIFY currentFolderUnreadCountChanged FINAL)

public:
    explicit FolderListModel(QObject *parent = 0);
    ~FolderListModel();

    enum Role {
        FolderName = Qt::UserRole + 1,
        FolderId = Qt::UserRole + 2,
        FolderUnreadCount = Qt::UserRole + 3,
        FolderServerCount = Qt::UserRole + 4,
        FolderNestingLevel = Qt::UserRole + 5,
        FolderMessageKey = Qt::UserRole + 6,
        FolderType = Qt::UserRole + 7,
        Index = Qt::UserRole + 8
    };

    enum FolderStandardType {
        NormalFolder = 0,
        InboxFolder,
        OutboxFolder,
        SentFolder,
        DraftsFolder,
        TrashFolder,
        JunkFolder
    };

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role) const;

    int currentFolderIdx() const;
    void setCurrentFolderIdx(int folderIdx);

    int currentFolderUnreadCount() const;

    Q_INVOKABLE int folderId(int idx);
    Q_INVOKABLE QVariant folderMessageKey(int idx);
    Q_INVOKABLE QString folderName(int idx);
    Q_INVOKABLE QVariant folderType(int idx);
    Q_INVOKABLE int folderUnreadCount(int idx);
    Q_INVOKABLE int folderServerCount(int folderId);
    Q_INVOKABLE int indexFromFolderId(int folderId);
    Q_INVOKABLE bool isOutgoingFolder(int idx);
    Q_INVOKABLE int numberOfFolders();
    Q_INVOKABLE void setAccountKey(int id);
    Q_INVOKABLE int standardFolderIndex(FolderStandardType folderType);

signals:
    void currentFolderIdxChanged();
    void currentFolderUnreadCountChanged();

protected:
    virtual QHash<int, QByteArray> roleNames() const;

private slots:
    void onFoldersChanged(const QMailFolderIdList &ids);
    void updateUnreadCount(const QMailFolderIdList &folderIds);

private:
    int folderUnreadCount(const QMailFolderId &folderId, FolderStandardType folderType, QMailMessageKey folderMessageKey) const;

private:
    struct FolderItem {
        QModelIndex index;
        QMailFolderId folderId;
        FolderStandardType folderType;
        QMailMessageKey messageKey;
        int unreadCount;

        FolderItem(QModelIndex idx, QMailFolderId mailFolderId,
                   FolderStandardType mailFolderType, QMailMessageKey folderMessageKey, int folderUnreadCount) :
            index(idx), folderId(mailFolderId), folderType(mailFolderType), messageKey(folderMessageKey),
            unreadCount(folderUnreadCount) {}
    };

    int m_currentFolderIdx;
    int m_currentFolderUnreadCount;
    FolderStandardType m_currentFolderType;
    QMailFolderId m_currentFolderId;
    QHash<int, QByteArray> roles;
    QMailAccountId m_accountId;
    QList<FolderItem*> m_folderList;

    FolderStandardType folderTypeFromId(const QMailFolderId &id) const;
    QString localFolderName(const FolderStandardType folderType) const;
    void updateCurrentFolderIndex();
    void resetModel();
};

#endif
