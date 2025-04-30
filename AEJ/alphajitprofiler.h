#pragma once

#include <QObject>

class AlphaJITProfiler : public QObject
{
public:
	void setHotThreshold(int threshold) { hotThreshold = threshold; }
	int getHotThreshold() const { return hotThreshold; }

	AlphaJITProfiler(QObject* parent) : QObject(parent)
	{

	}

	~AlphaJITProfiler();
private:
	int hotThreshold = 100;  // Threshold for considering a block "hot"
};

