#pragma once
#include "api/PapyrusVRTypes.h"
#include "api/OpenVRTypes.h"
#include "common/ITypes.h"
#include <string>
#include <vector>

class CQuickslot
{
	friend class CQuickslotManager;

public:

	CQuickslot() = default;
	CQuickslot(Vector3 pos, float radius, const char* command, const char* cmdAlt = nullptr, const char* name = nullptr)
	{
		mPosition = pos;
		mOrigin = pos;
		mRadius = radius;
		mCommand = command;

		if (cmdAlt)
		{
			mCommandAlt = cmdAlt;
		}

		if (name)
		{
			mName = name;
		}
	}

	void PrintInfo();  // log information about this quickslot (debugging)

protected:
	Vector3	mPosition;		// current position of quickslot (center of sphere)
	Vector3 mOrigin;		// original position (before transforming to be relative to HMD)
	float mRadius = 0.0f;	// radius of sphere
	std::string mCommand;   // one command to equip each hand
	std::string mCommandAlt;
	std::string mName;			// name of quickslot for debugging
};



class CQuickslotManager
{
public:

	bool			ReadConfig(const char* filename);
	CQuickslot*		FindQuickslot(const Vector3& pos, float radius);
	void			Update(PapyrusVR::TrackedDevicePose* hmdPose, PapyrusVR::TrackedDevicePose* leftCtrlPose, PapyrusVR::TrackedDevicePose* rightCtrlPose);
	void			ButtonPress(PapyrusVR::EVRButtonId buttonId, PapyrusVR::VRDevice deviceId);

private:

	std::vector<CQuickslot>			mQuickslotArray;  // array of all quickslot objects

	float							mControllerRadius = 0.1f;  // default sphere radius for controller overlap
	PapyrusVR::EVRButtonId			mActivateButton = PapyrusVR::k_EButton_SteamVR_Trigger;

	// headset and controller tracking information from last update
	PapyrusVR::TrackedDevicePose*	mHMDPose = nullptr;
	PapyrusVR::TrackedDevicePose*	mLeftControllerPose = nullptr;
	PapyrusVR::TrackedDevicePose*	mRightControllerPose = nullptr;

	int								mDebugLogVerb = 0;  // debug log verbosity - 0 means no logging


};
