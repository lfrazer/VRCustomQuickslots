#pragma once
#include "api/PapyrusVRTypes.h"
#include <string>
#include <vector>

class CQuickslot
{
public:

	CQuickslot() = default;
	CQuickslot(PapyrusVR::Vector3 pos, float radius, const char* command, const char* name = nullptr)
	{
		mPosition = pos;
		mRadius = radius;
		mCommand = command;

		if (name)
		{
			mName = name;
		}
	}

private:
	PapyrusVR::Vector3	mPosition;
	float mRadius = 0.0f;
	std::string mCommand;
	std::string mName;
};



class CQuickslotManager
{
public:

	bool	ReadConfig(const char* filename);

private:

	std::vector<CQuickslot>		mQuickslotArray;


};