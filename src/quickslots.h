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

protected:
	Vector3	mPosition;		// position of quickslot (center of sphere)
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

	float							mControllerRadius = 1.0f;  // default sphere radius for controller overlap
	PapyrusVR::EVRButtonId			mActivateButton = PapyrusVR::k_EButton_SteamVR_Trigger;

	// headset and controller tracking information from last update
	PapyrusVR::TrackedDevicePose*	mHMDPose = nullptr;
	PapyrusVR::TrackedDevicePose*	mLeftControllerPose = nullptr;
	PapyrusVR::TrackedDevicePose*	mRightControllerPose = nullptr;


};
