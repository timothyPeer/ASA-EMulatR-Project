#pragma once

#include <QObject>

class AlphaCoreAdapter  : public QObject
{
	Q_OBJECT

public:
	AlphaCoreAdapter(QObject *parent);
	~AlphaCoreAdapter();
	void connectSignals();
	void run();
	void start();
	void resume();
	void reset();
	void pause();
	void stop();
};
