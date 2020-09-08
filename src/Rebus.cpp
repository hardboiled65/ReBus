#include "Rebus.h"

#include <QLocalSocket>
#include <QSocketNotifier>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUuid>

#include <stdio.h>
#include <stdlib.h>

#include <sys/socket.h>
#include <signal.h>
#include <unistd.h>

#include <httproto/httproto.h>

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
    fprintf(stdout, "Host: %s, %s %s\n",
        host,
        httproto_request_method_to_string(request->method),
        httproto_protocol_get_uri(request)
    );
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
    QString path = httproto_protocol_get_path(request);
    if (httproto_protocol_get_path(request) == QString("/")) {
        this->ping_handler(request, conn);
    } else if (httproto_protocol_get_path(request) == QString("/ping")) {
        this->ping_handler(request, conn);
    } else if (httproto_protocol_get_path(request) == QString("/version")) {
        this->version_handler(request, conn);
    } else if (path.startsWith("/hosts")) {
        this->hosts_handler(request, conn);
    } else if (httproto_protocol_get_path(request) == QString("/kill")) {
        this->kill_handler(request, conn);
    } else {
        this->error_404(request, conn);
    }
}

void Rebus::route_proxy(httproto_protocol *request, QLocalSocket *conn)
{
    QString host_name = httproto_protocol_get_header(request, "Host");
    // Host not exists.
    if (!this->hostExists(host_name)) {
        QByteArray detail_json("{"
                               "  \"detail\": \"Host '" + host_name.toUtf8() + "' not exists.\""
                               "}");
        this->response(conn, detail_json, 404);
    }
    // Get host socket path.
    QString socket_path = QString(getenv("XDG_RUNTIME_DIR")) + "/rebus/"
        + this->getHostUuid(host_name);

    QLocalSocket host_socket;
    host_socket.connectToServer(socket_path);
    // Wait for timeout.
    if (!host_socket.waitForConnected(1000)) {
        fprintf(stderr, "CONNECTION TIMEOUT!\n");
        qDebug() << host_socket.error();
    }
        host_socket.waitForBytesWritten(3000);
        // Send request.
        this->request(
            &host_socket,
            QByteArray(request->content, request->content_length),
            httproto_protocol_get_uri(request),
            request->method
        );
        // Wait for timeout.
        host_socket.waitForReadyRead(3000);
        // Get response.
        QByteArray resp = host_socket.readAll();
        fprintf(stderr, "resp = %s\n", resp.constData());
        httproto_protocol *response = httproto_protocol_create(HTTPROTO_RESPONSE);
        httproto_protocol_parse(response, resp, resp.length());
        fprintf(stderr, "parsed status: %d\n", response->status_code);
        // Send response.
        this->response(conn, QByteArray(response->content, request->content_length), response->status_code);
}

//====================
// Router handlers
//====================
void Rebus::ping_handler(httproto_protocol *request, QLocalSocket *conn)
{
    switch (request->method) {
    case HTTPROTO_GET:
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
        this->response(conn, version_str);
        break;
    default:
        this->error_405(request, conn, { "GET", });
        break;
    }
}

void Rebus::hosts_handler(httproto_protocol *request, QLocalSocket *conn)
{
    switch (request->method) {
    case HTTPROTO_GET: {
        QJsonDocument json_doc;
        QJsonArray json_arr;
        for (auto&& host: this->m_hosts) {
            QJsonObject json_obj;
            json_obj["host_name"] = host.name();
            json_obj["uuid"] = host.uuid();
            json_arr.append(json_obj);
        }
        json_doc.setArray(json_arr);
        this->response(conn, json_doc.toJson(), 200);
        break;
    }
    case HTTPROTO_POST: {
        QJsonParseError err;
        QJsonDocument payload = QJsonDocument::fromJson(
            QByteArray(request->content, request->content_length), &err
        );
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
        // Duplicate check.
        QString host_name = payload.object().value("host_name").toString();
        if (this->hostExists(host_name)) {
            this->error_409(request, conn, "Host already exists.");
            break;
        }
        Host new_host(host_name);
        new_host.setUuid(QUuid::createUuid().toString(QUuid::StringFormat::WithoutBraces));
        this->m_hosts.append(new_host);
        QByteArray data("{"
                        "  \"host_name\": \"" + new_host.name().toUtf8() + "\","
                        "  \"uuid\": \"" + new_host.uuid().toUtf8() + "\""
                        "}");
        this->response(conn, data, HTTPROTO_CREATED);
        break;
    }
    case HTTPROTO_DELETE: {
        QString path = httproto_protocol_get_path(request);
        QList<QString> parts = path.split("/");
        fprintf(stdout, "Delete host %s\n", static_cast<const char*>(parts.last().toUtf8()));
        if (this->deleteHost(parts.last())) {
            // TODO: 204 response
            this->response(conn, "", HTTPROTO_NO_CONTENT);
        } else {
            // TODO: 404 response
            this->error_404(request, conn);
        }
        break;
    }
    default:
        this->error_405(request, conn, { "GET", "POST", "DELETE" });
        break;
    }
}

void Rebus::kill_handler(httproto_protocol *request, QLocalSocket *conn)
{
    switch (request->method) {
    case HTTPROTO_POST:
        this->response(conn, "", HTTPROTO_ACCEPTED);
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

    this->response(conn, detail_json, 400);
}

void Rebus::error_404(struct httproto_protocol *request, QLocalSocket *conn)
{
    QByteArray detail("{ \"detail\": \"No route to " + QByteArray(httproto_protocol_get_path(request)) + "\"");

    this->response(conn, detail, 404);
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

void Rebus::error_409(httproto_protocol *request, QLocalSocket *conn, const QString &detail)
{
    (void)request;

    QByteArray detail_json("{"
                           "  \"detail\": \"" + detail.toUtf8() + "\""
                           "}");

    this->response(conn, detail_json, 409);
}

void Rebus::response(QLocalSocket *conn, const QByteArray& data,
        unsigned int status_code, QMap<QString, QString> headers)
{
    QByteArray status_str = QByteArray::number(status_code);
    QByteArray status_desc = httproto_status_code_to_string((enum httproto_status_code)status_code);
    QByteArray server = "ReBus " + QByteArray::number(REBUS_VERSION_MAJOR) + "." + QByteArray::number(REBUS_VERSION_MINOR);
    QByteArray length = QByteArray::number(data.length());

    // Make headers.
    QByteArray headers_str = "";

    for (int i = 0; i < headers.keys().length(); ++i) {
        QString key = headers.keys()[i];
        QString value = headers.value(key);

        headers_str += key + ": " + value + "\r\n";
    }

    conn->write("HTTP/1.1 " + status_str + " " + status_desc + "\r\n"
                "Content-Type: application/json\r\n"
                "Server: " + server + "\r\n"
                + headers_str +
                "Content-Length: " + length + "\r\n"
                "\r\n"
                + data);
}

void Rebus::request(QLocalSocket *conn, const QByteArray &data, const QByteArray& uri,
        enum httproto_request_method method, QMap<QString, QString> headers)
{
    QByteArray length = QByteArray::number(data.length());

    // Make headers.
    QByteArray method_str = httproto_request_method_to_string(method);
    QByteArray headers_str = "";

    for (int i = 0; i < headers.keys().length(); ++i) {
        QString key = headers.keys()[i];
        QString value = headers.value(key);

        headers_str += key + ": " + value + "\r\n";
    }

    conn->write(method_str + " " + uri + " HTTP/1.1\r\n"
                + headers_str +
                "Content-Length: " + length + "\r\n"
                "\r\n"
                + data);
}

bool Rebus::hostExists(const QString& hostName)
{
    for (auto&& host: this->m_hosts) {
        if (host == hostName.toUtf8()) {
            return true;
        }
    }
    return false;
}

QString Rebus::getHostUuid(const QString& hostName)
{
    Host *pHost = nullptr;
    for (auto&& host: this->m_hosts) {
        if (host== hostName.toUtf8()) {
            pHost = &host;
            break;
        }
    }
    if (pHost == nullptr) {
        return "";
    }
    return pHost->uuid();
}

bool Rebus::deleteHost(const QString &hostName)
{
    int idx = 0;
    for (; idx < this->m_hosts.length(); ++idx) {
        if (this->m_hosts[idx] == hostName.toUtf8()) {
            break;
        }
    }
    if (idx != this->m_hosts.length()) {
        this->m_hosts.removeAt(idx);
        return true;
    }
    return false;
}

} // namespace rebus
