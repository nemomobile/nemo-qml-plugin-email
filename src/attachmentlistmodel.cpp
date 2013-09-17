/*
 * Copyright (C) 2013 Jolla Ltd.
 * Contact: Valerio Valerio <valerio.valerio@jollamobile.com>
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#include <QVector>

#include "attachmentlistmodel.h"
#include "emailagent.h"

AttachmentListModel::AttachmentListModel(QObject *parent) :
    QAbstractListModel(parent)
  , m_messageId(QMailMessageId())
{
    roles.insert(ContentLocation, "contentLocation");
    roles.insert(DisplayName, "displayName");
    roles.insert(Downloaded, "downloaded");
    roles.insert(MimeType, "mimeType");
    roles.insert(Size, "size");
    roles.insert(StatusInfo, "statusInfo");
    roles.insert(ProgressInfo, "progressInfo");
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    setRoleNames(roles);
#endif

    connect(EmailAgent::instance(), SIGNAL(attachmentDownloadStatusChanged(QString,EmailAgent::AttachmentStatus)),
            this, SLOT(onAttachmentDownloadStatusChanged(QString,EmailAgent::AttachmentStatus)));

    connect(EmailAgent::instance(), SIGNAL(attachmentDownloadProgressChanged(QString,int)),
            this, SLOT(onAttachmentDownloadProgressChanged(QString,int)));
}

AttachmentListModel::~AttachmentListModel()
{
}

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
QHash<int, QByteArray> AttachmentListModel::roleNames() const
{
    return roles;
}
#endif

int AttachmentListModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return m_attachmentsList.count();
}

QVariant AttachmentListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() > m_attachmentsList.count())
        return QVariant();

    const Attachment *item = static_cast<const Attachment *>(index.internalPointer());
    Q_ASSERT(item);

    if (role == ContentLocation) {
        return item->location;
    } else if (role == DisplayName) {
        return EmailAgent::instance()->attachmentName(item->part);
    } else if (role == Downloaded) {
        if (item->status == EmailAgent::Downloaded) {
            return true;
        } else {
            //Addresses the case where content size is missing
            return item->part.contentAvailable() || item->part.contentDisposition().size() <= 0;
        }
    } else if (role == MimeType) {
        return QString::fromLatin1(item->part.contentType().content());
    } else if (role == Size) {
        if (item->part.contentDisposition().size() != -1) {
            return item->part.contentDisposition().size();
        }
        // If size is -1 (unknown) try finding out part's body size
        if (item->part.contentAvailable()) {
            return item->part.hasBody() ? item->part.body().length() : 0;
        }
        return -1;
    } else if (role == StatusInfo) {
        return item->status;
    } else if (role == ProgressInfo) {
        return item->progressInfo;
    }
    return QVariant();
}

QModelIndex AttachmentListModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED (column);
    Q_UNUSED (parent);

    if (-1 < row && row < m_attachmentsList.count()) {
        return m_attachmentsList[row]->index;
    }

    qWarning() << Q_FUNC_INFO << "Row " << row << "is not present in the model";
    return QModelIndex();
}

QModelIndex AttachmentListModel::indexFromLocation(const QString &location)
{
    foreach (const Attachment *item, m_attachmentsList) {
        if (item->location == location) {
            return item->index;
        }
    }
    return QModelIndex();
}

void AttachmentListModel::onAttachmentDownloadStatusChanged(const QString &attachmentLocation, EmailAgent::AttachmentStatus status)
{
    for (int i=0; i< m_attachmentsList.size();i++) {
        if (m_attachmentsList[i]->location == attachmentLocation) {
            m_attachmentsList[i]->status = status;
            emit dataChanged(m_attachmentsList[i]->index, m_attachmentsList[i]->index, QVector<int>() << StatusInfo);
        }
    }
}

void AttachmentListModel::onAttachmentDownloadProgressChanged(const QString &attachmentLocation, int progress)
{
    for (int i=0; i< m_attachmentsList.size();i++) {
        if (m_attachmentsList[i]->location == attachmentLocation) {
            m_attachmentsList[i]->progressInfo = progress;
            emit dataChanged(m_attachmentsList[i]->index, m_attachmentsList[i]->index, QVector<int>() << ProgressInfo);
        }
    }
}

int AttachmentListModel::messageId() const
{
    return m_messageId.toULongLong();
}

void AttachmentListModel::setMessageId(int id)
{
    m_messageId = QMailMessageId(id);
    m_message = QMailMessage(m_messageId);
    resetModel();
}

void AttachmentListModel::resetModel()
{
    beginResetModel();
    m_attachmentsList.clear();
    if (m_messageId.isValid()) {
        int i=0;
        foreach (const QMailMessagePart::Location &location,  m_message.findAttachmentLocations()) {
            Attachment *item = new Attachment;
            item->location = location.toString(true);
            item->part = m_message.partAt(location);
            item->status = EmailAgent::instance()->attachmentDownloadStatus(item->location);
            item->progressInfo = EmailAgent::instance()->attachmentDownloadProgress(item->location);
            item->index = createIndex(i, 0, item);
            m_attachmentsList.append(item);
            i++;
        }
    }
    endResetModel();
}
