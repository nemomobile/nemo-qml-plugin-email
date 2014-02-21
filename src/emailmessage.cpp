/*
 * Copyright 2011 Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 */


#include <QFileInfo>

#include <qmailaccount.h>
#include <qmailstore.h>

#include "emailagent.h"
#include "emailmessage.h"
#include "qmailnamespace.h"
#include <qmaildisconnected.h>
#include <QTextDocument>

EmailMessage::EmailMessage(QObject *parent)
    : QObject(parent)
    , m_originalMessageId(QMailMessageId())
    , m_newMessage(true)
    , m_downloadActionId(0)
{
    setPriority(NormalPriority);
}

EmailMessage::~EmailMessage()
{
}

// ############ Slots ###############
void EmailMessage::onMessagesDownloaded(const QMailMessageIdList &ids, bool success)
{
    foreach (const QMailMessageId id, ids) {
        if (id == m_id) {
            disconnect(EmailAgent::instance(), SIGNAL(messagesDownloaded(QMailMessageIdList,bool)),
                    this, SLOT(onMessagesDownloaded(QMailMessageIdList,bool)));
            if (success) {
                // Reload the message
                m_msg = QMailMessage(m_id);
                m_bodyText = EmailAgent::instance()->bodyPlainText(m_msg);
                emitMessageReloadedSignals();
                emit messageDownloaded();
            } else {
                emit messageDownloadFailed();
            }
            return;
        }
    }
}

void EmailMessage::onMessagePartDownloaded(const QMailMessageId &messageId, const QString &partLocation, bool success)
{
    if (messageId == m_id) {
        // Reload the message
        m_msg = QMailMessage(m_id);
        QMailMessagePartContainer *plainTextcontainer = m_msg.findPlainTextContainer();
        // // Check if is the html text part first
        if (QMailMessagePartContainer *container = m_msg.findHtmlContainer()) {
            QMailMessagePart::Location location = static_cast<const QMailMessagePart *>(container)->location();
            if (location.toString(true) == partLocation) {
                disconnect(EmailAgent::instance(), SIGNAL(messagePartDownloaded(QMailMessageId,QString,bool)),
                        this, SLOT(onMessagePartDownloaded(QMailMessageId,QString,bool)));
                if (success) {
                    emit htmlBodyChanged();
                    // If plain text body is not present we also refresh quotedBody here
                    if (!plainTextcontainer) {
                        emit quotedBodyChanged();
                    }
                }
                return;
            }
        }
        // Check if is the plain text part
        if (plainTextcontainer) {
            QMailMessagePart::Location location = static_cast<const QMailMessagePart *>(plainTextcontainer)->location();
            if (location.toString(true) == partLocation) {
                m_bodyText = EmailAgent::instance()->bodyPlainText(m_msg);
                disconnect(EmailAgent::instance(), SIGNAL(messagePartDownloaded(QMailMessageId,QString,bool)),
                        this, SLOT(onMessagePartDownloaded(QMailMessageId,QString,bool)));
                if (success) {
                    emit bodyChanged();
                    emit quotedBodyChanged();
                }
            }
        }
    }
}

void EmailMessage::onSendCompleted()
{
    emit sendCompleted();
}

// ############# Invokable API ########################
void EmailMessage::cancelMessageDownload()
{
    if (m_downloadActionId) {
        EmailAgent::instance()->cancelAction(m_downloadActionId);
        disconnect(this, SLOT(onMessagesDownloaded(QMailMessageIdList,bool)));
        disconnect(this, SLOT(onMessagePartDownloaded(QMailMessageId,QString,bool)));
    }
}

void EmailMessage::downloadMessage()
{
    requestMessageDownload();
}

void EmailMessage::send()
{
    buildMessage();

    bool stored = false;

    if (!m_msg.id().isValid()) {
        // Message present only on the local device until we externalise or send it
        m_msg.setStatus(QMailMessage::LocalOnly, true);
        stored = QMailStore::instance()->addMessage(&m_msg);
    }
    else {
        stored = QMailStore::instance()->updateMessage(&m_msg);
        m_newMessage = false;
    }

    EmailAgent *emailAgent = EmailAgent::instance();
    if (stored) {
        connect(emailAgent, SIGNAL(sendCompleted()), this, SLOT(onSendCompleted()));
        emailAgent->sendMessages(m_msg.parentAccountId());
        emitSignals();
    }
    else
       qWarning() << "Error: queuing message, stored: " << stored;
}

void EmailMessage::saveDraft()
{
    buildMessage();

    QMailAccount account(m_msg.parentAccountId());
    QMailFolderId draftFolderId = account.standardFolder(QMailFolder::DraftsFolder);

    if (draftFolderId.isValid()) {
        m_msg.setParentFolderId(draftFolderId);
    } else {
        //local storage set on buildMessage step
        qWarning() << "Drafts folder not found, saving to local storage!";
    }

    bool saved = false;

    // Unset outgoing and outbox so it wont really send
    // when we sync to the server Drafts folder
    m_msg.setStatus(QMailMessage::Outgoing, false);
    m_msg.setStatus(QMailMessage::Outbox, false);
    m_msg.setStatus(QMailMessage::Draft, true);
    // This message is present only on the local device until we externalise or send it
    m_msg.setStatus(QMailMessage::LocalOnly, true);

    if (!m_msg.id().isValid()) {
        saved = QMailStore::instance()->addMessage(&m_msg);
    } else {
        saved = QMailStore::instance()->updateMessage(&m_msg);
        m_newMessage = false;
    }
    // Sync to the server, so the message will be in the remote Drafts folder
    if (saved) {
        QMailDisconnected::flagMessage(m_msg.id(), QMailMessage::Draft, QMailMessage::Temporary,
                                       "Flagging message as draft");
        QMailDisconnected::moveToFolder(QMailMessageIdList() << m_msg.id(), m_msg.parentFolderId());
        EmailAgent::instance()->exportUpdates(m_msg.parentAccountId());
        emitSignals();
    } else {
        qWarning() << "Failed to save message!";
    }
}

QStringList EmailMessage::attachments()
{
    if (m_id.isValid()) {
        if (!m_msg.status() & QMailMessageMetaData::HasAttachments)
            return QStringList();

        m_attachments.clear();
        foreach (const QMailMessagePart::Location &location, m_msg.findAttachmentLocations()) {
            const QMailMessagePart &attachmentPart = m_msg.partAt(location);
            m_attachments << attachmentPart.displayName();
        }
        return m_attachments;
    } else {
        return m_attachments;
    }
}

int EmailMessage::accountId() const
{
    return m_msg.parentAccountId().toULongLong();
}

// Email address of the account having the message
QString EmailMessage::accountAddress() const
{
    QMailAccount account(m_msg.parentAccountId());
    return account.fromAddress().address();
}

int EmailMessage::folderId() const
{
    return m_msg.parentFolderId().toULongLong();
}

QStringList EmailMessage::bcc() const
{
    return QMailAddress::toStringList(m_msg.bcc());
}

QString EmailMessage::body()
{
    if (QMailMessagePartContainer *container = m_msg.findPlainTextContainer()) {
        if (container->contentAvailable()) {
            return m_bodyText;
        } else {
            if (m_msg.multipartType() == QMailMessage::MultipartNone) {
                requestMessageDownload();
            } else {
                requestMessagePartDownload(container);
            }
            return QString();
        }
    } else {
        // Fallback to body text when message does not have container. E.g. when
        // we're composing an email message.
        return m_bodyText;
    }
}

QStringList EmailMessage::cc() const
{
    return QMailAddress::toStringList(m_msg.cc());
}

EmailMessage::ContentType EmailMessage::contentType() const
{
    // Treat only "text/plain" and invalid message as Plain and others as HTML.
    if (m_id.isValid()) {
        if (m_msg.findHtmlContainer()) {
            return EmailMessage::HTML;
        } else {
            return EmailMessage::Plain;
        }
    }
    return EmailMessage::HTML;
}

QDateTime EmailMessage::date() const
{
    return (m_msg.date().toLocalTime());
}

QString EmailMessage::from() const
{
    return m_msg.from().toString();
}

QString EmailMessage::fromAddress() const
{
    return m_msg.from().address();
}

QString EmailMessage::fromDisplayName() const
{
    return m_msg.from().name();
}

QString EmailMessage::htmlBody()
{
    // Fallback to plain message if no html body.
    QMailMessagePartContainer *container = m_msg.findHtmlContainer();
    if (contentType() == EmailMessage::HTML && container) {
        if (container->contentAvailable()) {
            return container->body().data();
        } else {
            if (m_msg.multipartType() == QMailMessage::MultipartNone) {
                requestMessageDownload();
            } else {
                requestMessagePartDownload(container);
            }
            return QString();
        }
    } else {
        return body();
    }
}

QString EmailMessage::inReplyTo() const
{
    return m_msg.inReplyTo();
}

int EmailMessage::messageId() const
{
    return m_id.toULongLong();
}

bool EmailMessage::multipleRecipients() const
{
    QStringList recipients = this->recipients();

    if (!recipients.size()) {
        return false;
    } else if (recipients.size() > 1) {
        return true;
    } else if (!recipients.contains(this->accountAddress(), Qt::CaseInsensitive)
               && !recipients.contains(this->replyTo(), Qt::CaseInsensitive)) {
        return true;
    } else {
        return false;
    }
}

int EmailMessage::numberOfAttachments() const
{
    if (!m_msg.status() & QMailMessageMetaData::HasAttachments)
        return 0;

    const QList<QMailMessagePart::Location> &attachmentLocations = m_msg.findAttachmentLocations();
    return attachmentLocations.count();
}

int EmailMessage::originalMessageId() const
{
    return m_originalMessageId.toULongLong();
}

QString EmailMessage::preview() const
{
    return m_msg.preview();
}

EmailMessage::Priority EmailMessage::priority() const
{
    if (m_msg.status() & QMailMessage::HighPriority) {
        return HighPriority;
    } else if (m_msg.status() & QMailMessage::LowPriority) {
        return LowPriority;
    }
    else {
        return NormalPriority;
    }
}

QString EmailMessage::quotedBody()
{
    QString qBody;
    QMailMessagePartContainer *container = m_msg.findPlainTextContainer();
    if (container) {
        qBody = body();
    } else {
        // If plain text body is not available we extract the text from the html part
        QTextDocument doc;
        doc.setHtml(htmlBody());
        qBody = doc.toPlainText();
    }

    qBody.prepend('\n');
    qBody.replace('\n', "\n> ");
    qBody.truncate(qBody.size() - 1);  // remove the extra ">" put there by QString.replace
    return qBody;
}

QStringList EmailMessage::recipients() const
{
    QStringList recipients;
    QList<QMailAddress> addresses = m_msg.recipients();
    foreach (const QMailAddress &address, addresses) {
        recipients << address.address();
    }
    return recipients;
}

bool EmailMessage::read() const
{
    if (m_msg.status() & QMailMessage::Read) {
        return true;
    } else {
        return false;
    }
}

QString EmailMessage::replyTo() const
{
    return m_msg.replyTo().address();
}

EmailMessage::ResponseType EmailMessage::responseType() const
{
    switch (m_msg.responseType()) {
    case QMailMessage::NoResponse:
        return NoResponse;
    case QMailMessage::Reply:
        return Reply;
    case QMailMessage::ReplyToAll:
        return ReplyToAll;
    case QMailMessage::Forward:
        return Forward;
    case QMailMessage::ForwardPart:
        return ForwardPart;
    case QMailMessage::Redirect:
        return Redirect;
    case QMailMessage::UnspecifiedResponse:
    default:
        return UnspecifiedResponse;
    }
}

void EmailMessage::setAttachments (const QStringList &uris)
{
    // Signals are only emited when message is constructed
    m_attachments = uris;
}

void EmailMessage::setBcc(const QStringList &bccList)
{
    if (bccList.size()) {
        m_msg.setBcc(QMailAddress::fromStringList(bccList));
        emit bccChanged();
        emit multipleRecipientsChanged();
    }
}

void EmailMessage::setBody(const QString &body)
{
    if (m_bodyText != body) {
        m_bodyText = body;
        emit bodyChanged();
    }
}

void EmailMessage::setCc(const QStringList &ccList)
{
    if (ccList.size()) {
        m_msg.setCc(QMailAddress::fromStringList(ccList));
        emit ccChanged();
        emit multipleRecipientsChanged();
    }
}

void EmailMessage::setFrom(const QString &sender)
{
    QMailAccountIdList accountIds = QMailStore::instance()->queryAccounts(QMailAccountKey::messageType(QMailMessage::Email)
                                                                          & QMailAccountKey::status(QMailAccount::Enabled)
                                                                          , QMailAccountSortKey::name());
    // look up the account id for the given sender
    foreach (QMailAccountId id, accountIds) {
        QMailAccount account(id);
        QMailAddress from = account.fromAddress();
        if (from.address() == sender || from.toString() == sender || from.name() == sender) {
            m_account = account;
            m_msg.setParentAccountId(id);
            m_msg.setFrom(account.fromAddress());
        }
    }
    emit fromChanged();
    emit accountIdChanged();
    emit accountAddressChanged();
}

void EmailMessage::setInReplyTo(const QString &messageId)
{
    m_msg.setInReplyTo(messageId);
    emit inReplyToChanged();
}

void EmailMessage::setMessageId(int messageId)
{
    QMailMessageId msgId(messageId);
    if (msgId != m_id) {
        if (msgId.isValid()) {
            m_id = msgId;
            m_msg = QMailMessage(msgId);
        } else {
            m_id = QMailMessageId();
            m_msg = QMailMessage();
            qWarning() << "Invalid message id " << msgId.toULongLong();
        }
        // Construct initial plain text body, even if not entirely available.
        m_bodyText = EmailAgent::instance()->bodyPlainText(m_msg);

        // Message loaded from the store (or a empty message), all properties changes
        emitMessageReloadedSignals();
    }
}

void EmailMessage::setOriginalMessageId(int messageId)
{
    m_originalMessageId = QMailMessageId(messageId);
    emit originalMessageIdChanged();
}

void EmailMessage::setPriority(EmailMessage::Priority priority)
{
    switch (priority) {
    case HighPriority:
        m_msg.setHeaderField("X-Priority", "1");
        m_msg.setHeaderField("X-MSMail-Priority", "High");
        break;
    case LowPriority:
        m_msg.setHeaderField("X-Priority", "5");
        m_msg.setHeaderField("X-MSMail-Priority", "Low");
        break;
    case NormalPriority:
    default:
        m_msg.setHeaderField("X-Priority", "3");
        m_msg.setHeaderField("X-MSMail-Priority", "Normal");
        break;
    }
    emit priorityChanged();
}

void EmailMessage::setRead(bool read) {
    if (read != this->read()) {
        if (read) {
            EmailAgent::instance()->markMessageAsRead(m_id.toULongLong());
        } else {
            EmailAgent::instance()->markMessageAsUnread(m_id.toULongLong());
        }
        m_msg.setStatus(QMailMessage::Read, read);
        emit readChanged();
    }
}

void EmailMessage::setReplyTo(const QString &address)
{
    QMailAddress addr(address);
    m_msg.setReplyTo(addr);
    emit replyToChanged();
}

void EmailMessage::setResponseType(EmailMessage::ResponseType responseType)
{
    switch (responseType) {
    case NoResponse:
        m_msg.setResponseType(QMailMessage::NoResponse);
        break;
    case Reply:
        m_msg.setResponseType(QMailMessage::Reply);
        break;
    case ReplyToAll:
        m_msg.setResponseType(QMailMessage::ReplyToAll);
        break;
    case Forward:
        m_msg.setResponseType(QMailMessage::Forward);
        break;
    case ForwardPart:
        m_msg.setResponseType(QMailMessage::ForwardPart);
        break;
    case Redirect:
        m_msg.setResponseType(QMailMessage::Redirect);
        break;
    case UnspecifiedResponse:
    default:
        m_msg.setResponseType(QMailMessage::UnspecifiedResponse);
        break;
    }
    emit responseTypeChanged();
}

void EmailMessage::setSubject(const QString &subject)
{
    m_msg.setSubject(subject);
    emit subjectChanged();
}

void EmailMessage::setTo(const QStringList &toList)
{
    m_msg.setTo(QMailAddress::fromStringList(toList));
    emit toChanged();
}

int EmailMessage::size()
{
    return m_msg.size();
}

QString EmailMessage::subject()
{
    return m_msg.subject();
}

QStringList EmailMessage::to()
{
    return QMailAddress::toStringList(m_msg.to());
}

// ############## Private API #########################
void EmailMessage::buildMessage()
{
    // remove all existent message parts if there's any
    m_msg.clearParts();

    if (m_msg.responseType() == QMailMessage::Reply || m_msg.responseType() == QMailMessage::ReplyToAll ||
            m_msg.responseType() == QMailMessage::Forward) {
        // Needed for conversations support
        if (m_originalMessageId.isValid()) {
            m_msg.setInResponseTo(m_originalMessageId);
            QMailMessage originalMessage(m_originalMessageId);
            updateReferences(m_msg, originalMessage);
        }
    }

    QMailMessageContentType type;
    type.setType("text/plain; charset=UTF-8");
    // Sending only supports plain text at the moment
    /*
    if (contentType() == EmailMessage::Plain)
        type.setType("text/plain; charset=UTF-8");
    else
        type.setType("text/html; charset=UTF-8");
    */
    // This should be improved to use QuotedPrintable when appending parts and inline references are implemented
    if (m_attachments.size() == 0)
        m_msg.setBody(QMailMessageBody::fromData(m_bodyText, type, QMailMessageBody::Base64));
    else {
        QMailMessagePart body;
        body.setBody(QMailMessageBody::fromData(m_bodyText.toUtf8(), type, QMailMessageBody::Base64));
        m_msg.setMultipartType(QMailMessagePartContainer::MultipartMixed);
        m_msg.appendPart(body);
    }

    // Include attachments into the message
    if (m_attachments.size()) {
        processAttachments();
    }

    // set message basic attributes
    m_msg.setDate(QMailTimeStamp::currentDateTime());
    m_msg.setStatus(QMailMessage::Outgoing, true);
    m_msg.setStatus(QMailMessage::ContentAvailable, true);
    m_msg.setStatus(QMailMessage::PartialContentAvailable, true);
    m_msg.setStatus(QMailMessage::Read, true);
    m_msg.setStatus((QMailMessage::Outbox | QMailMessage::Draft), true);

    m_msg.setParentFolderId(QMailFolder::LocalStorageFolderId);

    m_msg.setMessageType(QMailMessage::Email);
    m_msg.setSize(m_msg.indicativeSize() * 1024);
}

void EmailMessage::emitSignals()
{
    if (m_attachments.size()) {
        emit attachmentsChanged();
    }

    if (contentType() == EmailMessage::HTML)
        emit htmlBodyChanged();

    if (m_newMessage)
        emit messageIdChanged();

    emit folderIdChanged();
    emit storedMessageChanged();
    emit readChanged();
}

void EmailMessage::emitMessageReloadedSignals()
{
    if (contentType() == EmailMessage::HTML) {
        emit htmlBodyChanged();
    }

    emit accountIdChanged();
    emit accountAddressChanged();
    emit folderIdChanged();
    emit attachmentsChanged();
    emit bccChanged();
    emit ccChanged();
    emit dateChanged();
    emit fromChanged();
    emit bodyChanged();
    emit inReplyToChanged();
    emit messageIdChanged();
    emit multipleRecipientsChanged();
    emit priorityChanged();
    emit readChanged();
    emit recipientsChanged();
    emit replyToChanged();
    emit responseTypeChanged();
    emit subjectChanged();
    emit storedMessageChanged();
    emit toChanged();
    emit quotedBodyChanged();
}

void EmailMessage::processAttachments()
{
    QMailMessagePart attachmentPart;
    foreach (QString attachment, m_attachments) {
        // Attaching a file
        if (attachment.startsWith("file://"))
            attachment.remove(0, 7);
        QFileInfo fi(attachment);

        // Just in case..
        if (!fi.isFile())
            continue;

        QMailMessageContentType attachmenttype(QMail::mimeTypeFromFileName(attachment).toLatin1());
        attachmenttype.setName(fi.fileName().toLatin1());

        QMailMessageContentDisposition disposition(QMailMessageContentDisposition::Attachment);
        disposition.setFilename(fi.fileName().toLatin1());
        disposition.setSize(fi.size());

        attachmentPart = QMailMessagePart::fromFile(attachment,
                                                    disposition,
                                                    attachmenttype,
                                                    QMailMessageBody::Base64,
                                                    QMailMessageBody::RequiresEncoding);
        m_msg.appendPart(attachmentPart);
    }
}

void EmailMessage::requestMessageDownload()
{
    connect(EmailAgent::instance(), SIGNAL(messagesDownloaded(QMailMessageIdList, bool)),
            this, SLOT(onMessagesDownloaded(QMailMessageIdList, bool)));
     m_downloadActionId = EmailAgent::instance()->downloadMessages(QMailMessageIdList() << m_id, QMailRetrievalAction::Content);
}

void EmailMessage::requestMessagePartDownload(const QMailMessagePartContainer *container)
{
    connect(EmailAgent::instance(), SIGNAL(messagePartDownloaded(QMailMessageId,QString, bool)),
            this, SLOT(onMessagePartDownloaded(QMailMessageId,QString, bool)));

    QMailMessagePart::Location location = static_cast<const QMailMessagePart *>(container)->location();
    m_downloadActionId = EmailAgent::instance()->downloadMessagePart(location);
}

void EmailMessage::updateReferences(QMailMessage &message, const QMailMessage &originalMessage)
{
    QString references(originalMessage.headerFieldText("References"));
    if (references.isEmpty()) {
        references = originalMessage.headerFieldText("In-Reply-To");
    }
    QString precursorId(originalMessage.headerFieldText("Message-ID"));
    if (!precursorId.isEmpty()) {
        message.setHeaderField("In-Reply-To", precursorId);

        if (!references.isEmpty()) {
            references.append(' ');
        }
        references.append(precursorId);
    }
    if (!references.isEmpty()) {
        // TODO: Truncate references if they're too long
        message.setHeaderField("References", references);
    }
}
