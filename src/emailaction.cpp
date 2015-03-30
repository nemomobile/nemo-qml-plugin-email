/*
 * Copyright (C) 2013-2015 Jolla Ltd.
 * Contact: Valerio Valerio <valerio.valerio@jollamobile.com>
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#include "emailaction.h"

template<typename T>
QString idListToString(const QList<T> &ids)
{
    QString idsList;
    int idsCount = ids.count();
    int interatorPos = 0;
    foreach(const typename QList<T>::value_type &id, ids) {
        interatorPos++;
        if (interatorPos == idsCount) {
            idsList += QString("%1").arg(id.toULongLong());
        }
        else {
            idsList += QString("%1,").arg(id.toULongLong());
        }
    }
    return idsList;
}

/*
  EmailAction
*/
EmailAction::EmailAction(bool onlineAction)
    : _description(QString())
    , _type(Export)
    , _id(0)
    , _onlineAction(onlineAction)
{
}

EmailAction::~EmailAction()
{
}

QMailAccountId EmailAction::accountId() const
{
    return QMailAccountId();
}

bool EmailAction::operator==(const EmailAction &action) const
{
    if(action._description.isEmpty() || _description.isEmpty()) {
        return false;
    }
    if(action._description == _description) {
        return true;
    }
    else {
        return false;
    }
}

bool EmailAction::operator!=(const EmailAction &action) const
{
    if (action._description != _description) {
        return true;
    } else {
        return false;
    }
}

QString EmailAction::description() const
{
    return _description;
}

EmailAction::ActionType EmailAction::type() const
{
    return _type;
}

quint64 EmailAction::id() const
{
    return _id;
}

void EmailAction::setId(const quint64 id)
{
    _id = id;
}

/*
  CreateStandardFolders
*/
CreateStandardFolders::CreateStandardFolders(QMailRetrievalAction *retrievalAction, const QMailAccountId &id)
    :EmailAction()
    , _retrievalAction(retrievalAction)
    , _accountId(id)
{
    _description = QString("create-standard-folders:account-id=%1").arg(_accountId.toULongLong());
    _type = EmailAction::StandardFolders;
}

CreateStandardFolders::~CreateStandardFolders()
{
}

void CreateStandardFolders::execute()
{
    _retrievalAction->createStandardFolders(_accountId);
}

QMailServiceAction* CreateStandardFolders::serviceAction() const
{
    return _retrievalAction;
}

QMailAccountId CreateStandardFolders::accountId() const
{
    return _accountId;
}

/*
  DeleteMessages
*/
DeleteMessages::DeleteMessages(QMailStorageAction* storageAction, const QMailMessageIdList &ids)
    : EmailAction(false)
    , _storageAction(storageAction)
    , _ids(ids)
{
    QString idsList = idListToString(_ids);
    _description = QString("delete-messages:message-ids=%1").arg(idsList);
    _type = EmailAction::Storage;
}

DeleteMessages::~DeleteMessages()
{
}

void DeleteMessages::execute()
{
    _storageAction->deleteMessages(_ids);
}

QMailServiceAction* DeleteMessages::serviceAction() const
{
    return _storageAction;
}

/*
  ExportUpdates
*/
ExportUpdates::ExportUpdates(QMailRetrievalAction *retrievalAction, const QMailAccountId &id)
    : EmailAction()
    , _retrievalAction(retrievalAction)
    , _accountId(id)
{
    _description = QString("exporting-updates:account-id=%1").arg(_accountId.toULongLong());
    _type = EmailAction::Export;
}

ExportUpdates::~ExportUpdates()
{
}

void ExportUpdates::execute()
{
    _retrievalAction->exportUpdates(_accountId);
}

QMailServiceAction* ExportUpdates::serviceAction() const
{
    return _retrievalAction;
}

QMailAccountId ExportUpdates::accountId() const
{
    return _accountId;
}

/*
  FlagMessages
*/
FlagMessages::FlagMessages(QMailStorageAction* storageAction, const QMailMessageIdList &ids,
                           quint64 setMask, quint64 unsetMask)
    : EmailAction(false)
    , _storageAction(storageAction)
    , _ids(ids)
    , _setMask(setMask)
    , _unsetMask(unsetMask)
{
    QString idsList = idListToString(_ids);
    _description =  QString("flag-messages:message-ids=%1;setMark=%2;unsetMark=%3").arg(idsList)
            .arg(_setMask).arg(_unsetMask);
    _type = EmailAction::Storage;
}

FlagMessages::~FlagMessages()
{
}

void FlagMessages::execute()
{
    _storageAction->flagMessages(_ids, _setMask, _unsetMask);
}

QMailServiceAction* FlagMessages::serviceAction() const
{
    return _storageAction;
}

/*
    MoveToFolder
*/
MoveToFolder::MoveToFolder(QMailStorageAction *storageAction, const QMailMessageIdList &ids,
                           const QMailFolderId &folderId)
    : EmailAction(false)
    , _storageAction(storageAction)
    , _ids(ids)
    , _destinationFolder(folderId)
{
    QString idsList = idListToString(_ids);
    _description =  QString("move-messages-to-folder:message-ids=%1;folder-id=%2").arg(idsList)
            .arg(_destinationFolder.toULongLong());
    _type = EmailAction::Storage;
}

MoveToFolder::~MoveToFolder()
{
}

void MoveToFolder::execute()
{
    _storageAction->moveToFolder(_ids, _destinationFolder);
}

QMailServiceAction* MoveToFolder::serviceAction() const
{
    return _storageAction;
}

/*
   MoveToStandardFolder
*/
MoveToStandardFolder::MoveToStandardFolder(QMailStorageAction *storageAction,
                                           const QMailMessageIdList &ids, QMailFolder::StandardFolder standardFolder)
    : EmailAction(false)
    , _storageAction(storageAction)
    , _ids(ids)
    , _standardFolder(standardFolder)
{
    QString idsList = idListToString(_ids);
    _description =  QString("move-messages-to-standard-folder:message-ids=%1;standard-folder=%2").arg(idsList)
            .arg(_standardFolder);
    _type = EmailAction::Storage;
}

MoveToStandardFolder::~MoveToStandardFolder()
{
}

void MoveToStandardFolder::execute()
{
    _storageAction->moveToStandardFolder(_ids, _standardFolder);
}

QMailServiceAction* MoveToStandardFolder::serviceAction() const
{
    return _storageAction;
}

/*
  OnlineCreateFolder
*/
OnlineCreateFolder::OnlineCreateFolder(QMailStorageAction* storageAction, const QString &name,
                                       const QMailAccountId &id, const QMailFolderId &parentId)
    : EmailAction()
    , _storageAction(storageAction)
    , _name(name)
    , _accountId(id)
    , _parentId(parentId)
{
    QString pId;
    if (_parentId.isValid()) {
        pId = _parentId.toULongLong();
    }
    else {
        pId = "NULL";
    }
    _description = QString("create-folder:name=%1;account-id=%2;parent-id=%3").arg(_accountId.toULongLong())
            .arg(_name).arg(pId);
    _type = EmailAction::Storage;
}

OnlineCreateFolder::~OnlineCreateFolder()
{
}

void OnlineCreateFolder::execute()
{
    _storageAction->onlineCreateFolder(_name, _accountId, _parentId);
}

QMailServiceAction* OnlineCreateFolder::serviceAction() const
{
    return _storageAction;
}

QMailAccountId OnlineCreateFolder::accountId() const
{
    return _accountId;
}

/*
  OnlineDeleteFolder
*/
OnlineDeleteFolder::OnlineDeleteFolder(QMailStorageAction* storageAction, const QMailFolderId &folderId)
    : EmailAction()
    , _storageAction(storageAction)
    , _folderId(folderId)
{
    _description = QString("delete-folder:folder-id=%1").arg(_folderId.toULongLong());
    _type = EmailAction::Storage;
}

OnlineDeleteFolder::~OnlineDeleteFolder()
{
}

void OnlineDeleteFolder::execute()
{
    _storageAction->onlineDeleteFolder(_folderId);
}

QMailServiceAction* OnlineDeleteFolder::serviceAction() const
{
    return _storageAction;
}

QMailAccountId OnlineDeleteFolder::accountId() const
{
    QMailFolder folder(_folderId);
    return folder.parentAccountId();
}

/*
  OnlineMoveMessages
*/
OnlineMoveMessages::OnlineMoveMessages(QMailStorageAction *storageAction, const QMailMessageIdList &ids,
                                      const QMailFolderId &destinationId)
    : EmailAction()
    , _storageAction(storageAction)
    , _ids(ids)
    , _destinationId(destinationId)
{
    QString idsList = idListToString(_ids);
    _description = QString("move-messages:message-ids=%1;destination-folder=%2").arg(idsList)
            .arg(_destinationId.toULongLong());
    _type = EmailAction::Storage;
}

OnlineMoveMessages::~OnlineMoveMessages()
{
}

void OnlineMoveMessages::execute()
{
    _storageAction->onlineMoveMessages(_ids,_destinationId);
}

QMailServiceAction* OnlineMoveMessages::serviceAction() const
{
    return _storageAction;
}

/*
  OnlineRenameFolder
*/
OnlineRenameFolder::OnlineRenameFolder(QMailStorageAction* storageAction, const QMailFolderId &folderId, const QString &name)
    : EmailAction()
    , _storageAction(storageAction)
    , _folderId(folderId)
    , _name(name)
{
    _description = QString("rename-folder:folder-id=%1;new-name=%2").arg(_folderId.toULongLong()).arg(_name);
    _type = EmailAction::Storage;
}

OnlineRenameFolder::~OnlineRenameFolder()
{
}

void OnlineRenameFolder::execute()
{
    _storageAction->onlineRenameFolder(_folderId, _name);
}

QMailServiceAction* OnlineRenameFolder::serviceAction() const
{
    return _storageAction;
}

QMailAccountId OnlineRenameFolder::accountId() const
{
    QMailFolder folder(_folderId);
    return folder.parentAccountId();
}

/*
  RetrieveFolderList
*/
RetrieveFolderList::RetrieveFolderList(QMailRetrievalAction* retrievalAction, const QMailAccountId& id,
                                       const QMailFolderId &folderId, uint descending )
    : EmailAction()
    , _retrievalAction(retrievalAction)
    , _accountId(id)
    , _folderId(folderId)
    , _descending(descending)
{
    QString fId;
    if (_folderId.isValid()) {
        fId = _folderId.toULongLong();
    }
    else {
        fId = "NULL";
    }
    _description = QString("retrieve-folder-list:account-id=%1;folder-id=%2")
            .arg(_accountId.toULongLong())
            .arg(fId);
    _type = EmailAction::RetrieveFolderList;
}

RetrieveFolderList::~RetrieveFolderList()
{
}

void RetrieveFolderList::execute()
{
    _retrievalAction->retrieveFolderList(_accountId, _folderId, _descending);
}

QMailServiceAction* RetrieveFolderList::serviceAction() const
{
    return _retrievalAction;
}

QMailAccountId RetrieveFolderList::accountId() const
{
    return _accountId;
}

/*
  RetrieveMessageList
*/
RetrieveMessageList::RetrieveMessageList(QMailRetrievalAction* retrievalAction, const QMailAccountId& id,
                    const QMailFolderId &folderId, uint minimum, const QMailMessageSortKey &sort)
    : EmailAction()
    , _retrievalAction(retrievalAction)
    , _accountId(id)
    , _folderId(folderId)
    , _minimum(minimum)
    , _sort(sort)
{
    _description = QString("retrieve-message-list:account-id=%1;folder-id=%2")
            .arg(_accountId.toULongLong())
            .arg(_folderId.toULongLong());
    _type = EmailAction::Retrieve;
}

RetrieveMessageList::~RetrieveMessageList()
{
}

void RetrieveMessageList::execute()
{
    _retrievalAction->retrieveMessageList(_accountId, _folderId, _minimum, _sort);
}

QMailServiceAction* RetrieveMessageList::serviceAction() const
{
    return _retrievalAction;
}

QMailAccountId RetrieveMessageList::accountId() const
{
    return _accountId;
}

/*
  RetrieveMessageLists
*/
RetrieveMessageLists::RetrieveMessageLists(QMailRetrievalAction* retrievalAction, const QMailAccountId& id,
                    const QMailFolderIdList &folderIds, uint minimum, const QMailMessageSortKey &sort)
    : EmailAction()
    , _retrievalAction(retrievalAction)
    , _accountId(id)
    , _folderIds(folderIds)
    , _minimum(minimum)
    , _sort(sort)
{
    QString ids = idListToString(_folderIds);
    _description = QString("retrieve-message-lists:account-id=%1;folder-ids=%2")
            .arg(_accountId.toULongLong())
            .arg(ids);
    _type = EmailAction::Retrieve;
}

RetrieveMessageLists::~RetrieveMessageLists()
{
}

void RetrieveMessageLists::execute()
{
    _retrievalAction->retrieveMessageLists(_accountId, _folderIds, _minimum, _sort);
}

QMailServiceAction* RetrieveMessageLists::serviceAction() const
{
    return _retrievalAction;
}

QMailAccountId RetrieveMessageLists::accountId() const
{
    return _accountId;
}

/*
  RetrieveMessagePart
*/
RetrieveMessagePart::RetrieveMessagePart(QMailRetrievalAction *retrievalAction,
                                         const QMailMessagePartContainer::Location &partLocation,
                                         bool isAttachment)
    : EmailAction()
    , _messageId(partLocation.containingMessageId())
    , _retrievalAction(retrievalAction)
    , _partLocation(partLocation)
    , _isAttachment(isAttachment)
{
    _description = QString("retrieve-message-part:partLocation-id=%1")
            .arg(_partLocation.toString(true));
    _type = EmailAction::RetrieveMessagePart;
}

RetrieveMessagePart::~RetrieveMessagePart()
{
}

void RetrieveMessagePart::execute()
{
     _retrievalAction->retrieveMessagePart(_partLocation);
}

QMailMessageId RetrieveMessagePart::messageId() const
{
    return _messageId;
}

QString RetrieveMessagePart::partLocation() const
{
    return _partLocation.toString(true);
}

QMailServiceAction* RetrieveMessagePart::serviceAction() const
{
    return _retrievalAction;
}

bool RetrieveMessagePart::isAttachment() const
{
    return _isAttachment;
}

QMailAccountId RetrieveMessagePart::accountId() const
{
    QMailMessage message(_messageId);
    return message.parentAccountId();
}

/*
  RetrieveMessagePartRange
*/
RetrieveMessagePartRange::RetrieveMessagePartRange(QMailRetrievalAction *retrievalAction,
                                         const QMailMessagePartContainer::Location &partLocation, uint minimum)
    : EmailAction()
    , _retrievalAction(retrievalAction)
    , _partLocation(partLocation)
    , _minimum(minimum)
{
    _description = QString("retrieve-message-part:partLocation-id=%1;minimumBytes=%2")
            .arg(_partLocation.toString(true))
            .arg(_minimum);
    _type = EmailAction::Retrieve;
}

RetrieveMessagePartRange::~RetrieveMessagePartRange()
{
}

void RetrieveMessagePartRange::execute()
{
     _retrievalAction->retrieveMessagePartRange(_partLocation, _minimum);
}

QMailServiceAction* RetrieveMessagePartRange::serviceAction() const
{
    return _retrievalAction;
}

QMailAccountId RetrieveMessagePartRange::accountId() const
{
    QMailMessage message(_partLocation.containingMessageId());
    return message.parentAccountId();
}

/*
  RetrieveMessageRange
*/
RetrieveMessageRange::RetrieveMessageRange(QMailRetrievalAction *retrievalAction,
                                         const QMailMessageId &messageId, uint minimum)
    : EmailAction()
    , _retrievalAction(retrievalAction)
    , _messageId(messageId)
    , _minimum(minimum)
{
    _description = QString("retrieve-message-range:message-id=%1;minimumBytes=%2")
            .arg(_messageId.toULongLong())
            .arg(_minimum);
    _type = EmailAction::Retrieve;
}

RetrieveMessageRange::~RetrieveMessageRange()
{
}

void RetrieveMessageRange::execute()
{
     _retrievalAction->retrieveMessageRange(_messageId, _minimum);
}

QMailServiceAction* RetrieveMessageRange::serviceAction() const
{
    return _retrievalAction;
}

QMailAccountId RetrieveMessageRange::accountId() const
{
    QMailMessage message(_messageId);
    return message.parentAccountId();
}

/*
  RetrieveMessages
*/
RetrieveMessages::RetrieveMessages(QMailRetrievalAction *retrievalAction,
                                   const QMailMessageIdList &messageIds,
                                   QMailRetrievalAction::RetrievalSpecification spec)
    : EmailAction()
    , _retrievalAction(retrievalAction)
    , _messageIds(messageIds)
    , _spec(spec)
{
    QString idsList = idListToString(_messageIds);
    _description = QString("retrieve-messages:message-ids=%1").arg(idsList);
    _type = EmailAction::RetrieveMessages;
}

RetrieveMessages::~RetrieveMessages()
{
}

void RetrieveMessages::execute()
{
    _retrievalAction->retrieveMessages(_messageIds, _spec);
}

QMailServiceAction* RetrieveMessages::serviceAction() const
{
    return _retrievalAction;
}

QMailMessageIdList RetrieveMessages::messageIds() const
{
    return _messageIds;
}

/*
  SearchMessages
*/
SearchMessages::SearchMessages(QMailSearchAction *searchAction,
                               const QMailMessageKey &filter, const QString &bodyText,
                               QMailSearchAction::SearchSpecification spec, quint64 limit, bool searchBody, const QMailMessageSortKey &sort)
    : EmailAction(spec == QMailSearchAction::Local ? false : true)
    , _searchAction(searchAction)
    , _filter(filter)
    , _bodyText(bodyText)
    , _spec(spec)
    , _limit(limit)
    , _sort(sort)
    , _searchBody(searchBody)
{
    _description = QString("search-messages:body-text=%1").arg(bodyText);
    _type = EmailAction::Search;
}

SearchMessages::~SearchMessages()
{
}

void SearchMessages::execute()
{
    QString bodyText;
    if (_searchBody) {
        bodyText = _bodyText;
    }
    _searchAction->searchMessages(_filter, bodyText, _spec, _limit, _sort);
}

QMailServiceAction* SearchMessages::serviceAction() const
{
    return _searchAction;
}

bool SearchMessages::isRemote() const
{
    return (_spec == QMailSearchAction::Remote);
}

QString SearchMessages::searchText() const
{
    return _bodyText;
}

/*
  Synchronize
*/
Synchronize::Synchronize(QMailRetrievalAction* retrievalAction, const QMailAccountId& id)
        : EmailAction()
        , _retrievalAction(retrievalAction)
        , _accountId(id)
{
    _description = QString("synchronize:account-id=%1").arg(_accountId.toULongLong());
    _type = EmailAction::Retrieve;
}

Synchronize::~Synchronize()
{
}

void Synchronize::execute()
{
    _retrievalAction->synchronize(_accountId, 20);
}

QMailServiceAction* Synchronize::serviceAction() const
{
    return _retrievalAction;
}

QMailAccountId Synchronize::accountId() const
{
    return _accountId;
}

/*
    TransmitMessage
*/
TransmitMessage::TransmitMessage(QMailTransmitAction* transmitAction, const QMailMessageId &messageId)
    : EmailAction()
    , _transmitAction(transmitAction)
    , _messageId(messageId)
{
    _description = QString("transmit-message:message-id=%1").arg(_messageId.toULongLong());
    _type = EmailAction::Transmit;
}

TransmitMessage::~TransmitMessage()
{
}

void TransmitMessage::execute()
{
    _transmitAction->transmitMessage(_messageId);
}

QMailServiceAction* TransmitMessage::serviceAction() const
{
    return _transmitAction;
}

QMailMessageId TransmitMessage::messageId() const
{
    return _messageId;
}

QMailAccountId TransmitMessage::accountId() const
{
    QMailMessageMetaData msg(_messageId);
    return msg.parentAccountId();
}

/*
    TransmitMessages
*/
TransmitMessages::TransmitMessages(QMailTransmitAction* transmitAction, const QMailAccountId &id)
    : EmailAction()
    , _transmitAction(transmitAction)
    , _accountId(id)
{
    _description = QString("transmit-messages:account-id=%1").arg(_accountId.toULongLong());
    _type = EmailAction::Transmit;
}

TransmitMessages::~TransmitMessages()
{
}

void TransmitMessages::execute()
{
    _transmitAction->transmitMessages(_accountId);
}

QMailServiceAction* TransmitMessages::serviceAction() const
{
    return _transmitAction;
}

QMailAccountId TransmitMessages::accountId() const
{
    return _accountId;
}
