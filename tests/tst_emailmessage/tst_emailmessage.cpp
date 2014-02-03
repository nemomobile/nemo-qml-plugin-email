/*
 * Copyright (C) 2013-2014 Jolla Ltd.
 * Contact: Valerio Valerio <valerio.valerio@jolla.com>
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#include <QObject>
#include <QTest>
#include <QSignalSpy>
#include <qmailstore.h>

#include "emailmessage.h"

/*
    Unit test for EmailMessage class.
*/
class tst_EmailMessage : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void accountId();
    void folderId();
    void attachments();
    void setAttachments();

private:
    QMailAccount m_account;
    QMailAccount m_account2;
    QMailFolder m_folder;
    QMailFolder m_folder2;
    QMailMessage m_message;
    QMailMessage m_message2;
};

void tst_EmailMessage::initTestCase()
{
    QMailAccountConfiguration config1;
    m_account.setName("Account 1");
    m_account2.setName("Account 2");
    QVERIFY(QMailStore::instance()->addAccount(&m_account, &config1));
    QVERIFY(QMailStore::instance()->addAccount(&m_account2, &config1));

    m_folder = QMailFolder("TestFolder", QMailFolderId(), m_account.id());
    QVERIFY(QMailStore::instance()->addFolder(&m_folder));
    QVERIFY(m_folder.id().isValid());

    m_folder2 = QMailFolder("TestFolder2", QMailFolderId(), m_account2.id());
    QVERIFY(QMailStore::instance()->addFolder(&m_folder2));
    QVERIFY(m_folder2.id().isValid());

    m_message.setMessageType(QMailMessage::Email);
    m_message.setParentAccountId(m_account.id());
    m_message.setParentFolderId(m_folder.id());
    m_message.setFrom(QMailAddress("account1@example.org"));
    m_message.setTo(QMailAddress("account2@example.org"));
    m_message.setCc(QList<QMailAddress>() << QMailAddress("account1@example.org") << QMailAddress("account3@example.org"));
    m_message.setBcc(QList<QMailAddress>() << QMailAddress("account1@example.org"));
    m_message.setSubject("inboxMessage1");
    m_message.setDate(QMailTimeStamp(QDateTime(QDate::currentDate())));
    m_message.setReceivedDate(QMailTimeStamp(QDateTime(QDate::currentDate())));
    m_message.setStatus(QMailMessage::Incoming, true);
    m_message.setStatus(QMailMessage::New, true);
    m_message.setStatus(QMailMessage::Read, false);
    m_message.setServerUid("inboxMessage1");
    m_message.setSize(5 * 1024);
    m_message.setContent(QMailMessage::PlainTextContent);

    QVERIFY(QMailStore::instance()->addMessage(&m_message));

    m_message2.setMessageType(QMailMessage::Email);
    m_message2.setParentAccountId(m_account2.id());
    m_message2.setParentFolderId(m_folder2.id());
    m_message2.setFrom(QMailAddress("account2@example.org"));
    m_message2.setTo(QMailAddress("account1@example.org"));
    m_message2.setSubject("RE:inboxMessage1");
    m_message2.setDate(QMailTimeStamp(QDateTime(QDate::currentDate())));
    m_message2.setReceivedDate(QMailTimeStamp(QDateTime(QDate::currentDate())));
    m_message2.setStatus(QMailMessage::Incoming, true);
    m_message2.setStatus(QMailMessage::New, true);
    m_message2.setStatus(QMailMessage::Read, true);
    m_message2.setServerUid("inboxMessage2");
    m_message2.setSize(5 * 1024);
    m_message2.setContent(QMailMessage::HtmlContent);
    m_message2.setInResponseTo(m_message.id());
    m_message2.setResponseType(QMailMessage::Reply);

    QVERIFY(QMailStore::instance()->addMessage(&m_message2));
}

void tst_EmailMessage::cleanupTestCase()
{
    QMailStore::instance()->removeAccount(m_account.id());
    QMailStore::instance()->removeAccount(m_account2.id());
}

void tst_EmailMessage::accountId()
{
    QScopedPointer<EmailMessage> emailMessage(new EmailMessage);
    QSignalSpy messageIdSpy(emailMessage.data(), SIGNAL(messageIdChanged()));

    emailMessage->setMessageId(m_message.id().toULongLong());
    QCOMPARE(messageIdSpy.count(), 1);
    QCOMPARE(emailMessage->accountId(),static_cast<int>(m_account.id().toULongLong()));
}

void tst_EmailMessage::folderId()
{
    QScopedPointer<EmailMessage> emailMessage(new EmailMessage);
    QSignalSpy messageIdSpy(emailMessage.data(), SIGNAL(messageIdChanged()));

    emailMessage->setMessageId(m_message.id().toULongLong());
    QCOMPARE(messageIdSpy.count(), 1);
    QCOMPARE(emailMessage->folderId(),static_cast<int>(m_folder.id().toULongLong()));
}

//Todo: Test also with deployed messages containing real attachments
void tst_EmailMessage::attachments()
{
    QScopedPointer<EmailMessage> emailMessage(new EmailMessage);
    QSignalSpy messageIdSpy(emailMessage.data(), SIGNAL(messageIdChanged()));

    emailMessage->setMessageId(m_message.id().toULongLong());
    QCOMPARE(messageIdSpy.count(), 1);
    QCOMPARE(emailMessage->attachments(), QStringList());
}

void tst_EmailMessage::setAttachments()
{
    QScopedPointer<EmailMessage> emailMessage(new EmailMessage);

    QStringList attachments;
    attachments << "Attachment1.txt" << "Attachment2.txt" << "Attachment3.txt";
    emailMessage->setAttachments(attachments);
    emailMessage->processAttachments();
    QCOMPARE(emailMessage->attachments(), attachments);
}

#include "tst_emailmessage.moc"
QTEST_MAIN(tst_EmailMessage)
