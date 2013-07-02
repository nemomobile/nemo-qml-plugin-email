/*
 * Copyright 2011 Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#ifndef EMAILMESSAGE_H
#define EMAILMESSAGE_H

#include <QObject>

#include <qmailaccount.h>
#include <qmailstore.h>

class EmailMessage : public QObject
{
    Q_OBJECT
    Q_ENUMS(Priority)

public:
    explicit EmailMessage(QObject *parent = 0);
    ~EmailMessage ();

    Q_PROPERTY(int accountId READ accountId NOTIFY accountIdChanged)
    Q_PROPERTY(QStringList attachments READ attachments WRITE setAttachments NOTIFY attachmentsChanged)
    Q_PROPERTY(QStringList bcc READ bcc WRITE setBcc NOTIFY bccChanged)
    Q_PROPERTY(QString body READ body WRITE setBody NOTIFY storedMessageChanged)
    Q_PROPERTY(QStringList cc READ cc WRITE setCc NOTIFY ccChanged)
    Q_PROPERTY(QDateTime date READ date NOTIFY storedMessageChanged)
    Q_PROPERTY(QString from READ from WRITE setFrom NOTIFY fromChanged)
    Q_PROPERTY(QString fromAddress READ fromAddress NOTIFY fromChanged)
    Q_PROPERTY(QString fromDisplayName READ fromDisplayName NOTIFY fromChanged)
    Q_PROPERTY(QString htmlBody READ htmlBody WRITE setHtmlBody NOTIFY htmlBodyChanged)
    Q_PROPERTY(QString inReplyTo READ inReplyTo WRITE setInReplyTo NOTIFY inReplyToChanged)
    Q_PROPERTY(int messageId READ messageId WRITE setMessageId NOTIFY messageIdChanged)
    Q_PROPERTY(int numberOfAttachments READ numberOfAttachments NOTIFY attachmentsChanged)
    Q_PROPERTY(QString preview READ preview NOTIFY storedMessageChanged)
    Q_PROPERTY(Priority priority READ priority WRITE setPriority NOTIFY priorityChanged)
    Q_PROPERTY(QString quotedBody READ quotedBody NOTIFY storedMessageChanged)
    Q_PROPERTY(QStringList recipients READ recipients NOTIFY recipientsChanged)
    Q_PROPERTY(bool read READ read NOTIFY readChanged)
    Q_PROPERTY(QString replyTo READ replyTo WRITE setReplyTo NOTIFY replyToChanged)
    Q_PROPERTY(int size READ size NOTIFY storedMessageChanged)
    Q_PROPERTY(QString subject READ subject WRITE setSubject NOTIFY subjectChanged)
    Q_PROPERTY(QStringList to READ to WRITE setTo NOTIFY toChanged)

    enum Priority { LowPriority, NormalPriority, HighPriority };

    Q_INVOKABLE void send();
    Q_INVOKABLE void saveDraft();

    int accountId() const;
    QStringList attachments();
    QStringList bcc() const;
    QString body() const;
    QStringList cc() const;
    QDateTime date() const;
    QString from() const;
    QString fromAddress() const;
    QString fromDisplayName() const;
    QString htmlBody() const;
    QString inReplyTo() const;
    int messageId() const;
    int numberOfAttachments() const;
    QString preview() const;
    Priority priority() const;
    QString quotedBody() const;
    QStringList recipients() const;
    bool read() const;
    QString replyTo() const;
    void setAttachments(const QStringList &uris);
    void setBcc(const QStringList &bccList);
    void setBody(const QString &body);
    void setCc(const QStringList &ccList);
    void setFrom(const QString &sender);
    void setHtmlBody(const QString &htmlBody);
    void setInReplyTo(const QString &messageId);
    void setMessageId(int messageId);
    void setPriority(Priority priority);
    void setReplyTo(const QString &address);
    void setSubject(const QString &subject);
    void setTo(const QStringList &toList);
    int size();
    QString subject();
    QStringList to();


signals:
    void sendCompleted();

    void accountIdChanged();
    void attachmentsChanged();
    void bccChanged();
    void ccChanged();
    void dateChanged();
    void fromChanged();
    void htmlBodyChanged();
    void inReplyToChanged();
    void messageIdChanged();
    void priorityChanged();
    void readChanged();
    void recipientsChanged();
    void replyToChanged();
    void subjectChanged();
    void storedMessageChanged();
    void toChanged();

private slots:
    void onSendCompleted();

private:
    void buildMessage();
    void emitSignals();
    void processAttachments();

    QMailAccount m_account;
    QStringList m_attachments;
    QString m_bodyText;
    QMailMessageId m_id;
    QMailMessage m_msg;
    bool m_newMessage;
    bool m_textOnly;
};

#endif
