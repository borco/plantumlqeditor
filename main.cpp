#include <QtSingleApplication>
#include <QFileInfo>
#include <QDebug>
#include <QTranslator>
#include "mainwindow.h"

namespace {
const char* APPLICATION_NAME = "PlantUML Editor";
const char* ORGANIZATION_NAME = APPLICATION_NAME;

const char* OPTION_HELP_SHORT = "-h";
const char* OPTION_HELP_LONG = "--help";
const char* OPTION_SINGLE_INSTANCE_SHORT = "-s";
const char* OPTION_SINGLE_INSTANCE_LONG = "--single-instance";

void displayHelp(const char* app_name) {
    qDebug() << qPrintable(QString("Usage: %1 [options] [FILE]").arg(QFileInfo(app_name).fileName()));
    qDebug() << qPrintable(QString("\n"
                                   "%1  %2    Display this help\n"
                                   "%3  %4    Single-instance mode\n"
                                   "\n"
                                   "If FILE is provided, load it on start. If running in single\n"
                                   "instance mode, the instance is signaled to load FILE.\n")
                           .arg(OPTION_HELP_SHORT)
                           .arg(OPTION_HELP_LONG)
                           .arg(OPTION_SINGLE_INSTANCE_SHORT)
                           .arg(OPTION_SINGLE_INSTANCE_LONG)
                           )
                ;
}

bool singleInstanceModeRequested(QStringList& options) {
    bool ret = options.contains("-s") || options.contains("--single-instance");
    options.removeAll(OPTION_SINGLE_INSTANCE_LONG);
    options.removeAll(OPTION_SINGLE_INSTANCE_SHORT);
    return ret;
}
}

int main(int argc, char *argv[])
{
    QCoreApplication::setOrganizationName(ORGANIZATION_NAME);
    QCoreApplication::setApplicationName(APPLICATION_NAME);

    QtSingleApplication a(argc, argv);

    QStringList options;
    for (int i = 1; i < argc; ++i) {
        options << argv[i];
    }

    if (options.contains(OPTION_HELP_SHORT) || options.contains(OPTION_HELP_LONG)) {
        displayHelp(argv[0]);
        return 0;
    }

    bool single_instance_mode = singleInstanceModeRequested(options);
    QString open_document_path;
    if (options.size() > 0) {
        open_document_path = options.last();
    }

    if (a.isRunning() && single_instance_mode) {
        a.sendMessage(open_document_path);
        qDebug() << qPrintable(QObject::tr("%1 already running. Requested instance to open: %2. Now exitting...")
                               .arg(APPLICATION_NAME)
                               .arg(open_document_path));
        return 0;
    }

    QTranslator appTranslator;

    appTranslator.load("plantumlqeditor_" + QLocale::system().name(), "translations");
    a.installTranslator(&appTranslator);

    MainWindow w;
    if (open_document_path.isEmpty()) {
        w.newDocument();
    } else {
        w.openDocument(open_document_path);
    }
    w.show();

    return a.exec();
}
