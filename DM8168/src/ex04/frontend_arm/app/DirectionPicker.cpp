#include "DirectionPicker.h"
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QVariant>

DirectionPicker::DirectionPicker(const QString &name, const SoundDirection &initial, QWidget *parent)
    : QWidget(parent)
{
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    QLabel *label = new QLabel(name, this);
    label->setFixedWidth(36);
    elevation_ = new QComboBox(this);
    azimuth_ = new QComboBox(this);
    elevation_->setObjectName("elevation");
    azimuth_->setObjectName("azimuth");
    elevation_->setAccessibleName(QString::fromUtf8("仰角"));
    azimuth_->setAccessibleName(QString::fromUtf8("方位角"));
    setFocusProxy(azimuth_);
    foreach (int value, SoundDirection::elevations())
        elevation_->addItem(QString::number(value) + QString::fromUtf8("°"), value);
    elevation_->setCurrentIndex(elevation_->findData(initial.elevation));
    updateAzimuths();
    azimuth_->setCurrentIndex(azimuth_->findData(initial.azimuth));
    layout->addWidget(label);
    layout->addWidget(elevation_, 1);
    layout->addWidget(azimuth_, 1);
    connect(elevation_, SIGNAL(currentIndexChanged(int)), this, SLOT(updateAzimuths()));
    connect(azimuth_, SIGNAL(currentIndexChanged(int)), this, SIGNAL(changed()));
}
SoundDirection DirectionPicker::direction() const
{
    return SoundDirection(elevation_->itemData(elevation_->currentIndex()).toInt(),
        azimuth_->currentIndex() > 0 ? azimuth_->itemData(azimuth_->currentIndex()).toInt() : -1);
}
bool DirectionPicker::isValid() const { return direction().isValid(); }
void DirectionPicker::updateAzimuths()
{
    const int previous = azimuth_->currentIndex() > 0 ? azimuth_->itemData(azimuth_->currentIndex()).toInt() : -1;
    azimuth_->blockSignals(true);
    azimuth_->clear();
    azimuth_->addItem(QString::fromUtf8("选择方位"), -1);
    const int elevation = elevation_->itemData(elevation_->currentIndex()).toInt();
    foreach (int value, SoundDirection::azimuths(elevation))
        azimuth_->addItem(QString::number(value) + QString::fromUtf8("°"), value);
    const int index = azimuth_->findData(previous);
    azimuth_->setCurrentIndex(index >= 0 ? index : 0);
    azimuth_->blockSignals(false);
    emit changed();
}
