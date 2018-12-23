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


extern CQuickslotManager* g_quickslotMgr;

CQuickslotManager::CQuickslotManager()
{
	mTimer.Init();



	MenuManager * mm = MenuManager::GetSingleton();
	if (mm) {
		mm->MenuOpenCloseEventDispatcher()->AddEventSink(&this->mMenuEventHandler);
	}
	else {
		_MESSAGE("Failed to register SKSE AllMenuEventHandler!");
	}

	GetVRSystem();

}

bool	CQuickslotManager::IsMenuOpen()
{ 
	return mIsMenuOpen || mMenuLastCloseTime + kMenuBlockDelay > mTimer.GetLastTime(); 
}

void  CQuickslotManager::GetVRSystem()
{
	vr::EVRInitError eError = vr::VRInitError_None;
	//mVRSystem = (vr::IVRSystem *)vr::VR_GetGenericInterface(vr::IVRSystem_Version, &eError);


	mVRSystem = vr::VRSystem();

	if (mVRSystem)
	{
		_MESSAGE("Found VR System ptr");
	}
	else if(eError != mLastVRError)
	{
		_MESSAGE("VR System ptr not found, last unique error: %d", eError);
		mLastVRError = eError;
	}
}


void	CQuickslotManager::Update(PapyrusVR::TrackedDevicePose* hmdPose, PapyrusVR::TrackedDevicePose* leftCtrlPose, PapyrusVR::TrackedDevicePose* rightCtrlPose)
{
	mHMDPose = hmdPose;
	mLeftControllerPose = leftCtrlPose;
	mRightControllerPose = rightCtrlPose;

	mTimer.TimerUpdate();

	if (!IsMenuOpen() && hmdPose->bPoseIsValid && leftCtrlPose->bPoseIsValid && rightCtrlPose->bPoseIsValid)
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
					if (mTimer.GetLastTime() - quickslot->mLastOverlapTime > kHapticTimeout)
					{
						auto controllerId = mVRSystem->GetTrackedDeviceIndexForControllerRole( controllerRoles[i]);
						mVRSystem->TriggerHapticPulse(controllerId, 0, 3999);

						if (mDebugLogVerb > 0)
						{
							_MESSAGE("Triggered haptic pulse on controller %d", controllerId);
						}
					}

					quickslot->mLastOverlapTime = mTimer.GetLastTime();

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
	if (buttonId != mActivateButton || IsMenuOpen())
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
		if (quickslot->mButtonHoldTime > kHoldActionTime)
		{
			_MESSAGE("Hold button action on quickslot %s !", quickslot->mName.c_str());
			quickslot->mButtonHoldTime = 0.0;
		}
		else if (quickslot->mButtonHoldTime <= 0.0)
		{
			// one action for each hand (right and left)
			quickslot->DoAction(quickslot->mCommand);
			quickslot->DoAction(quickslot->mCommandAlt);

			// increase press time on this quickslot
			quickslot->mButtonHoldTime = mTimer.GetTimeSlice();
		}
		else
		{
			// increase press time on this quickslot
			quickslot->mButtonHoldTime += mTimer.GetTimeSlice();
		}


		if (mDebugLogVerb > 0)
		{
			_MESSAGE("Found a quickslot at pos (%f,%f,%f) !", controllerPos.x, controllerPos.y, controllerPos.z);

			quickslot->PrintInfo();
		}
	}
	else if(mDebugLogVerb > 0)
	{
		_MESSAGE("NO quickslot found pos (%f,%f,%f)", controllerPos.x, controllerPos.y, controllerPos.z);
	}

	// reset button hold time for all other quickslots
	for (auto it = mQuickslotArray.begin(); it != mQuickslotArray.end(); ++it)
	{
		if (&(*it) != quickslot)
		{
			it->mButtonHoldTime = 0.0;
		}
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
		g_quickslotMgr->mIsMenuOpen = true;
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
		g_quickslotMgr->mMenuLastCloseTime = g_quickslotMgr->mTimer.GetLastTime();
		g_quickslotMgr->mIsMenuOpen = false;
	}

	//_MESSAGE("MenuCloseEvent: %s", menuName);
}


void CQuickslot::PrintInfo()
{
	_MESSAGE("Quickslot (%s) position: (%f,%f,%f) radius: %f", this->mName.c_str(), mPosition.x, mPosition.y, mPosition.z, mRadius);
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
			_MESSAGE("Invalid slot Type %d for spell: %x equip.", cmd.mSlot, cmd.mFormID);
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


