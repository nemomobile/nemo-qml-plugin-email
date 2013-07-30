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
#include <QProcess>

#include <qmailnamespace.h>
#include <qmailaccount.h>
#include <qmailstore.h>
#include <qmaildisconnected.h>

#include "emailagent.h"
#include "emailaction.h"

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#define MESSAGESERVER "/usr/bin/messageserver"
#else
#define MESSAGESERVER "/usr/bin/messageserver5"
#endif

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
    , m_transmitting(false)
    , m_cancelling(false)
    , m_synchronizing(false)
    , m_enqueing(false)
    , m_waitForIpc(false)
    , m_retrievalAction(new QMailRetrievalAction(this))
    , m_storageAction(new QMailStorageAction(this))
    , m_transmitAction(new QMailTransmitAction(this))
{

    connect(QMailStore::instance(), SIGNAL(ipcConnectionEstablished()),
            this, SLOT(onIpcConnectionEstablished()));

    initMailServer();
    setupAccountFlags();

    connect(m_transmitAction.data(), SIGNAL(progressChanged(uint, uint)),
            this, SLOT(progressChanged(uint,uint)));

    connect(m_retrievalAction.data(), SIGNAL(activityChanged(QMailServiceAction::Activity)),
            this, SLOT(activityChanged(QMailServiceAction::Activity)));

    connect(m_storageAction.data(), SIGNAL(activityChanged(QMailServiceAction::Activity)),
            this, SLOT(activityChanged(QMailServiceAction::Activity)));

    connect(m_transmitAction.data(), SIGNAL(activityChanged(QMailServiceAction::Activity)),
            this, SLOT(activityChanged(QMailServiceAction::Activity)));

    m_instance = this;
}

EmailAgent::~EmailAgent()
{
}

QString EmailAgent::bodyPlainText(const QMailMessage &mailMsg) const
{
    if (QMailMessagePartContainer *container = mailMsg.findPlainTextContainer()) {
        return container->body().data();
    }

    return QString();
}

void EmailAgent::exportUpdates(const QMailAccountId accountId)
{
    enqueue(new ExportUpdates(m_retrievalAction.data(),accountId));
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

quint64 EmailAgent::newAction()
{
    return quint64(++m_actionCount);
}

void EmailAgent::sendMessages(const QMailAccountId &accountId)
{
    if (accountId.isValid()) {
        m_transmitting = true;
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
            emit synchronizingChanged();
            m_transmitting = false;
            m_cancelling = false;
            m_actionQueue.clear();
            emit error(status.accountId, status.text, status.errorCode);
            qDebug() << "Canceled by the user";
            break;
        } else {
            // Report the error
            dequeue();
            if (m_currentAction->type() == EmailAction::Transmit) {
                m_transmitting = false;
                emit sendCompleted();
                qDebug() << "Error: Send failed";
            }

            m_currentAction = getNext();
            emit error(status.accountId, status.text, status.errorCode);

            if (m_currentAction.isNull()) {
                qDebug() << "Sync completed with Errors!!!.";
                m_synchronizing = false;
                emit synchronizingChanged();
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

        if(m_currentAction->type() == EmailAction::RetrieveFolderList) {
            emit folderRetrievalCompleted(m_currentAction->accountId());
        }

        m_currentAction = getNext();

        if (m_currentAction.isNull()) {
            qDebug() << "Sync completed.";
            m_synchronizing = false;
            emit synchronizingChanged();
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

void EmailAgent::attachmentDownloadActivityChanged(QMailServiceAction::Activity activity)
{
    if (QMailServiceAction *action = static_cast<QMailServiceAction*>(sender())) {
        if (activity == QMailServiceAction::Successful) {
            if (action == m_attachmentRetrievalAction) {
                QMailMessage message(m_messageId);
                for (uint i = 1; i < message.partCount(); ++i) {
                    const QMailMessagePart &sourcePart(message.partAt(i));
                    if (sourcePart.location().toString(false) == m_attachmentPart.location().toString(false)) {
                        QString downloadFolder = QDir::homePath() + "/Downloads/";
                        sourcePart.writeBodyTo(downloadFolder);
                        emit attachmentDownloadCompleted();
                    }
                }
            }
        }
        else if (activity == QMailServiceAction::Failed) {
            emit attachmentDownloadCompleted();
        }
    }
}

void EmailAgent::onIpcConnectionEstablished()
{
    if (m_waitForIpc) {
        m_waitForIpc = false;

        if (m_currentAction.isNull())
            m_currentAction = getNext();

        if (m_currentAction.isNull()) {
            qDebug() << "Ipc connection established, no action in the queue.";
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
    qDebug() << "progress " << value << " of " << total;
    int percent = (value * 100) / total;
    emit progressUpdate (percent);
}

// ############# Invokable API ########################

//Sync all accounts (both ways)
void EmailAgent::accountsSync(const bool syncOnlyInbox, const uint minimum)
{
    m_enabledAccounts.clear();
    QMailAccountKey enabledAccountKey = QMailAccountKey::status(QMailAccount::Enabled |
                                                         QMailAccount::CanRetrieve |
                                                         QMailAccount::CanTransmit,
                                                         QMailDataComparator::Includes);
    m_enabledAccounts = QMailStore::instance()->queryAccounts(enabledAccountKey);

    if (m_enabledAccounts.isEmpty()) {
        qDebug() << Q_FUNC_INFO << "No enabled accounts, nothing to do.";
        emit synchronizingChanged();
        return;
    } else {
        foreach (QMailAccountId accountId, m_enabledAccounts) {
            if (syncOnlyInbox) {
                synchronizeInbox(accountId.toULongLong(), minimum);
            }
            else {
                enqueue(new Synchronize(m_retrievalAction.data(), accountId));
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
    if (((m_currentAction->serviceAction())->activity() == QMailServiceAction::Pending ||
         (m_currentAction->serviceAction())->activity() == QMailServiceAction::InProgress)) {
        (m_currentAction->serviceAction())->cancelOperation();
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

    enqueue(new OnlineDeleteFolder(m_storageAction.data(),id));
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

    QMailMessageId id(ids[0]);
    QMailAccountId accountId = accountForMessageId(id);

    if (m_transmitting) {
        // Do not delete messages from the outbox folder while we're sending
        QMailMessageKey outboxFilter(QMailMessageKey::status(QMailMessage::Outbox));
        if (QMailStore::instance()->countMessages(QMailMessageKey::id(ids) & outboxFilter)) {
            //TODO: emit proper error
            return;
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
        if(!idsToRemove.isEmpty())
            enqueue(new DeleteMessages(m_storageAction.data(), idsToRemove));
    }
    else {
        m_enqueing = true;
        enqueue(new MoveToStandardFolder(m_storageAction.data(), ids, QMailFolder::TrashFolder));
        enqueue(new FlagMessages(m_storageAction.data(), ids, QMailMessage::Trash,0));
        m_enqueing = false;
        enqueue(new ExportUpdates(m_retrievalAction.data(),accountId));
    }
}

void EmailAgent::downloadAttachment(int messageId, const QString &attachmentDisplayName)
{
    m_messageId = QMailMessageId(messageId);
    QMailMessage message (m_messageId);

    emit attachmentDownloadStarted();
    qDebug()<<"PartsCount: "<<message.partCount();
    qDebug()<<"attachmentDisplayName: "<<attachmentDisplayName;

    for (uint i = 1; i < message.partCount(); i++) {
        QMailMessagePart sourcePart = message.partAt(i);
        if (attachmentDisplayName == sourcePart.displayName()) {
            QMailMessagePart::Location location = sourcePart.location();
            location.setContainingMessageId(m_messageId);
            if (sourcePart.hasBody()) {
                // The content has already been downloaded to local device.
                QString downloadFolder = QDir::homePath() + "/Downloads/";
                QFile f(downloadFolder+sourcePart.displayName());
                if (f.exists())
                    f.remove();
                sourcePart.writeBodyTo(downloadFolder);
                emit attachmentDownloadCompleted();
                return;
            }
            m_attachmentPart = sourcePart;
            m_attachmentRetrievalAction = new QMailRetrievalAction(this);
            connect (m_attachmentRetrievalAction, SIGNAL(activityChanged(QMailServiceAction::Activity)),
                     this, SLOT(attachmentDownloadActivityChanged(QMailServiceAction::Activity)));
            connect(m_attachmentRetrievalAction, SIGNAL(progressChanged(uint, uint)),
                    this, SLOT(progressChanged(uint,uint)));

            m_attachmentRetrievalAction->retrieveMessagePart(location);
        }
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

int EmailAgent::standardFolderId(int accountId, QMailFolder::StandardFolder folder)
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
    return -1;
}

int EmailAgent::inboxFolderId(int accountId)
{
    return standardFolderId(accountId, QMailFolder::InboxFolder);
}

int EmailAgent::draftsFolderId(int accountId)
{
    return standardFolderId(accountId, QMailFolder::DraftsFolder);
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

bool EmailAgent::openAttachment(const QString & uri)
{
    bool status = true;

    // let's determine the file type
    QString filePath = QDir::homePath() + "/Downloads/" + uri;

    QProcess fileProcess;
    fileProcess.start("file", QStringList() << "-b" << filePath);
    if (!fileProcess.waitForFinished())
        return false;

    QString s(fileProcess.readAll());
    QStringList parameters;
    parameters << "--opengl" << "--fullscreen";
    if (s.contains("video", Qt::CaseInsensitive)) {
        parameters << "--app" << "meego-app-video";
        parameters << "--cmd" << "playVideo";
    }
    else if (s.contains("image", Qt::CaseInsensitive)) {
        parameters << "--app" << "meego-app-photos";
        parameters << "--cmd" << "showPhoto";
    }
    else if (s.contains("audio", Qt::CaseInsensitive)) {
        parameters << "--app" << "meego-app-music";
        parameters << "--cmd" << "playSong";
    }
    else if (s.contains("Ogg data", Qt::CaseInsensitive)) {
        // Fix Me:  need more research on Ogg format. For now, default to video.
        parameters << "--app" << "meego-app-video";
        parameters << "--cmd" << "video";
    }
    else {
        // Unsupported file type.
        return false;
    }

    QString executable("meego-qml-launcher");
    filePath.prepend("file://");
    parameters << "--cdata" << filePath;
    QProcess::startDetached(executable, parameters);

    return status;
}

void EmailAgent::openBrowser(const QString & url)
{
    QString executable("xdg-open");
    QStringList parameters;
    parameters << url;
    QProcess::startDetached(executable, parameters);
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
        m_enqueing = true;
        enqueue(new ExportUpdates(m_retrievalAction.data(),acctId));
        enqueue(new RetrieveFolderList(m_retrievalAction.data(), acctId, QMailFolderId(), true));
        m_enqueing = false;
        enqueue(new RetrieveMessageList(m_retrievalAction.data(), acctId, foldId, minimum));
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
        foreach (const QSharedPointer<EmailAction> &a, m_actionQueue) {
            if (*(a.data()) == *(action.data())) {
                return true;
            }
        }
    }
    return false;
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

void EmailAgent::enqueue(EmailAction *actionPointer)
{
    Q_ASSERT(actionPointer);
    QSharedPointer<EmailAction> action(actionPointer);
    bool foundAction = actionInQueue(action);

   // Add checks for network availablity if online action

    if (!foundAction) {
        // It's a new action.
        action->setId(newAction());
        m_actionQueue.append(action);

        if (!m_enqueing && m_currentAction.isNull()) {
            // Nothing is running, start first action.
            m_currentAction = getNext();
            executeCurrent();
        }
    }
    else {
        qWarning() << "This request already exists in the queue: " << action->description();
        qDebug() << "Number of actions in the queue: " << m_actionQueue.size();
    }
}

void EmailAgent::executeCurrent()
{
    Q_ASSERT (!m_currentAction.isNull());

    if (!QMailStore::instance()->isIpcConnectionEstablished()) {
        qWarning() << "Ipc connection not established, can't execute service action";
        m_waitForIpc = true;
    } else {

        if (!m_synchronizing) {
            m_synchronizing = true;
            emit synchronizingChanged();
        }

        //add network checks here.
        qDebug() << "Executing " << m_currentAction->description();

        m_currentAction->execute();
    }
}

QSharedPointer<EmailAction> EmailAgent::getNext()
{
    if(m_actionQueue.isEmpty())
        return QSharedPointer<EmailAction>();

    return m_actionQueue.first();
}

