#include "client.h"

Client::Client()
{
    connect(&socket, SIGNAL(readyRead()), this, SLOT(receive()));
}

exitCode Client::send(QStringList what)
{
    //queue++;
    bool probowacDalej = true;
    int serverId = 0;
    while(probowacDalej && serverId < servers_->size())
    {
        socket.close();
        if(!socket.isOpen())
            socket.connectToHost(servers_->at(serverId).ip, servers_->at(serverId).port);
        if(socket.waitForConnected(3000))
        {
            socket.write(QString("("+what.join(",")+")").toStdString().c_str());
            if(!socket.waitForBytesWritten(3000))
            {
                serverId++;
                //połączono, ale nie odebrano zapytania
            }
            else
            {
               //
                probowacDalej = false;
                return OK;
            }
        }
        else
        {
            //nie udało się połączyć
            serverId++;
        }
    }

    return NO_SERVERS;
}

void Client::receive()
{
    QString response = socket.readAll();

    int pos1 = response.indexOf("(");
    int pos2 = response.indexOf(")");
    int len = pos2-pos1-1;
    if(len > 0)
        handle(response.mid(pos1+1, len).split(","));
}

void Client::handle(QStringList what)
{
    //std::cout << "<" << what.join("").toStdString() << ">" << std::endl;
    if(what.at(0) == "ACTIVE_SERVERS")
    {
        //std::cout << "ACT_SRC" << std::endl;
        int num = what.at(1).toInt();
        settings->clear();
        servers_->clear();
        for(int i=0; i< num; ++i)
        {
            Server s;
            if(what.at(i*2+2) == "localhost")
                s.ip = QHostAddress("127.0.0.1");
            else s.ip = QHostAddress(what.at(i*2+2));

            s.port = what.at(i*2+3).toInt();
            servers_->push_back(s);
            settings->setValue("server"+QString::number(i+1)+"/ip", s.ip.toString());
            settings->setValue("server"+QString::number(i+1)+"/port", QString::number(s.port));
        }

        settings->setValue("info/serversCount", servers_->size());
        settings->sync();
    }
    else if(what.at(0) == "RESULTS")
    {
        int num = what.at(1).toInt();
        for(int i=0; i< num; ++i)
        {
            std::cout << "id,nazwa badania,kraj,płeć,rasa,wiek" << std::endl;
            QStringList a = QStringList() << what.at(i*7+2) << "," << what.at(i*7+3) << "," << what.at(i*7+4) <<\
                          "," << what.at(i*7+5) << "," << what.at(i*7+6) << "," << what.at(i*7+7);
            std::cout << a.join(",").toStdString() << std::endl;
        }
    }
    else if(what.at(0) == "RESULT")
    {
        QString filename = what.at(1);
        std::cout << filename.toStdString() << std::endl;
        QByteArray content = QByteArray::fromBase64(what.at(3).toLatin1());
        QFile file(filename);
        if(file.open(QIODevice::WriteOnly))
        {
            file.write(content);
            file.close();
        }
    }
    else if(what.at(0) == "STATISTICS")
    {
        int num = what.at(1).toInt();
        for(int i=0; i< num; ++i)
        {
            std::cout << "grupa,liczba badań" << std::endl;
            QStringList a = QStringList() << what.at(i*2+2) << "," << what.at(i*2+3);
            std::cout << a.join(",").toStdString() << std::endl;
        }
    }
    //queue--;
}
