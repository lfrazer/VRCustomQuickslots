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
#include "api/openvr.h"
#include "api/VRHookAPI.h"
#include <string>
#include <vector>

#include "skse64/InternalTasks.h"
#include "skse64/PapyrusEvents.h"
#include "common/ISingleton.h"

#include "timer.h"
#include "quickslotutil.h"

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
		EQUIP_OTHER, //To be used for potions,food,misc items etc.		
		CONSOLE_CMD,
		DROP_OBJECT  //Some mods use this functionality like BasicCampGear
	};

	enum eItemType
	{
		Ingestible = 1,
		Ammunition = 2		
	};

	enum eOrderType
	{
		DEFAULT = 0,
		FIRST = 1,
		RANDOM = 2,
		ALL = 3,
		TOGGLE = 4
	};

	// Quickslot action / cmd structure, what should the quickslot do?
	// Should eventually support equip/use item, equip spell, equip shout or run console command
	struct CQuickslotCmd
	{
		eCmdActionType mAction = NO_ACTION;
		int mSlot = SLOT_DEFAULT;  // slot (which hand to use)
		std::vector<UInt32> mFormIDList;  // Form object (item/spell/shout) numerical ID
		std::string mConsoleCommand;  // console command

		//Extra stuff to be saved 
		std::string mPluginName;
		std::string mFormIdStr;
		int mItemType = 0;
		std::string mKeyword;     //Included keywords
		std::string mKeywordNot;  //Excluded keywords
		int mPotion = 1;
		int mFood = 1;
		int mPoison = 1;
		int mCount = 1;
	};

	CQuickslot() = default;
	CQuickslot(PapyrusVR::Vector3 pos, float radius, std::vector<CQuickslotCmd> cmdList, int order, const char* name = nullptr)
	{
		mPosition = pos;
		mOrigin = pos;
		mRadius = radius;
		mOrder = order;
		if (mOrder == eOrderType::DEFAULT)
		{
			if (!cmdList.empty())
			{
				mCommand = cmdList[0];

				QSLOG_INFO("Quickslot %s created with command: %d", name, mCommand.mAction);
				if (cmdList.size() > 1)
				{
					mCommandAlt = cmdList[1];
					QSLOG_INFO("Quickslot %s created with commandAlt: %d", name, mCommandAlt.mAction);
				}
			}			
		}
		else
		{
			mOtherCommands = cmdList;
			QSLOG_INFO("Quickslot created with multiple commands: %d", mOtherCommands.size());
		}
		if (name)
		{
			mName = name;
		}
	}

	void PrintInfo();  // log information about this quickslot (debugging)
	bool DoAction(const CQuickslotCmd& cmd, UInt32 formId);  // perform set quickslot action (call on button press)
	void SetAction(PapyrusVR::VRDevice deviceId); // set quickslot action to currently used item or spell
	void UnsetAction();  // unset the action (remove any action from the slot, the user can later equip it with a new action)
	bool PlayerHasItem(TESForm * itemForm); //Checks if player has the item

protected:
	PapyrusVR::Vector3	mPosition;		// current position of quickslot (center of sphere)
	PapyrusVR::Vector3	mOrigin;		// original position (before transforming to be relative to HMD)
	float				mRadius = 0.0f;	// radius of sphere
	CQuickslotCmd		mCommand;   // one command to equip each hand
	CQuickslotCmd		mCommandAlt;
	std::vector<CQuickslotCmd> mOtherCommands; //command list to be used with order != 0
	std::string			mName;			// name of quickslot for debugging
	double				mLastOverlapTime = 0.0;  // last overlap time
	double				mButtonHoldTime = 0.0; // track time user held button on this quickslot
	int					mOrder = eOrderType::DEFAULT; //Order to select which commands to execute. 0 means default usage with one command to equip each hand.
										//1 means execute first one that is applicable(item in user's inventory, player knows the spell/shout etc.)
										//2 means execute random one that is applicable(item in user's inventory, player knows the spell/shout etc.)
										//3 means execute all commands that are applicable(item in user's inventory, player knows the spell/shout etc.)
										//4 means execute first one that is not currently equipped(toggles it) and applicable(item in user's inventory, player knows the spell/shout etc.)

};


class CQuickslotManager: public ISingleton<CQuickslotManager>
{

public:

	CQuickslotManager();
	bool			ReadConfig(const char* filename);
	bool			WriteConfig(const char* filename);
	bool			IsTrackingDataValid() const;
	CQuickslot*		FindQuickslot(const PapyrusVR::Vector3& pos, float radius);
	CQuickslot*		FindNearestQuickslot(const PapyrusVR::Vector3 pos); // debug helper func
	// find out if a controller is hovering over a quickslot
	CQuickslot*		FindQuickslotByDeviceId(PapyrusVR::VRDevice deviceId);

	void			Update(PapyrusVR::TrackedDevicePose* hmdPose, PapyrusVR::TrackedDevicePose* leftCtrlPose, PapyrusVR::TrackedDevicePose* rightCtrlPose);
	// button press/release now return true depending if the button press was triggered on a quickslot (this is for new feature: consuming inputs when used on quickslots)
	bool			ButtonPress(PapyrusVR::EVRButtonId buttonId, PapyrusVR::VRDevice deviceId);
	bool			ButtonRelease(PapyrusVR::EVRButtonId buttonId, PapyrusVR::VRDevice deviceId);
	void			Reset(); // Reset quickslot manager data
	
	// start haptic response for <timeLenght>, pass in LeftHand or RightHand controller from enum
	void			StartHaptics(vr::ETrackedControllerRole controller, double timeLength); 
	void			UpdateHaptics(); // called every frame to update haptic response
	int				GetEffectiveSlot(int inSlot); // Get effective slot to equip with, this mainly can change due to left handed mode and Skyrim VR's awkward left handed mode implementation
	void			SetInGame(bool flag) { mInGame = flag; }
	int				AllowEdit() const { return mAllowEditSlots; }
	int				DisableRawAPI() const { return mDisableRawAPI; }
	PapyrusVR::EVRButtonId GetActivateButton() const;

	void			SetHookMgr(OpenVRHookManagerAPI* hookMgr) 
	{ 
		mHookMgrAPI = hookMgr; 
		mVRSystem = hookMgr->GetVRSystem();
	}

private:

	void	GetVRSystem();

	std::vector<CQuickslot>			mQuickslotArray;  // array of all quickslot objects

	int								mLastVRError = 0;
	vr::IVRSystem*					mVRSystem = nullptr;

	// VR hook manager for new RAW api
	OpenVRHookManagerAPI*			mHookMgrAPI = nullptr;

	float							mControllerRadius = 0.1f;  // default sphere radius for controller overlap
	PapyrusVR::EVRButtonId			mActivateButton = PapyrusVR::k_EButton_SteamVR_Trigger;

	// headset and controller tracking information from last update
	PapyrusVR::TrackedDevicePose*	mHMDPose = nullptr;
	PapyrusVR::TrackedDevicePose*	mLeftControllerPose = nullptr;
	PapyrusVR::TrackedDevicePose*	mRightControllerPose = nullptr;

	int								mDebugLogVerb = 0;  // debug log verbosity - 0 means no logging
	int								mHapticOnOverlap = 1;  // haptic feedback on quickslot overlap
	int								mAllowEditSlots = 1;   // editing quickslots in game allowed?
	int								mLeftHandedMode = 0;  // left handed mode? 
	int								mDisableRawAPI = 0; // if true, do not attempt to load new Raw OpenVR API
	float							mDefaultRadius = 0.1f;
	bool							mInGame = false; // do not start processing until in-game (after load game or new game event from SKSE)	
	double							mLongPressTime = 3.0;  // length of time to trigger long press action
	double							mShortPressTime = 0.3; // lenght of time to trigger short press action (basically to check if more than a single click)
	double							mControllerHapticTime[2] = { 0.0 };
	double							mHoverQuickslotHapticTime = 0.05; // length of time to send haptics when hovering over a quickslot (disable if <= 0)

};

typedef bool(*_HasSpell)(VMClassRegistry * registry, UInt64 stackID, Actor *actor, TESForm *akSpell);
extern RelocAddr <_HasSpell> HasSpell;

typedef void(*_ActorEquipItem)(VMClassRegistry * registry, UInt64 stackID, Actor *actor, TESForm *akItem, bool abPreventRemoval, bool abSilent);
extern RelocAddr <_ActorEquipItem> ActorEquipItem;

typedef UInt32(*_GetItemCount)(VMClassRegistry * registry, UInt64 stackID, TESObjectREFR *actorRefr, TESForm *akItem);
extern RelocAddr <_GetItemCount> GetItemCount;

typedef TESObjectREFR*(*_DropObject)(VMClassRegistry * registry, UInt64 stackID, TESObjectREFR *actorRefr, TESForm *akItem, int aiCount);
extern RelocAddr <_DropObject> DropObject;

extern SKSETaskInterface	* g_task;

class taskActorEquipItem : public TaskDelegate
{
public:
	virtual void Run();
	virtual void Dispose();

	taskActorEquipItem(TESForm* akItem);
	TESForm* m_akItem;
};
