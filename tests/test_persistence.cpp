#include <QCoreApplication>
#include <QSettings>
#include <QString>
#include <QtTest>

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QString instanceName = "WebClip-ABC123";
    {
        QSettings s("Burhanverse", "WebClip");
        s.setValue("mdnsInstanceName", instanceName);
    }
    {
        QSettings s("Burhanverse", "WebClip");
        QString loaded = s.value("mdnsInstanceName", "").toString();
        if (loaded != instanceName) {
            qDebug() << "FAIL: expected" << instanceName << "got" << loaded;
            return 1;
        }
        qDebug() << "PASS: mdnsInstanceName persistence verified";
    }
    return 0;
}
