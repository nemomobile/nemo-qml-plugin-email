/*
 * Copyright 2011 Intel Corporation.
 * Copyright (C) 2012-2015 Jolla Ltd.
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

Q_LOGGING_CATEGORY(lcGeneral, "org.nemomobile.email.general")
Q_LOGGING_CATEGORY(lcDebug, "org.nemomobile.email.debug")

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
    , m_sendFailed(false)
    , m_retrievalAction(new QMailRetrievalAction(this))
    , m_storageAction(new QMailStorageAction(this))
    , m_transmitAction(new QMailTransmitAction(this))
    , m_searchAction(new QMailSearchAction(this))
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

    connect(m_searchAction.data(), SIGNAL(activityChanged(QMailServiceAction::Activity)),
            this, SLOT(activityChanged(QMailServiceAction::Activity)));

    connect(m_searchAction.data(), SIGNAL(messageIdsMatched(const QMailMessageIdList&)),
            this, SIGNAL(searchMessageIdsMatched(const QMailMessageIdList&)));

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
    if (m_currentAction && (m_currentAction->id() == actionId)) {
        if(m_currentAction->serviceAction()->isRunning()) {
            m_cancellingSingleAction = true;
            m_currentAction->serviceAction()->cancelOperation();
        } else {
            m_currentAction.reset();
            processNextAction();
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

void EmailAgent::exportUpdates(const QMailAccountIdList &accountIdList)
{
    if (!m_enqueing && accountIdList.size()) {
        m_enqueing = true;
    }
    for (int i = 0; i < accountIdList.size(); i++) {
        if (i+1 == accountIdList.size()) {
            m_enqueing = false;
        }
        enqueue(new ExportUpdates(m_retrievalAction.data(), accountIdList.at(i)));
    }
}

bool EmailAgent::hasMessagesInOutbox(const QMailAccountId &accountId)
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
    qCDebug(lcGeneral) << Q_FUNC_INFO << "Starting messageserver process...";
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

bool EmailAgent::isOnline()
{
    return m_nmanager->isOnline();
}

void EmailAgent::searchMessages(const QMailMessageKey &filter,
                                const QString &bodyText, QMailSearchAction::SearchSpecification spec,
                                quint64 limit, bool searchBody, const QMailMessageSortKey &sort)
{
    // Only one search action should be running at time,
    // cancel any running or queued
    m_enqueing = true;
    cancelSearch();
    qCDebug(lcDebug) << "Enqueuing new search " << bodyText;
    m_enqueing = false;
    enqueue(new SearchMessages(m_searchAction.data(), filter, bodyText, spec, limit, searchBody, sort));
}

void EmailAgent::cancelSearch()
{
    //cancel running action if is search
    if (m_currentAction && (m_currentAction->type() == EmailAction::Search)) {
        if (m_currentAction->serviceAction()->isRunning()) {
            m_cancellingSingleAction = true;
            m_currentAction->serviceAction()->cancelOperation();
        } else {
            m_currentAction.reset();
            processNextAction();
        }
    }
    // Starts from 1 since top of the queue will be removed if above conditions are met.
    for (int i = 1; i < m_actionQueue.size();) {
         if (m_actionQueue.at(i).data()->type() == EmailAction::Search) {
            m_actionQueue.removeAt(i);
            qCDebug(lcDebug) <<  "Search action removed from the queue";
        } else {
            ++i;
        }
    }
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

    QMailDisconnected::moveToFolder(ids, destinationId);

    exportUpdates(QMailAccountIdList() << accountId);
}

void EmailAgent::sendMessage(const QMailMessageId &messageId)
{
    if (messageId.isValid()) {
        enqueue(new TransmitMessage(m_transmitAction.data(), messageId));
    }
}

void EmailAgent::sendMessages(const QMailAccountId &accountId)
{
    if (accountId.isValid()) {
        enqueue(new TransmitMessages(m_transmitAction.data(),accountId));
    }
}

void EmailAgent::setMessagesReadState(const QMailMessageIdList &ids, bool state)
{
    Q_ASSERT(!ids.empty());
    QMailAccountIdList accountIdList;
    // Messages can be from several accounts
    foreach (const QMailMessageId &id, ids) {
       QMailAccountId accountId = accountForMessageId(id);
       if (!accountIdList.contains(accountId)) {
           accountIdList.append(accountId);
       }
    }

    QMailStore::instance()->updateMessagesMetaData(QMailMessageKey::id(ids), QMailMessage::Read, state);
    exportUpdates(accountIdList);
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
            qCDebug(lcGeneral) << "Canceled by the user";
            break;
        } else if (m_cancellingSingleAction) {
            if (m_currentAction->type() == EmailAction::Search) {
                qCDebug(lcGeneral) << "Search canceled by the user";
                emitSearchStatusChanges(m_currentAction, EmailAgent::SearchCanceled);
            }
            dequeue();
            m_currentAction.clear();
            qCDebug(lcGeneral) << "Single action canceled by the user";
            m_cancellingSingleAction = false;
            m_accountSynchronizing = -1;
            emit currentSynchronizingAccountIdChanged();
            if (m_actionQueue.empty()) {
                m_synchronizing = false;
                m_accountSynchronizing = -1;
                emit currentSynchronizingAccountIdChanged();
                emit synchronizingChanged(EmailAgent::Completed);
                qCDebug(lcGeneral) << "Sync completed.";
            } else {
                processNextAction();
            }
            break;
        } else {
            // Report the error
            dequeue();
            if (m_currentAction->type() == EmailAction::Transmit) {
                m_transmitting = false;
                m_sendFailed = true;
                emit sendCompleted(false);
                qCDebug(lcGeneral) << "Error: Send failed";
            }

            if (m_currentAction->type() == EmailAction::Search) {
                qCDebug(lcGeneral) << "Error: Search failed";
                emitSearchStatusChanges(m_currentAction, EmailAgent::SearchFailed);
            }

            if (m_currentAction->type() == EmailAction::RetrieveMessagePart) {
                RetrieveMessagePart* messagePartAction = static_cast<RetrieveMessagePart *>(m_currentAction.data());
                if (messagePartAction->isAttachment()) {
                    updateAttachmentDowloadStatus(messagePartAction->partLocation(), Failed);
                    qCDebug(lcGeneral) << "Attachment dowload failed for " << messagePartAction->partLocation();
                } else {
                    emit messagePartDownloaded(messagePartAction->messageId(), messagePartAction->partLocation(), false);
                    qCDebug(lcGeneral) << "Failed to dowload message part!!";
                }
            }

            if (m_currentAction->type() == EmailAction::RetrieveMessages) {
                RetrieveMessages* retrieveMessagesAction = static_cast<RetrieveMessages *>(m_currentAction.data());
                emit messagesDownloaded(retrieveMessagesAction->messageIds(), false);
                qCDebug(lcGeneral) << "Failed to download messages";
            }

            reportError(status.accountId, status.errorCode);
            processNextAction(true);
            break;
        }

    case QMailServiceAction::Successful:
        dequeue();
        if (m_currentAction->type() == EmailAction::Transmit) {
            qCDebug(lcGeneral) << "Finished sending for " << m_currentAction->accountId();
            m_transmitting = false;
            emit sendCompleted(true);
        }

        if (m_currentAction->type() == EmailAction::Search) {
            qCDebug(lcGeneral) << "Search done";
            emitSearchStatusChanges(m_currentAction, EmailAgent::SearchDone);
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

        processNextAction();
        break;

    default:
        //emit acctivity changed here
        qCDebug(lcDebug) << "Activity State Changed:" << activity;
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
            qCDebug(lcDebug) << "Ipc connection established, but no action in the queue.";
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
    qCDebug(lcGeneral) << Q_FUNC_INFO << "Online State changed, device is now connected ? " << isOnline;
    if (isOnline) {
        if (m_currentAction.isNull())
            m_currentAction = getNext();

        if (m_currentAction.isNull()) {
            qCDebug(lcDebug) << "Network connection established, but no action in the queue.";
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
        qCCritical(lcGeneral) << "Error: Inbox not found!!!";
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
    qCDebug(lcDebug) << "Enabled accounts size is: " << m_enabledAccounts.count();

    if (m_enabledAccounts.isEmpty()) {
        qCDebug(lcDebug) << Q_FUNC_INFO << "No enabled accounts, nothing to do.";
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
        qCDebug(lcDebug) << "Error: Can't create a folder with empty name";
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

    bool exptUpdates;

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
            exptUpdates = true;
        }
    } else {
        QMapIterator<QMailAccountId, QMailMessageIdList> iter(accountMap);
        while (iter.hasNext()) {
            iter.next();
            QMailAccount account(iter.key());
            QMailFolderId trashFolderId = account.standardFolder(QMailFolder::TrashFolder);
            // If standard folder is not valid we use local storage
            if (!trashFolderId.isValid()) {
                qCDebug(lcGeneral) << "Trash folder not found using local storage";
                trashFolderId = QMailFolder::LocalStorageFolderId;
            }
            m_enqueing = true;
            enqueue(new MoveToFolder(m_storageAction.data(), iter.value(), trashFolderId));
            enqueue(new FlagMessages(m_storageAction.data(), iter.value(), QMailMessage::Trash, 0));
            if (!iter.hasNext()) {
                m_enqueing = false;
            }
        }
        exptUpdates = true;
    }

    // Do online actions at the end
    if (exptUpdates) {
        // Export updates for all accounts that we deleted messages from
        QMailAccountIdList accountList = accountMap.uniqueKeys();
        exportUpdates(accountList);
    }
}

void EmailAgent::expungeMessages(const QMailMessageIdList &ids)
{
    m_enqueing = true;
    enqueue(new DeleteMessages(m_storageAction.data(), ids));

    QMailAccountIdList accountList;
    // Messages can be from several accounts
    foreach (const QMailMessageId &id, ids) {
        QMailAccountId accountId = accountForMessageId(id);
        if (!accountList.contains(accountId)) {
            accountList.append(accountId);
        }
    }

    // Export updates for all accounts that we deleted messages from
    exportUpdates(accountList);
}

void EmailAgent::downloadAttachment(int messageId, const QString &attachmentLocation)
{
    m_messageId = QMailMessageId(messageId);
    const QMailMessage message(m_messageId);
    QMailMessagePart::Location location(attachmentLocation);
    if (message.contains(location)) {
        const QMailMessagePart attachmentPart = message.partAt(location);
        location.setContainingMessageId(m_messageId);
        if (attachmentPart.hasBody()) {
            saveAttachmentToDownloads(m_messageId, attachmentLocation);
        } else {
            qCDebug(lcDebug) << "Start Download for: " << attachmentLocation;
            enqueue(new RetrieveMessagePart(m_retrievalAction.data(), location, true));
        }
    } else {
       qCDebug(lcDebug) << "ERROR: Attachment location not found " << attachmentLocation;
    }
}

void EmailAgent::exportUpdates(int accountId)
{
    QMailAccountId acctId(accountId);

    if (acctId.isValid()) {
        exportUpdates(QMailAccountIdList() << acctId);
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
    qCDebug(lcGeneral) << "Error: Standard folder " << folder << " not found for account: " << accountId;
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
    exportUpdates(QMailAccountIdList() << accountForMessageId(id));
}

void EmailAgent::markMessageAsUnread(int messageId)
{
    QMailMessageId id(messageId);
    quint64 status(QMailMessage::Read);
    QMailStore::instance()->updateMessagesMetaData(QMailMessageKey::id(id), status, false);
    exportUpdates(QMailAccountIdList() << accountForMessageId(id));
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
        qCDebug(lcDebug) << "Error: Can't rename a folder to a empty name";
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
        qCDebug(lcDebug) << Q_FUNC_INFO << "No enabled accounts, nothing to do.";
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
        qCWarning(lcGeneral) << "Error: can't dequeue a emtpy list";
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
            qCDebug(lcGeneral) << "Discarding online action!!";
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
        qCWarning(lcGeneral) << "This request already exists in the queue: " << action->description();
        qCDebug(lcDebug) << "Number of actions in the queue: " << m_actionQueue.size();
        return actionInQueueId(action);
    }
#else
    Q_ASSERT(actionPointer);
    QSharedPointer<EmailAction> action(actionPointer);
    bool foundAction = actionInQueue(action);

    // Check if action neeeds connectivity and if we are not running from a background process
    if(action->needsNetworkConnection() && !backgroundProcess() && !isOnline()) {
        if (m_backgroundProcess) {
            qCDebug(lcDebug) << "Network not available to execute background action, exiting...";
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
    }

    if (!m_enqueing && (m_currentAction.isNull() || !m_currentAction->serviceAction()->isRunning())) {
        // Nothing is running or current action is in waiting state, start first action.
        QSharedPointer<EmailAction> nextAction = getNext();
        if (m_currentAction.isNull() || (!nextAction.isNull() && (*(m_currentAction.data()) != *(nextAction.data())))) {
            m_currentAction = nextAction;
            executeCurrent();
        }
    }

    if (!foundAction) {
         return action->id();
    } else {
        qCWarning(lcGeneral) << "This request already exists in the queue: " << action->description();
        qCDebug(lcDebug) << "Number of actions in the queue: " << m_actionQueue.size();
        return actionInQueueId(action);
    }

#endif
}

void EmailAgent::executeCurrent()
{
    Q_ASSERT (!m_currentAction.isNull());

    if (!QMailStore::instance()->isIpcConnectionEstablished()) {
        if (m_backgroundProcess) {
            qCWarning(lcGeneral) << "IPC not connected to execute background action, exiting...";
            m_synchronizing = false;
            emit synchronizingChanged(EmailAgent::Error);
        } else {
            qCWarning(lcGeneral) << "Ipc connection not established, can't execute service action";
            m_waitForIpc = true;
        }
    } else if (m_currentAction->needsNetworkConnection() && !isOnline()) {
        qCDebug(lcGeneral) << "Current action not executed, waiting for newtwork";
        if (m_backgroundProcess) {
            qCWarning(lcGeneral) << "Network not available to execute background action, exiting...";
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

        qCDebug(lcGeneral) << "Executing " << m_currentAction->description();

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

    QSharedPointer<EmailAction> firstAction = m_actionQueue.first();
    // if we are offline move the first offline action to the top of the queue if one exists
    if (!isOnline() && firstAction->needsNetworkConnection() && m_actionQueue.size() > 1) {
        for (int i = 1; i < m_actionQueue.size(); i++) {
            QSharedPointer<EmailAction> action = m_actionQueue.at(i);
            if (!action->needsNetworkConnection()) {
                m_actionQueue.move(i,0);
                return action;
            }
        }
    }
    return firstAction;
}

void EmailAgent::processNextAction(bool error)
{
    m_currentAction = getNext();
    if (m_currentAction.isNull()) {
        m_synchronizing = false;
        m_accountSynchronizing = -1;
        emit currentSynchronizingAccountIdChanged();
        if (error) {
            emit synchronizingChanged(EmailAgent::Error);
            qCDebug(lcGeneral) << "Sync completed with Errors!!!.";
        } else {
            emit synchronizingChanged(EmailAgent::Completed);
            qCDebug(lcGeneral) << "Sync completed.";
        }
    } else {
        executeCurrent();
    }
}

quint64 EmailAgent::newAction()
{
    return quint64(++m_actionCount);
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
    case QMailServiceAction::Status::ErrTimeout:
    case QMailServiceAction::Status::ErrInternalStateReset:
        if (m_sendFailed) {
            m_sendFailed = false;
            emit error(accountId.toULongLong(), SendFailed);
        } else {
            emit error(accountId.toULongLong(), SyncFailed);
        }
        break;
    case QMailServiceAction::Status::ErrLoginFailed:
        emit error(accountId.toULongLong(), LoginFailed);
        break;
    case QMailServiceAction::Status::ErrFileSystemFull:
        emit error(accountId.toULongLong(), DiskFull);
        break;
    case QMailServiceAction::Status::ErrConfiguration:
    case QMailServiceAction::Status::ErrInvalidAddress:
    case QMailServiceAction::Status::ErrInvalidData:
    case QMailServiceAction::Status::ErrNotImplemented:
    case QMailServiceAction::Status::ErrNoSslSupport:
        emit error(accountId.toULongLong(), InvalidConfiguration);
        break;
    case QMailServiceAction::Status::ErrUntrustedCertificates:
        emit error(accountId.toULongLong(), UntrustedCertificates);
        break;
    case QMailServiceAction::Status::ErrCancel:
        // The operation was cancelled by user intervention.
        break;
    default:
        emit error(accountId.toULongLong(), InternalError);
        break;
    }
}

void EmailAgent::removeAction(quint64 actionId)
{
    for (int i = 0; i < m_actionQueue.size();) {
        if (m_actionQueue.at(i).data()->id() == actionId) {
            m_actionQueue.removeAt(i);
            return;
        } else {
            ++i;
        }
    }
}

void EmailAgent::saveAttachmentToDownloads(const QMailMessageId messageId, const QString &attachmentLocation)
{
    QString attachmentDownloadFolder = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation) + "/mail_attachments/" + attachmentLocation;
    // Message and part structure can be updated during attachment download
    // is safer to reload everything
    const QMailMessage message (messageId);
    const QMailMessagePart::Location location(attachmentLocation);
    if (message.contains(location)) {
        const QMailMessagePart attachmentPart = message.partAt(location);
        QString attachmentPath = attachmentDownloadFolder + "/" + attachmentPart.displayName();
        QFile attachmentFile(attachmentPath);

        if (attachmentFile.exists()) {
            emit attachmentUrlChanged(attachmentLocation, attachmentDownloadFolder);
            updateAttachmentDowloadStatus(attachmentLocation, Downloaded);
        } else {
            QString path = attachmentPart.writeBodyTo(attachmentDownloadFolder);
            if (!path.isEmpty()) {
                emit attachmentUrlChanged(attachmentLocation, path);
                updateAttachmentDowloadStatus(attachmentLocation, Downloaded);
            } else {
                qCDebug(lcDebug) << "ERROR: Failed to save attachment file to location " << attachmentDownloadFolder;
                updateAttachmentDowloadStatus(attachmentLocation, FailedToSave);
            }
        }
    } else {
        qCDebug(lcDebug) << "ERROR: Can't save attachment, location not found " << attachmentLocation;
    }
}

void EmailAgent::updateAttachmentDowloadStatus(const QString &attachmentLocation, AttachmentStatus status)
{
    if (status == Failed) {
        emit attachmentDownloadStatusChanged(attachmentLocation, status);
        emit attachmentDownloadProgressChanged(attachmentLocation, 0);
        m_attachmentDownloadQueue.remove(attachmentLocation);
    } else if (status == Downloaded) {
        emit attachmentDownloadStatusChanged(attachmentLocation, status);
        emit attachmentDownloadProgressChanged(attachmentLocation, 100);
        m_attachmentDownloadQueue.remove(attachmentLocation);
    } else if (m_attachmentDownloadQueue.contains(attachmentLocation)) {
        AttachmentInfo attInfo = m_attachmentDownloadQueue.value(attachmentLocation);
        attInfo.status = status;
        m_attachmentDownloadQueue.insert(attachmentLocation, attInfo);
        emit attachmentDownloadStatusChanged(attachmentLocation, status);
    } else {
        updateAttachmentDowloadStatus(attachmentLocation, Failed);
        qCDebug(lcDebug) << "ERROR: Can't update attachment download status for items outside of the download queue, part location: "
                         << attachmentLocation;
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

void EmailAgent::emitSearchStatusChanges(QSharedPointer<EmailAction> action, EmailAgent::SearchStatus status)
{
    SearchMessages* searchAction = static_cast<SearchMessages *>(action.data());
    if (searchAction) {
        qCDebug(lcDebug) << "Search completed for " << searchAction->searchText();
        emit searchCompleted(searchAction->searchText(), m_searchAction->matchingMessageIds(), searchAction->isRemote(), m_searchAction->remainingMessagesCount(), status);
    } else {
        qCDebug(lcDebug) << "Error: Invalid search action.";
    }
}
