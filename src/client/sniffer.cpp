#define TINS_STATIC

#include "sniffer.h"

Sniffer::Sniffer(QObject* parent, const QString& path_, const std::string& filter_):
    QObject(parent),
    path(path_),
    date(QDate::currentDate().toString("dd.MM.yyyy")),
    iface{std::make_unique<Tins::NetworkInterface>(Tins::NetworkInterface::default_interface())},
    config{std::make_unique<Tins::SnifferConfiguration>()},
    totalUpSize{0},
    totalDownSize{0},
    ip{iface->addresses().ip_addr.to_string()},
    timer{std::make_unique<QTimer>()}
{
    loadData();
    timer->setInterval(60000); //1 min
    timer->start();

    connect(timer.get(), &QTimer::timeout, [this](){
        //Save data
        saveData();
        emit newData(QString::number(totalUpSize >> 10) + '|' + QString::number(upConn.size()) + '|'
                    + QString::number(totalDownSize >> 10) + '|' + QString::number(downConn.size()));
        // >> 10 = Kbytes
    });
    config->set_promisc_mode(true);
    config->set_filter(filter_);

    //Now instantiate the sniffer
    sniffer = std::make_unique<Tins::Sniffer>(iface->name(), *config);
}

void Sniffer::start()
{
    sniffer->sniff_loop(bind(
                            &Sniffer::callbackIP,
                            this,
                            std::placeholders::_1
                            )
                        );
}

void Sniffer::printData()
{
    qDebug() << "Up: total=" << (totalUpSize >> 10) << " conn=" << upConn.size() <<"\n";
    qDebug() << "Down: total=" << (totalDownSize >> 10) << " conn=" << downConn.size() <<"\n";
}

bool Sniffer::callbackIP(const Tins::PDU &pdu) {

    //Find the IP layer
    const Tins::IP &ip = pdu.rfind_pdu<Tins::IP>();

    //Upload
    if (ip.src_addr() == this->ip)
    {
        totalUpSize += ip.size();
        //Add new address
        QString dstIp = QString::fromStdString(ip.dst_addr().to_string());
        upConn.insert(dstIp);

    }
    else //Download
    {
        totalDownSize += ip.size();
        //Add new address
        QString srcIp = QString::fromStdString(ip.src_addr().to_string());
        downConn.insert(srcIp);
    }

    return true;
}

void Sniffer::saveData()
{
    QFile file(path + "/sniffer.dat");
    if ( !file.open(QIODevice::WriteOnly) )
        return;

    QString currDate = QDate::currentDate().toString("dd.MM.yyyy");
    if(date != currDate)
    {
        totalUpSize = 0;
        upConn.clear();
        totalDownSize = 0;
        downConn.clear();
        date = currDate;
    }

    QDataStream stream(&file);
    stream << date <<
              totalUpSize <<
              upConn <<
              totalDownSize <<
              downConn;

    file.close();
}

void Sniffer::loadData()
{
    QFile file(path + "/sniffer.dat");
    if ( file.exists() )
    {
        if ( !file.open(QIODevice::ReadOnly) )
            return;

        QDataStream stream(&file);
        QString loadedDate;
        stream >> loadedDate;

        if(loadedDate == date)
            stream >> totalUpSize >> upConn >> totalDownSize >> downConn;

        file.close();
    }
}


