/*
 * Copyright 2011 Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at 	
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#ifndef EMAILAGENT_H
#define EMAILAGENT_H

#include <QProcess>
#include <QTimer>

#include <qmailaccount.h>
#include <qmailstore.h>
#include <qmailserviceaction.h>

#include "emailaction.h"

class EmailAgent : public QObject
{
    Q_OBJECT

public:
    static EmailAgent *instance();

    explicit EmailAgent(QObject *parent = 0);
    ~EmailAgent();

    QString bodyPlainText(const QMailMessage &mailMsg) const;
    void exportUpdates(const QMailAccountId accountId);
    void initMailServer();
    bool isSynchronizing() const;
    void flagMessages(const QMailMessageIdList &ids, quint64 setMask, quint64 unsetMask);
    void moveMessages(const QMailMessageIdList &ids, const QMailFolderId &destinationId);
    quint64 newAction();
    void sendMessages(const QMailAccountId &accountId);
    void setupAccountFlags();

    Q_INVOKABLE QMailAccountId accountIdFromVariant(QVariant accountId);
    Q_INVOKABLE void accountsSync(const bool syncOnlyInbox = false, const uint minimum = 20);
    Q_INVOKABLE void cancelSync();
    Q_INVOKABLE void createFolder(const QString &name, QVariant mailAccountId, QVariant parentFolderId);
    Q_INVOKABLE void deleteFolder(QVariant folderId);
    Q_INVOKABLE void deleteMessage(QVariant messageId);
    Q_INVOKABLE void deleteMessages(const QMailMessageIdList &ids);
    Q_INVOKABLE void downloadAttachment(QVariant messageId, const QString &attachmentDisplayName);
    Q_INVOKABLE qint64 folderIdToInt(QVariant folderId);
    Q_INVOKABLE QString getMessageBodyFromFile(const QString& bodyFilePath);
    Q_INVOKABLE void getMoreMessages(QVariant folderId, uint minimum = 20);
    Q_INVOKABLE QString getSignatureForAccount(QVariant accountId);
    Q_INVOKABLE QVariant inboxFolderId(QVariant accountId);
    Q_INVOKABLE bool isAccountValid(QVariant accountId);
    Q_INVOKABLE bool isMessageValid(QVariant messageId);
    Q_INVOKABLE void markMessageAsRead(QVariant messageId);
    Q_INVOKABLE void markMessageAsUnread(QVariant messageId);
    Q_INVOKABLE QMailMessageId messageIdFromVariant(QVariant messageId);
    Q_INVOKABLE void moveMessage(QVariant messageId, QVariant destinationId);
    Q_INVOKABLE bool openAttachment(const QString& attachmentDisplayName);
    Q_INVOKABLE void openBrowser(const QString& url);
    Q_INVOKABLE void renameFolder(QVariant folderId, const QString &name);
    Q_INVOKABLE void retrieveFolderList(QVariant accountId, QVariant folderId = 0, const bool descending = true);
    Q_INVOKABLE void retrieveMessageList(QVariant accountId, QVariant folderId, const uint minimum = 20);
    Q_INVOKABLE void retrieveMessageRange(QVariant messageId, uint minimum);
    Q_INVOKABLE void synchronize(QVariant accountId);
    Q_INVOKABLE void synchronizeInbox(QVariant accountId, const uint minimum = 20);

signals:
    void attachmentDownloadCompleted();
    void attachmentDownloadStarted();
    void error(const QMailAccountId &accountId, const QString &message, int code);
    void folderRetrievalCompleted(const QMailAccountId &accountId);
    void progressUpdate(int percent);
    void retrievalCompleted();
    void sendCompleted();
    void standardFoldersCreated(const QMailAccountId &accountId);
    void syncCompleted();
    void syncBegin();

private slots:
    void activityChanged(QMailServiceAction::Activity activity);
    void attachmentDownloadActivityChanged(QMailServiceAction::Activity activity);
    void onStandardFoldersCreated(const QMailAccountId &accountId);
    void progressChanged(uint value, uint total);

private:
    static EmailAgent *m_instance;

    uint m_actionCount;
    bool m_transmitting;
    bool m_cancelling;
    bool m_synchronizing;

    QMailAccountIdList m_enabledAccounts;

    QScopedPointer<QMailRetrievalAction> const m_retrievalAction;
    QScopedPointer<QMailStorageAction> const m_storageAction;
    QScopedPointer<QMailTransmitAction> const m_transmitAction;
    QMailRetrievalAction *m_attachmentRetrievalAction;
    QMailMessageId m_messageId;
    QMailMessagePart m_attachmentPart;

    QProcess m_messageServerProcess;

    QList<QSharedPointer<EmailAction> > m_actionQueue;
    QSharedPointer<EmailAction> m_currentAction;

    bool actionInQueue(QSharedPointer<EmailAction> action) const;
    void dequeue();
    void enqueue(EmailAction *action);
    void executeCurrent();
    QSharedPointer<EmailAction> getNext();
};

#endif
