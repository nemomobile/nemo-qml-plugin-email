/*
 * Copyright (C) 2013 Jolla Ltd.
 * Contact: Valerio Valerio <valerio.valerio@jollamobile.com>
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#ifndef EMAILFOLDER_H
#define EMAILFOLDER_H

#include <QObject>
#include <qmailfolder.h>

class Q_DECL_EXPORT EmailFolder : public QObject
{
    Q_OBJECT
public:
    explicit EmailFolder(QObject *parent = 0);
     ~EmailFolder();

    Q_PROPERTY(QString displayName READ displayName WRITE setDisplayName NOTIFY displayNameChanged)
    Q_PROPERTY(int folderId READ folderId WRITE setFolderId NOTIFY folderIdChanged)
    Q_PROPERTY(int parentAccountId READ parentAccountId NOTIFY folderIdChanged)
    Q_PROPERTY(int parentFolderId READ parentFolderId NOTIFY folderIdChanged)
    Q_PROPERTY(QString path READ path NOTIFY folderIdChanged)
    Q_PROPERTY(int serverCount READ serverCount NOTIFY folderIdChanged)
    Q_PROPERTY(int serverUndiscoveredCount READ serverUndiscoveredCount NOTIFY folderIdChanged)
    Q_PROPERTY(int serverUnreadCount READ serverUnreadCount NOTIFY folderIdChanged)

    QString displayName() const;
    int folderId() const;
    int parentAccountId() const;
    int parentFolderId() const;
    QString path() const;
    int serverCount() const;
    int serverUndiscoveredCount() const;
    int serverUnreadCount() const;
    void setDisplayName(const QString &displayName);
    void setFolderId(int folderId);

signals:
    void displayNameChanged();
    void folderIdChanged();

private slots:
    void onFoldersUpdated(const QMailFolderIdList &);
    
private:
    QMailFolder m_folder;
};

#endif // EMAILFOLDER_H
