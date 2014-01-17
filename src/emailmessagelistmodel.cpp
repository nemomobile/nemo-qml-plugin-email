/*
 * Copyright 2011 Intel Corporation.
 * Copyright (C) 2012 Jolla Ltd.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at 	
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#include <QDateTime>
#include <QTimer>
#include <QProcess>

#include <qmailmessage.h>
#include <qmailmessagekey.h>
#include <qmailstore.h>
#include <qmailserviceaction.h>

#include <qmailnamespace.h>

#include <mlocale.h>

#include "emailagent.h"
#include "emailmessagelistmodel.h"

namespace {

Q_GLOBAL_STATIC(ML10N::MLocale, m_locale)

QString firstChar(const QString& str)
{
    QString group = m_locale()->indexBucket(str);
    return group.isNull() ? QString::fromLatin1("#") : group;
}

}

QString EmailMessageListModel::bodyHtmlText(const QMailMessage &mailMsg) const
{
    // TODO: This function assumes that at least the structure has been retrieved already
    if (const QMailMessagePartContainer *container = mailMsg.findHtmlContainer()) {
        if (!container->contentAvailable()) {
            // Retrieve the data for this part
            connect (m_retrievalAction, SIGNAL(activityChanged(QMailServiceAction::Activity)),
                                        this, SLOT(downloadActivityChanged(QMailServiceAction::Activity)));
            QMailMessagePart::Location location = static_cast<const QMailMessagePart *>(container)->location();
            m_retrievalAction->retrieveMessagePart(location);
            return " ";  // Put a space here as a place holder to notify UI that we do have html body.
        }
        return container->body().data();
    }

    return QString();
}

//![0]
EmailMessageListModel::EmailMessageListModel(QObject *parent)
    : QMailMessageListModel(parent),
      m_combinedInbox(false),
      m_filterUnread(true),
      m_selectedMessage(-1),
      m_selectedMessageIndex(-1),
      m_retrievalAction(new QMailRetrievalAction(this))
{
    roles[QMailMessageModelBase::MessageAddressTextRole] = "sender";
    roles[QMailMessageModelBase::MessageSubjectTextRole] = "subject";
    roles[QMailMessageModelBase::MessageFilterTextRole] = "messageFilter";
    roles[QMailMessageModelBase::MessageTimeStampTextRole] = "timeStamp";
    roles[QMailMessageModelBase::MessageSizeTextRole] = "size";
    roles[QMailMessageModelBase::MessageTypeIconRole] = "icon";
    roles[QMailMessageModelBase::MessageStatusIconRole] = "statusIcon";
    roles[QMailMessageModelBase::MessageDirectionIconRole] = "directionIcon";
    roles[QMailMessageModelBase::MessagePresenceIconRole] = "presenceIcon";
    roles[QMailMessageModelBase::MessageBodyTextRole] = "body";
    roles[MessageAttachmentCountRole] = "numberOfAttachments";
    roles[MessageAttachmentsRole] = "listOfAttachments";
    roles[MessageRecipientsRole] = "recipients";
    roles[MessageRecipientsDisplayNameRole] = "recipientsDisplayName";
    roles[MessageReadStatusRole] = "readStatus";
    roles[MessageHtmlBodyRole] = "htmlBody";
    roles[MessageQuotedBodyRole] = "quotedBody";
    roles[MessageIdRole] = "messageId";
    roles[MessageSenderDisplayNameRole] = "senderDisplayName";
    roles[MessageSenderEmailAddressRole] = "senderEmailAddress";
    roles[MessageToRole] = "to";
    roles[MessageCcRole] = "cc";
    roles[MessageBccRole] = "bcc";
    roles[MessageTimeStampRole] = "qDateTime";
    roles[MessageSelectModeRole] = "selected";
    roles[MessagePreviewRole] = "preview";
    roles[MessageTimeSectionRole] = "timeSection";
    roles[MessagePriorityRole] = "priority";
    roles[MessageAccountIdRole] = "accountId";
    roles[MessageHasAttachmentsRole] = "hasAttachments";
    roles[MessageSizeSectionRole] = "sizeSection";
    roles[MessageFolderIdRole] = "folderId";
    roles[MessageSortByRole] = "sortBy";
    roles[MessageSubjectFirstCharRole] = "subjectFirstChar";
    roles[MessageSenderFirstCharRole] = "senderFirstChar";

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    setRoleNames(roles);
#endif
    m_key = key();
    m_sortKey = QMailMessageSortKey::timeStamp(Qt::DescendingOrder);
    m_sortBy = Time;
    QMailMessageListModel::setSortKey(m_sortKey);
    m_selectedMsgIds.clear();

    connect(this, SIGNAL(rowsInserted(QModelIndex,int,int)),
            this,SLOT(indexesChanged(QModelIndex,int,int)));

    connect(this, SIGNAL(rowsRemoved(QModelIndex,int,int)),
            this,SLOT(indexesChanged(QModelIndex,int,int)));
}

EmailMessageListModel::~EmailMessageListModel()
{
    delete m_retrievalAction;
}

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
QHash<int, QByteArray> EmailMessageListModel::roleNames() const
{
    return roles;
}
#endif

int EmailMessageListModel::rowCount(const QModelIndex & parent) const {
    return QMailMessageListModel::rowCount(parent);
}

QVariant EmailMessageListModel::data(const QModelIndex & index, int role) const {
    if (!index.isValid() || index.row() > rowCount(parent(index))) {
        qWarning() << Q_FUNC_INFO << "Invalid Index";
        return QVariant();
    }

    if (role == MessageSortByRole) {
        return m_sortBy;
    }

    QMailMessageId msgId = idFromIndex(index);

    if (role == QMailMessageModelBase::MessageBodyTextRole) {
        QMailMessage message (msgId);
        return EmailAgent::instance()->bodyPlainText(message);
    }
    else if (role == MessageHtmlBodyRole) {
        QMailMessage message (msgId);
        return bodyHtmlText(message);
    }
    else if (role == MessageQuotedBodyRole) {
        QMailMessage message (msgId);
        QString body = EmailAgent::instance()->bodyPlainText(message);
        body.prepend('\n');
        body.replace('\n', "\n>");
        body.truncate(body.size() - 1);  // remove the extra ">" put there by QString.replace
        return body;
    }
    else if (role == MessageIdRole) {
        return msgId.toULongLong();
    }
    else if (role == MessageToRole) {
        QMailMessage message (msgId);
        return QMailAddress::toStringList(message.to());
    }
    else if (role == MessageCcRole) {
        QMailMessage message (msgId);
        return QMailAddress::toStringList(message.cc());
    }
    else if (role == MessageBccRole) {
        QMailMessage message (msgId);
        return QMailAddress::toStringList(message.bcc());
    }
    else if (role == MessageSelectModeRole) {
       int selected = 0;
       if (m_selectedMsgIds.contains(msgId) == true)
           selected = 1;
        return (selected);
    }

    QMailMessageMetaData messageMetaData(msgId);

    if (role == QMailMessageModelBase::MessageTimeStampTextRole) {
        QDateTime timeStamp = messageMetaData.date().toLocalTime();
        return (timeStamp.toString("hh:mm MM/dd/yyyy"));
    }
    else if (role == MessageAttachmentCountRole) {
        // return number of attachments
        if (!messageMetaData.status() & QMailMessageMetaData::HasAttachments)
            return 0;

        QMailMessage message(msgId);
        const QList<QMailMessagePart::Location> &attachmentLocations = message.findAttachmentLocations();
        return attachmentLocations.count();
    }
    else if (role == MessageAttachmentsRole) {
        // return a stringlist of attachments
        if (!messageMetaData.status() & QMailMessageMetaData::HasAttachments)
            return QStringList();

        QMailMessage message(msgId);
        QStringList attachments;
        foreach (const QMailMessagePart::Location &location, message.findAttachmentLocations()) {
            const QMailMessagePart &attachmentPart = message.partAt(location);
            attachments << attachmentPart.displayName();
        }
        return attachments;
    }
    else if (role == MessageRecipientsRole) {
        QStringList recipients;
        QList<QMailAddress> addresses = messageMetaData.recipients();
        foreach (const QMailAddress &address, addresses) {
            recipients << address.address();
        }
        return recipients;
    }
    else if (role == MessageRecipientsDisplayNameRole) {
        QStringList recipients;
        QList<QMailAddress> addresses = messageMetaData.recipients();
        foreach (const QMailAddress &address, addresses) {
            recipients << address.name();
        }
        return recipients;
    }
    else if (role == MessageReadStatusRole) {
        if (messageMetaData.status() & QMailMessage::Read)
            return 1; // 1 for read
        else
            return 0; // 0 for unread
    }
    else if (role == MessageSenderDisplayNameRole) {
        return messageMetaData.from().name();
    }
    else if (role == MessageSenderEmailAddressRole) {
        return messageMetaData.from().address();
    }
    else if (role == MessageTimeStampRole) {
        return (messageMetaData.date().toLocalTime());
    }
    else if (role == MessagePreviewRole) {
        return messageMetaData.preview().simplified();
    }
    else if (role == MessageTimeSectionRole) {
        const int daysDiff = QDate::currentDate().toJulianDay()
                - (messageMetaData.date().toLocalTime()).date().toJulianDay();

        if (daysDiff < 7) {
            return (messageMetaData.date().toLocalTime()).date();
        }
        else {
            //returns epoch time for items older than a week
            return QDateTime::fromTime_t(0);
        }
    }
    else if (role == MessagePriorityRole) {
        if (messageMetaData.status() & QMailMessage::HighPriority) {
            return HighPriority;
        } else if (messageMetaData.status() & QMailMessage::LowPriority) {
            return LowPriority;
        }
        else {
            return NormalPriority;
        }
    }
    else if (role == MessageAccountIdRole) {
        return messageMetaData.parentAccountId().toULongLong();
    }
    else if (role == MessageHasAttachmentsRole) {
        if (messageMetaData.status() & QMailMessageMetaData::HasAttachments)
            return 1;
        else
            return 0;
    }
    else if (role == MessageSizeSectionRole) {
        const uint size(messageMetaData.size());
        // <100 KB
        if (size < 100 * 1024) {
            return 0;
        }
        // <500 KB
        else if (size < 500 * 1024) {
            return 1;
        }
        // >500 KB
        else {
            return 2;
        }
    } else if (role == MessageFolderIdRole) {
        return messageMetaData.parentFolderId().toULongLong();
    }
    else if (role == MessageSubjectFirstCharRole) {
        QString subject = data(index, QMailMessageModelBase::MessageSubjectTextRole).toString();
        return firstChar(subject);
    }
    else if (role == MessageSenderFirstCharRole) {
        QString sender = messageMetaData.from().name();
        return firstChar(sender);
    }

    return QMailMessageListModel::data(index, role);
}

int EmailMessageListModel::count() const
{
    return rowCount();
}

void EmailMessageListModel::setSearch(const QString search)
{

    if(search.isEmpty()) {
        setKey(QMailMessageKey::nonMatchingKey());
    } else {
        if(m_search == search)
            return;
        QMailMessageKey subjectKey = QMailMessageKey::subject(search, QMailDataComparator::Includes);
        QMailMessageKey toKey = QMailMessageKey::recipients(search, QMailDataComparator::Includes);
        QMailMessageKey fromKey = QMailMessageKey::sender(search, QMailDataComparator::Includes);
        setKey(m_key & (subjectKey | toKey | fromKey));
    }
    m_search = search;
}

void EmailMessageListModel::setFolderKey(int id, QMailMessageKey messageKey)
{
    m_currentFolderId = QMailFolderId(id);
    if (!m_currentFolderId.isValid())
        return;
    // Local folders (e.g outbox) can have messages from several accounts.
    QMailMessageKey accountKey = QMailMessageKey::parentAccountId(m_mailAccountIds) & messageKey;
    QMailMessageKey folderKey = accountKey & QMailMessageKey::parentFolderId(m_currentFolderId);
    QMailMessageListModel::setKey(folderKey);
    m_key=key();
    QMailMessageListModel::setSortKey(m_sortKey);

    if (combinedInbox())
        setCombinedInbox(false);
}

void EmailMessageListModel::setAccountKey(int id)
{
    QMailAccountId accountId = QMailAccountId(id);
    if (!accountId.isValid()) {
        //If accountId is invalid, empty key will be set.
        QMailMessageListModel::setKey(QMailMessageKey::nonMatchingKey());
    }
    else {
        m_mailAccountIds.clear();
        m_mailAccountIds.append(accountId);
        QMailAccount account(accountId);
        QMailFolderId folderId = account.standardFolder(QMailFolder::InboxFolder);

        QMailMessageKey accountKey = QMailMessageKey::parentAccountId(accountId);
        QMailMessageListModel::setKey(accountKey);

        if(folderId.isValid()) {
            // default to INBOX
            QMailMessageKey folderKey = QMailMessageKey::parentFolderId(folderId);
            QMailMessageListModel::setKey(folderKey);
        }
        else {
            QMailMessageListModel::setKey(QMailMessageKey::nonMatchingKey());
            connect(QMailStore::instance(), SIGNAL(foldersAdded ( const QMailFolderIdList &)), this,
                    SLOT(foldersAdded( const QMailFolderIdList &)));
        }
    }
    QMailMessageListModel::setSortKey(m_sortKey);

    m_key = key();

    if (combinedInbox())
        setCombinedInbox(false);
}

void EmailMessageListModel::foldersAdded(const QMailFolderIdList &folderIds)
{
    QMailFolderId folderId;
    foreach (const QMailFolderId &mailFolderId, folderIds) {
        QMailFolder folder(mailFolderId);
        if(m_mailAccountIds.contains(folder.parentAccountId())) {
            QMailAccount account(folder.parentAccountId());
            folderId = account.standardFolder(QMailFolder::InboxFolder);
            break;
        }
    }
    if(folderId.isValid()) {
        // default to INBOX
        QMailMessageKey folderKey = QMailMessageKey::parentFolderId(folderId);
        QMailMessageListModel::setKey(folderKey);
        disconnect(QMailStore::instance(), SIGNAL(foldersAdded ( const QMailFolderIdList &)), this,
                   SLOT(foldersAdded( const QMailFolderIdList &)));
        m_key = key();
    }
}

int EmailMessageListModel::selectedMessage() const
{
    return m_selectedMessage;
}

void EmailMessageListModel::setSelectedMessage(int messageId)
{
    if (messageId < 0) {
        // reset indexes and avoid transversing the list of messages
        m_selectedMessage = messageId;
        emit selectedMessageChanged();
        m_selectedMessageIndex = messageId;
        emit selectedMessageIndexChanged();
    } else if (messageId != m_selectedMessage) {
        m_selectedMessage = messageId;
        emit selectedMessageChanged();

        int index = indexFromMessageId(m_selectedMessage);
        if (m_selectedMessageIndex != index) {
            m_selectedMessageIndex = index;
            emit selectedMessageIndexChanged();
        }
    }
}

int EmailMessageListModel::selectedMessageIndex() const
{
    return m_selectedMessageIndex;
}

EmailMessageListModel::Sort EmailMessageListModel::sortBy() const
{
    return m_sortBy;
}

void EmailMessageListModel::sortBySender(int order)
{
    Qt::SortOrder sortOrder = static_cast<Qt::SortOrder>(order);
    m_sortKey = QMailMessageSortKey::sender(sortOrder);
    m_sortBy = Sender;
    QMailMessageListModel::setSortKey(m_sortKey);
    emit sortByChanged();
}

void EmailMessageListModel::sortBySubject(int order)
{
    Qt::SortOrder sortOrder = static_cast<Qt::SortOrder>(order);
    m_sortKey = QMailMessageSortKey::subject(sortOrder) &
            QMailMessageSortKey::timeStamp(Qt::DescendingOrder);
    m_sortBy = Subject;
    QMailMessageListModel::setSortKey(m_sortKey);
    emit sortByChanged();
}

void EmailMessageListModel::sortByDate(int order)
{
    Qt::SortOrder sortOrder = static_cast<Qt::SortOrder>(order);
    m_sortKey = QMailMessageSortKey::timeStamp(sortOrder);
    m_sortBy = Time;
    QMailMessageListModel::setSortKey(m_sortKey);
    emit sortByChanged();
}

void EmailMessageListModel::sortByAttachment(int order)
{
    // 0 - messages with attachments before messages without
    Qt::SortOrder sortOrder = static_cast<Qt::SortOrder>(order);
    m_sortKey = QMailMessageSortKey::status(QMailMessage::HasAttachments, sortOrder) &
            QMailMessageSortKey::timeStamp(Qt::DescendingOrder);
    m_sortBy = Attachments;
    QMailMessageListModel::setSortKey(m_sortKey);
    emit sortByChanged();
}

void EmailMessageListModel::sortByReadStatus(int order)
{
    // 0 - read before non-read
    Qt::SortOrder sortOrder = static_cast<Qt::SortOrder>(order);
    m_sortKey = QMailMessageSortKey::status(QMailMessage::Read, sortOrder) &
            QMailMessageSortKey::timeStamp(Qt::DescendingOrder);
    m_sortBy = ReadStatus;
    QMailMessageListModel::setSortKey(m_sortKey);
    emit sortByChanged();
}

void EmailMessageListModel::sortByPriority(int order)
{
    Qt::SortOrder sortOrder = static_cast<Qt::SortOrder>(order);

    if (sortOrder == Qt::AscendingOrder) {
        m_sortKey = QMailMessageSortKey::status(QMailMessage::HighPriority, sortOrder) &
                  QMailMessageSortKey::status(QMailMessage::LowPriority, Qt::DescendingOrder) &
                  QMailMessageSortKey::timeStamp(Qt::DescendingOrder);
    }
    else {
        m_sortKey = QMailMessageSortKey::status(QMailMessage::HighPriority, sortOrder) &
                QMailMessageSortKey::status(QMailMessage::LowPriority, Qt::AscendingOrder) &
                QMailMessageSortKey::timeStamp(Qt::DescendingOrder);
    }
    m_sortBy = Priority;
    QMailMessageListModel::setSortKey(m_sortKey);
    emit sortByChanged();
}

void EmailMessageListModel::sortBySize(int order)
{
    Qt::SortOrder sortOrder = static_cast<Qt::SortOrder>(order);
    m_sortKey = QMailMessageSortKey::size(sortOrder) &
            QMailMessageSortKey::timeStamp(Qt::DescendingOrder);
    m_sortBy = Size;
    QMailMessageListModel::setSortKey(m_sortKey);
    emit sortByChanged();
}

int EmailMessageListModel::accountIdForMessage(int messageId)
{
    QMailMessageId msgId(messageId);
    QMailMessageMetaData metaData(msgId);
    return metaData.parentAccountId().toULongLong();
}

int EmailMessageListModel::folderIdForMessage(int messageId)
{
    QMailMessageId msgId(messageId);
    QMailMessageMetaData metaData(msgId);
    return metaData.parentFolderId().toULongLong();
}

int EmailMessageListModel::indexFromMessageId(int messageId)
{
    QMailMessageId msgId(messageId);
    for (int row = 0; row < rowCount(); row++) {
        QVariant vMsgId = data(index(row), QMailMessageModelBase::MessageIdRole);
        
        if (msgId == vMsgId.value<QMailMessageId>())
            return row;
    }
    return -1;
}

int EmailMessageListModel::messageId(int idx)
{
    QMailMessageId id = idFromIndex(index(idx));
    return id.toULongLong();
}

QString EmailMessageListModel::subject(int idx)
{
    return data(index(idx), QMailMessageModelBase::MessageSubjectTextRole).toString();
}

QString EmailMessageListModel::mailSender(int idx)
{
    return data(index(idx), QMailMessageModelBase::MessageAddressTextRole).toString();
}

QString EmailMessageListModel::senderDisplayName(int idx)
{
    return data(index(idx), MessageSenderDisplayNameRole).toString();
}

QString EmailMessageListModel::senderEmailAddress(int idx)
{
    return data(index(idx), MessageSenderEmailAddressRole).toString();
}

QDateTime EmailMessageListModel::timeStamp(int idx)
{
    return data(index(idx), QMailMessageModelBase::MessageTimeStampTextRole).toDateTime();
}

QString EmailMessageListModel::body(int idx)
{
    return data(index(idx), QMailMessageModelBase::MessageBodyTextRole).toString();
}

QString EmailMessageListModel::quotedBody(int idx)
{
    return data(index(idx), MessageQuotedBodyRole).toString();
}

QString EmailMessageListModel::htmlBody(int idx)
{
    return data(index(idx), MessageHtmlBodyRole).toString();
}

QStringList EmailMessageListModel::attachments(int idx)
{
    return data(index(idx), MessageAttachmentsRole).toStringList();
}

int EmailMessageListModel::numberOfAttachments(int idx)
{
    return data(index(idx), MessageAttachmentCountRole).toInt();
}

QStringList EmailMessageListModel::toList(int idx)
{
    return data(index(idx), MessageToRole).toStringList();
}

QStringList EmailMessageListModel::recipients(int idx)
{
    return data(index(idx), MessageRecipientsRole).toStringList();
}

QStringList EmailMessageListModel::ccList(int idx)
{
    return data(index(idx), MessageCcRole).toStringList();
}

QStringList EmailMessageListModel::bccList(int idx)
{
    return data(index(idx), MessageBccRole).toStringList();
}

bool EmailMessageListModel::messageRead(int idx)
{
    return data(index(idx), MessageReadStatusRole).toBool();
}

QString EmailMessageListModel::size(int idx)
{
    return data(index(idx), QMailMessageModelBase::MessageSizeTextRole).toString();
}

int EmailMessageListModel::accountId(int idx)
{
    return data(index(idx), MessageAccountIdRole).toInt();
}

QVariant EmailMessageListModel::priority(int idx)
{
    return data(index(idx), MessagePriorityRole);
}

void EmailMessageListModel::deSelectAllMessages()
{
    if (m_selectedMsgIds.size() == 0)
        return;

    QMailMessageIdList msgIds = m_selectedMsgIds;
    m_selectedMsgIds.clear();
    foreach (const QMailMessageId &msgId,  msgIds) {
        for (int row = 0; row < rowCount(); row++) {
            QVariant vMsgId = data(index(row), QMailMessageModelBase::MessageIdRole);
    
            if (msgId == vMsgId.value<QMailMessageId>())
                dataChanged (index(row), index(row));
        }
    }
}

void EmailMessageListModel::selectMessage(int idx)
{
    QMailMessageId msgId = idFromIndex(index(idx));

    if (!m_selectedMsgIds.contains (msgId)) {
        m_selectedMsgIds.append(msgId);
        dataChanged(index(idx), index(idx));
    }
}

void EmailMessageListModel::deSelectMessage(int idx)
{
    QMailMessageId msgId = idFromIndex(index(idx));

    m_selectedMsgIds.removeOne(msgId);
    dataChanged(index(idx), index(idx));
}

void EmailMessageListModel::moveSelectedMessageIds(int vFolderId)
{
    if (m_selectedMsgIds.empty())
        return;

    QMailFolderId const id(vFolderId);

    QMailMessage const msg(m_selectedMsgIds[0]);

    EmailAgent::instance()->moveMessages(m_selectedMsgIds, id);
    m_selectedMsgIds.clear();
    EmailAgent::instance()->exportUpdates(msg.parentAccountId());
}
//TODO: support to delete messages from mutiple accounts
void EmailMessageListModel::deleteSelectedMessageIds()
{
    if (m_selectedMsgIds.empty())
        return;

    QMailMessage const msg(m_selectedMsgIds[0]);

    EmailAgent::instance()->deleteMessages(m_selectedMsgIds);
    m_selectedMsgIds.clear();
    EmailAgent::instance()->exportUpdates(msg.parentAccountId());
}

void EmailMessageListModel::markAllMessagesAsRead()
{
    if(rowCount()) {
        QMailAccountIdList accountIdList;
        QMailMessageIdList msgIds;
        quint64 status(QMailMessage::Read);

        for (int row = 0; row < rowCount(); row++) {
            if(!data(index(row), MessageReadStatusRole).toBool()) {
                QMailMessageId  id = (data(index(row), QMailMessageModelBase::MessageIdRole)).value<QMailMessageId>();
                msgIds.append(id);

                QMailAccountId accountId = (data(index(row), MessageAccountIdRole)).value<QMailAccountId>();
                if(!accountIdList.contains(accountId)) {
                    accountIdList.append(accountId);
                }
            }
        }
        if (msgIds.size()) {
            QMailStore::instance()->updateMessagesMetaData(QMailMessageKey::id(msgIds), status, true);
        }
        foreach (const QMailAccountId &accId,  accountIdList) {
            EmailAgent::instance()->exportUpdates(accId);
        }
    }
    else {
        return;
    }
}

void EmailMessageListModel::downloadActivityChanged(QMailServiceAction::Activity activity)
{
    if (QMailServiceAction *action = static_cast<QMailServiceAction*>(sender())) {
        if (activity == QMailServiceAction::Successful) {
            if (action == m_retrievalAction) {
                emit messageDownloadCompleted();
            }
        }
        else if (activity == QMailServiceAction::Failed) {
            //  Todo:  hmm.. may be I should emit an error here.
            emit messageDownloadCompleted();
        }
    }
}

void EmailMessageListModel::indexesChanged(const QModelIndex &parent, int start, int end)
{
    Q_UNUSED(parent);
    Q_UNUSED(start);
    Q_UNUSED(end);
    emit countChanged();

    // determine if current message index changed
    if (m_selectedMessageIndex >= 0) {
        int index = indexFromMessageId(m_selectedMessage);
        if (m_selectedMessageIndex != index) {
            m_selectedMessageIndex = index;
            emit selectedMessageIndexChanged();
        }
    }
}

bool EmailMessageListModel::combinedInbox() const
{
    return m_combinedInbox;
}

void EmailMessageListModel::setCombinedInbox(bool c)
{
    if(c == m_combinedInbox) {
        return;
    }

    m_mailAccountIds.clear();
    m_mailAccountIds = QMailStore::instance()->queryAccounts(QMailAccountKey::messageType(QMailMessage::Email)
                                                             & QMailAccountKey::status(QMailAccount::Enabled),
                                                             QMailAccountSortKey::name());
    if (c) {
        QMailFolderIdList folderIds;
        foreach (const QMailAccountId &accountId, m_mailAccountIds) {
            QMailAccount account(accountId);
            QMailFolderId foldId = account.standardFolder(QMailFolder::InboxFolder);
            if(foldId.isValid())
                folderIds << account.standardFolder(QMailFolder::InboxFolder);
        }

        QMailFolderKey inboxKey = QMailFolderKey::id(folderIds, QMailDataComparator::Includes);
        QMailMessageKey messageKey =  QMailMessageKey::parentFolderId(inboxKey);

        if (m_filterUnread) {
            QMailMessageKey unreadKey = QMailMessageKey::parentFolderId(inboxKey)
                    & QMailMessageKey::status(QMailMessage::Read, QMailDataComparator::Excludes);
            QMailMessageListModel::setKey(unreadKey);
        }
        else {
            QMailMessageListModel::setKey(messageKey);
        }

        m_combinedInbox = true;
        m_key = key();
    }
    else {
        QMailMessageKey accountKey;

        if (m_filterUnread) {
            accountKey = QMailMessageKey::parentAccountId(m_mailAccountIds)
                    &  QMailMessageKey::status(QMailMessage::Read, QMailDataComparator::Excludes);
        }
        else {
            accountKey = QMailMessageKey::parentAccountId(m_mailAccountIds);
        }
        QMailMessageListModel::setKey(accountKey);
        m_key = key();
        QMailMessageListModel::setSortKey(m_sortKey);
        m_combinedInbox = false;
    }
    emit combinedInboxChanged();
}

bool EmailMessageListModel::filterUnread() const
{
    return m_filterUnread;
}

void EmailMessageListModel::setFilterUnread(bool u)
{
    if(u == m_filterUnread) {
        return;
    }

    m_filterUnread = u;
    emit filterUnreadChanged();
}
