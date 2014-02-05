/*
 * Copyright (C) 2013 Jolla Ltd.
 * Contact: Valerio Valerio <valerio.valerio@jollamobile.com>
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#ifndef EMAILATTACHMENTLISTMODEL_H
#define EMAILATTACHMENTLISTMODEL_H

#include <QAbstractListModel>
#include <qmailmessage.h>
#include "emailagent.h"

class Q_DECL_EXPORT AttachmentListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit AttachmentListModel(QObject *parent = 0);
    ~AttachmentListModel();

    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int messageId READ messageId WRITE setMessageId NOTIFY messageIdChanged FINAL)

    enum Role {
        ContentLocation = Qt::UserRole + 1,
        DisplayName = Qt::UserRole + 2,
        Downloaded = Qt::UserRole + 3,
        MimeType = Qt::UserRole + 4,
        Size = Qt::UserRole + 5,
        StatusInfo = Qt::UserRole + 6,
        Url = Qt::UserRole + 7,
        ProgressInfo = Qt::UserRole + 8,
        Index = Qt::UserRole + 9
    };

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role) const;
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    QModelIndex indexFromLocation(const QString &location);
    QString attachmentUrl(const QMailMessage message, const QString &attachmentLocation);

    Q_INVOKABLE QString displayName(int idx);
    Q_INVOKABLE bool downloadStatus(int idx);
    Q_INVOKABLE QString mimeType(int idx);
    Q_INVOKABLE QString url(int idx);

    int count() const;
    int messageId() const;
    void setMessageId(int id);

protected:
    virtual QHash<int, QByteArray> roleNames() const;

signals:
    void countChanged();
    void messageIdChanged();

private slots:
    void onAttachmentDownloadStatusChanged(const QString &attachmentLocation, EmailAgent::AttachmentStatus status);
    void onAttachmentDownloadProgressChanged(const QString &attachmentLocation, int progress);
    void onAttachmentUrlChanged(const QString &attachmentLocation, const QString &url);

private:
    QHash<int, QByteArray> roles;
    QMailMessageId m_messageId;
    QMailMessage m_message;
    struct Attachment {
        QModelIndex index;
        QMailMessagePart part;
        QString location;
        EmailAgent::AttachmentStatus status;
        QString url;
        int progressInfo;
    };

    QList<Attachment*> m_attachmentsList;

    void resetModel();

};
#endif // EMAILATTACHMENTLISTMODEL_H
