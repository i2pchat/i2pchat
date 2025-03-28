/***************************************************************************
 *   Copyright (C) 2008 by I2P-Messenger   				   *
 *   Messenger-Dev@I2P-Messenger   					   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/
#include "Core.h"

#include "I2PStream.h"
#include "PacketManager.h"
#include "FileTransferSend.h"
#include "FileTransferRecive.h"
#include "ConnectionManager.h"
#include "UserManager.h"
#include "FileTransferManager.h"
#include <QtGlobal>
#include <QMessageBox>
#include <QApplication>
#include <QStandardPaths>

CCore::CCore(QString configPath)
{	
    mConfigPath=configPath;

    mDebugMessageHandler= new CDebugMessageManager("General", configPath);
    mSoundManager= new CSoundManager(mConfigPath);


    QSettings settings(mConfigPath+"/application.ini",QSettings::IniFormat);
    settings.beginGroup("Network");
    mMyDestinationB32=settings.value("MyDestinationB32","").toString(),

            mConnectionManager=new CConnectionManager(
                settings.value("SamHost","127.0.0.1").toString(),
                settings.value("SamPort","7656").toString(),
                mConfigPath);

    connect (mConnectionManager,SIGNAL(signNamingReplyRecived(const SAM_Message_Types::RESULT, QString, QString, QString)),this,
             SLOT(slotNamingReplyRecived(const SAM_Message_Types::RESULT, QString, QString, QString)),Qt::DirectConnection);

    connect (mConnectionManager,SIGNAL(signStreamControllerStatusOK(bool)),this,
             SLOT(slotStreamControllerStatusOK(bool)));

    connect (mConnectionManager,SIGNAL(signNewSamPrivKeyGenerated(const QString)),this,
             SLOT(slotNewSamPrivKeyGenerated(const QString)));

    connect (mConnectionManager,SIGNAL(signStreamStatusRecived(const SAM_Message_Types::RESULT, const qint32, const QString)),this,
             SLOT(slotStreamStatusRecived(const SAM_Message_Types::RESULT, const qint32, QString)));

    connect (mConnectionManager,SIGNAL(signIncomingStream(CI2PStream*)),this,
             SLOT(slotIncomingStream(CI2PStream*)));

    connect (mConnectionManager,SIGNAL(signDebugMessages(const QString)),mDebugMessageHandler,
             SLOT(slotNewIncomingDebugMessage(const QString)));

    if(settings.contains("Destination")){
        settings.remove("Destination");
    }
    settings.endGroup();
    settings.sync();

    mProtocol= new CProtocol(*this);
    this->mCurrentOnlineStatus=User::USEROFFLINE;

    loadUserInfos();
/*
    settings.beginGroup("Usersearch");
    if((settings.value("Enabled",false).toBool()) ==true && getUserInfos().Nickname.isEmpty()==false){
        mSeedlessManager= new CSeedlessManager(*this,
                                               mConfigPath+"/SeedlessServerList.dat",
                                               mConfigPath+"/application.ini");

        mDebugSeedlessHandler= new CDebugMessageManager("Usersearch");
        connect(mSeedlessManager,SIGNAL(DebugMessages(QString)),mDebugSeedlessHandler,
                SLOT(slotNewIncomingDebugMessage(const QString)));
    }else{
        mSeedlessManager=NULL;
        mDebugSeedlessHandler=NULL;
    }
    settings.endGroup();
    settings.sync();

*/
    mUnsentChatMessageStorage= new CUnsentChatMessageStorage(mConfigPath+"/UnsentChatMessageStorage.ini");
    mUserBlockManager= new CUserBlockManager(*this,mConfigPath+"/UserBlockList.dat");
    mUserManager= new CUserManager(*this,mConfigPath+"/users.config",*mUnsentChatMessageStorage);

    mUserBlockManager->readBlockListe();

    connect(mUserManager,SIGNAL(signUserStatusChanged()),this,
            SIGNAL(signUserStatusChanged()));
    mUserManager->loadUserList();

    mFileTransferManager= new CFileTransferManager(*this);
    connect(mFileTransferManager,SIGNAL(signUserStatusChanged()),this,
            SIGNAL(signUserStatusChanged()));

}

CCore::~CCore(){

    this->closeAllActiveConnections();
    this->mUserManager->saveUserList();
    delete mUnsentChatMessageStorage;
    delete mUserManager;
    delete mSoundManager;

    QList<CPacketManager*>::Iterator it;
    for(it=mDataPacketsManagers.begin(); it<mDataPacketsManagers.end() ;++it){
        mDataPacketsManagers.erase(it);
    }

    if(mProtocol!=NULL){
        delete this->mProtocol;
    }

    delete mConnectionManager;

    delete mUserBlockManager;
    delete mDebugMessageHandler;
    delete mFileTransferManager;

}

void CCore::doNamingLookUP(QString Name)const{
    mConnectionManager->doNamingLookUP(Name);
}

QString CCore::calcSessionOptionString() const
{
    QString SessionOptionString="";
    QString Signature_Type="";

    QSettings settings(mConfigPath+"/application.ini",QSettings::IniFormat);

    settings.beginGroup("Network");
    SessionOptionString.append("inbound.nickname="+settings.value("TunnelName","I2P-Messenger").toString()+" ");
    ///FIXME TunnelName no whitespace allowed...

    //inbound options
    SessionOptionString.append("inbound.quantity="+settings.value("inbound.quantity","1").toString()+ " ");
    SessionOptionString.append("inbound.backupQuantity="+settings.value("inbound.backupQuantity","1").toString()+ " ");
    SessionOptionString.append("inbound.length="+settings.value("inbound.length","3").toString()+ " ");

    //outbound options
    SessionOptionString.append("outbound.quantity="+settings.value("outbound.quantity","1").toString()+ " ");
    SessionOptionString.append("outbound.backupQuantity="+settings.value("outbound.backupQuantity","1").toString()+ " ");
    SessionOptionString.append("outbound.length="+settings.value("outbound.length","3").toString()+ " ");

    //SIGNATURE_TYPE
    SessionOptionString.append("SIGNATURE_TYPE="+settings.value("Signature_Type","DSA_SHA1").toString()+ " ");
    ///TODO check for valid string match DSA_SHA1 || ECDSA_SHA256_P256 ...
    ///TODO which Signature_Type as default for best security ???

    settings.remove("SessionOptionString");//no longer used,- so erase it
    settings.endGroup();
    settings.sync();

    return SessionOptionString;
}

void CCore::init(){
    using namespace SESSION_ENUMS;

    this->mMyDestination="";

    QSettings settings(mConfigPath+"/application.ini",QSettings::IniFormat);
    settings.beginGroup("Network");

    QString SamPrivKey=settings.value("SamPrivKey","").toString();

    if(mConnectionManager->isComponentStopped()){
        mConnectionManager->doReStart();
    }

    mConnectionManager->doCreateSession(
                STREAM,
                SamPrivKey,
                calcSessionOptionString());

    settings.endGroup();
    settings.sync();
}

void CCore::slotStreamStatusRecived(const SAM_Message_Types::RESULT result,const qint32 ID,QString Message) {

    CI2PStream* stream=mConnectionManager->getStreamObjectByID(ID);
    CUser* user=NULL;
    QString Destination;

    if(stream==NULL){
        //Stream where delete,- asyn SIGNAL/SLOT
        return;
    }

    if(stream!=NULL){
        Destination=stream->getDestination();
        user=mUserManager->getUserByI2P_ID(ID);
    }

    if(result==SAM_Message_Types::OK && ID>0){
        if(mFileTransferManager->isThisID_a_FileSendID(ID)==false){
            mProtocol->newConnectionChat(ID);
        }
    }
    else if( result==SAM_Message_Types::CANT_REACH_PEER ||
             result==SAM_Message_Types::TIMEOUT
             ){
        if(user==NULL){
            mConnectionManager->doDestroyStreamObjectByID(ID);
            return;
        }
        else{
            if(user->getOnlineState()!=USEROFFLINE && user->getOnlineState()!=USERTRYTOCONNECT){
                user->slotIncomingMessageFromSystem(tr("The Connection is broken: %1\nConnection closed").arg(Message));
            }
            deletePacketManagerByID(ID);
            user->setConnectionStatus(TRYTOCONNECT);
            user->setOnlineState(USEROFFLINE);
            //reconnect stream
        }
    }
    else if( result==SAM_Message_Types::CLOSED){
        if(ID<0){
            //incoming stream
            QString Destination=stream->getDestination();

            //remove old stream
            deletePacketManagerByID(ID);
            mConnectionManager->doDestroyStreamObjectByID(ID);
            //---------------------------

            if(user!=NULL && user->getI2PStreamID()==ID){
                user->setConnectionStatus(OFFLINE);
                user->setOnlineState(USEROFFLINE);
                //create a new outgoing stream for this user
                createStreamObjectForUser(*user);
            }
        }
        else{//ID>0
            //outgoing stream
            if(user!=NULL){
                user->setConnectionStatus(TRYTOCONNECT);
                user->setOnlineState(USEROFFLINE);
            }
            stream->setConnectionType(UNKNOWN);

            deletePacketManagerByID(ID);

            connect(stream,SIGNAL(signDataRecived(const qint32, const QByteArray)),mProtocol,
                    SLOT(slotInputUnknown(const qint32, const QByteArray)));

        }
    }


    else if(result==SAM_Message_Types::INVALID_KEY){
        if(user==NULL){
            mConnectionManager->doDestroyStreamObjectByID(ID);
            return;
        }
        if(user->getUsedB32Dest()==true){
            user->setOnlineState(USEROFFLINE);
            user->setConnectionStatus(TRYTOCONNECT);
        }else{
            mConnectionManager->doDestroyStreamObjectByID(ID);
            user->slotIncomingMessageFromSystem(tr("Invalid User - Destination: please delete the user\n"));
            user->setConnectionStatus(CONNECTERROR);
        }
        deletePacketManagerByID(ID);

    }
    else if(result==SAM_Message_Types::I2P_ERROR){
        if(user==NULL){
            mConnectionManager->doDestroyStreamObjectByID(ID);
            return;
        }
        user->slotIncomingMessageFromSystem(tr("I2P_Error: %1").arg(Message));
        deletePacketManagerByID(ID);
        mConnectionManager->doDestroyStreamObjectByID(ID);
        user->setConnectionStatus(CONNECTERROR);
    }

}

void CCore::setUserProtocolVersionByStreamID(qint32 ID,QString Version){
    CUser* theUser=mUserManager->getUserByI2P_ID(ID);

    if(theUser!=NULL){
        theUser->setProtocolVersion(Version);
    }
}

void CCore::closeAllActiveConnections(){
    //close all known Online||TrytoConnect Connections
    QList<CUser*> users=mUserManager->getUserList();

    for(int i=0;i<users.size();i++){
        if(users.at(i)->getConnectionStatus()==ONLINE ||
                users.at(i)->getConnectionStatus()==TRYTOCONNECT)
        {
            deletePacketManagerByID(users.at(i)->getI2PStreamID());
            mConnectionManager->doDestroyStreamObjectByID(users.at(i)->getI2PStreamID());
            users.at(i)->setConnectionStatus(User::OFFLINE);
            users.at(i)->setOnlineState(USEROFFLINE);
        }
    }
}

void CCore::slotNamingReplyRecived(const SAM_Message_Types::RESULT result,QString Name,QString Value,QString Message){
    if(result==SAM_Message_Types::OK && Name=="ME"&& mMyDestination.isEmpty()){
        this->mMyDestination=Value;
    }
    else if(result==SAM_Message_Types::OK){
        CUser* theUser=mUserManager->getUserByI2P_Destination(Name);
        if(theUser!=NULL && theUser->getUsedB32Dest()==true){
            theUser->setReplaceB32WithB64(Value);
        }
    }else if(result==SAM_Message_Types::FAILED){
        qWarning()<<"File\t"<<__FILE__<<endl
                 <<"Line:\t"<<__LINE__<<endl
                <<"Function:\t"<<"CCore::slotNamingReplyRecived"<<endl
               <<"Message:\t"<<"slotNamingReplyRecived\nSAM_Message_Types::FAILED"<<endl
              <<"Name:\t"<<Name<<endl
             <<"Value:\t"<<Value<<endl
            <<"Message:\t"<<Message<<endl;
    }

}
const QString CCore::getMyDestination()const{
    return this->mMyDestination;
}

void CCore::deletePacketManagerByID(qint32 ID){

    const CI2PStream* Stream=NULL;
    if(mConnectionManager->isComponentStopped()==false){
        Stream=mConnectionManager->getStreamObjectByID(ID);
    }

    for(int i=0;i<mDataPacketsManagers.count();i++){

        if(ID==mDataPacketsManagers.at(i)->getID()){
            CPacketManager* CurrentManager=mDataPacketsManagers.takeAt(i);

            if(Stream!=NULL){
                disconnect(Stream,SIGNAL(signDataRecived(const qint32, const QByteArray)),(CurrentManager),
                           SLOT(slotDataInput(qint32, QByteArray)));
            }
            CurrentManager->deleteLater();
            break;
        }
    }
}

QString CCore::getConnectionDump()const{
    QString Message;
    QString StreamControllerBridgeName;

    if(mConnectionManager->isComponentStopped()==true){
        Message="The Core is stopped !";
        return Message;
    }
    StreamControllerBridgeName=mConnectionManager->getStreamControllerBridgeName();

    Message="< Stream Controller >\n";
    Message+="\tNetwork:\tI2P\n";
    Message+="\tStreamControllerBridgeName:\t"+StreamControllerBridgeName+"\n";
    Message+="-----------------------------------------------\n\n";


    const QMap<qint32,CI2PStream*>* allListener=mConnectionManager->getAllStreamIncomingListenerObjects();
    const QList<CI2PStream*> allStreamsListenerList=allListener->values();
    Message="< Stream Incoming Listener >\n";
    for(int i=0;i<allStreamsListenerList.count();i++){
        CI2PStream* Stream=allStreamsListenerList.value(i);
        Message+="\n\tStreamID:\t\t"+QString::number(Stream->getID())+"\n";
        //Print StreamMode
        if(Stream->getStreamMode()==STREAMS::CONNECT){
            Message+="\tStreamMode:\tCONNECT\n";
        }
        else if(Stream->getStreamMode()==STREAMS::ACCEPT){
            Message+="\tStreamMode:\tACCEPT\n";
        }
        else{
            Message+="\tStreamMode:\t???\n";
        }
        //---------------------------------------------------

        //Print ConnectionType
        if(Stream->getConnectionType()==UNKNOWN){
            Message+="\tConnectionTrust:\tUNKNOWN\n";
        }
        else if (Stream->getConnectionType()==KNOWN){
            Message+="\tConnectionTrust:\tKNOWN\n";
        }
        else{
            Message+="\tConnectionTrust:\t???\n";
        }
        //---------------------------------------------------

        Message+="\tUsed for:\t\t"+Stream->getUsedFor()+"\n";

    }
    Message+="-----------------------------------------------\n\n";



    Message+="< Streams >\n";
    const QMap<qint32,CI2PStream*>* allStreams=mConnectionManager->getAllStreamObjects();
    const QList<CI2PStream*> allStreamsList=allStreams->values();



    for(int n=0;n<allStreamsList.size();n++){

        CI2PStream* Stream=allStreamsList.value(n);
        QString StreamID;
        CUser* theUser=NULL;

        StreamID.setNum(Stream->getID(),10);

        Message+="\n\tStreamID:\t\t"+StreamID+"\n";

        //Print StreamMode
        if(Stream->getStreamMode()==STREAMS::CONNECT){
            Message+="\tStreamMode:\tCONNECT\n";
        }
        else if(Stream->getStreamMode()==STREAMS::ACCEPT){
            Message+="\tStreamMode:\tACCEPT\n";
        }
        else{
            Message+="\tStreamMode:\t???\n";
        }
        //-----------------------------------------

        //Print ConnectionType
        if(Stream->getConnectionType()==UNKNOWN){
            Message+="\tConnectionTrust:\tUNKNOWN\n";
        }
        else if (Stream->getConnectionType()==KNOWN){
            Message+="\tConnectionTrust:\tKNOWN\n";
        }
        else{
            Message+="\tConnectionTrust:\t???\n";
        }
        //------------------------------------------
        Message+="\tUsed for:\t\t"+Stream->getUsedFor()+"\n";

        theUser=mUserManager->getUserByI2P_ID(Stream->getID());
        if(theUser==NULL){
            Message+="\tUser: \n";
        }
        else{
            Message+="\tUser:\t\t"		+theUser->getName()+"\n";
            Message+="\tClientName:\t\t"	+theUser->getClientName()+"\n";
            Message+="\tClientVersion:\t"	+theUser->getClientVersion()+"\n";
            Message+="\tProtocolVersion:\t" +theUser->getProtocolVersion()+"\n";
        }

    }
    Message+="-----------------------------------------------\n\n";

    return Message;
}

ONLINESTATE CCore::getOnlineStatus() const
{
    return this->mCurrentOnlineStatus;
}

void CCore::setOnlineStatus(const ONLINESTATE newStatus)
{
    if(mCurrentOnlineStatus==newStatus) return;

    if(mCurrentOnlineStatus==USEROFFLINE){
        mNextOnlineStatus=newStatus;
        mCurrentOnlineStatus=USERTRYTOCONNECT;
        init();
        emit signOnlineStatusChanged();
        return;
    }


    if(newStatus==User::USEROFFLINE){
        this->mCurrentOnlineStatus=newStatus;
        stopCore();
    }
    else if(newStatus==USERTRYTOCONNECT){
        if(mCurrentOnlineStatus==USEROFFLINE)
        {
            this->mCurrentOnlineStatus=newStatus;
            restartCore();
        }
    }
    else
    {
        //send new Status to every connected User
        this->mCurrentOnlineStatus=newStatus;
        QList<CUser*> Users=mUserManager->getUserList();

        for(int i=0;i<Users.size();i++){
            if(Users.at(i)->getIsInvisible()==true){
                mProtocol->send(USER_ONLINESTATUS_OFFLINE,Users.at(i)->getI2PStreamID(),QString(""));
            }else{
                if(Users.at(i)->getConnectionStatus()==ONLINE)
                {
                    switch(this->mCurrentOnlineStatus)
                    {
                    case USERONLINE:
                    {
                        mProtocol->send(USER_ONLINESTATUS_ONLINE,Users.at(i)->getI2PStreamID(),QString(""));
                        break;
                    }
                    case USEROFFLINE:
                    {
                        break;
                    }
                    case USERINVISIBLE:
                    {
                        mProtocol->send(USER_ONLINESTATUS_OFFLINE,Users.at(i)->getI2PStreamID(),QString(""));
                        break;
                    }
                    case USERAWAY:
                    {
                        mProtocol->send(USER_ONLINESTATUS_AWAY,Users.at(i)->getI2PStreamID(),QString(""));
                        break;

                    }
                    case USERWANTTOCHAT:
                    {
                        mProtocol->send(USER_ONLINESTATUS_WANTTOCHAT,Users.at(i)->getI2PStreamID(),QString(""));
                        break;
                    }
                    case USERDONT_DISTURB:
                    {
                        mProtocol->send(USER_ONLINESTATUS_DONT_DISTURB,Users.at(i)->getI2PStreamID(),QString(""));
                        break;
                    }
                    case USERTRYTOCONNECT:
                    {
                        break;
                    }
                    default:
                    {
                        QMessageBox* msgBox= new QMessageBox(NULL);
                        msgBox->setIcon(QMessageBox::Warning);
                        msgBox->setText("CCore(setOnlineStatus)");
                        msgBox->setInformativeText("Unknown USERSTATE");
                        msgBox->setStandardButtons(QMessageBox::Ok);
                        msgBox->setDefaultButton(QMessageBox::Ok);
                        msgBox->setWindowModality(Qt::NonModal);
                        msgBox->show();
                    }
                    }
                }//if
            }//else
        }//for
    }
    emit signOnlineStatusChanged();
}

void CCore::stopCore()
{
    mMyDestination="";
    closeAllActiveConnections();
    mConnectionManager->doStopp();
}

void CCore::restartCore()
{
    this->init();
}

QString CCore::getDestinationByID(qint32 ID) const
{
    CUser* user=mUserManager->getUserByI2P_ID(ID);
    if(user!=NULL){
        return user->getI2PDestination();
    }

    CFileTransferSend* send=mFileTransferManager->getFileTransferSendsByID(ID);
    if(send!=NULL){
        return send->getDestination();
    }

    CFileTransferRecive* recive=mFileTransferManager->getFileTransferReciveByID(ID);
    if(recive!=NULL){
        return recive->getDestination();
    }

    return "";
}

void CCore::slotStreamControllerStatusOK(bool Status)
{
    if(Status==true){
        mCurrentOnlineStatus=mNextOnlineStatus;
        if(mMyDestination.isEmpty()){
            mConnectionManager->doNamingLookUP("ME");//get the current Destination from this client
        }
        createStreamObjectsForAllUsers();
        emit signOnlineStatusChanged();
    }

}


void CCore::createStreamObjectsForAllUsers()
{
    QList<CUser*> users=mUserManager->getUserList();

    for(int i=0;i<users.size();i++){
        if(mConnectionManager->isComponentStopped()==false){
            this->createStreamObjectForUser(*(users.at(i)));
        }
    }

}

void CCore::setStreamTypeToKnown(qint32 ID,const QByteArray Data,bool isFileTransfer_Recive)
{
    CI2PStream* t=mConnectionManager->getStreamObjectByID(ID);
    t->setConnectionType(KNOWN);
    disconnect(t,SIGNAL(signDataRecived(const qint32, const QByteArray)),mProtocol,
               SLOT(slotInputUnknown(const qint32, const QByteArray)));

    if(isFileTransfer_Recive==false){
        CPacketManager* packetManager= new CPacketManager(*mConnectionManager,ID);
        connect(t,SIGNAL(signDataRecived(const qint32, const QByteArray)),packetManager,
                SLOT(slotDataInput(qint32, QByteArray)));

        connect(packetManager,SIGNAL(signAPacketIsCompleate(const qint32, const QByteArray)),mProtocol,
                SLOT(slotInputKnown(const qint32, const QByteArray)));

        t->setUsedFor("ChatConnection");
        if(Data.isEmpty()==false){
            packetManager->slotDataInput(ID,Data);
        }
        mDataPacketsManagers.push_back(packetManager);


    }
    if(isFileTransfer_Recive==true){

    }

}

CI2PStream * CCore::getI2PStreamObjectByID(qint32 ID)const
{
    return mConnectionManager->getStreamObjectByID(ID);
}

void CCore::slotIncomingStream(CI2PStream * stream)
{
    //all incoming stream are first Unknown
    connect(stream,SIGNAL(signDataRecived(const qint32, const QByteArray)),mProtocol,
            SLOT(slotInputUnknown(const qint32, const QByteArray)));
}

void CCore::createStreamObjectForUser(CUser& User)
{
    qint32 msec=0;
    qint32 ID=User.getI2PStreamID();

    if(mConnectionManager->isComponentStopped()==true) return;

    if(mConnectionManager->getStreamObjectByID(ID)!=NULL){
        return;
    }

    QSettings* settings= new QSettings(mConfigPath+"/application.ini",QSettings::IniFormat);
    settings->beginGroup("General");
    msec=settings->value("Waittime_between_rechecking_offline_mUsers","30000").toInt();
    settings->endGroup();

    CI2PStream* t =mConnectionManager->doCreateNewStreamObject(CONNECT);
    connect(t,SIGNAL(signStreamStatusRecived(const SAM_Message_Types::RESULT, const qint32, const QString)),this,
            SLOT(slotStreamStatusRecived(const SAM_Message_Types::RESULT, const qint32, QString)));

    User.setI2PStreamID(t->getID());

    if(t!=NULL){
        connect(t,SIGNAL(signDataRecived(const qint32, const QByteArray)),mProtocol,
                SLOT(slotInputUnknown(const qint32, const QByteArray)));
        t->doConnect(User.getI2PDestination());
        t->startUnlimintedReconnect(msec);
    }
    settings->sync();
    delete settings;
}

void CCore::slotNewSamPrivKeyGenerated(const QString SamPrivKey)
{
    QSettings* settings= new QSettings(mConfigPath+"/application.ini",QSettings::IniFormat);
    settings->beginGroup("Network");
    settings->setValue("SamPrivKey",SamPrivKey);
    settings->endGroup();
    settings->sync();
    delete settings;
}

bool CCore::useThisChatConnection(const QString Destination, const qint32 ID)
{
    CUser* theUser=NULL;

    theUser=mUserManager->getUserByI2P_Destination(Destination);
    if(theUser!=NULL){
        //check if connection to user allready exist
        if(theUser->getI2PStreamID()!=ID){
            if(theUser->getConnectionStatus()==ONLINE){
                //close the new connection,- we have allready a connection to this user
                mConnectionManager->doDestroyStreamObjectByID(ID);
                return false;
            }
            else if(theUser->getConnectionStatus()==TRYTOCONNECT){
                //use the new connection
                //close the TRYTOCONNECT connection
                deletePacketManagerByID(theUser->getI2PStreamID());
                mConnectionManager->doDestroyStreamObjectByID(theUser->getI2PStreamID());
                theUser->setI2PStreamID(ID);
                return true;
            }
        }
    }
    return true;
}

void CCore::loadUserInfos()
{
    QSettings settings(mConfigPath+"/application.ini",QSettings::IniFormat);
    settings.beginGroup("User-Infos");
    if(mUserInfos.Nickname!=settings.value("Nickname","").toString()){
        mUserInfos.Nickname=settings.value("Nickname","").toString();
        emit signNicknameChanged();
    }

    if(mUserInfos.Nickname.isEmpty()==true){
        //generate random Nickname (8 Chars)
        const QString possibleCharacters("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
        const int randomStringLength = 8;

        QString randomString;
        for(int i=0; i<randomStringLength; ++i)
        {
            int index = qrand() % possibleCharacters.length();
            QChar nextChar = possibleCharacters.at(index);
            randomString.append(nextChar);
        }
        mUserInfos.Nickname=randomString;

        settings.setValue("Nickname",mUserInfos.Nickname);
        emit signNicknameChanged();

        QMessageBox* msgBox= new QMessageBox(NULL);
        msgBox->setIcon(QMessageBox::Information);
        msgBox->setInformativeText(tr("No username...\ngenerating one: %1\n\nplease change your userprofile in the settings").arg(randomString));
        msgBox->setStandardButtons(QMessageBox::Ok);
        msgBox->setDefaultButton(QMessageBox::Ok);
        msgBox->setWindowModality(Qt::NonModal);
        msgBox->show();
    }

    mUserInfos.Age=         settings.value("Age","0").toInt();
    mUserInfos.Interests=   settings.value("Interests","").toString();

    if(settings.value("Gender","").toString()=="Male"){
        mUserInfos.Gender="Male";
    }
    else if(settings.value("Gender","").toString()=="Female"){
        mUserInfos.Gender="Female";
    }
    if(mUserInfos.AvatarImage!=settings.value("AvatarBinaryImage","").toByteArray()){
        mUserInfos.AvatarImage=settings.value("AvatarBinaryImage","").toByteArray();
        emit signOwnAvatarImageChanged();
    }
    settings.endGroup();
    settings.sync();
}

const CRecivedInfos CCore::getUserInfos() const
{
    return mUserInfos;
}



void CCore::doConvertNumberToTransferSize(quint64 inNumber, QString & outNumber, QString& outType,bool addStoOutType)const
{
    QString SSize;

    quint64 Size=inNumber;
    if(Size>=(1024*1024))
    {//MB
        double dMB= double(Size) / double(1024*1024);
        SSize=QString( "%1" ).arg(dMB,0,'d',2);
        outNumber=SSize;
        addStoOutType==true ?  outType="MB/s":outType="MB";


        return;
    }
    else if(Size>=1024){
        //KB
        double dKB= double(Size) / double(1024);
        SSize=QString( "%1" ).arg(dKB,0,'d',2);
        outNumber=SSize;
        addStoOutType==true ?  outType="KB/s":outType="KB";
        return;
    }
    else{
        //Byte
        SSize.setNum(Size,10);
        outNumber=SSize;
        addStoOutType==true ?  outType="Bytes/s":outType="Bytes";
        return;
    }
}

const QString CCore::getMyDestinationB32() const
{
    return mMyDestinationB32;
}


void CCore::setMyDestinationB32(QString B32Dest)
{
    if(mMyDestinationB32==B32Dest) return;
    
    if(!B32Dest.right(8).contains(".b32.i2p",Qt::CaseInsensitive)){
        qCritical()<<"File\t"<<__FILE__<<endl
                  <<"Line:\t"<<__LINE__<<endl
                 <<"Function:\t"<<"CCore::setMyDestinationB32"<<endl
                <<"Message:\t" <<"the last 8 char must be .b32.i2p"<<endl
               <<"\tDestination:\n"<<B32Dest<<endl
              <<"\tAction apported"<<endl;
    }
    
    QSettings settings(mConfigPath+"/application.ini",QSettings::IniFormat);
    settings.beginGroup("Network");
    settings.setValue("MyDestinationB32",B32Dest);
    settings.endGroup();
    settings.sync();
    
    mMyDestinationB32=B32Dest;
}

QString CCore::canonicalizeTopicId(QString topicIdNonCanonicalized) {

    //FIXME canonicalizeTopicId(topicIdNonCanonicalized);
    return topicIdNonCanonicalized;
}

