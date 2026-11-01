#include "RunRequest.h"
#include "../../shared/hrir_catalog.h"

bool SoundDirection::isValid() const
{
    if (elevation < -90 || elevation > 90 || azimuth < 0 || azimuth > 359)
        return false;
    return hrir_direction_index(HRIR_DIRECTION_CODE(elevation, azimuth)) >= 0;
}

QList<int> SoundDirection::elevations()
{
    QList<int> values;
    for (int i = 0; i < HRIR_DIRECTION_COUNT; ++i)
        if (!values.contains(kHrirDirections[i].elevation))
            values.append(kHrirDirections[i].elevation);
    return values;
}

QList<int> SoundDirection::azimuths(int elevation)
{
    QList<int> values;
    for (int i = 0; i < HRIR_DIRECTION_COUNT; ++i)
        if (kHrirDirections[i].elevation == elevation)
            values.append(kHrirDirections[i].azimuth);
    return values;
}

QStringList RunRequest::algorithms(const QString &mode)
{
    QStringList result;
    if (mode == "record")
        return result;
    result << "passthrough" << "swap" << "gain2" << "gain3" << "gain5";
    if (mode == "smoke" || mode == "file")
        result << "speed2";
    if (mode == "file" || mode == "audio")
        result << "spatial" << "dual";
    return result;
}

QString RunRequest::validate(bool requirePaths) const
{
    if (mode != "smoke" && mode != "record" && mode != "file" && mode != "audio")
        return QString::fromUtf8("未知运行模式。");
    // Existing capture computes durationMs * 1000 using signed int.
    if (seconds < 0 || seconds > 2147)
        return QString::fromUtf8("时长必须在 0 到 2147 秒之间，0 表示持续运行。");
    if (mode != "record" && !algorithms(mode).contains(algorithm))
        return QString::fromUtf8("当前模式不支持所选算法。");
    if (requirePaths && mode == "file" && inputWav.trimmed().isEmpty())
        return QString::fromUtf8("请选择板端输入 WAV。");
    if (requirePaths && mode == "record" && outputWav.trimmed().isEmpty())
        return QString::fromUtf8("录音需要输出 WAV 路径。");
    if (mode != "record" && algorithm == "spatial" && !direction.isValid())
        return QString::fromUtf8("请选择有效的声源方向");
    if (mode != "record" && algorithm == "dual" &&
            (!sourceA.isValid() || !sourceB.isValid()))
        return QString::fromUtf8("请选择有效的 A / B 声源方向");
    return QString();
}

QStringList RunRequest::arguments() const
{
    QStringList args;
    if (mode != "record")
        args << "DSP";
    args << mode;
    if (mode == "file") args << "--input" << inputWav;
    if (mode != "smoke" && !outputWav.isEmpty()) args << "--output" << outputWav;
    if (mode == "audio" || mode == "record") args << "--seconds" << QString::number(seconds);
    if (mode != "record") {
        args << "--algo" << algorithm;
        if (algorithm == "spatial")
            args << "--elevation" << QString::number(direction.elevation)
                 << "--azimuth" << QString::number(direction.azimuth);
        else if (algorithm == "dual")
            args << "--a-elevation" << QString::number(sourceA.elevation)
                 << "--a-azimuth" << QString::number(sourceA.azimuth)
                 << "--b-elevation" << QString::number(sourceB.elevation)
                 << "--b-azimuth" << QString::number(sourceB.azimuth);
    }
    return args;
}
