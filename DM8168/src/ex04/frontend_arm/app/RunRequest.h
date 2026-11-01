#ifndef EX04_ARM_RUN_REQUEST_H
#define EX04_ARM_RUN_REQUEST_H

#include <QString>
#include <QStringList>
#include <QList>

struct SoundDirection
{
    SoundDirection(int e = 0, int a = 0) : elevation(e), azimuth(a) {}
    int elevation;
    int azimuth;
    bool isValid() const;
    static QList<int> elevations();
    static QList<int> azimuths(int elevation);
};

struct RunRequest
{
    RunRequest() : mode("smoke"), algorithm("passthrough"), seconds(5),
                   sourceA(0, 270), sourceB(0, 90) {}
    QString mode;
    QString algorithm;
    QString inputWav;
    QString outputWav;
    int seconds;
    SoundDirection direction;
    SoundDirection sourceA;
    SoundDirection sourceB;

    QString validate(bool requirePaths = true) const;
    QStringList arguments() const;
    static QStringList algorithms(const QString &mode);
};

#endif
