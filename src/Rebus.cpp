#include "Rebus.h"

#include <QLocalSocket>
#include <QSocketNotifier>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

#include <stdio.h>
#include <stdlib.h>

#include <sys/socket.h>
#include <signal.h>
#include <unistd.h>

#include <httproto/httproto.h>

#include "Host.h"

namespace rebus {

int Rebus::sigHupFd[2];
int Rebus::sigIntFd[2];

Rebus::Rebus(QObject *parent) : QObject(parent)
{
    fprintf(stdout, "Starting ReBus server %d.%d.%d ...\n",
        REBUS_VERSION_MAJOR, REBUS_VERSION_MINOR, REBUS_VERSION_PATCH);

    QString socketPath = QString(getenv("XDG_RUNTIME_DIR")) + "/rebus.sock";
    if (!this->m_server.listen(socketPath)) {
        fprintf(stderr, "[Warning] Cannot listen to the socket.\n");
        return;
    }

    QObject::connect(&this->m_server, &QLocalServer::newConnection,
                     this, &Rebus::onConnection);

    // Unix signal handlers.
    ::socketpair(AF_UNIX, SOCK_STREAM, 0, this->sigHupFd);
    this->snHup = new QSocketNotifier(this->sigHupFd[1], QSocketNotifier::Read, this);
    QObject::connect(this->snHup, &QSocketNotifier::activated,
                     this, &Rebus::handleSigHup);

    ::socketpair(AF_UNIX, SOCK_STREAM, 0, this->sigIntFd);
    this->snInt = new QSocketNotifier(this->sigIntFd[1], QSocketNotifier::Read, this);
    QObject::connect(this->snInt, &QSocketNotifier::activated,
                     this, &Rebus::handleSigInt);

    struct sigaction sa_hup, sa_int;

    sa_hup.sa_handler = Rebus::sigHupHandler;
    sigemptyset(&sa_hup.sa_mask);
    sa_hup.sa_flags = 0;
    sa_hup.sa_flags |= SA_RESTART;

    if (sigaction(SIGHUP, &sa_hup, 0))
       fprintf(stderr, "[Warning] sigaction(SIGHUP) failed.\n");

    sa_int.sa_handler = Rebus::sigIntHandler;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = 0;
    sa_int.sa_flags |= SA_RESTART;

    if (sigaction(SIGINT, &sa_int, 0))
        fprintf(stderr, "[Warning] sigaction(SIGINT) failed.\n");
}

Rebus::~Rebus()
{
    this->m_server.close();

    /* Qt will remove socket file.
    QString socketPath = QString(getenv("XDG_RUNTIME_DIR")) + "/rebus.sock";
    if (unlink(socketPath.toUtf8()) != 0) {
        fprintf(stderr, "[Warning] Cannot delete socket file.\n");
        fprintf(stderr, "%d\n", errno);
    } else {
        fprintf(stdout, "[Info] Socket file successfully deleted.\n");
    }
    */
}

void Rebus::sigHupHandler(int)
{
    char a = 1;
    ::write(sigHupFd[0], &a, 1);
}

void Rebus::sigIntHandler(int)
{
    char a = 1;
    ::write(sigIntFd[0], &a, 1);
}

void Rebus::handleSigHup()
{
    snHup->setEnabled(false);
    char tmp;
    ::read(sigHupFd[1], &tmp, 1);

    // Quit normally.
    fprintf(stderr, "SIGHUP received. trying to quit normally.\n");
    qApp->quit();

    snHup->setEnabled(true);
}

void Rebus::handleSigInt()
{
    snInt->setEnabled(false);
    char tmp;
    ::read(sigIntFd[1], &tmp, 1);

    // Quit normally.
    fprintf(stderr, "SIGINT(Ctrl+C) received. trying to quit normally.\n");
    qApp->quit();

    snInt->setEnabled(true);
}

QLocalServer* Rebus::server()
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
    const char *host = httproto_protocol_get_header(request, "Host");
    fprintf(stdout, "Host: %s\n", host);
    if (host == QString("rebus")) {
        route_rebus(request, conn);
    } else {
        route_proxy(request, conn);
    }

    conn->flush();
    conn->close();
}

void Rebus::route_rebus(httproto_protocol *request, QLocalSocket *conn)
{
    if (httproto_protocol_get_path(request) == QString("/")) {
        this->ping_handler(request, conn);
    } else if (httproto_protocol_get_path(request) == QString("/ping")) {
        this->ping_handler(request, conn);
    } else if (httproto_protocol_get_path(request) == QString("/version")) {
        this->version_handler(request, conn);
    } else if (httproto_protocol_get_path(request) == QString("/hosts")) {
        this->hosts_handler(request, conn);
    } else if (httproto_protocol_get_path(request) == QString("/kill")) {
        this->kill_handler(request, conn);
    } else {
        this->error_404(request, conn);
    }
}

void Rebus::route_proxy(httproto_protocol *request, QLocalSocket *conn)
{
    (void)request;
    (void)conn;
}

//====================
// Router handlers
//====================
void Rebus::ping_handler(httproto_protocol *request, QLocalSocket *conn)
{
    switch (request->method) {
    case HTTPROTO_GET:
//        conn->write("HTTP/1.1 200 OK\r\n"
//                    "Content-Type: application/json\r\n"
//                    "Content-Length: 6\r\n"
//                    "\r\n"
//                    "\"pong\"");
        this->response(conn, "\"pong\"");
        break;
    default:
        QList<QString> allow;
        allow.append("GET");
        this->error_405(request, conn, allow);
        break;
    }
}

void Rebus::version_handler(httproto_protocol *request, QLocalSocket *conn)
{
    QByteArray version_str = "\"" + QByteArray::number(REBUS_VERSION_MAJOR)
        + "." + QByteArray::number(REBUS_VERSION_MINOR)
        + "." + QByteArray::number(REBUS_VERSION_PATCH) + "\"";
    switch (request->method) {
    case HTTPROTO_GET:
        conn->write("HTTP/1.1 200 OK\r\n"
                    "Content-Type: application/json\r\n"
                    "Content-Length: " + QByteArray::number(version_str.length()) + "\r\n"
                    "\r\n"
                    + version_str);
        break;
    default:
        this->error_405(request, conn, { "GET", });
        break;
    }
}

void Rebus::hosts_handler(httproto_protocol *request, QLocalSocket *conn)
{
    switch (request->method) {
    case HTTPROTO_GET:
        break;
    case HTTPROTO_POST: {
        QJsonParseError err;
        QJsonDocument payload = QJsonDocument::fromJson(request->content, &err);
        // JSON parse error.
        if (err.error != QJsonParseError::NoError) {
            fprintf(stderr, "[Warning] JSON parse error. %s\n", err.errorString().toUtf8().constData());
            this->error_400(request, conn, "Invalid JSON format.");
            break;
        }
        // Is not object.
        if (!payload.isObject()) {
            this->error_400(request, conn, "Data must be an object.");
            break;
        }
        // Field omitted.
        if (!payload.object().keys().contains("host_name")) {
            this->error_400(request, conn, "Field 'host_name' omitted.");
            break;
        }
        Host new_host(payload.object().value("host_name").toString());
        new_host.setUuid(QUuid::createUuid().toString(QUuid::StringFormat::WithoutBraces));
        break;
    }
    case HTTPROTO_DELETE:
        break;
    default:
        this->error_405(request, conn, { "GET", "POST", "DELETE" });
        break;
    }
}

void Rebus::kill_handler(httproto_protocol *request, QLocalSocket *conn)
{
    switch (request->method) {
    case HTTPROTO_POST:
        conn->write("HTTP/1.1 202 Accepted\r\n"
                    "Content-Type: application/json\r\n"
                    "Content-Length: 0\r\n"
                    "\r\n");
        qApp->quit();
        break;
    default:
        this->error_405(request, conn, { "POST", });
        break;
    }
}

void Rebus::error_400(httproto_protocol *request, QLocalSocket *conn, const QString& detail)
{
    (void)request;

    QByteArray detail_json("{"
                           "  \"detail\": \"");
    detail_json += detail.toUtf8() + "\"";
    detail_json += "}";
    QByteArray length = QByteArray::number(detail_json.length());

    conn->write("HTTP/1.1 400 Bad Request\r\n"
                "Content-Type: application/json\r\n"
                "Content-Length: " + length + "\r\n"
                "\r\n"
                + detail_json);
}

void Rebus::error_404(struct httproto_protocol *request, QLocalSocket *conn)
{
    QString detail("{ \"detail\": \"No route to " + QString(httproto_protocol_get_path(request)) + "\"");
    QString length = QString::number(detail.length());
    conn->write("HTTP/1.1 404 Not Found\r\n"
                "Content-Type: application/json\r\n"
                "Content-Length: " + length.toLocal8Bit() + "\r\n"
                "\r\n"
                + detail.toLocal8Bit());
}

void Rebus::error_405(httproto_protocol *request, QLocalSocket *conn, QList<QString> allow)
{
    (void)request;

    QByteArray allow_list = allow.join(", ").toUtf8();
    conn->write("HTTP/1.1 405 Method Not Allowed\r\n"
                "Content-Type: application/json\r\n"
                "Content-Length: 0\r\n"
                "Allow: " + allow_list +
                "\r\n");
}

void Rebus::response(QLocalSocket *conn, const QByteArray& data,
        unsigned int status_code, QMap<QString, QString> headers)
{
    QByteArray status_str = QByteArray::number(status_code);
    QByteArray status_desc = httproto_status_code_to_string((enum httproto_status_code)status_code);
    QByteArray server = "ReBus " + QByteArray::number(REBUS_VERSION_MAJOR) + "." + QByteArray::number(REBUS_VERSION_MINOR);
    QByteArray length = QByteArray::number(data.length());

    conn->write("HTTP/1.1 " + status_str + " " + status_desc + "\r\n"
                "Content-Type: application/json\r\n"
                "Server: " + server + "\r\n"
                "Content-Length: " + length + "\r\n"
                "\r\n"
                + data);
}

} // namespace rebus
