/*
 * Copyright 2011 Intel Corporation.
 * Copyright (C) 2012 Jolla Ltd.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at 	
 * http://www.apache.org/licenses/LICENSE-2.0
 */


#include <QTimer>
#include <QDir>
#include <QUrl>
#include <QFile>
#include <QMap>
#include <QProcess>
#include <QStandardPaths>

#include <qmailnamespace.h>
#include <qmailaccount.h>
#include <qmailstore.h>
#include <qmaildisconnected.h>

#include "emailagent.h"
#include "emailaction.h"

#define MESSAGESERVER "/usr/bin/messageserver5"

namespace {

QMailAccountId accountForMessageId(const QMailMessageId &msgId)
{
    QMailMessageMetaData metaData(msgId);
    return metaData.parentAccountId();
}
}

EmailAgent *EmailAgent::m_instance = 0;

EmailAgent *EmailAgent::instance()
{
    if (!m_instance)
        m_instance = new EmailAgent();
    return m_instance;
}

EmailAgent::EmailAgent(QObject *parent)
    : QObject(parent)
    , m_actionCount(0)
    , m_accountSynchronizing(-1)
    , m_transmitting(false)
    , m_cancelling(false)
    , m_cancellingSingleAction(false)
    , m_synchronizing(false)
    , m_enqueing(false)
    , m_backgroundProcess(false)
    , m_retrievalAction(new QMailRetrievalAction(this))
    , m_storageAction(new QMailStorageAction(this))
    , m_transmitAction(new QMailTransmitAction(this))
    , m_nmanager(new QNetworkConfigurationManager(this))
    , m_networkSession(0)
{

    connect(QMailStore::instance(), SIGNAL(ipcConnectionEstablished()),
            this, SLOT(onIpcConnectionEstablished()));

    initMailServer();
    setupAccountFlags();

    connect(m_transmitAction.data(), SIGNAL(progressChanged(uint, uint)),
            this, SLOT(progressChanged(uint,uint)));

    connect(m_retrievalAction.data(), SIGNAL(activityChanged(QMailServiceAction::Activity)),
            this, SLOT(activityChanged(QMailServiceAction::Activity)));

    connect(m_retrievalAction.data(), SIGNAL(progressChanged(uint, uint)),
            this, SLOT(progressChanged(uint,uint)));

    connect(m_storageAction.data(), SIGNAL(activityChanged(QMailServiceAction::Activity)),
            this, SLOT(activityChanged(QMailServiceAction::Activity)));

    connect(m_transmitAction.data(), SIGNAL(activityChanged(QMailServiceAction::Activity)),
            this, SLOT(activityChanged(QMailServiceAction::Activity)));

    connect(m_nmanager, SIGNAL(onlineStateChanged(bool)), this, SLOT(onOnlineStateChanged(bool)));

    m_waitForIpc = !QMailStore::instance()->isIpcConnectionEstablished();
    m_instance = this;

}

EmailAgent::~EmailAgent()
{
}

int EmailAgent::currentSynchronizingAccountId() const
{
    return m_accountSynchronizing;
}

int EmailAgent::attachmentDownloadProgress(const QString &attachmentLocation)
{
    if (m_attachmentDownloadQueue.contains(attachmentLocation)) {
        AttachmentInfo attInfo = m_attachmentDownloadQueue.value(attachmentLocation);
        return attInfo.progress;
    }
    return 0;
}

EmailAgent::AttachmentStatus EmailAgent::attachmentDownloadStatus(const QString &attachmentLocation)
{
    if (m_attachmentDownloadQueue.contains(attachmentLocation)) {
        AttachmentInfo attInfo = m_attachmentDownloadQueue.value(attachmentLocation);
        return attInfo.status;
    }
    return NotDownloaded;
}

QString EmailAgent::attachmentName(const QMailMessagePart &part) const
{
    bool isRFC822 = (part.contentType().type().toLower() == "message") &&
        (part.contentType().subType().toLower() == "rfc822");
    if (isRFC822) {
        bool noName = (part.contentDisposition().parameter("name").isEmpty() &&
                       part.contentDisposition().parameter("filename").isEmpty() &&
                       part.contentType().name().isEmpty());
        if (noName) {
            // Show email subject of attached message as attachment name
            QMailMessage msg = QMailMessage::fromRfc2822(part.body().data(QMailMessageBody::Decoded));
            if (!msg.subject().isEmpty())
                return msg.subject();
        }
    }
    return part.displayName();
}

QString EmailAgent::bodyPlainText(const QMailMessage &mailMsg) const
{
    if (QMailMessagePartContainer *container = mailMsg.findPlainTextContainer()) {
        return container->body().data();
    }

    return QString();
}

bool EmailAgent::backgroundProcess() const
{
    return m_backgroundProcess;
}

void EmailAgent::setBackgroundProcess(const bool isBackgroundProcess)
{
    m_backgroundProcess = isBackgroundProcess;
}

void EmailAgent::cancelAction(quint64 actionId)
{
    //cancel running action
    if (m_currentAction->id() == actionId) {
        if(m_currentAction->serviceAction()->isRunning()) {
            m_cancellingSingleAction = true;
            m_currentAction->serviceAction()->cancelOperation();
        }
    } else {
        removeAction(actionId);
    }
}

quint64 EmailAgent::downloadMessages(const QMailMessageIdList &messageIds, QMailRetrievalAction::RetrievalSpecification spec)
{
     return enqueue(new RetrieveMessages(m_retrievalAction.data(), messageIds, spec));
}

quint64 EmailAgent::downloadMessagePart(const QMailMessagePart::Location &location)
{
    return enqueue(new RetrieveMessagePart(m_retrievalAction.data(), location, false));
}

void EmailAgent::exportUpdates(const QMailAccountId accountId)
{
    enqueue(new ExportUpdates(m_retrievalAction.data(),accountId));
}

bool EmailAgent::hasMessagesInOutbox(const QMailAccountId accountId)
{
    // Local folders can have messages from several accounts.
    QMailMessageKey outboxFilter(QMailMessageKey::status(QMailMessage::Outbox) & ~QMailMessageKey::status(QMailMessage::Trash));
    QMailMessageKey accountKey(QMailMessageKey::parentAccountId(accountId));
    if (QMailStore::instance()->countMessages(accountKey & outboxFilter)) {
        return true;
    } else {
        return false;
    }
}

void EmailAgent::initMailServer()
{
    // starts the messageserver if it is not already running.

    QString lockfile = "messageserver-instance.lock";
    int id = QMail::fileLock(lockfile);
    if (id == -1) {
        // Server is currently running
        return;
    }
    QMail::fileUnlock(id);
    qDebug() << Q_FUNC_INFO << "Starting messageserver process...";
    m_messageServerProcess = new QProcess(this);
    connect(m_messageServerProcess, SIGNAL(error(QProcess::ProcessError)),
            this, SLOT(onMessageServerProcessError(QProcess::ProcessError)));
    m_messageServerProcess->startDetached(MESSAGESERVER);
    return;
}

bool EmailAgent::ipcConnected()
{
    return !m_waitForIpc;
}

bool EmailAgent::synchronizing() const
{
    return m_synchronizing;
}

void EmailAgent::flagMessages(const QMailMessageIdList &ids, quint64 setMask,
        quint64 unsetMask)
{
    Q_ASSERT(!ids.empty());

    enqueue(new FlagMessages(m_storageAction.data(), ids, setMask, unsetMask));
}

void EmailAgent::moveMessages(const QMailMessageIdList &ids, const QMailFolderId &destinationId)
{
    Q_ASSERT(!ids.empty());

    QMailMessageId id(ids[0]);
    QMailAccountId accountId = accountForMessageId(id);

    m_enqueing = true;
    enqueue(new MoveToFolder(m_storageAction.data(), ids, destinationId));
    m_enqueing = false;
    enqueue(new ExportUpdates(m_retrievalAction.data(), accountId));
}

void EmailAgent::sendMessages(const QMailAccountId &accountId)
{
    if (accountId.isValid()) {
        enqueue(new TransmitMessages(m_transmitAction.data(),accountId));
    }
}

void EmailAgent::setupAccountFlags()
{
    if (!QMailStore::instance()->accountStatusMask("StandardFoldersRetrieved")) {
        QMailStore::instance()->registerAccountStatusFlag("StandardFoldersRetrieved");
    }
}

// ############ Slots ###############
void EmailAgent::activityChanged(QMailServiceAction::Activity activity)
{
    QMailServiceAction *action = static_cast<QMailServiceAction*>(sender());
    const QMailServiceAction::Status status(action->status());

    switch (activity) {
    case QMailServiceAction::Failed:
        //TODO: coordinate with stop logic
        // don't try to synchronise extra accounts if the user cancelled the sync
        if (m_cancelling) {
            m_synchronizing = false;
            m_accountSynchronizing = -1;
            emit currentSynchronizingAccountIdChanged();
            emit synchronizingChanged(EmailAgent::Error);
            m_transmitting = false;
            m_cancelling = false;
            m_actionQueue.clear();
            // Cancel by the user skip error reporting
            qDebug() << "Canceled by the user";
            break;
        } else if (m_cancellingSingleAction) {
            dequeue();
            m_currentAction.clear();
            qDebug() << "Single action canceled by the user";
            m_cancellingSingleAction = false;
            m_accountSynchronizing = -1;
            emit currentSynchronizingAccountIdChanged();
            if (m_actionQueue.empty()) {
                m_synchronizing = false;
            }
            break;
        } else {
            // Report the error
            dequeue();
            if (m_currentAction->type() == EmailAction::Transmit) {
                m_transmitting = false;
                emit sendCompleted();
                qDebug() << "Error: Send failed";
            }

            if (m_currentAction->type() == EmailAction::RetrieveMessagePart) {
                RetrieveMessagePart* messagePartAction = static_cast<RetrieveMessagePart *>(m_currentAction.data());
                if (messagePartAction->isAttachment()) {
                    updateAttachmentDowloadStatus(messagePartAction->partLocation(), Failed);
                    qDebug() << "Attachment dowload failed for " << messagePartAction->partLocation();
                } else {
                    emit messagePartDownloaded(messagePartAction->messageId(), messagePartAction->partLocation(), false);
                    qDebug() << "Failed to dowload message part!!";
                }
            }

            if (m_currentAction->type() == EmailAction::RetrieveMessages) {
                RetrieveMessages* retrieveMessagesAction = static_cast<RetrieveMessages *>(m_currentAction.data());
                emit messagesDownloaded(retrieveMessagesAction->messageIds(), false);
                qDebug() << "Failed to download messages";
            }

            m_currentAction = getNext();
            reportError(status.accountId, status.errorCode);

            if (m_currentAction.isNull()) {
                qDebug() << "Sync completed with Errors!!!.";
                m_synchronizing = false;
                m_accountSynchronizing = -1;
                emit currentSynchronizingAccountIdChanged();
                emit synchronizingChanged(EmailAgent::Error);
            }
            else {
                executeCurrent();
            }
            break;
        }

    case QMailServiceAction::Successful:
        dequeue();
        //Clients should not wait for send, check if this is
        //really necessary
        if (m_currentAction->type() == EmailAction::Transmit) {
            qDebug() << "Finished sending for " << m_currentAction->accountId();
            m_transmitting = false;
            emit sendCompleted();
        }

        if (m_currentAction->type() == EmailAction::StandardFolders) {
            QMailAccount *account = new QMailAccount(m_currentAction->accountId());
            account->setStatus(QMailAccount::statusMask("StandardFoldersRetrieved"), true);
            QMailStore::instance()->updateAccount(account);
            emit standardFoldersCreated(m_currentAction->accountId());
        }

        if (m_currentAction->type() == EmailAction::RetrieveFolderList) {
            emit folderRetrievalCompleted(m_currentAction->accountId());
        }

        if (m_currentAction->type() == EmailAction::RetrieveMessagePart) {
            RetrieveMessagePart* messagePartAction = static_cast<RetrieveMessagePart *>(m_currentAction.data());
            if (messagePartAction->isAttachment()) {
                saveAttachmentToDownloads(messagePartAction->messageId(), messagePartAction->partLocation());
            } else {
                emit messagePartDownloaded(messagePartAction->messageId(), messagePartAction->partLocation(), true);
            }
        }

        if (m_currentAction->type() == EmailAction::RetrieveMessages) {
            RetrieveMessages* retrieveMessagesAction = static_cast<RetrieveMessages *>(m_currentAction.data());
            emit messagesDownloaded(retrieveMessagesAction->messageIds(), true);
        }

        m_currentAction = getNext();

        if (m_currentAction.isNull()) {
            qDebug() << "Sync completed.";
            m_synchronizing = false;
            m_accountSynchronizing = -1;
            emit currentSynchronizingAccountIdChanged();
            emit synchronizingChanged(EmailAgent::Completed);
        }
        else {
            executeCurrent();
        }
        break;

    default:
        //emit acctivity changed here
        qDebug() << "Activity State Changed:" << activity;
        break;
    }
}

void EmailAgent::onIpcConnectionEstablished()
{
    if (m_waitForIpc) {
        m_waitForIpc = false;
        emit ipcConnectionEstablished();

        if (m_currentAction.isNull())
            m_currentAction = getNext();

        if (m_currentAction.isNull()) {
            qDebug() << "Ipc connection established, but no action in the queue.";
        } else {
            executeCurrent();
        }
    }
}

void EmailAgent::onMessageServerProcessError(QProcess::ProcessError error)
{
    QString errorMsg(QString("Could not start messageserver process, unable to communicate with the remove servers.\nQProcess exit with error: (%1)")
                     .arg(static_cast<int>(error)));
    qFatal(errorMsg.toLatin1());
}

void EmailAgent::onOnlineStateChanged(bool isOnline)
{
    qDebug() << Q_FUNC_INFO << "Online State changed, device is now connected ? " << isOnline;
    if (isOnline) {
        if (m_currentAction.isNull())
            m_currentAction = getNext();

        if (m_currentAction.isNull()) {
            qDebug() << "Network connection established, but no action in the queue.";
        } else {
            executeCurrent();
        }
    } else if (!m_currentAction.isNull() && m_currentAction->needsNetworkConnection() && m_currentAction->serviceAction()->isRunning()) {
        m_currentAction->serviceAction()->cancelOperation();
    }
}

void EmailAgent::onStandardFoldersCreated(const QMailAccountId &accountId)
{
    //TODO: default minimum should be kept
    QMailAccount account(accountId);
    QMailFolderId foldId = account.standardFolder(QMailFolder::InboxFolder);
    if(foldId.isValid()) {
        synchronizeInbox(accountId.toULongLong());
    }
    else {
        qDebug() << "Error: Inbox not found!!!";
    }
}

void EmailAgent::progressChanged(uint value, uint total)
{
    if (value < total) {
        int percent = (value * 100) / total;
        emit progressUpdated(percent);

        // Attachment download, do not spam the UI check should be done here
        if (m_currentAction->type() == EmailAction::RetrieveMessagePart) {
            RetrieveMessagePart* messagePartAction = static_cast<RetrieveMessagePart *>(m_currentAction.data());
            if (messagePartAction->isAttachment()) {
                updateAttachmentDowloadProgress(messagePartAction->partLocation(), percent);
            }
        }
    }
}

// ############# Invokable API ########################

//Sync all accounts (both ways)
void EmailAgent::accountsSync(const bool syncOnlyInbox, const uint minimum)
{
    m_enabledAccounts.clear();
    m_enabledAccounts = QMailStore::instance()->queryAccounts(QMailAccountKey::messageType(QMailMessage::Email)
                                                              & QMailAccountKey::status(QMailAccount::Enabled));
    qDebug() << "Enabled accounts size is: " << m_enabledAccounts.count();

    if (m_enabledAccounts.isEmpty()) {
        qDebug() << Q_FUNC_INFO << "No enabled accounts, nothing to do.";
        m_synchronizing = false;
        emit synchronizingChanged(EmailAgent::Error);
        return;
    } else {
        foreach (QMailAccountId accountId, m_enabledAccounts) {
            if (syncOnlyInbox) {
                synchronizeInbox(accountId.toULongLong(), minimum);
            }
            else {
                bool messagesToSend = hasMessagesInOutbox(accountId);
                if (messagesToSend) {
                    m_enqueing = true;
                }
                enqueue(new Synchronize(m_retrievalAction.data(), accountId));
                if (messagesToSend) {
                    m_enqueing = false;
                    // Send any message waiting in the outbox
                    enqueue(new TransmitMessages(m_transmitAction.data(),accountId));
                }
            }
        }
    }
}

void EmailAgent::cancelSync()
{
    if (!m_synchronizing)
        return;

    m_cancelling = true;

    //clear the actions queue
    m_actionQueue.clear();

    //cancel running action
    if (!m_currentAction.isNull() && m_currentAction->serviceAction()->isRunning()) {
        m_currentAction->serviceAction()->cancelOperation();
    }
}

void EmailAgent::createFolder(const QString &name, int mailAccountId, int parentFolderId)
{

    if(!name.isEmpty()) {
        qDebug() << "Error: Can't create a folder with empty name";
    }

    else {
        QMailAccountId accountId(mailAccountId);
        Q_ASSERT(accountId.isValid());

        QMailFolderId parentId(parentFolderId);

        enqueue(new OnlineCreateFolder(m_storageAction.data(), name, accountId, parentId));
    }
}

void EmailAgent::deleteFolder(int folderId)
{
    QMailFolderId id(folderId);
    Q_ASSERT(id.isValid());

    enqueue(new OnlineDeleteFolder(m_storageAction.data(), id));
}

void EmailAgent::deleteMessage(int messageId)
{
    QMailMessageId msgId(messageId);
    QMailMessageIdList msgIdList;
    msgIdList << msgId;
    deleteMessages (msgIdList);
}

void EmailAgent::deleteMessages(const QMailMessageIdList &ids)
{
    Q_ASSERT(!ids.isEmpty());

    if (m_transmitting) {
        // Do not delete messages from the outbox folder while we're sending
        QMailMessageKey outboxFilter(QMailMessageKey::status(QMailMessage::Outbox));
        if (QMailStore::instance()->countMessages(QMailMessageKey::id(ids) & outboxFilter)) {
            //TODO: emit proper error
            return;
        }
    }

    bool exportUpdates;

    QMap<QMailAccountId, QMailMessageIdList> accountMap;
    // Messages can be from several accounts
    foreach (const QMailMessageId &id, ids) {
       QMailAccountId accountId = accountForMessageId(id);
       if (accountMap.contains(accountId)) {
           QMailMessageIdList idList = accountMap.value(accountId);
           idList.append(id);
           accountMap.insert(accountId, idList);
       } else {
           accountMap.insert(accountId, QMailMessageIdList() << id);
       }
    }

    // If any of these messages are not yet trash, then we're only moved to trash
    QMailMessageKey idFilter(QMailMessageKey::id(ids));
    QMailMessageKey notTrashFilter(QMailMessageKey::status(QMailMessage::Trash, QMailDataComparator::Excludes));

    const bool deleting(QMailStore::instance()->countMessages(idFilter & notTrashFilter) == 0);

    if (deleting) {
        //delete LocalOnly messages clientside first
        QMailMessageKey localOnlyKey(QMailMessageKey::id(ids) & QMailMessageKey::status(QMailMessage::LocalOnly));
        QMailMessageIdList localOnlyIds(QMailStore::instance()->queryMessages(localOnlyKey));
        QMailMessageIdList idsToRemove(ids);
        if(!localOnlyIds.isEmpty()) {
            QMailStore::instance()->removeMessages(QMailMessageKey::id(localOnlyIds));
            idsToRemove = (ids.toSet().subtract(localOnlyIds.toSet())).toList();
        }
        if(!idsToRemove.isEmpty()) {
            m_enqueing = true;
            enqueue(new DeleteMessages(m_storageAction.data(), idsToRemove));
            exportUpdates = true;
        }
    } else {
        QMapIterator<QMailAccountId, QMailMessageIdList> iter(accountMap);
        while (iter.hasNext()) {
            iter.next();
            QMailAccount account(iter.key());
            QMailFolderId trashFolderId = account.standardFolder(QMailFolder::TrashFolder);
            // If standard folder is not valid we use local storage
            if (!trashFolderId.isValid()) {
                qDebug() << "Trash folder not found using local storage";
                trashFolderId = QMailFolder::LocalStorageFolderId;
            }
            m_enqueing = true;
            enqueue(new MoveToFolder(m_storageAction.data(), iter.value(), trashFolderId));
            enqueue(new FlagMessages(m_storageAction.data(), iter.value(), QMailMessage::Trash, 0));
            if (!iter.hasNext()) {
                m_enqueing = false;
            }
        }
        exportUpdates = true;
    }

    // Do online actions at the end
    if (exportUpdates) {
        // Export updates for all accounts that we deleted messages from
        QMailAccountIdList accountList = accountMap.uniqueKeys();
        for (int i = 0; i < accountList.size(); i++) {
            if (i == accountList.size()) {
                m_enqueing = false;
            }
            enqueue(new ExportUpdates(m_retrievalAction.data(), accountList.at(i)));
        }
    }
}

void EmailAgent::downloadAttachment(int messageId, const QString &attachmentlocation)
{
    m_messageId = QMailMessageId(messageId);
    QMailMessage message (m_messageId);

    for (uint i = 0; i < message.partCount(); i++) {
        QMailMessagePart sourcePart = message.partAt(i);
        if (attachmentlocation == sourcePart.location().toString(true)) {
            QMailMessagePart::Location location = sourcePart.location();
            location.setContainingMessageId(m_messageId);
            if (sourcePart.hasBody()) {
                saveAttachmentToDownloads(m_messageId, attachmentlocation);
            } else {
                qDebug() << "Start Dowload for: " << attachmentlocation;
                enqueue(new RetrieveMessagePart(m_retrievalAction.data(), location, true));
            }
        }
    }
}

void EmailAgent::exportUpdates(int accountId)
{
    QMailAccountId acctId(accountId);

    if (acctId.isValid()) {
        enqueue(new ExportUpdates(m_retrievalAction.data(),acctId));;
    }
}

void EmailAgent::getMoreMessages(int folderId, uint minimum)
{
    QMailFolderId foldId(folderId);
    if (foldId.isValid()) {
        QMailFolder folder(foldId);
        QMailMessageKey countKey(QMailMessageKey::parentFolderId(foldId));
        countKey &= ~QMailMessageKey::status(QMailMessage::Temporary);
        minimum += QMailStore::instance()->countMessages(countKey);
        enqueue(new RetrieveMessageList(m_retrievalAction.data(), folder.parentAccountId(), foldId, minimum));
    }
}

QString EmailAgent::signatureForAccount(int accountId)
{
    QMailAccountId mailAccountId(accountId);
    if (mailAccountId.isValid()) {
        QMailAccount mailAccount (mailAccountId);
        return mailAccount.signature();
    }
    return QString();
}

int EmailAgent::standardFolderId(int accountId, QMailFolder::StandardFolder folder) const
{
    QMailAccountId acctId(accountId);
    if (acctId.isValid()) {
        QMailAccount account(acctId);
        QMailFolderId foldId = account.standardFolder(folder);

        if (foldId.isValid()) {
            return foldId.toULongLong();
        }
    }
    qDebug() << "Error: Standard folder " << folder << " not found for account: " << accountId;
    return 0;
}

int EmailAgent::inboxFolderId(int accountId)
{
    return standardFolderId(accountId, QMailFolder::InboxFolder);
}

int EmailAgent::outboxFolderId(int accountId)
{
    return standardFolderId(accountId, QMailFolder::OutboxFolder);
}

int EmailAgent::draftsFolderId(int accountId)
{
    return standardFolderId(accountId, QMailFolder::DraftsFolder);
}

int EmailAgent::sentFolderId(int accountId)
{
    return standardFolderId(accountId, QMailFolder::SentFolder);
}

int EmailAgent::trashFolderId(int accountId)
{
    return standardFolderId(accountId, QMailFolder::TrashFolder);
}

int EmailAgent::junkFolderId(int accountId)
{
    return standardFolderId(accountId, QMailFolder::JunkFolder);
}

bool EmailAgent::isAccountValid(int accountId)
{
    QMailAccountId id(accountId);
    QMailAccount account = QMailStore::instance()->account(id);
    return account.id().isValid();
}

bool EmailAgent::isMessageValid(int messageId)
{
    QMailMessageId id(messageId);
    QMailMessageMetaData message = QMailStore::instance()->messageMetaData(id);
    return message.id().isValid();
}

void EmailAgent::markMessageAsRead(int messageId)
{
    QMailMessageId id(messageId);
    quint64 status(QMailMessage::Read);
    QMailStore::instance()->updateMessagesMetaData(QMailMessageKey::id(id), status, true);
    exportUpdates(accountForMessageId(id));
}

void EmailAgent::markMessageAsUnread(int messageId)
{
    QMailMessageId id(messageId);
    quint64 status(QMailMessage::Read);
    QMailStore::instance()->updateMessagesMetaData(QMailMessageKey::id(id), status, false);
    exportUpdates(accountForMessageId(id));
}

void EmailAgent::moveMessage(int messageId, int destinationId)
{
    QMailMessageId msgId(messageId);
    QMailMessageIdList msgIdList;
    msgIdList << msgId;
    QMailFolderId destId(destinationId);
    moveMessages(msgIdList, destId);
}

void EmailAgent::renameFolder(int folderId, const QString &name)
{
    if(!name.isEmpty()) {
        qDebug() << "Error: Can't rename a folder to a empty name";
    }

    else{
        QMailFolderId id(folderId);
        Q_ASSERT(id.isValid());

        enqueue(new OnlineRenameFolder(m_storageAction.data(),id, name));
    }
}

void EmailAgent::retrieveFolderList(int accountId, int folderId, const bool descending)
{
    QMailAccountId acctId(accountId);
    QMailFolderId foldId(folderId);

    if (acctId.isValid()) {
        enqueue(new RetrieveFolderList(m_retrievalAction.data(),acctId, foldId, descending));
    }
}

void EmailAgent::retrieveMessageList(int accountId, int folderId, const uint minimum)
{
    QMailAccountId acctId(accountId);
    QMailFolderId foldId(folderId);

    if (acctId.isValid()) {
        enqueue(new RetrieveMessageList(m_retrievalAction.data(),acctId, foldId, minimum));
    }
}

void EmailAgent::retrieveMessageRange(int messageId, uint minimum)
{
    QMailMessageId id(messageId);
    enqueue(new RetrieveMessageRange(m_retrievalAction.data(), id, minimum));
}

void EmailAgent::purgeSendingQueue(int accountId)
{
    QMailAccountId acctId(accountId);
    if (hasMessagesInOutbox(acctId)) {
        sendMessages(acctId);
    }
}

void EmailAgent::synchronize(int accountId)
{
    QMailAccountId acctId(accountId);

    if (acctId.isValid()) {
        enqueue(new Synchronize(m_retrievalAction.data(), acctId));
    }
}

void EmailAgent::synchronizeInbox(int accountId, const uint minimum)
{
    QMailAccountId acctId(accountId);

    QMailAccount account(acctId);
    QMailFolderId foldId = account.standardFolder(QMailFolder::InboxFolder);
    if(foldId.isValid()) {
        bool messagesToSend = hasMessagesInOutbox(acctId);
        m_enqueing = true;
        enqueue(new ExportUpdates(m_retrievalAction.data(),acctId));
        enqueue(new RetrieveFolderList(m_retrievalAction.data(), acctId, QMailFolderId(), true));
        if (!messagesToSend) {
            m_enqueing = false;
        }
        enqueue(new RetrieveMessageList(m_retrievalAction.data(), acctId, foldId, minimum));
        if (messagesToSend) {
            m_enqueing = false;
            // send any message in the outbox
            enqueue(new TransmitMessages(m_transmitAction.data(), acctId));
        }
    }
    //Account was never synced, retrieve list of folders and come back here.
    else {
        connect(this, SIGNAL(standardFoldersCreated(const QMailAccountId &)),
                this, SLOT(onStandardFoldersCreated(const QMailAccountId &)));
        m_enqueing = true;
        enqueue(new RetrieveFolderList(m_retrievalAction.data(), acctId, QMailFolderId(), true));
        m_enqueing = false;
        enqueue(new CreateStandardFolders(m_retrievalAction.data(), acctId));
    }
}

//Sync accounts list (both ways)
void EmailAgent::syncAccounts(const QMailAccountIdList &accountIdList, const bool syncOnlyInbox, const uint minimum)
{
    if (accountIdList.isEmpty()) {
        qDebug() << Q_FUNC_INFO << "No enabled accounts, nothing to do.";
        emit synchronizingChanged(EmailAgent::Error);
        return;
    } else {
        foreach (QMailAccountId accountId, accountIdList) {
            if (syncOnlyInbox) {
                synchronizeInbox(accountId.toULongLong(), minimum);
            }
            else {
                enqueue(new Synchronize(m_retrievalAction.data(), accountId));
            }
        }
    }
}

// ############## Private API #########################

bool EmailAgent::actionInQueue(QSharedPointer<EmailAction> action) const
{
    //check current first, there's chances that
    //user taps same action several times.
    if (!m_currentAction.isNull()
        && *(m_currentAction.data()) == *(action.data())) {
        return true;
    }
    else {
        return actionInQueueId(action) != quint64(0);
    }
    return false;
}

quint64 EmailAgent::actionInQueueId(QSharedPointer<EmailAction> action) const
{
    foreach (const QSharedPointer<EmailAction> &a, m_actionQueue) {
        if (*(a.data()) == *(action.data())) {
            return a.data()->id();
        }
    }
    return quint64(0);
}

void EmailAgent::dequeue()
{
    if(m_actionQueue.isEmpty()) {
        qDebug() << "Error: can't dequeue a emtpy list";
    }
    else {
        m_actionQueue.removeFirst();
    }
}

quint64 EmailAgent::enqueue(EmailAction *actionPointer)
{
#ifdef OFFLINE
    Q_ASSERT(actionPointer);
    QSharedPointer<EmailAction> action(actionPointer);
    bool foundAction = actionInQueue(action);

    if (!foundAction) {

        // Check if action neeeds connectivity and if we are not running from a background process
        if(action->needsNetworkConnection()) {
            //discard action in this case
            m_synchronizing = false;
            qDebug() << "Discarding online action!!";
            emit synchronizingChanged(EmailAgent::Completed);
            return quint64(0);
        } else {

            // It's a new action.
            action->setId(newAction());

            // Attachment dowload
            if (action->type() == EmailAction::RetrieveMessagePart) {
                RetrieveMessagePart* messagePartAction = static_cast<RetrieveMessagePart *>(action.data());
                if (messagePartAction->isAttachment()) {
                    AttachmentInfo attInfo;
                    attInfo.status = Queued;
                    attInfo.progress = 0;
                    m_attachmentDownloadQueue.insert(messagePartAction->partLocation(), attInfo);
                    emit attachmentDownloadStatusChanged(messagePartAction->partLocation(), attInfo.status);
                }
            }

            m_actionQueue.append(action);

            if (!m_enqueing && m_currentAction.isNull()) {
                // Nothing is running, start first action.
                m_currentAction = getNext();
                executeCurrent();
            }
        }
        return action->id();
    }
    else {
        qWarning() << "This request already exists in the queue: " << action->description();
        qDebug() << "Number of actions in the queue: " << m_actionQueue.size();
        return actionInQueueId(action);
    }
#else
    Q_ASSERT(actionPointer);
    QSharedPointer<EmailAction> action(actionPointer);
    bool foundAction = actionInQueue(action);

    // Check if action neeeds connectivity and if we are not running from a background process
    if(action->needsNetworkConnection() && !backgroundProcess() && !isOnline()) {
        if (m_backgroundProcess) {
            qDebug() << "Network not available to execute background action, exiting...";
            m_synchronizing = false;
            emit synchronizingChanged(EmailAgent::Error);
            return quint64(0);
        } else {
            // Request connection. Expecting the application to handle this.
            // Actions will be resumed on onlineStateChanged signal.
            emit networkConnectionRequested();
        }
    }

    if (!foundAction) {
        // It's a new action.
        action->setId(newAction());

        // Attachment dowload
        if (action->type() == EmailAction::RetrieveMessagePart) {
            RetrieveMessagePart* messagePartAction = static_cast<RetrieveMessagePart *>(action.data());
            if (messagePartAction->isAttachment()) {
                AttachmentInfo attInfo;
                attInfo.status = Queued;
                attInfo.progress = 0;
                m_attachmentDownloadQueue.insert(messagePartAction->partLocation(), attInfo);
                emit attachmentDownloadStatusChanged(messagePartAction->partLocation(), attInfo.status);
            }
        }

        m_actionQueue.append(action);

        if (!m_enqueing && m_currentAction.isNull()) {
            // Nothing is running, start first action.
            m_currentAction = getNext();
            executeCurrent();
        }
        return action->id();
    } else {
        qWarning() << "This request already exists in the queue: " << action->description();
        qDebug() << "Number of actions in the queue: " << m_actionQueue.size();
        return actionInQueueId(action);
    }
#endif
}

void EmailAgent::executeCurrent()
{
    Q_ASSERT (!m_currentAction.isNull());

    if (!QMailStore::instance()->isIpcConnectionEstablished()) {
        if (m_backgroundProcess) {
            qDebug() << "IPC not connected to execute background action, exiting...";
            m_synchronizing = false;
            emit synchronizingChanged(EmailAgent::Error);
        } else {
            qWarning() << "Ipc connection not established, can't execute service action";
            m_waitForIpc = true;
        }
    } else if (m_currentAction->needsNetworkConnection() && !isOnline()) {
        qDebug() << "Current action not executed, waiting for newtwork";
        if (m_backgroundProcess) {
            qDebug() << "Network not available to execute background action, exiting...";
            m_synchronizing = false;
            emit synchronizingChanged(EmailAgent::Error);
        }
    } else {
        if (!m_synchronizing) {
            m_synchronizing = true;
            emit synchronizingChanged(EmailAgent::Synchronizing);
        }

        QMailAccountId aId = m_currentAction->accountId();
        if (aId.isValid() && m_accountSynchronizing != aId.toULongLong()) {
            m_accountSynchronizing = aId.toULongLong();
            emit currentSynchronizingAccountIdChanged();
        }

        qDebug() << "Executing " << m_currentAction->description();

        // Attachment download
        if (m_currentAction->type() == EmailAction::RetrieveMessagePart) {
            RetrieveMessagePart* messagePartAction = static_cast<RetrieveMessagePart *>(m_currentAction.data());
            if (messagePartAction->isAttachment()) {
                updateAttachmentDowloadStatus(messagePartAction->partLocation(), Downloading);
            }
        } else if (m_currentAction->type() == EmailAction::Transmit) {
            m_transmitting = true;
        }
        m_currentAction->execute();
    }
}

QSharedPointer<EmailAction> EmailAgent::getNext()
{
    if(m_actionQueue.isEmpty())
        return QSharedPointer<EmailAction>();

    return m_actionQueue.first();
}

quint64 EmailAgent::newAction()
{
    return quint64(++m_actionCount);
}

bool EmailAgent::isOnline()
{
    return m_nmanager->isOnline();
}

void EmailAgent::reportError(const QMailAccountId &accountId, const QMailServiceAction::Status::ErrorCode &errorCode)
{
    switch (errorCode) {
    case QMailServiceAction::Status::ErrFrameworkFault:
    case QMailServiceAction::Status::ErrSystemError:
    case QMailServiceAction::Status::ErrInternalServer:
    case QMailServiceAction::Status::ErrUnknownResponse:
    case QMailServiceAction::Status::ErrEnqueueFailed:
    case QMailServiceAction::Status::ErrNoConnection:
    case QMailServiceAction::Status::ErrConnectionInUse:
    case QMailServiceAction::Status::ErrConnectionNotReady:
    case QMailServiceAction::Status::ErrConfiguration:
    case QMailServiceAction::Status::ErrInvalidAddress:
    case QMailServiceAction::Status::ErrInvalidData:
    case QMailServiceAction::Status::ErrTimeout:
    case QMailServiceAction::Status::ErrInternalStateReset:
        emit error(accountId.toULongLong(), SyncFailed);
        break;
    case QMailServiceAction::Status::ErrLoginFailed:
        emit error(accountId.toULongLong(), LoginFailed);
        break;
    case QMailServiceAction::Status::ErrFileSystemFull:
        emit error(accountId.toULongLong(), DiskFull);
        break;
    default:
        break;
    }
}

void EmailAgent::removeAction(quint64 actionId)
{
    for (int i = 0; i < m_actionQueue.size(); ++i) {
        if (m_actionQueue.at(i).data()->id() == actionId) {
            m_actionQueue.removeAt(i);
            return;
        }
    }
}

void EmailAgent::saveAttachmentToDownloads(const QMailMessageId messageId, const QString &attachmentLocation)
{
    QString attachmentDownloadFolder = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation) + "/mail_attachments/" + attachmentLocation;

    // Message and part structure can be updated during attachment download
    // is safer to reload everything
    QMailMessage message (messageId);
    for (uint i = 0; i < message.partCount(); i++) {
        QMailMessagePart sourcePart = message.partAt(i);
        if (attachmentLocation == sourcePart.location().toString(true)) {
            QString attachmentPath = attachmentDownloadFolder + "/" + sourcePart.displayName();
            QFile attachmentFile(attachmentPath);

            if (attachmentFile.exists()) {
                emit attachmentUrlChanged(attachmentLocation, attachmentDownloadFolder);
                updateAttachmentDowloadStatus(attachmentLocation, Downloaded);
            } else {
                QString path = sourcePart.writeBodyTo(attachmentDownloadFolder);
                if (!path.isEmpty()) {
                    emit attachmentUrlChanged(attachmentLocation, path);
                    updateAttachmentDowloadStatus(attachmentLocation, Downloaded);
                } else {
                    updateAttachmentDowloadStatus(attachmentLocation, FailedToSave);
                }
            }
        }
    }
}

void EmailAgent::updateAttachmentDowloadStatus(const QString &attachmentLocation, AttachmentStatus status)
{
    if (m_attachmentDownloadQueue.contains(attachmentLocation)) {
        if (status == Failed) {
            emit attachmentDownloadStatusChanged(attachmentLocation, status);
            emit attachmentDownloadProgressChanged(attachmentLocation, 0);
            m_attachmentDownloadQueue.remove(attachmentLocation);
        } else if (status == Downloaded) {
            emit attachmentDownloadStatusChanged(attachmentLocation, status);
            emit attachmentDownloadProgressChanged(attachmentLocation, 100);
            m_attachmentDownloadQueue.remove(attachmentLocation);
        } else {
            AttachmentInfo attInfo = m_attachmentDownloadQueue.value(attachmentLocation);
            attInfo.status = status;
            m_attachmentDownloadQueue.insert(attachmentLocation, attInfo);
            emit attachmentDownloadStatusChanged(attachmentLocation, status);
        }
    }
}

void EmailAgent::updateAttachmentDowloadProgress(const QString &attachmentLocation, int progress)
{
    if (m_attachmentDownloadQueue.contains(attachmentLocation)) {
        AttachmentInfo attInfo = m_attachmentDownloadQueue.value(attachmentLocation);
        // Avoid reporting progress too often
        if (progress >= attInfo.progress + 5) {
            attInfo.progress = progress;
            m_attachmentDownloadQueue.insert(attachmentLocation, attInfo);
            emit attachmentDownloadProgressChanged(attachmentLocation, progress);
        }
    }
}
