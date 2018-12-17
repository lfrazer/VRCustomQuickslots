/*
	VRCustomQuickslots - VR Custom Quickslots SKSE extension for SkyrimVR
	Copyright (C) 2018 L Frazer
	https://github.com/lfrazer

	This file is part of VRCustomQuickslots.

	VRCustomQuickslots is free software: you can redistribute it and/or modify
	it under the terms of the GNU Lesser General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SKSE-VRInputPlugin is distributed in the hope that it will be useful,
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

#include "skse64\PapyrusEvents.h"

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
	class AllMenuEventHandler : public BSTEventSink <MenuOpenCloseEvent>
	{
	public:
		virtual EventResult	ReceiveEvent(MenuOpenCloseEvent * evn, EventDispatcher<MenuOpenCloseEvent> * dispatcher);
		void MenuOpenEvent(const char* menuName);
		void MenuCloseEvent(const char* menuName);

		bool							mIsMenuOpen = false; // use events to block quickslots when menu is open, set this flag to true when menu is open
	};

public:

	bool			ReadConfig(const char* filename);
	CQuickslot*		FindQuickslot(const PapyrusVR::Vector3& pos, float radius);
	void			Update(PapyrusVR::TrackedDevicePose* hmdPose, PapyrusVR::TrackedDevicePose* leftCtrlPose, PapyrusVR::TrackedDevicePose* rightCtrlPose);
	void			ButtonPress(PapyrusVR::EVRButtonId buttonId, PapyrusVR::VRDevice deviceId);
	bool			IsMenuOpen() const { return mMenuEventHandler.mIsMenuOpen; }



private:

	std::vector<CQuickslot>			mQuickslotArray;  // array of all quickslot objects

	AllMenuEventHandler				mMenuEventHandler;

	float							mControllerRadius = 0.1f;  // default sphere radius for controller overlap
	PapyrusVR::EVRButtonId			mActivateButton = PapyrusVR::k_EButton_SteamVR_Trigger;

	// headset and controller tracking information from last update
	PapyrusVR::TrackedDevicePose*	mHMDPose = nullptr;
	PapyrusVR::TrackedDevicePose*	mLeftControllerPose = nullptr;
	PapyrusVR::TrackedDevicePose*	mRightControllerPose = nullptr;

	int								mDebugLogVerb = 0;  // debug log verbosity - 0 means no logging
};
