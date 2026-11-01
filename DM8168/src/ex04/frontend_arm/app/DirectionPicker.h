#ifndef EX04_DIRECTION_PICKER_H
#define EX04_DIRECTION_PICKER_H
#include <QWidget>
#include "RunRequest.h"
class QComboBox;
class DirectionPicker : public QWidget
{
    Q_OBJECT
public:
    explicit DirectionPicker(const QString &name, const SoundDirection &initial, QWidget *parent = 0);
    SoundDirection direction() const;
    bool isValid() const;
signals:
    void changed();
private slots:
    void updateAzimuths();
private:
    QComboBox *elevation_;
    QComboBox *azimuth_;
};
#endif
