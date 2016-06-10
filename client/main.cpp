#include <QCoreApplication>
#include <QSettings>
#include <QCommandLineParser>
#include <QTcpSocket>
#include <QHostAddress>
#include <QVector>
#include <QString>
#include <iostream>
#include "client.h"
#include "server.h"

//kody wyjścia:
// 10 nie ma serwerów z którymi się można połączyć

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    QVector<Server> servers;

    //wczytanie konfiguracji
    QSettings settings("client.cfg", QSettings::IniFormat);
    settings.sync();

    int serversCount = settings.value("info/serversCount", 0).toInt();

    if(serversCount == 0)
    {
        //błąd, nie ma adresów
        std::cerr << "No known servers in configuration file" << std::endl;
        return 2;
    }

    for(int i=1; i<=serversCount; ++i)
    {
        Server server;
        server.name = settings.value("server"+QString::number(i)+"/name", "unknown server").toString();
        server.ip = QHostAddress(settings.value("server"+QString::number(i)+"/ip", "0.0.0.0").toString());
        server.port = settings.value("server"+QString::number(i)+"/port", "0").toInt();
        servers.push_back(server);
    }

    //połączenie i pobranie listy serwerów
    Client client;
    client.setServers(&servers);
    client.settings = &settings;

    //std::cout << "ACT?" << std::endl;
    if(client.send(QStringList() << "ACTIVE_SERVERS") == NO_SERVERS)
    {
        std::cout << "nie można się z niczym połączyć lub nikt nie akceptuje zapytań\n";
        exit(10);
    }
    client.socket.waitForReadyRead(15000);

    /*if(client.queue == 0)
        exit(0);*/
    //sprawdzenie parametrów wywołania
    QStringList args = QCoreApplication::arguments();

    if(args.size() > 1)
    {
        if(args.at(1) == "list")
        {
            if(client.send(QStringList() << "GET_AVAILABLE_RESULTS" << "*") == NO_SERVERS)
            {
                std::cout << "nie można się z niczym połączyć lub nikt nie akceptuje zapytań\n";
                exit(10);
            }
        }
        else if(args.at(1) == "get")
        {
            if(client.send(QStringList() << "GET_RESULT" << args.at(2)) == NO_SERVERS)
            {
                std::cout << "nie można się z niczym połączyć lub nikt nie akceptuje zapytań\n";
                exit(10);
            }
        }
        else if(args.at(1) == "find")
        {
            //<nazwa badania>,<kraj>,<płeć>,<rasa>,<minimalny wiek>,<maksymalny wiek>
            if(client.send(QStringList() << "GET_AVAILABLE_RESULTS" << args.at(2) << args.at(3) << \
                           args.at(4) << args.at(5) << args.at(6) << args.at(7)) == NO_SERVERS)
            {
                std::cout << "nie można się z niczym połączyć lub nikt nie akceptuje zapytań\n";
                exit(10);
            }
        }
        else if(args.at(1) == "stats")
        {
            //<nazwa badania>,<data od>,<data do>,<kraj>,<płeć>,<rasa>,<minimalny wiek>,<maksymalny wiek>,
            //{<pole grupowania 1>,<pole grupowania 2>,<...>}
            QStringList a = args.at(10).split("");
            a.pop_back();
            a.pop_front();
            QString grupowanie = "{"+a.join(",")+"}";
            std::cout << grupowanie.toStdString() << std::endl;
            if(client.send(QStringList() << "GET_AVAILABLE_RESULTS" << args.at(2) << args.at(3) << \
                           args.at(4) << args.at(5) << args.at(6) << args.at(7) << args.at(8) << args.at(9)\
                           << grupowanie) == NO_SERVERS)
            {
                std::cout << "nie można się z niczym połączyć lub nikt nie akceptuje zapytań\n";
                exit(10);
            }
        }
    }
    client.socket.waitForReadyRead(15000);
    exit(0);

    return a.exec();
}
