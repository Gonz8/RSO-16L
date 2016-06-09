#include "tcpserver.h"

TcpServer::TcpServer(int port): port_(port)
{
    connect(this, SIGNAL(newConnection()), this, SLOT(newConnection()));
}

void TcpServer::start()
{
    listen(QHostAddress::Any, port_);
}

void TcpServer::sendFrame(QHostAddress recipient, int port, QStringList content)
{
    QTcpSocket socket;
    QString packet = "("+content.join(",")+")";
    socket.connectToHost(recipient, port);
    socket.waitForConnected(50000);
    socket.write(packet.toStdString().c_str());
    if(!socket.waitForBytesWritten(3000))
    {
        emit log("(QHostAddress)Could not send frame to "+socket.peerAddress().toString()+":"+QString::number(socket.localPort())+" "+\
                 content.join(","));
        emit error(recipient.toString(), "SEND_FRAME_ERROR");    //todo
    }
}

void TcpServer::sendFrame(QTcpSocket *recipient, QStringList content)
{
    if(!recipient || !recipient->isOpen())
        return;
    QString packet = "("+content.join(",")+")";
    recipient->write(packet.toStdString().c_str());
    if(!recipient->waitForBytesWritten(3000))
    {
        emit log("(QTcpSocket)Could not send frame to "+recipient->peerAddress().toString()+":"+QString::number(recipient->localPort())+" "+\
                 content.join(","));
        emit error(recipient->peerAddress().toString(), "SEND_FRAME_ERROR");    //todo
    }
}

void TcpServer::newConnection()
{
    while(hasPendingConnections())
    {
        QTcpSocket* socket = nextPendingConnection();
        connect(socket, SIGNAL(readyRead()), this, SLOT(grabFrame()));
        connect(socket, SIGNAL(disconnected()), this, SLOT(disconnected()));
        emit log("New connection from " + socket->peerAddress().toString()+ ":"+QString::number(socket->localPort()));
    }
}

void TcpServer::disconnected()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if(socket)
    {
        emit log("Disconnected from " + socket->peerAddress().toString()+ ":"+QString::number(socket->localPort()));
    }
}

void TcpServer::grabFrame()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if(socket)
    {
        QString frame(socket->readAll());
        //emit log(" >R: " + frame);

        bool nadal = true;
        int pos1=0, pos2=0;
        while(nadal)
        {
            pos1 = frame.indexOf("(", pos1);
            pos2 = frame.indexOf(")", pos2);
            int len = pos2-pos1-1;
            if(len > 0)
                emit frameContent(socket, frame.mid(pos1+1, len).split(","));
            if(pos1 == -1 || pos2 == -1)
                nadal = false;
            pos1++;
            pos2++;
        }
    }
}
