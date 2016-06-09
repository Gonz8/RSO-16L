#include "extserver.h"

extServer::extServer(int extPort, int dbPort, int clientPort){
    this->log("Init ExtServer, extPort: " + QString::number(extPort) + " dbPort: " + QString::number(dbPort) + " clientPort: " + QString::number(clientPort));
    isMasterCandidate = false;
    elecErrorCnt = 0;
    lastAskingTime = QTime::currentTime().addSecs( - Configuration::getInstance().interval() - 1);
    lastBeingAskedTime = QTime::currentTime();
    //std::cout<<"myNum:"<<Configuration::getInstance().myNum()<<", ports: "<<extPort<<" "<<dbPort<<" "<<clientPort<<std::endl;
    extPortListener = new TcpServer(extPort);
    dbPortListener = new TcpServer(dbPort);
    clientPortListener = new TcpServer(clientPort);

    QObject::connect(extPortListener, SIGNAL(log(QString)), this, SLOT(log(QString)));
    QObject::connect(extPortListener, SIGNAL(frameContent(QTcpSocket*,QStringList)), this, SLOT(frameExtReceived(QTcpSocket*,QStringList)));
    QObject::connect(extPortListener, SIGNAL(error(QString, QString)), this, SLOT(frameExtReceivedError(QString,QString)));

    QObject::connect(dbPortListener, SIGNAL(log(QString)), this, SLOT(log(QString)));
    QObject::connect(dbPortListener, SIGNAL(frameContent(QTcpSocket*,QStringList)), this, SLOT(frameDBReceived(QTcpSocket*,QStringList)));
    QObject::connect(dbPortListener, SIGNAL(error(QString, QString)), this, SLOT(frameExtReceivedError(QString,QString)));

    QObject::connect(clientPortListener, SIGNAL(log(QString)), this, SLOT(log(QString)));
    QObject::connect(clientPortListener, SIGNAL(frameContent(QTcpSocket*,QStringList)), this, SLOT(frameClientReceived(QTcpSocket*,QStringList)));
    QObject::connect(clientPortListener, SIGNAL(error(QString, QString)), this, SLOT(frameExtReceivedError(QString,QString)));

    extFunctionMap[FrameType::STATUS] = &extServer::status;
    extFunctionMap[FrameType::SERVER_STATUS_OK] = &extServer::statusOK;

    extFunctionMap[FrameType::ACTIVE_SERVERS_EXT] = &extServer::activeServersExt;

    extFunctionMap[FrameType::ELECTION] = &extServer::election;
    extFunctionMap[FrameType::ELECTION_STOP] = &extServer::electionStop;
    extFunctionMap[FrameType::COORDINATOR] = &extServer::coordinator;
    extFunctionMap[FrameType::ACTIVE_SERVERS_DB] = &extServer::activeServersDB;

    dbFunctionMap[FrameType::GET_ACTIVE_SERVERS_EXT] = &extServer::getActiveServersExt;
    dbFunctionMap[FrameType::ACTIVE_SERVERS_DB] = &extServer::activeServersDB;
    dbFunctionMap[FrameType::RESULTS] = &extServer::results;
    dbFunctionMap[FrameType::RESULT] = &extServer::result;
    dbFunctionMap[FrameType::STATISTICS] = &extServer::statistics;

    clientFunctionMap[FrameType::ACTIVE_SERVERS] = &extServer::activeServers;
    clientFunctionMap[FrameType::GET_AVAILABLE_RESULTS] = &extServer::getAvailableResults;
    clientFunctionMap[FrameType::GET_RESULT] = &extServer::getResult;
    clientFunctionMap[FrameType::GET_STATISTICS] = &extServer::getStatistics;

    extFunctionMap[FrameType::ERROR] = &extServer::errorFrame;
    clientFunctionMap[FrameType::ERROR] = &extServer::errorFrame;
    dbFunctionMap[FrameType::ERROR] = &extServer::errorFrame;
}

void extServer::start(){
    this->log("Start server");
    running = true;
    qsrand((uint)QTime::currentTime().msec());
    extPortListener->start();
    dbPortListener->start();
    clientPortListener->start();
    while(running){
        mainLoop();
    }
}

void extServer::mainLoop(){
    log("loop");
    if(this->clientQueue.isEmpty() && this->extQueue.isEmpty() && this->dbQueue.isEmpty()){
        QTime dieTime= QTime::currentTime().addSecs(5);
        while (QTime::currentTime() < dieTime)
            QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
    }
    else{
        if(!this->dbQueue.isEmpty()){
            frameDBAnalyze(dbQueue.first());
            dbQueue.pop_front();
        }
        if(!this->extQueue.isEmpty()){
            frameExtAnalyze(extQueue.first());
            extQueue.pop_front();
        }
        if(!this->clientQueue.isEmpty()){
            frameClientAnalyze(clientQueue.first());
            clientQueue.pop_front();
        }
        //masterAction();  //tymczas na zewnątrz
    }
    masterAction();
}

void extServer::frameExtReceived(QTcpSocket* socket, QStringList msg){
    log("extFrame: " + msg.join(","));
    Request r;
    r.msg = msg;
    r.socket = socket;
    this->extQueue.append(r);
}

void extServer::frameDBReceived(QTcpSocket*socket, QStringList msg){
    log("dbFrame: " + msg.join(","));
    Request r;
    r.msg = msg;
    r.socket = socket;
    this->dbQueue.append(r);
}

void extServer::frameClientReceived(QTcpSocket*socket, QStringList msg){
    log("client Frame: " + msg.join(","));
    std::cout<<"Received msg from client"<<std::endl;
    Request r;
    r.msg = msg;
    r.socket = socket;
    this->clientQueue.append(r);
}

void extServer::frameExtAnalyze(Request &r){
    log('analyze ext frame ' + r.msg.join(","));
    int sender = r.msg[0].toInt();
    QString type = r.msg[1];
    if(extFunctionMap.find(type)!=extFunctionMap.end())
        (this->*(extFunctionMap[type]))(r, sender);
}

void extServer::frameDBAnalyze(Request r){
    log('analyze db frame ' + r.msg.join(","));
    int sender = r.msg[0].toInt();
    QString type = r.msg[1];
    if(dbFunctionMap.find(type)!=dbFunctionMap.end())
        (this->*(dbFunctionMap[type]))(r, sender);
}

void extServer::frameClientAnalyze(Request r){
    log('analyze client frame ' + r.msg.join(","));
    QString type = r.msg[0];
    if(clientFunctionMap.find(type)!=clientFunctionMap.end())
        (this->*(clientFunctionMap[type]))(r, 0);
}

void extServer::frameExtReceivedError(QString ip, QString error){
    log("FrameExt error: " + error);
    //
    if(isMasterCandidate){
        elecErrorCnt++;
        log("master candidate election error counter: " + QString::number(elecErrorCnt));
    }
    //
    QMap<int, SServer> servers = Configuration::getInstance().getExtServers();
    for (auto i = servers.begin(); i!=servers.end(); i++){
        if(i.value().getIp()==ip && i.value().isActive()){
            log("set " + ip + " inactive");
            Configuration::getInstance().setServerActive(i.key(), false);
            this->sendExtStateToAll();
            return;
        }
    }
}

void extServer::frameDBReceivedError(QString ip, QString error){
    log("FrameDB error: " + error);
    QMap<int, SServer> dbServers = Configuration::getInstance().getDBServers();
    for (auto i = dbServers.begin(); i!=dbServers.end(); i++){
        if(i.value().getIp()==ip && i.value().isActive()){
            log("set " + ip + " inactive");
            Configuration::getInstance().setServerActive(i.key(), false);
            QStringList state = getDBState();
            log("Propagate DBServers state:  " + state.join(" "));
            QMap<int, SServer> servers = Configuration::getInstance().getExtServers();
            for (auto i = servers.begin(); i!=servers.end(); i++){
                extPortListener->sendFrame(QHostAddress(i.value().getIp()), i.value().getPortExt(), makeFrame(FrameType::ACTIVE_SERVERS_DB, state));
            }
            return;
        }
    }
}

void extServer::frameClientReceivedError(QString ip, QString error){
    log("Client error: " + error);
}

void extServer::masterAction(){
    log("master action");
    //std::cout<<isMasterCandidate<<std::endl;
    if(Configuration::getInstance().isMaster()){
        log("i am master");
        if(lastAskingTime.secsTo(QTime::currentTime()) >= Configuration::getInstance().interval() )
            askForState();
            askDBForState();

    }
    else{
        log("i am not master");
        if(lastBeingAskedTime.secsTo(QTime::currentTime())> 2 * Configuration::getInstance().interval()){
            if(!isMasterCandidate){
                log("ExtServers:: NO master!");
                startElection();
            }
        }
    }
    if(isMasterCandidate){
        log("master candidate");
        QVector<SServer> serversUnder = Configuration::getInstance().getExtServersUnderMe();
        if(serversUnder.size() == elecErrorCnt){
            log("no active servers under me");
            isMasterCandidate = false;
            Configuration::getInstance().setMaster(Configuration::getInstance().myNum());
            this->sendNewMasterToAll();
            elecErrorCnt = 0;
        }
    }
}

void extServer::startElection(){
    log("ExtServers:: START ELECTION!");
    QVector<int> active = Configuration::getInstance().getActiveExtServers();
    //if(active.size() == 1){
    //    Configuration::getInstance().setMaster(Configuration::getInstance().myNum());
    //    this->sendNewMasterToAll();
    //    return;
    //}
    QVector<SServer> serversUnder = Configuration::getInstance().getExtServersUnderMe();
    if(serversUnder.isEmpty()){
        log("ExtServers:: server under is empty");
        Configuration::getInstance().setMaster(Configuration::getInstance().myNum());
        this->sendNewMasterToAll();
        elecErrorCnt = 0;
        isMasterCandidate = false;
    } else {
        isMasterCandidate = true;
        for (auto it = serversUnder.begin(); it!=serversUnder.end(); it++){
            SServer srv = *it;
            log("ExtServers:: send election to !" + QString::number(srv.getNum()));
            extPortListener->sendFrame(QHostAddress(srv.getIp()), srv.getPortExt(), makeFrame(FrameType::ELECTION));
        }
    }

}

void extServer::askForState(){
    log("ExtServers:: Check if alive");
    QMap<int, SServer> servers = Configuration::getInstance().getExtServers();
    for (auto i = servers.begin(); i!=servers.end(); i++){
        if(i.value().getNum()!=Configuration::getInstance().myNum()){
            log("ask " + QString::number( i.value().getNum()));
            extPortListener->sendFrame(QHostAddress(i.value().getIp()), i.value().getPortExt(), makeFrame(FrameType::STATUS));
        }
    }
    lastAskingTime = QTime::currentTime();
}

void extServer::askDBForState(){
    log("ExtServers:: SEND GET_AVAIALBLE_SERVERS_TO to random DBServer");
    QMap<int, SServer> servers = Configuration::getInstance().getDBServers();

        int dbServer_nb = randInt(1, servers.size());
        SServer srv = servers[dbServer_nb];
        log("Send to dbserver" + QString::number(srv.getNum()) + " " + srv.getIp());
        dbPortListener->sendFrame(QHostAddress(srv.getIp()), srv.getPortExt(), makeFrame(FrameType::GET_ACTIVE_SERVERS_DB));
}

QStringList extServer::getExtState(){
    log("Read  ExtServers state");
    QVector<int> active = Configuration::getInstance().getActiveExtServers();
    QStringList result;
    for (auto i = active.begin(); i!=active.end(); i++){
           result << QString::number(*i);
    }
    return result;
}

QStringList extServer::getDBState(){
    log("Read  DBServers state");
    QVector<int> active = Configuration::getInstance().getActiveDBServers();
    QStringList result;
    for (auto i = active.begin(); i!=active.end(); i++){
           result << QString::number(*i);
    }
    return result;
}

void extServer::sendExtStateToAll(){
    log("Propagate ExtServers state");
    QStringList state = getExtState();
    log(state.join(" "));
    QMap<int, SServer> servers = Configuration::getInstance().getExtServers();
    for (auto i = servers.begin(); i!=servers.end(); i++){
        if(i.value().getNum()!=Configuration::getInstance().myNum()){
            log("send to " + i.value().getIp());
            extPortListener->sendFrame(QHostAddress(i.value().getIp()), i.value().getPortExt(), makeFrame(FrameType::ACTIVE_SERVERS_EXT, state));
        }
    }
}

void extServer::sendNewMasterToAll(){
    log("ExtServers:: NEW Master! -> " + QString::number(Configuration::getInstance().myNum()));
    QMap<int, SServer> servers = Configuration::getInstance().getExtServers();
    for (auto i = servers.begin(); i!=servers.end(); i++){
        if(i.value().getNum()!=Configuration::getInstance().myNum()){
            log("send to " + i.value().getIp());
            extPortListener->sendFrame(QHostAddress(i.value().getIp()), i.value().getPortExt(), makeFrame(FrameType::COORDINATOR));
        }
    }
}

QStringList extServer::makeFrame(QString frameType, QStringList data){
    return QStringList() << QString::number(Configuration::getInstance().myNum()) << frameType << data;
}

QStringList extServer::makeClientFrame(QString frameType, QStringList data){
    return QStringList() << frameType << data;
}

QStringList extServer::makeFrame(QString frameType){
    return QStringList() << QString::number(Configuration::getInstance().myNum()) << frameType;
}

QStringList extServer::makeClientFrame(QString frameType){
    return QStringList() << frameType;
}

void extServer::status(Request& r, int sender){
    log("master asked if i am ok");
    lastBeingAskedTime = QTime::currentTime();
    log("send him ok");
    if(r.socket->isOpen()){

    }else{
        log("Socket is not open!");
    }
    SServer srv = Configuration::getInstance().getExtServer(sender);
    extPortListener->sendFrame(QHostAddress(srv.getIp()), srv.getPortExt(), makeFrame(FrameType::SERVER_STATUS_OK));
    if(Configuration::getInstance().isMaster()){
        log("set new master " + QString::number(sender));
        Configuration::getInstance().setMaster(sender);
    }
}

void extServer::statusOK(Request& r, int sender){
    log(QString::number(sender) + " is OK/Alive");
    if(Configuration::getInstance().isMaster()){
        if(!Configuration::getInstance().getExtServer(sender).isActive()){
            log("new server active! " + QString::number(sender));
            Configuration::getInstance().setServerActive(sender, true);
            sendExtStateToAll();
        }
    }
}

void extServer::election(Request& r, int sender){
    log("get election");
    isMasterCandidate = true;//jest w startElection
    log("send election stop");
    SServer srv = Configuration::getInstance().getExtServer(sender);
    extPortListener->sendFrame(QHostAddress(srv.getIp()), srv.getPortExt(), makeFrame(FrameType::ELECTION_STOP));
    startElection();
}

void extServer::electionStop(Request& r, int sender){
    log("ExtServers:: Election Stop!");
    isMasterCandidate = false;
    elecErrorCnt = 0;
}

void extServer::coordinator(Request& r, int sender){
    log("ExtServers:: NEW Master! -> " + QString::number(sender));
    isMasterCandidate = false;
    Configuration::getInstance().setMaster(sender);
    lastAskingTime=QTime::currentTime();
}

void extServer::getActiveServersExt(Request& r, int sender){
    log("Send active ExtServers to DBServer: " + QString::number(sender));
    QMap<int, SServer> servers = Configuration::getInstance().getExtServers();
    QStringList result;
    for (auto i = servers.begin(); i!=servers.end(); i++){
        if(i.value().isActive())
            result<<QString::number(i.key());
    }
    log(result.join(" "));
    SServer srv = Configuration::getInstance().getDBServer(sender);
    dbPortListener->sendFrame(QHostAddress(srv.getIp()), srv.getPortExt(), makeFrame(FrameType::ACTIVE_SERVERS_EXT, result));
}

void extServer::activeServersExt(Request& r, int sender){
    log("Received active ExtServers from: " + QString::number(sender));
    QMap<int, SServer> servers = Configuration::getInstance().getExtServers();
    for (auto i = servers.begin(); i!=servers.end(); i++){
        if(r.msg.contains(QString::number(i.key())))
            Configuration::getInstance().setServerActive(i.key(), true);
        else
            Configuration::getInstance().setServerActive(i.key(), false);
    }
}

void extServer::activeServersDB(Request& r, int sender){
    if(Configuration::getInstance().isMaster()) {
        log("Received active DBServers from: (DBServer)" + QString::number(sender) + " and propagate DBServers state");
        QStringList result;
        for(int i = 2; i < r.msg.size(); i++){
            result << r.msg.at(i);
        }
        log("send db state to all servers");
        QMap<int, SServer> servers = Configuration::getInstance().getExtServers();
        for (auto i = servers.begin(); i!=servers.end(); i++){
            dbPortListener->sendFrame(QHostAddress(i.value().getIp()), i.value().getPortExt(), makeFrame(FrameType::ACTIVE_SERVERS_DB, result));
        }
    } else {
        log("Received active DBServers from: (myMaster)" + QString::number(sender));
    }
    QMap<int, SServer> servers = Configuration::getInstance().getDBServers();
    for (auto i = servers.begin(); i!=servers.end(); i++){
        if(r.msg.contains(QString::number(i.key())))
            Configuration::getInstance().setServerActive(i.key(), true);
        else
            Configuration::getInstance().setServerActive(i.key(), false);
    }
}

void extServer::activeServers(Request &r, int sender) {
    log("Send active ExtServers to Client");
    QMap<int, SServer> servers = Configuration::getInstance().getExtServers();
    QVector<int> active = Configuration::getInstance().getActiveExtServers();
    QStringList result;
    result << QString::number(active.size());
    for (auto i = servers.begin(); i!=servers.end(); i++){
        if(i.value().isActive())
            result << i.value().getIp() << QString::number(i.value().getPortClient());
    }
    log(result.join(" "));
    clientPortListener->sendFrame(r.socket, makeClientFrame(FrameType::ACTIVE_SERVERS, result));
}

void extServer::getAvailableResults(Request& r, int sender){
    log("Received getAvailableResults from Client");
    QString currTime = QTime::currentTime().toString();
//    std::cout<<"currTime: "<<currTime.toStdString()<<std::endl;
//    QByteArray qba;
//    QByteArray hash = QCryptographicHash::hash(qba.append(currTime),QCryptographicHash::Md5);
    clientSocketMap[currTime] = r.socket;
    //send to random active DBServer
    QVector<int> active = Configuration::getInstance().getActiveDBServers();
    if(!active.isEmpty()){
        //przy przekazywaniu ramki od klienta do serwera danych
        //sprawdzić czy nie (1,active.size()) Tak mi poprawnie zadziałało przy askDBForState
        int dbServer_nb = randInt(0, active.size()-1);
        QStringList result;
        result << currTime;
        for(int i = 1; i < r.msg.size(); i++){
            result << r.msg.at(i);
        }
        SServer srv = Configuration::getInstance().getDBServer(active.at(dbServer_nb));
        log("send " + result.join(",") + " to " + QString::number(srv.getNum()));
        log(srv.getIp() + " " + QString::number(srv.getPortExt()));
        dbPortListener->sendFrame(QHostAddress(srv.getIp()), srv.getPortExt(), makeFrame(FrameType::GET_AVAILABLE_RESULTS, result));
        //clientPortListener->sendFrame(QHostAddress(srv.getIp()), srv.getPortExt(), makeFrame(FrameType::GET_AVAILABLE_RESULTS, result));
    } else{
        log("DB LOST CONNECTION");
        clientPortListener->sendFrame(r.socket, makeClientFrame(FrameType::ERROR, QStringList() << "DB_LOST_CONNECTION"));
    }
}

void extServer::getResult(Request& r, int sender){
    log("Received getResult from Client");
    QString currTime = QTime::currentTime().toString();
    std::cout<<"currTime: "<<currTime.toStdString()<<std::endl;
//    QByteArray qba;
//    QByteArray hash = QCryptographicHash::hash(qba.append(currTime),QCryptographicHash::Md5);
    clientSocketMap[currTime] = r.socket;
    //send to random active DBServer
    QVector<int> active = Configuration::getInstance().getActiveDBServers();
    if(!active.isEmpty()){
        //przy przekazywaniu ramki od klienta do serwera danych
        //sprawdzić czy nie (1,active.size()) Tak mi poprawnie zadziałało przy askDBForState
        int dbServer_nb = randInt(0, active.size()-1);
        QStringList result;
        result << currTime;
        for(int i = 1; i < r.msg.size(); i++){
            result << r.msg.at(i);
        }
        SServer srv = Configuration::getInstance().getDBServer(active.at(dbServer_nb));
        log("send " + result.join(",") + " to " + QString::number(srv.getNum()));
        dbPortListener->sendFrame(QHostAddress(srv.getIp()), srv.getPortExt(), makeFrame(FrameType::GET_RESULT, result));
        //clientPortListener->sendFrame(QHostAddress(srv.getIp()), srv.getPortExt(), makeFrame(FrameType::GET_RESULT, result));
    } else{
        log("DB LOST CONNECTION");
        clientPortListener->sendFrame(r.socket, makeClientFrame(FrameType::ERROR, QStringList() << "DB_LOST_CONNECTION"));
    }
}

void extServer::getStatistics(Request& r, int sender){
    log("Received getStatistics from Client");
    QString currTime = QTime::currentTime().toString();
//    std::cout<<"currTime: "<<currTime.toStdString()<<std::endl;
//    QByteArray qba;
//    QByteArray hash = QCryptographicHash::hash(qba.append(currTime),QCryptographicHash::Md5);
    clientSocketMap[currTime] = r.socket;
    //send to random active DBServer
    QVector<int> active = Configuration::getInstance().getActiveDBServers();
    if(!active.isEmpty()){
        //przy przekazywaniu ramki od klienta do serwera danych
        //sprawdzić czy nie (1,active.size()) Tak mi poprawnie zadziałało przy askDBForState
        int dbServer_nb = randInt(0, active.size()-1);
        QStringList result;
        result << currTime;
        for(int i = 1; i < r.msg.size(); i++){
            result << r.msg.at(i);
        }
        SServer srv = Configuration::getInstance().getDBServer(active.at(dbServer_nb));
        log("send " + result.join(",") + " to " + QString::number(srv.getNum()));
        dbPortListener->sendFrame(QHostAddress(srv.getIp()), srv.getPortExt(), makeFrame(FrameType::GET_STATISTICS, result));
        //clientPortListener->sendFrame(QHostAddress(srv.getIp()), srv.getPortExt(), makeFrame(FrameType::GET_STATISTICS, result));
    } else{
        log("DB LOST CONNECTION");
        clientPortListener->sendFrame(r.socket, makeClientFrame(FrameType::ERROR, QStringList() << "DB_LOST_CONNECTION"));
    }
}

void extServer::results(Request &r, int sender){
    log("results from db: " + QString::number(sender));
    QString mapId = r.msg[2];
    if(clientSocketMap.find(mapId)!=clientSocketMap.end()){
        QStringList result;
        for(int i = 3; i < r.msg.size(); i++){//0->nr_nadawcy, 1->frameType, 2->currTime
            result << r.msg.at(i);
        }
        //dbPortListener
        log("send to client: " + result.join(","));
        clientPortListener->sendFrame(clientSocketMap[mapId], makeClientFrame(FrameType::RESULTS, result));
        clientSocketMap.remove(mapId);
        return;
    } else {
        log("Oops! Key not found in clientSocketMap");
    }
}

void extServer::result(Request &r, int sender){
    log("result from db: " + QString::number(sender));
    QString mapId = r.msg[2];
    if(clientSocketMap.find(mapId)!=clientSocketMap.end()){
        QStringList result;
        for(int i = 3; i < r.msg.size(); i++){//0->nr_nadawcy, 1->frameType, 2->currTime
            result << r.msg.at(i);
        }
        //dbPortListener
        log("send to client: " + result.join(","));
        clientPortListener->sendFrame(clientSocketMap[mapId], makeClientFrame(FrameType::RESULT, result));
        clientSocketMap.remove(mapId);
        return;
    } else {
        log("Oops! Key not found in clientSocketMap");
    }
}

void extServer::statistics(Request &r, int sender){
    log("statistics from db: " + QString::number(sender));
    QString mapId = r.msg[2];
    if(clientSocketMap.find(mapId)!=clientSocketMap.end()){
        QStringList result;
        for(int i = 3; i < r.msg.size(); i++){//0->nr_nadawcy, 1->frameType, 2->currTime
            result << r.msg.at(i);
        }
        //dbPortListener
        log("send to client: " + result.join(","));
        clientPortListener->sendFrame(clientSocketMap[mapId], makeClientFrame(FrameType::STATISTICS, result));
        clientSocketMap.remove(mapId);
        return;
    } else {
        log("Oops! Key not found in clientSocketMap");
    }
}

void extServer::errorFrame(Request &r, int sender){

}

int extServer::randInt(int low, int high)
{
// Random number between low and high
return qrand() % ((high + 1) - low) + low;
}

QString extServer::logFile="/log.txt";

void extServer::log(QString text)
{
    QFile file(QCoreApplication::applicationDirPath() + logFile);
    file.open(QIODevice::Append);
    QTextStream out(&file);
    out <<QTime::currentTime().toString()<<": "<< text <<"\n";
    file.flush();
    file.close();
    std::cout<<text.toStdString()<<std::endl;
}

void extServer::stop(){
    running = false;
}
