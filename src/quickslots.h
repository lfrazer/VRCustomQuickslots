#pragma once
#include "api/PapyrusVRTypes.h"
#include "api/OpenVRTypes.h"
#include <string>
#include <vector>

class CQuickslot
{
	friend class CQuickslotManager;

public:

	enum eSlotType
	{
		SLOT_DEFAULT = 0,
		SLOT_RIGHTHAND = 1,
		SLOT_LEFTHAND = 2,
	};

	enum eCmdActionType
	{
		NO_ACTION = 0,
		EQUIP_ITEM,
		EQUIP_SPELL,
		EQUIP_SHOUT,
		CONSOLE_CMD
	};

	// Quickslot action / cmd structure, what should the quickslot do?
	// Should eventually support equip/use item, equip spell, equip shout or run console command
	struct CQuickslotCmd
	{
		eCmdActionType mAction = NO_ACTION;
		int mSlot = SLOT_DEFAULT;  // slot (which hand to use)
		unsigned int mFormID = 0;  // Form object (item/spell/shout) numerical ID
		std::string mCommand;  // console command
	};

	CQuickslot() = default;
	CQuickslot(PapyrusVR::Vector3 pos, float radius, const CQuickslotCmd& cmd, const CQuickslotCmd& cmdAlt, const char* name = nullptr)
	{
		mPosition = pos;
		mOrigin = pos;
		mRadius = radius;
		mCommand = cmd;
		mCommandAlt = cmdAlt;
		
		if (name)
		{
			mName = name;
		}
	}

	void PrintInfo();  // log information about this quickslot (debugging)
	void DoAction(const CQuickslotCmd& cmd);  // perform set quickslot action (call on button press)

protected:
	PapyrusVR::Vector3	mPosition;		// current position of quickslot (center of sphere)
	PapyrusVR::Vector3 mOrigin;		// original position (before transforming to be relative to HMD)
	float mRadius = 0.0f;	// radius of sphere
	CQuickslotCmd mCommand;   // one command to equip each hand
	CQuickslotCmd mCommandAlt;
	std::string mName;			// name of quickslot for debugging
};



class CQuickslotManager
{
public:

	bool			ReadConfig(const char* filename);
	CQuickslot*		FindQuickslot(const PapyrusVR::Vector3& pos, float radius);
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
