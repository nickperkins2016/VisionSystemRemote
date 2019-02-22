#ifndef SERVER_H
#define SERVER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QWebSocketServer>
#include <QWebSocket>
#include <QList>
#include <QImage>
#include <QMap>

#include "SerialPortList.h"


class Server : public QObject
{
    Q_OBJECT
public:
    explicit Server(SerialPortList* list, QObject *parent = 0);

public slots:
    void onNewFrame(QImage frame);

signals:

private slots:
    void start();
    void onNewImageConnection();
    void onNewMessageConnection();
    void addNameToMap(QString name);
    void onNewMessage(QString portName, QString message);
    void onNewName();
    void onMessageReceived(QString message);
    void onNewCommand(QString portName, QString type, QString message);
    void onImageConnectionEnded();
    void onMessageConnectionEnded();

private:
    QString jsonify(QMap<QString, SerialPort*>);
    QString jsonify(QString);
    QString jsonify(QString type, QString message);

    QTcpServer mImageServer;
    QList<QTcpSocket*> mImageClients;

    QWebSocketServer mMessageServer;
    QMap<QString, QList<QWebSocket*>> mMessageClients;

    SerialPortList* mSerialPortList;

};

#endif // SERVER_H
