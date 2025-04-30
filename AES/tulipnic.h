#pragma once

#include <QObject>

class TulipNIC  : public QObject
{
	Q_OBJECT

public:
	TulipNIC(QObject *parent);
	~TulipNIC();
};
