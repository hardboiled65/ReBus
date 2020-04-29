#include <QCoreApplication>

#include "Rebus.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    rebus::Rebus rebus;

    return app.exec();
}
