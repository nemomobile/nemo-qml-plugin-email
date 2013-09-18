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

class Q_DECL_EXPORT EmailAgent : public QObject
{
    Q_OBJECT
    Q_ENUMS(Status)
    Q_ENUMS(AttachmentStatus)
public:
    static EmailAgent *instance();

    explicit EmailAgent(QObject *parent = 0);
    ~EmailAgent();

    Q_PROPERTY(bool synchronizing READ synchronizing NOTIFY synchronizingChanged)

    enum Status {
        Synchronizing = 0,
        Completed,
        Error
    };

    enum AttachmentStatus {
        Idle = 0,
        Queued,
        Downloaded,
        Downloading,
        Failed
    };

    EmailAgent::AttachmentStatus attachmentDownloadStatus(const QString& attachmentLocation);
    int attachmentDownloadProgress(const QString& attachmentLocation);
    QString attachmentName(const QMailMessagePart &part) const;
    QString bodyPlainText(const QMailMessage &mailMsg) const;
    void exportUpdates(const QMailAccountId accountId);
    void initMailServer();
    bool synchronizing() const;
    void flagMessages(const QMailMessageIdList &ids, quint64 setMask, quint64 unsetMask);
    void moveMessages(const QMailMessageIdList &ids, const QMailFolderId &destinationId);
    quint64 newAction();
    void sendMessages(const QMailAccountId &accountId);
    void setupAccountFlags();
    int standardFolderId(int accountId, QMailFolder::StandardFolder folder) const;

    Q_INVOKABLE void accountsSync(const bool syncOnlyInbox = false, const uint minimum = 20);
    Q_INVOKABLE void cancelSync();
    Q_INVOKABLE void createFolder(const QString &name, int mailAccountId, int parentFolderId);
    Q_INVOKABLE void deleteFolder(int folderId);
    Q_INVOKABLE void deleteMessage(int messageId);
    Q_INVOKABLE void deleteMessages(const QMailMessageIdList &ids);
    Q_INVOKABLE void downloadAttachment(int messageId, const QString &attachmentlocation);
    Q_INVOKABLE void getMoreMessages(int folderId, uint minimum = 20);
    Q_INVOKABLE QString signatureForAccount(int accountId);
    Q_INVOKABLE int inboxFolderId(int accountId);
    Q_INVOKABLE int draftsFolderId(int accountId);
    Q_INVOKABLE bool isAccountValid(int accountId);
    Q_INVOKABLE bool isMessageValid(int messageId);
    Q_INVOKABLE void markMessageAsRead(int messageId);
    Q_INVOKABLE void markMessageAsUnread(int messageId);
    Q_INVOKABLE void moveMessage(int messageId, int destinationId);
    Q_INVOKABLE void renameFolder(int folderId, const QString &name);
    Q_INVOKABLE void retrieveFolderList(int accountId, int folderId = 0, const bool descending = true);
    Q_INVOKABLE void retrieveMessageList(int accountId, int folderId, const uint minimum = 20);
    Q_INVOKABLE void retrieveMessageRange(int messageId, uint minimum);
    Q_INVOKABLE void synchronize(int accountId);
    Q_INVOKABLE void synchronizeInbox(int accountId, const uint minimum = 20);

signals:
    void attachmentDownloadProgressChanged(const QString &attachmentLocation, int progress);
    void attachmentDownloadStatusChanged(const QString &attachmentLocation, EmailAgent::AttachmentStatus status);
    void attachmentUrlChanged(const QString &attachmentLocation, const QString &url);
    void error(const QMailAccountId &accountId, const QString &message, int code);
    void folderRetrievalCompleted(const QMailAccountId &accountId);
    void progressUpdated(int percent);
    void sendCompleted();
    void standardFoldersCreated(const QMailAccountId &accountId);
    void synchronizingChanged(EmailAgent::Status status);

private slots:
    void activityChanged(QMailServiceAction::Activity activity);
    void attachmentDownloadActivityChanged(QMailServiceAction::Activity activity);
    void onIpcConnectionEstablished();
    void onMessageServerProcessError(QProcess::ProcessError error);
    void onStandardFoldersCreated(const QMailAccountId &accountId);
    void progressChanged(uint value, uint total);

private:
    static EmailAgent *m_instance;

    uint m_actionCount;
    bool m_transmitting;
    bool m_cancelling;
    bool m_synchronizing;
    bool m_enqueing;
    bool m_waitForIpc;

    QMailAccountIdList m_enabledAccounts;

    QScopedPointer<QMailRetrievalAction> const m_retrievalAction;
    QScopedPointer<QMailStorageAction> const m_storageAction;
    QScopedPointer<QMailTransmitAction> const m_transmitAction;
    QMailRetrievalAction *m_attachmentRetrievalAction;
    QMailMessageId m_messageId;

    QProcess* m_messageServerProcess;

    QList<QSharedPointer<EmailAction> > m_actionQueue;
    QSharedPointer<EmailAction> m_currentAction;
    struct AttachmentInfo { AttachmentStatus status; int progress;};
    // Holds a list of the attachments currently dowloading or queued for download
    QHash<QString, AttachmentInfo> m_attachmentDownloadQueue;

    bool actionInQueue(QSharedPointer<EmailAction> action) const;
    void dequeue();
    void enqueue(EmailAction *action);
    void executeCurrent();
    QSharedPointer<EmailAction> getNext();
    void saveAttachmentToTemporaryFile(const QMailMessageId messageId, const QString &attachmentlocation);
    void updateAttachmentDowloadStatus(const QString &attachmentLocation, AttachmentStatus status);
    void updateAttachmentDowloadProgress(const QString &attachmentLocation, int progress);
};

#endif
