#include "Rebus.h"

#include <QLocalSocket>

#include <stdio.h>
#include <stdlib.h>

#include <httproto/httproto.h>

namespace rebus {

Rebus::Rebus(QObject *parent) : QObject(parent)
{
    QString socketPath = QString(getenv("XDG_RUNTIME_DIR")) + "/rebus.sock";
    if (!this->m_server.listen(socketPath)) {
        fprintf(stderr, "[Warning] Cannot listen to the socket.\n");
        return;
    }

    QObject::connect(&this->m_server, &QLocalServer::newConnection,
                     this, &Rebus::onConnection);
}

Rebus::~Rebus()
{
    this->m_server.close();

    QString socketPath = QString(getenv("XDG_RUNTIME_DIR")) + "/rebus.sock";
    if (remove(socketPath.toUtf8()) != 0) {
        fprintf(stderr, "[Warning] Cannot delete socket file.\n");
    } else {
        fprintf(stdout, "[Info] Socket file successfully deleted.\n");
    }
}

QLocalServer* Rebus::Rebus::server()
{
    return &(this->m_server);
}

void Rebus::onConnection()
{
    QLocalSocket *conn = this->server()->nextPendingConnection();
    if (conn->waitForReadyRead(3000) == false) {
        fprintf(stderr, "[Warning] Waiting timeout.");
    }
    QByteArray received = conn->readAll();
    httproto_protocol *request = httproto_protocol_create(HTTPROTO_REQUEST);
    httproto_protocol_parse(request, received, received.length());

    this->route(request, conn);
}

void Rebus::route(struct httproto_protocol *request, QLocalSocket *conn)
{
    if (httproto_protocol_get_path(request) == QString("/")) {
        this->ping_handler(request, conn);
    } else {
        this->error_404(request, conn);
    }

    conn->flush();
    conn->close();
}

//====================
// Router handlers
//====================
void Rebus::ping_handler(struct httproto_protocol *request, QLocalSocket *conn)
{
    switch (request->method) {
    case HTTPROTO_GET:
        conn->write("HTTP/1.1 200 OK\r\n"
                    "Content-Type: application/json\r\n"
                    "Content-Length: 6\r\n"
                    "\r\n"
                    "\"pong\"");
        break;
    default:
        break;
    }

}

void Rebus::error_404(struct httproto_protocol *request, QLocalSocket *conn)
{
    QString detail("{ \"detail\": \"No route to " + QString(httproto_protocol_get_path(request)) + "\"");
    QString length = QString::number(detail.length());
    conn->write("HTTP/1.1 404 Not Found\r\n"
                "Content-Type: application/json\r\n"
                "Content-Lenght: " + length.toLocal8Bit() + "\r\n"
                "\r\n"
                + detail.toLocal8Bit());
}

} // namespace rebus
