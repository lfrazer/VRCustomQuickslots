/*
	VRCustomQuickslots - VR Custom Quickslots SKSE extension for SkyrimVR
	Copyright (C) 2018 L Frazer
	https://github.com/lfrazer

	This file is part of VRCustomQuickslots.

	VRCustomQuickslots is free software: you can redistribute it and/or modify
	it under the terms of the GNU Lesser General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	VRCustomQuickslots is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public License
	along with VRCustomQuickslots.  If not, see <https://www.gnu.org/licenses/>.
*/

#pragma once
#include "api/PapyrusVRTypes.h"
#include "api/OpenVRTypes.h"
#include <string>
#include <vector>

#include "skse64/PapyrusEvents.h"
#include "common/ISingleton.h"

#include "timer.h"

// forward decl
namespace vr
{
	class IVRSystem;
}

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
	void SetAction(PapyrusVR::VRDevice deviceId); // set quickslot action to currently used item or spell
	void UnsetAction();  // unset the action (remove any action from the slot, the user can later equip it with a new action)


protected:
	PapyrusVR::Vector3	mPosition;		// current position of quickslot (center of sphere)
	PapyrusVR::Vector3	mOrigin;		// original position (before transforming to be relative to HMD)
	float				mRadius = 0.0f;	// radius of sphere
	CQuickslotCmd		mCommand;   // one command to equip each hand
	CQuickslotCmd		mCommandAlt;
	std::string			mName;			// name of quickslot for debugging
	double				mLastOverlapTime = 0.0;  // last overlap time
	double				mButtonHoldTime = 0.0; // track time user held button on this quickslot


};


class CQuickslotManager: public ISingleton<CQuickslotManager>
{
	friend class AllMenuEventHandler;

	class AllMenuEventHandler : public BSTEventSink <MenuOpenCloseEvent>
	{
	public:
		virtual EventResult	ReceiveEvent(MenuOpenCloseEvent * evn, EventDispatcher<MenuOpenCloseEvent> * dispatcher) override;
		void MenuOpenEvent(const char* menuName);
		void MenuCloseEvent(const char* menuName);
	};

public:

	CQuickslotManager();
	bool			ReadConfig(const char* filename);
	CQuickslot*		FindQuickslot(const PapyrusVR::Vector3& pos, float radius);
	void			Update(PapyrusVR::TrackedDevicePose* hmdPose, PapyrusVR::TrackedDevicePose* leftCtrlPose, PapyrusVR::TrackedDevicePose* rightCtrlPose);
	void			ButtonPress(PapyrusVR::EVRButtonId buttonId, PapyrusVR::VRDevice deviceId);
	void			ButtonRelease(PapyrusVR::EVRButtonId buttonId, PapyrusVR::VRDevice deviceId);
	// check when menu is open, plus for a short delay after it has been closed
	bool			IsMenuOpen();
	void			SetInGame(bool flag) { mInGame = flag; }



private:

	void	GetVRSystem();

	std::vector<CQuickslot>			mQuickslotArray;  // array of all quickslot objects

	int								mLastVRError = 0;
	vr::IVRSystem*					mVRSystem = nullptr;

	AllMenuEventHandler				mMenuEventHandler;

	float							mControllerRadius = 0.1f;  // default sphere radius for controller overlap
	PapyrusVR::EVRButtonId			mActivateButton = PapyrusVR::k_EButton_SteamVR_Trigger;

	// headset and controller tracking information from last update
	PapyrusVR::TrackedDevicePose*	mHMDPose = nullptr;
	PapyrusVR::TrackedDevicePose*	mLeftControllerPose = nullptr;
	PapyrusVR::TrackedDevicePose*	mRightControllerPose = nullptr;

	int								mDebugLogVerb = 0;  // debug log verbosity - 0 means no logging
	int								mHapticOnOverlap = 1;  // haptic feedback on quickslot overlap
	int								mAllowEditSlots = 1;   // editing quickslots in game allowed?
	bool							mIsMenuOpen = false; // use events to block quickslots when menu is open, set this flag to true when menu is open
	bool							mInGame = false; // do not start processing until in-game (after load game or new game event from SKSE)
	double							mMenuLastCloseTime = -1.0; // track the last time the menu was closed (negative means invalid time / do not track time)

	// constants
	const double					kMenuBlockDelay = 0.25;  // time in seconds to block actions after menu was closed
};
