/*
 * Copyright 2011 Intel Corporation.
 * Copyright (C) 2012 Jolla Ltd.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at 	
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#ifndef EMAILMESSAGELISTMODEL_H
#define EMAILMESSAGELISTMODEL_H

#include <QAbstractListModel>
#include <QProcess>

#include <qmailmessage.h>
#include <qmailmessagelistmodel.h>
#include <qmailserviceaction.h>
#include <qmailaccount.h>


class Q_DECL_EXPORT EmailMessageListModel : public QMailMessageListModel
{
    Q_OBJECT
    Q_ENUMS(Priority)
    Q_ENUMS(Sort)

    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(bool combinedInbox READ combinedInbox WRITE setCombinedInbox NOTIFY combinedInboxChanged)
    Q_PROPERTY(bool filterUnread READ filterUnread WRITE setFilterUnread NOTIFY filterUnreadChanged)
    Q_PROPERTY(EmailMessageListModel::Sort sortBy READ sortBy NOTIFY sortByChanged)
    Q_PROPERTY(int selectedMessage READ selectedMessage WRITE setSelectedMessage NOTIFY selectedMessageChanged)
    Q_PROPERTY(int selectedMessageIndex READ selectedMessageIndex NOTIFY selectedMessageIndexChanged)


public:
    enum Roles
    {
        MessageAttachmentCountRole = QMailMessageModelBase::MessageIdRole + 1, // returns number of attachment
        MessageAttachmentsRole,                                // returns a list of attachments
        MessageRecipientsRole,                                 // returns a list of recipients (email address)
        MessageRecipientsDisplayNameRole,                      // returns a list of recipients (displayName)
        MessageReadStatusRole,                                 // returns the read/unread status
        MessageQuotedBodyRole,                                 // returns the quoted body
        MessageHtmlBodyRole,                                   // returns the html body
        MessageIdRole,                                         // returns the message id
        MessageSenderDisplayNameRole,                          // returns sender's display name
        MessageSenderEmailAddressRole,                         // returns sender's email address
        MessageToRole,                                         // returns a list of To (email + displayName)
        MessageCcRole,                                         // returns a list of Cc (email + displayName)
        MessageBccRole,                                        // returns a list of Bcc (email + displayName)
        MessageTimeStampRole,                                  // returns timestamp in QDateTime format
        MessageSelectModeRole,                                 // returns the select mode
        MessagePreviewRole,                                    // returns message preview if available
        MessageTimeSectionRole,                                // returns time section relative to the current time
        MessagePriorityRole,                                   // returns message priority
        MessageAccountIdRole,                                  // returns parent account id for the message
        MessageHasAttachmentsRole,                             // returns 1 if message has attachments, 0 otherwise
        MessageSizeSectionRole,                                // returns size section (0-2)
        MessageFolderIdRole,                                   // returns parent folder id for the message
        MessageSortByRole,                                     // returns the sorting order of the list model
        MessageSubjectFirstCharRole,                           // returns the first character of the subject
        MessageSenderFirstCharRole                             // returns the first character of the sender's display name
    };

    EmailMessageListModel(QObject *parent = 0);
    ~EmailMessageListModel();
    int rowCount(const QModelIndex & parent = QModelIndex()) const;
    QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const;
    QString bodyHtmlText(const QMailMessage &) const;

    enum Priority { LowPriority, NormalPriority, HighPriority };

    enum Sort { Time, Sender, Size, ReadStatus, Priority, Attachments, Subject};

    // property accessors.
    int count() const;
    bool combinedInbox() const;
    void setCombinedInbox(bool c);
    bool filterUnread() const;
    void setFilterUnread(bool u);
    int selectedMessage() const;
    void setSelectedMessage(int messageId);
    int selectedMessageIndex() const;
    EmailMessageListModel::Sort sortBy() const;

Q_SIGNALS:
    void countChanged();
    void combinedInboxChanged();
    void filterUnreadChanged();
    void sortByChanged();
    void selectedMessageChanged();
    void selectedMessageIndexChanged();

signals:
    void messageDownloadCompleted();

public slots:
    Q_INVOKABLE void setFolderKey(int id, QMailMessageKey messageKey = QMailMessageKey());
    Q_INVOKABLE void setAccountKey(int id);
    Q_INVOKABLE void sortBySender(int order = 0);
    Q_INVOKABLE void sortBySubject(int order = 0);
    Q_INVOKABLE void sortByDate(int order = 1);
    Q_INVOKABLE void sortByAttachment(int order = 1);
    Q_INVOKABLE void sortByReadStatus(int order = 0);
    Q_INVOKABLE void sortByPriority(int order = 1);
    Q_INVOKABLE void sortBySize(int order = 1);
    Q_INVOKABLE void setSearch(const QString search);

    Q_INVOKABLE int accountIdForMessage(int messageId);
    Q_INVOKABLE int folderIdForMessage(int messageId);
    Q_INVOKABLE int indexFromMessageId(int messageId);
    Q_INVOKABLE int messageId(int index);
    Q_INVOKABLE QString subject(int index);
    Q_INVOKABLE QString mailSender(int index);
    Q_INVOKABLE QString senderDisplayName(int index);
    Q_INVOKABLE QString senderEmailAddress(int index);
    Q_INVOKABLE QDateTime timeStamp(int index);
    Q_INVOKABLE QString body(int index);
    Q_INVOKABLE QString htmlBody(int index);
    Q_INVOKABLE QString quotedBody(int index);
    Q_INVOKABLE QStringList attachments(int index);
    Q_INVOKABLE int numberOfAttachments(int index);
    Q_INVOKABLE QStringList recipients(int index);
    Q_INVOKABLE QStringList ccList(int index);
    Q_INVOKABLE QStringList bccList(int index);
    Q_INVOKABLE QStringList toList(int index);
    Q_INVOKABLE bool messageRead(int index);
    Q_INVOKABLE QString size(int index);
    Q_INVOKABLE int accountId(int index);
    Q_INVOKABLE QVariant priority(int index);

    Q_INVOKABLE void deSelectAllMessages();
    Q_INVOKABLE void selectMessage(int index);
    Q_INVOKABLE void deSelectMessage(int index);
    Q_INVOKABLE void moveSelectedMessageIds(int vFolderId);
    Q_INVOKABLE void deleteSelectedMessageIds();
    Q_INVOKABLE void markAllMessagesAsRead();

    void foldersAdded(const QMailFolderIdList &folderIds);

private slots:
    void downloadActivityChanged(QMailServiceAction::Activity);
    void indexesChanged(const QModelIndex &parent, int start, int end);

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
protected:
    virtual QHash<int, QByteArray> roleNames() const;
#endif

private:
    QHash<int, QByteArray> roles;
    bool m_combinedInbox;
    bool m_filterUnread;
    int m_selectedMessage;
    int m_selectedMessageIndex;
    QProcess m_msgAccount;
    QMailFolderId m_currentFolderId;
    QMailAccountIdList m_mailAccountIds;
    QMailRetrievalAction *m_retrievalAction;
    QString m_search;
    QMailMessageKey m_key;                  // key set externally other than search
    QMailMessageSortKey m_sortKey;
    EmailMessageListModel::Sort m_sortBy;
    QList<QMailMessageId> m_selectedMsgIds;
};

#endif
