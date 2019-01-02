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

#include "quickslots.h"
#include "quickslotutil.h"
#include "console.h"
#include "api/openvr.h"

// SKSE includes
#include "skse64/PapyrusActor.h"
#include "skse64/GameAPI.h"
#include "skse64/GameForms.h"
#include "skse64/GameRTTI.h"
#include "skse64/GameTypes.h"


CQuickslotManager::CQuickslotManager()
{
	MenuManager * mm = MenuManager::GetSingleton();
	if (mm) {
		mm->MenuOpenCloseEventDispatcher()->AddEventSink(&this->mMenuEventHandler);
	}
	else {
		QSLOG_ERR("Failed to register SKSE AllMenuEventHandler!");
	}

	GetVRSystem();

}

bool	CQuickslotManager::IsMenuOpen()
{ 
	return mIsMenuOpen || mMenuLastCloseTime + kMenuBlockDelay > CUtil::GetSingleton().GetLastTime(); 
}

void  CQuickslotManager::GetVRSystem()
{
	vr::EVRInitError eError = vr::VRInitError_None;
	//mVRSystem = (vr::IVRSystem *)vr::VR_GetGenericInterface(vr::IVRSystem_Version, &eError);


	mVRSystem = vr::VRSystem();

	if (mVRSystem)
	{
		QSLOG("Found VR System ptr");
	}
	else if(eError != mLastVRError)
	{
		QSLOG_ERR("VR System ptr not found, last unique error: %d", eError);
		mLastVRError = eError;
	}
}


void	CQuickslotManager::Update(PapyrusVR::TrackedDevicePose* hmdPose, PapyrusVR::TrackedDevicePose* leftCtrlPose, PapyrusVR::TrackedDevicePose* rightCtrlPose)
{
	mHMDPose = hmdPose;
	mLeftControllerPose = leftCtrlPose;
	mRightControllerPose = rightCtrlPose;

	CUtil::GetSingleton().Update();

	if (mInGame && !IsMenuOpen() && hmdPose->bPoseIsValid && leftCtrlPose->bPoseIsValid && rightCtrlPose->bPoseIsValid)
	{
		PapyrusVR::Vector3 hmdPos = GetPositionFromVRPose(mHMDPose);
		PapyrusVR::Matrix33 rotMatrix = CreateRotMatrixAroundY(mHMDPose->mDeviceToAbsoluteTracking);

		for (auto it = mQuickslotArray.begin(); it != mQuickslotArray.end(); ++it)
		{
			// update quickslots based on HMD rotation
			it->mPosition = MultMatrix33(rotMatrix, it->mOrigin);

			// update translation for each quickslot 
			it->mPosition = it->mPosition + hmdPos;
		}

		// Check for overlaps for haptic feedback
		if (mHapticOnOverlap && mVRSystem)
		{
			// setup array of controllers and loop through it
			const double kHapticTimeout = 1.0;
			const int numControllers = 2;
			PapyrusVR::TrackedDevicePose* controllers[numControllers] = {mLeftControllerPose, mRightControllerPose};
			vr::ETrackedControllerRole controllerRoles[numControllers] = { vr::ETrackedControllerRole::TrackedControllerRole_LeftHand, vr::ETrackedControllerRole::TrackedControllerRole_RightHand };

			for (int i = 0; i < numControllers; ++i)
			{
				PapyrusVR::Vector3 controllerPos = GetPositionFromVRPose(controllers[i]);
				CQuickslot* quickslot = FindQuickslot(controllerPos, mControllerRadius);

				if (quickslot)
				{
					// Do haptic response (but not constantly, check for timeout)
					if (CUtil::GetSingleton().GetLastTime() - quickslot->mLastOverlapTime > kHapticTimeout)
					{
						auto controllerId = mVRSystem->GetTrackedDeviceIndexForControllerRole( controllerRoles[i]);
						mVRSystem->TriggerHapticPulse(controllerId, 0, 3999);

						QSLOG_INFO("Triggered haptic pulse on controller %d", controllerId);
						
					}

					quickslot->mLastOverlapTime = CUtil::GetSingleton().GetLastTime();

				}
			}

		}
		// if VR system is still invalid, keep trying to load it.. Has to be loaded after skyrimVR, and PapyrusVR/SkVRTools does not send us an event for this (TODO)
		else if (mHapticOnOverlap && !mVRSystem)  
		{
			GetVRSystem();
		}
	}
}

void	CQuickslotManager::ButtonPress(PapyrusVR::EVRButtonId buttonId, PapyrusVR::VRDevice deviceId)
{
	const double kHoldActionTime = 2.0;

	// check if relevant button was pressed, or if a menu was open and early exit
	if (buttonId != mActivateButton || IsMenuOpen() || !mInGame)
	{
		return;
	}

	// find quickslot based on current hand 
	PapyrusVR::TrackedDevicePose* currControllerPose = nullptr;

	if (deviceId == PapyrusVR::VRDevice_LeftController && mLeftControllerPose->bPoseIsValid)
	{
		currControllerPose = mLeftControllerPose;
	}
	else if (deviceId == PapyrusVR::VRDevice_RightController && mRightControllerPose->bPoseIsValid)
	{
		currControllerPose = mRightControllerPose;
	}
	else
	{
		return;
	}

	// find the relevant quickslot which is overlapped by the controllers current position
	PapyrusVR::Vector3 controllerPos = GetPositionFromVRPose(currControllerPose);
	CQuickslot* quickslot = FindQuickslot(controllerPos, mControllerRadius);

	if (quickslot)  // if there is one, execute quickslot equip changes
	{
		if (quickslot->mCommand.mAction != CQuickslot::NO_ACTION)
		{
			// one action for each hand (right and left)
			quickslot->DoAction(quickslot->mCommand);
			quickslot->DoAction(quickslot->mCommandAlt);
		}
		else if(mAllowEditSlots)
		{
			// modify the quickslot with current item/spell if none is set
			quickslot->SetAction(deviceId);
		}

		// increase press time on this quickslot
		quickslot->mButtonHoldTime = CUtil::GetSingleton().GetLastTime();
		
		QSLOG_INFO("Found a quickslot at pos (%f,%f,%f) !", controllerPos.x, controllerPos.y, controllerPos.z);
		quickslot->PrintInfo();
		
	}
	else
	{
		QSLOG_INFO("NO quickslot found pos (%f,%f,%f)", controllerPos.x, controllerPos.y, controllerPos.z);
	}
}


void	CQuickslotManager::ButtonRelease(PapyrusVR::EVRButtonId buttonId, PapyrusVR::VRDevice deviceId)
{
	const double kHoldActionTime = 2.0;

	// check if relevant button was pressed, or if a menu was open and early exit
	if (buttonId != mActivateButton || IsMenuOpen() || !mInGame)
	{
		return;
	}

	// find quickslot based on current hand 
	PapyrusVR::TrackedDevicePose* currControllerPose = nullptr;

	if (deviceId == PapyrusVR::VRDevice_LeftController && mLeftControllerPose->bPoseIsValid)
	{
		currControllerPose = mLeftControllerPose;
	}
	else if (deviceId == PapyrusVR::VRDevice_RightController && mRightControllerPose->bPoseIsValid)
	{
		currControllerPose = mRightControllerPose;
	}
	else
	{
		return;
	}

	// find the relevant quickslot which is overlapped by the controllers current position
	PapyrusVR::Vector3 controllerPos = GetPositionFromVRPose(currControllerPose);
	CQuickslot* quickslot = FindQuickslot(controllerPos, mControllerRadius);

	if (quickslot)
	{

		if (quickslot->mButtonHoldTime > 0.0 && CUtil::GetSingleton().GetLastTime() - quickslot->mButtonHoldTime > kHoldActionTime)
		{
			QSLOG_INFO("Hold button action on quickslot %s !", quickslot->mName.c_str());

			// on long press, unset the quickslot action so the user can modify it later
			if (mAllowEditSlots)
			{
				// trigger haptic response
				const vr::ETrackedControllerRole deviceToControllerRoleLookup[3] = { vr::ETrackedControllerRole::TrackedControllerRole_Invalid, vr::ETrackedControllerRole::TrackedControllerRole_RightHand, vr::ETrackedControllerRole::TrackedControllerRole_LeftHand };
				auto controllerId = mVRSystem->GetTrackedDeviceIndexForControllerRole(deviceToControllerRoleLookup[deviceId]);
				mVRSystem->TriggerHapticPulse(controllerId, 0, 3999);

				quickslot->UnsetAction();
			}
		}

	}

	// reset button hold time for all other quickslots
	for (auto it = mQuickslotArray.begin(); it != mQuickslotArray.end(); ++it)
	{
		it->mButtonHoldTime = -1.0;
		
	}

}

CQuickslot*	 CQuickslotManager::FindQuickslot(const PapyrusVR::Vector3& pos, float radius)
{
	const size_t numQuickslots = mQuickslotArray.size();
	
	for (size_t i = 0; i < numQuickslots; ++i)
	{
		// check for sphere overlap with quickslot i
		const float combinedRadius = (radius + mQuickslotArray[i].mRadius);
		const float combinedRadiusSqr = combinedRadius * combinedRadius;

		if (DistBetweenVecSqr(pos, mQuickslotArray[i].mPosition) < combinedRadiusSqr)
		{
			return &mQuickslotArray[i];  // this quickslot is overlapping
		}
	}

	return nullptr;
}



EventResult CQuickslotManager::AllMenuEventHandler::ReceiveEvent(MenuOpenCloseEvent * evn, EventDispatcher<MenuOpenCloseEvent> * dispatcher)
{
	if (evn->opening)
	{
		MenuOpenEvent(evn->menuName.c_str());
	}
	else
	{
		MenuCloseEvent(evn->menuName.c_str());
	}

	return EventResult::kEvent_Continue;
}

void CQuickslotManager::AllMenuEventHandler::MenuOpenEvent(const char* menuName)
{

	std::string name = menuName;

	if (name == "HUD Menu" || name == "WSEnemyMeters" || name == "WSDebugOverlay" || name == "Overlay Interaction Menu" || name == "Overlay Menu" || name == "StatsMenu" || name == "TitleSequence Menu" || name == "Top Menu")
	{
		return;
	}
	else
	{
		CQuickslotManager::GetSingleton().mIsMenuOpen = true;
	}

	//_MESSAGE("MenuOpenEvent: %s", menuName);
}

void CQuickslotManager::AllMenuEventHandler::MenuCloseEvent(const char* menuName)
{
	std::string name = menuName;

	if (name == "HUD Menu" || name == "WSEnemyMeters" || name == "WSDebugOverlay" || name == "Overlay Interaction Menu" || name == "Overlay Menu" || name == "StatsMenu" || name == "TitleSequence Menu" || name == "Top Menu")
	{
		return;
	}
	else
	{
		CQuickslotManager::GetSingleton().mMenuLastCloseTime = CUtil::GetSingleton().GetLastTime();
		CQuickslotManager::GetSingleton().mIsMenuOpen = false;
	}

	//_MESSAGE("MenuCloseEvent: %s", menuName);
}


void CQuickslot::PrintInfo()
{
	QSLOG_INFO("Quickslot (%s) position: (%f,%f,%f) radius: %f", this->mName.c_str(), mPosition.x, mPosition.y, mPosition.z, mRadius);
}

void CQuickslot::DoAction(const CQuickslotCmd& cmd)
{
	if (cmd.mAction == EQUIP_ITEM)
	{
		TESForm* itemFormObj = LookupFormByID(cmd.mFormID);
		papyrusActor::EquipItemEx( (Actor*)(*g_thePlayer), itemFormObj, cmd.mSlot, false, true);
	}
	else if (cmd.mAction == EQUIP_SPELL)
	{
		const char* slotNames[3] = { "default", "right", "left" };  // should match eSlotType
		const size_t cmdBufferSize = 255;
		char cmdBuffer[cmdBufferSize];

		if (cmd.mSlot == SLOT_DEFAULT)  // equip in both hands if its slot default
		{
			sprintf_s(cmdBuffer, cmdBufferSize, "player.equipspell %x left", cmd.mFormID);
			CSkyrimConsole::RunCommand(cmdBuffer);

			sprintf_s(cmdBuffer, cmdBufferSize, "player.equipspell %x right", cmd.mFormID);
			CSkyrimConsole::RunCommand(cmdBuffer);
		}
		else if(cmd.mSlot <= SLOT_LEFTHAND)
		{
			sprintf_s(cmdBuffer, cmdBufferSize, "player.equipspell %x %s", cmd.mFormID, slotNames[cmd.mSlot]);
			CSkyrimConsole::RunCommand(cmdBuffer);
		}
		else
		{
			QSLOG_ERR("Invalid slot Type %d for spell: %x equip.", cmd.mSlot, cmd.mFormID);
		}
	}
	else if (cmd.mAction == EQUIP_SHOUT)
	{
		const size_t cmdBufferSize = 255;
		char cmdBuffer[cmdBufferSize];
		sprintf_s(cmdBuffer, cmdBufferSize, "player.equipshout %x", cmd.mFormID);
		CSkyrimConsole::RunCommand(cmdBuffer);
	}
	else if (cmd.mAction == CONSOLE_CMD)
	{
		CSkyrimConsole::RunCommand(cmd.mCommand.c_str());
	}
}

void CQuickslot::SetAction(PapyrusVR::VRDevice deviceId)
{
	// convert VRDevice id to skyrim Slot ID (left/right hand)
	const int deviceToSlotLookup[3] = { 0, SLOT_RIGHTHAND, SLOT_LEFTHAND };
	const int slot = deviceToSlotLookup[deviceId];

	// set action for only the current hand
	TESForm* formObj = (*g_thePlayer)->GetEquippedObject(slot == SLOT_LEFTHAND);

	if (formObj)
	{
		if (formObj->GetFormType() == kFormType_Weapon)
		{
			mCommand.mAction = EQUIP_ITEM;
		}
		else
		{
			mCommand.mAction = EQUIP_SPELL;
		}

		mCommand.mSlot = slot;
		mCommand.mFormID = formObj->formID;

		QSLOG("Set new action formid=%x on quickslot %s !", formObj->formID, this->mName.c_str());
	}
}


void CQuickslot::UnsetAction()
{
	mCommand.mAction = NO_ACTION;
	mCommand.mFormID = 0;
	mCommandAlt.mAction = NO_ACTION;
	mCommandAlt.mFormID = 0;

}





