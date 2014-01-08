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

#include "emailfolder.h"
/*
    Unit test for EmailFolder class.
*/
class tst_EmailFolder : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void displayName();
    void setDisplayName();
    void folderId();
    void setFolderId();
    void parentAccountId();
    void parentFolderId();
    void path();

private:
    QMailAccount m_account;

    QMailFolder m_folder;
    QMailFolder m_folder2;
    QMailFolder m_folder3;
};

void tst_EmailFolder::initTestCase()
{
    QMailAccountConfiguration config1;
    m_account.setName("Account 1");
    QVERIFY(QMailStore::instance()->addAccount(&m_account, &config1));

    //root folder
    m_folder = QMailFolder("TestFolder1", QMailFolderId(), m_account.id());
    QVERIFY(QMailStore::instance()->addFolder(&m_folder));
    QCOMPARE(QMailStore::instance()->lastError(), QMailStore::NoError);
    QVERIFY(m_folder.id().isValid());

    //root folder
    m_folder2 = QMailFolder("TestFolder2", QMailFolderId(), m_account.id());
    QVERIFY(QMailStore::instance()->addFolder(&m_folder2));
    QCOMPARE(QMailStore::instance()->lastError(), QMailStore::NoError);
    QVERIFY(m_folder2.id().isValid());

    //root folder with valid parent
    m_folder3 = QMailFolder("TestFolder3", m_folder.id(), m_account.id());
    QVERIFY(QMailStore::instance()->addFolder(&m_folder3));
    QCOMPARE(QMailStore::instance()->lastError(), QMailStore::NoError);
    QVERIFY(m_folder3.id().isValid());
}

void tst_EmailFolder::cleanupTestCase()
{
    //Removes also all folders associated with the account
    QMailStore::instance()->removeAccount(m_account.id());
}

void tst_EmailFolder::displayName()
{
    QScopedPointer<EmailFolder> emailFolder(new EmailFolder);
    QSignalSpy folderIdSpy(emailFolder.data(), SIGNAL(folderIdChanged()));

    emailFolder->setFolderId(m_folder.id().toULongLong());
    QCOMPARE(folderIdSpy.count(), 1);
    QCOMPARE(emailFolder->displayName(),QString(QLatin1String("TestFolder1")));

    emailFolder->setFolderId(m_folder2.id().toULongLong());
    QCOMPARE(folderIdSpy.count(), 2);
    QCOMPARE(emailFolder->displayName(),QString(QLatin1String("TestFolder2")));

    emailFolder->setFolderId(m_folder3.id().toULongLong());
    QCOMPARE(folderIdSpy.count(), 3);
    QCOMPARE(emailFolder->displayName(),QString(QLatin1String("TestFolder3")));
}

void tst_EmailFolder::setDisplayName()
{
    QScopedPointer<EmailFolder> emailFolder(new EmailFolder);
    QSignalSpy displayNameSpy(emailFolder.data(), SIGNAL(displayNameChanged()));
    QSignalSpy folderIdSpy(emailFolder.data(), SIGNAL(folderIdChanged()));

    emailFolder->setFolderId(m_folder.id().toULongLong());
    QCOMPARE(folderIdSpy.count(), 1);
    emailFolder->setDisplayName("Test1");
    QCOMPARE(displayNameSpy.count(), 1);
    QCOMPARE(emailFolder->displayName(),QString(QLatin1String("Test1")));
    emailFolder->setDisplayName("TestFolder1");
    QCOMPARE(displayNameSpy.count(), 2);
    QCOMPARE(emailFolder->displayName(),QString(QLatin1String("TestFolder1")));
}

void tst_EmailFolder::folderId()
{
    QScopedPointer<EmailFolder> emailFolder(new EmailFolder);
    QSignalSpy folderIdSpy(emailFolder.data(), SIGNAL(folderIdChanged()));

    emailFolder->setFolderId(m_folder.id().toULongLong());
    QCOMPARE(folderIdSpy.count(), 1);
    QCOMPARE(emailFolder->folderId(),static_cast<int>(m_folder.id().toULongLong()));

    emailFolder->setFolderId(m_folder2.id().toULongLong());
    QCOMPARE(folderIdSpy.count(), 2);
    QCOMPARE(emailFolder->folderId(),static_cast<int>(m_folder2.id().toULongLong()));

    emailFolder->setFolderId(m_folder3.id().toULongLong());
    QCOMPARE(folderIdSpy.count(), 3);
    QCOMPARE(emailFolder->folderId(),static_cast<int>(m_folder3.id().toULongLong()));
}

void tst_EmailFolder::setFolderId()
{
    // Tested by: folderId()
}

void tst_EmailFolder::parentAccountId()
{
    QScopedPointer<EmailFolder> emailFolder(new EmailFolder);
    QSignalSpy folderIdSpy(emailFolder.data(), SIGNAL(folderIdChanged()));

    emailFolder->setFolderId(m_folder.id().toULongLong());
    QCOMPARE(folderIdSpy.count(), 1);
    QCOMPARE(emailFolder->parentAccountId(),static_cast<int>(m_account.id().toULongLong()));
}

void tst_EmailFolder::parentFolderId()
{
    QScopedPointer<EmailFolder> emailFolder(new EmailFolder);
    QSignalSpy folderIdSpy(emailFolder.data(), SIGNAL(folderIdChanged()));

    emailFolder->setFolderId(m_folder3.id().toULongLong());
    QCOMPARE(folderIdSpy.count(), 1);
    QCOMPARE(emailFolder->parentFolderId(),static_cast<int>(m_folder.id().toULongLong()));
}

void tst_EmailFolder::path()
{
    QScopedPointer<EmailFolder> emailFolder(new EmailFolder);
    QSignalSpy folderIdSpy(emailFolder.data(), SIGNAL(folderIdChanged()));

    emailFolder->setFolderId(m_folder3.id().toULongLong());
    QCOMPARE(folderIdSpy.count(), 1);
    QCOMPARE(emailFolder->path(),QString(QLatin1String("TestFolder3")));
}

#include "tst_emailfolder.moc"
QTEST_MAIN(tst_EmailFolder)
