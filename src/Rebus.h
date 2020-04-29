#ifndef REBUS_H
#define REBUS_H

#include <QObject>

#include <QLocalServer>

extern "C" {
    struct httproto_protocol;
}
class QLocalSocket;

namespace rebus {

class Rebus : public QObject
{
    Q_OBJECT
public:
    explicit Rebus(QObject *parent = nullptr);
    ~Rebus();

    QLocalServer* server();

    void route(struct httproto_protocol *protocol, QLocalSocket *conn);

    // Routes handlers
    void ping_handler(struct httproto_protocol *request, QLocalSocket *conn);
    void hosts_handler(struct httproto_protocol *request, QLocalSocket *conn);
    void kill_handler(struct httproto_protocol *request, QLocalSocket *conn);

    void error_404(struct httproto_protocol *request, QLocalSocket *conn);

private:
    QLocalServer m_server;

signals:

public slots:
    void onConnection();

};

} // namespace rebus

#endif // REBUS_H
