#ifndef REBUS_H
#define REBUS_H

#include <QObject>

#include <QLocalServer>

#define REBUS_VERSION_MAJOR 0
#define REBUS_VERSION_MINOR 1
#define REBUS_VERSION_PATCH 0

extern "C" {
    struct httproto_protocol;
}
class QLocalSocket;
class QSocketNotifier;

namespace rebus {

class Rebus : public QObject
{
    Q_OBJECT
public:
    explicit Rebus(QObject *parent = nullptr);
    ~Rebus();

    QLocalServer* server();

    void route(httproto_protocol *request, QLocalSocket *conn);
    void route_rebus(httproto_protocol *request, QLocalSocket *conn);
    void route_proxy(httproto_protocol *request, QLocalSocket *conn);

    // Routes handlers
    void ping_handler(httproto_protocol *request, QLocalSocket *conn);
    void version_handler(httproto_protocol *request, QLocalSocket *conn);
    void hosts_handler(httproto_protocol *request, QLocalSocket *conn);
    void kill_handler(httproto_protocol *request, QLocalSocket *conn);

    void error_400(httproto_protocol *request, QLocalSocket *conn, const QString& detail);
    void error_404(httproto_protocol *request, QLocalSocket *conn);
    void error_405(httproto_protocol *request, QLocalSocket *conn, QList<QString> allow);

    // Unix signal handlers
    static void sigHupHandler(int);
    static void sigIntHandler(int);

private:
    QLocalServer m_server;

signals:

public slots:
    void onConnection();

    // Qt signal handlers
    void handleSigHup();
    void handleSigInt();

private:
    // Unix signals
    static int sigHupFd[2];
    static int sigIntFd[2];
    QSocketNotifier *snHup;
    QSocketNotifier *snInt;
};

} // namespace rebus

#endif // REBUS_H
