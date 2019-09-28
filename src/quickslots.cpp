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
#include <algorithm>

#include "MenuChecker.h"

// SKSE includes
#include "skse64/PapyrusActor.h"
#include "skse64/GameAPI.h"
#include "skse64/GameForms.h"
#include "skse64/GameRTTI.h"
#include "skse64/GameTypes.h"
#include "skse64/GameExtraData.h"

#define QS_DEBUG_FEATURES   0

RelocAddr <_HasSpell> HasSpell(0x0984420);
RelocAddr <_ActorEquipItem> ActorEquipItem(0x09849C0);
RelocAddr <_GetItemCount> GetItemCount(0x09CEC90);
RelocAddr <_DropObject> DropObject(0x09CE580);


CQuickslotManager::CQuickslotManager()
{
	MenuManager * mm = MenuManager::GetSingleton();
	if (mm) {
		mm->MenuOpenCloseEventDispatcher()->AddEventSink(&MenuChecker::menuEvent);
	}
	else {
		QSLOG_ERR("Failed to register SKSE AllMenuEventHandler!");
	}

	GetVRSystem();

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

// make sure all pointers for tracking data are valid - error checking
bool	CQuickslotManager::IsTrackingDataValid() const
{
	return (this && mHMDPose && mRightControllerPose && mLeftControllerPose);
}

void	CQuickslotManager::UpdateHaptics()
{
	const unsigned short kVRHapticConstant = 2000;  // max value is 3999 but ue4 suggest max 2000? - time in microseconds to pulse per frame, also described by Valve as "strength"

	if (mVRSystem)
	{
		for (int i = 0; i < 2; ++i)
		{
			if (mControllerHapticTime[i] > CUtil::GetSingleton().GetLastTime())
			{
				// haptic remaining times are indexed in array by (ETrackedControllerRole enum value - 1) - so add 1 to loop index i to get the correct value - see StartHaptics()
				auto controllerId = mVRSystem->GetTrackedDeviceIndexForControllerRole( (vr::ETrackedControllerRole)(i + 1) );
				mVRSystem->TriggerHapticPulse(controllerId, 0, kVRHapticConstant);
			}
		}
	}
}

void	CQuickslotManager::StartHaptics(vr::ETrackedControllerRole controller, double timeLength)
{
	mControllerHapticTime[controller - 1] = CUtil::GetSingleton().GetLastTime() + timeLength;

	QSLOG_INFO("Started haptic feedback for %f seconds on controller %d", timeLength, controller);
}

void	CQuickslotManager::Update(PapyrusVR::TrackedDevicePose* hmdPose, PapyrusVR::TrackedDevicePose* leftCtrlPose, PapyrusVR::TrackedDevicePose* rightCtrlPose)
{
	mHMDPose = hmdPose;
	mLeftControllerPose = leftCtrlPose;
	mRightControllerPose = rightCtrlPose;

	CUtil::GetSingleton().Update();
	UpdateHaptics();

	if (mInGame && !MenuChecker::isGameStopped() && hmdPose->bPoseIsValid && leftCtrlPose->bPoseIsValid && rightCtrlPose->bPoseIsValid)
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
			const vr::ETrackedControllerRole controllerRoles[numControllers] = { vr::ETrackedControllerRole::TrackedControllerRole_LeftHand, vr::ETrackedControllerRole::TrackedControllerRole_RightHand };
			const PapyrusVR::VRDevice controllerDeviceIds[numControllers] = { PapyrusVR::VRDevice::VRDevice_LeftController, PapyrusVR::VRDevice::VRDevice_RightController };

			for (int i = 0; i < numControllers; ++i)
			{
				PapyrusVR::Vector3 controllerPos = GetPositionFromVRPose(controllers[i]);
				CQuickslot* quickslot = FindQuickslot(controllerPos, mControllerRadius);

				if (quickslot)
				{
					// Do haptic response (but not constantly, check for timeout)
					if (mHoverQuickslotHapticTime > 0.0 && CUtil::GetSingleton().GetLastTime() - quickslot->mLastOverlapTime > kHapticTimeout)
					{						
						StartHaptics(controllerRoles[i], mHoverQuickslotHapticTime);						
					}

					quickslot->mLastOverlapTime = CUtil::GetSingleton().GetLastTime();

					// trigger constant haptics on long press to indicate long press to the user
					const double currButtonHeldTime = CUtil::GetSingleton().GetLastTime() - quickslot->mButtonHoldTime;
					if (quickslot->mButtonHoldTime > 0.0 &&  currButtonHeldTime > mShortPressTime)
					{
						if (std::fmod(currButtonHeldTime, 0.5) < 0.05)
						{
							StartHaptics(controllerRoles[i], 0.075);
						}
					}

					if (quickslot->mButtonHoldTime > 0.0 && currButtonHeldTime > mLongPressTime)
					{
						QSLOG_INFO("Hold button action on quickslot %s !", quickslot->mName.c_str());

						// on long press, unset the quickslot action so the user can modify it later
						if (mAllowEditSlots)
						{
							// trigger haptic response

							// Empty slot if it is bound to an action, otherwise modify it (also special case for non-default orders to check "mOtherCommands")
							if ( (quickslot->mOrder == CQuickslot::DEFAULT && quickslot->mCommand.mAction != CQuickslot::NO_ACTION)
								|| (quickslot->mOtherCommands.size() > 0 && quickslot->mOtherCommands[0].mAction != CQuickslot::NO_ACTION) )
							{
								StartHaptics(controllerRoles[i], 0.5); // haptics on un-equip slot
								quickslot->UnsetAction();
							}
							else // modify the quickslot with current item/spell if none is set
							{
								StartHaptics(controllerRoles[i], 1.0); // longer haptics on equip
								quickslot->SetAction(controllerDeviceIds[i]);
							}

							quickslot->mButtonHoldTime = -1.0;
						}
					}

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

CQuickslot*	CQuickslotManager::FindQuickslotByDeviceId(PapyrusVR::VRDevice deviceId)
{
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
		QSLOG_INFO("No valid poses in button press! deviceId: %d", deviceId);
		return nullptr;
	}

	// find the relevant quickslot which is overlapped by the controllers current position
	PapyrusVR::Vector3 controllerPos = GetPositionFromVRPose(currControllerPose);
	CQuickslot* quickslot = FindQuickslot(controllerPos, mControllerRadius);
	return quickslot;
}

bool	CQuickslotManager::ButtonPress(PapyrusVR::EVRButtonId buttonId, PapyrusVR::VRDevice deviceId)
{

	// check if relevant button was pressed, or if a menu was open and early exit
	if (buttonId != mActivateButton || MenuChecker::isGameStopped() || !mInGame)
	{
		return false;
	}

	CQuickslot* quickslot = FindQuickslotByDeviceId(deviceId);

#if QS_DEBUG_FEATURES
	CQuickslot* nearestQS = FindNearestQuickslot(controllerPos);
	if (quickslot && nearestQS != quickslot)
	{
		QSLOG_INFO("nearest quickslot and FindQuickslot() were not the same!");
	}
#endif

	if (quickslot)  // if there is one, track button hold time on quickslot ( DoActions moved to OnRelease event -> ButtonRelease() )
	{
		// increase press time on this quickslot
		quickslot->mButtonHoldTime = CUtil::GetSingleton().GetLastTime();
		
		// TODO: fix logging here
		//QSLOG_INFO("Found a quickslot at pos (%f,%f,%f) !", controllerPos.x, controllerPos.y, controllerPos.z);
		quickslot->PrintInfo();
		
	}
	else
	{
		//QSLOG_INFO("NO quickslot found pos (%f,%f,%f)", controllerPos.x, controllerPos.y, controllerPos.z);
	}

	return quickslot != nullptr;
}


bool	CQuickslotManager::ButtonRelease(PapyrusVR::EVRButtonId buttonId, PapyrusVR::VRDevice deviceId)
{
	// check if relevant button was pressed, or if a menu was open and early exit
	if (buttonId != mActivateButton || MenuChecker::isGameStopped() || !mInGame)
	{
		QSLOG_INFO("Menu open. Cancelling...");
		return false;
	}

	CQuickslot* quickslot = FindQuickslotByDeviceId(deviceId);

	if (quickslot && quickslot->mButtonHoldTime > 0.0)  // only do action if the button was pressed on originally
	{
		if (quickslot->mCommand.mAction != CQuickslot::NO_ACTION || !quickslot->mOtherCommands.empty())
		{			
			QSLOG_INFO("Order is set to %d...", quickslot->mOrder);
			if (quickslot->mOrder == CQuickslot::eOrderType::DEFAULT) // one action for each hand (right and left), and execute first one that is applicable
			{
				for (UInt32 f = 0; f < quickslot->mCommand.mFormIDList.size(); f++)
				{
					if (quickslot->DoAction(quickslot->mCommand, quickslot->mCommand.mFormIDList[f]))
					{
						break;
					}
				}
				for (UInt32 f = 0; f < quickslot->mCommandAlt.mFormIDList.size(); f++)
				{
					if (quickslot->DoAction(quickslot->mCommandAlt, quickslot->mCommandAlt.mFormIDList[f]))
					{
						break;
					}
				}
			}
			else if(quickslot->mOrder == CQuickslot::eOrderType::FIRST) // execute first one that is applicable
			{
				//Here we loop through the commands array to see if we can find an applicable formid to do the action with.
				//DoAction returns bool value for us to check.
				bool success = false;
				for(UInt32 i=0; i<quickslot->mOtherCommands.size(); i++)
				{					
					for (UInt32 f = 0; f < quickslot->mOtherCommands[i].mFormIDList.size(); f++)
					{
						if (quickslot->DoAction(quickslot->mOtherCommands[i], quickslot->mOtherCommands[i].mFormIDList[f]))
						{
							success = true;
							break;
						}
					}
					
					if(success)
					{
						break;
					}
				}
			}
			else if (quickslot->mOrder == CQuickslot::eOrderType::RANDOM) // execute random one that is applicable
			{
				//Random function creates a possibilities array to select a random command and formid from that command.
				//To ensure that possible variable is never selected again, possibilities array item is deleted after each false return from DoAction.
				if (!quickslot->mOtherCommands.empty())
				{
					std::vector<int> possibilities(quickslot->mOtherCommands.size());
					for (UInt32 i = 0; i < quickslot->mOtherCommands.size(); i++)
					{
						possibilities[i] = i;
					}

					for (UInt32 i = 0; i < quickslot->mOtherCommands.size(); i++)
					{
						if (possibilities.empty())
							break;
						
						bool success = false;
						const int randIndex = randomGenerator(0, possibilities.size()-1);

						int positionIndex = possibilities[randIndex];
						if (!quickslot->mOtherCommands[positionIndex].mFormIDList.empty())
						{
							std::vector<int> formIdPossibilities(quickslot->mOtherCommands[positionIndex].mFormIDList.size());
							for (UInt32 p = 0; p < quickslot->mOtherCommands[positionIndex].mFormIDList.size(); p++)
							{
								formIdPossibilities[p] = p;
							}

							for (UInt32 f = 0; f < quickslot->mOtherCommands[positionIndex].mFormIDList.size(); f++)
							{
								if (formIdPossibilities.empty())
									break;

								const int randFormIdIndex = randomGenerator(0, formIdPossibilities.size()-1);
								int position2Index = formIdPossibilities[randFormIdIndex];
								if (quickslot->DoAction(quickslot->mOtherCommands[positionIndex], quickslot->mOtherCommands[positionIndex].mFormIDList[position2Index]))
								{
									success = true;
									break;
								}
								else
								{
									formIdPossibilities.erase(std::remove(formIdPossibilities.begin(), formIdPossibilities.end(), position2Index), formIdPossibilities.end());
								}
							}
						}
						if (success)
						{
							break;
						}
						else
						{
							possibilities.erase(std::remove(possibilities.begin(), possibilities.end(), positionIndex), possibilities.end());
						}
					}
				}
				
			}
			else if (quickslot->mOrder == CQuickslot::eOrderType::ALL) // execute all commands that are applicable
			{
				//Here we loop through the commands array and call DoAction for all the commands. But only one of the formIds if multiple are supplied.
				for (UInt32 i = 0; i<quickslot->mOtherCommands.size(); i++)
				{
					for (UInt32 f = 0; f < quickslot->mCommand.mFormIDList.size(); f++)
					{
						if (quickslot->DoAction(quickslot->mOtherCommands[i], quickslot->mCommand.mFormIDList[f])) //Use only one of the possible formIds.
						{
							break;
						}
					}
				}
			}
			else if (quickslot->mOrder == CQuickslot::eOrderType::TOGGLE) //Skip currently equipped stuff and execute first one that is applicable
			{
				//This is almost the same as eOrderType::FIRST cause above. We also check if the form is currently equipped right now and skip it.
				//If the user defines 2 items and selects this order, this makes them to toggle the spell, hence the order name.
				bool success = false;
				for (UInt32 i = 0; i<quickslot->mOtherCommands.size(); i++)
				{
					for (UInt32 f = 0; f < quickslot->mOtherCommands[i].mFormIDList.size(); f++)
					{
						if (!FormCurrentlyEquipped(quickslot->mOtherCommands[i].mFormIDList[f]))
						{
							if (quickslot->DoAction(quickslot->mOtherCommands[i], quickslot->mOtherCommands[i].mFormIDList[f]))
							{
								success = true;
								break;
							}
						}
					}

					if (success)
					{
						break;
					}
				}
			}
		}
		else
		{
			QSLOG_INFO("No action set for this quickslot...");
			// short haptic feedback if no action is set for the quickslot
			vr::ETrackedControllerRole controllerRole = (deviceId == PapyrusVR::VRDevice_LeftController) ? vr::TrackedControllerRole_LeftHand : vr::TrackedControllerRole_RightHand;

			StartHaptics(controllerRole, 0.2);
		}
	}

	// reset button hold time for all other quickslots
	for (auto it = mQuickslotArray.begin(); it != mQuickslotArray.end(); ++it)
	{
		it->mButtonHoldTime = -1.0;
	}

	return quickslot != nullptr;
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

// debugging helper func
CQuickslot*  CQuickslotManager::FindNearestQuickslot(const PapyrusVR::Vector3 pos)
{
	const size_t numQuickslots = mQuickslotArray.size();
	float closestDistSqr = FLT_MAX;
	CQuickslot* closestQuickslot = nullptr;

	for (size_t i = 0; i < numQuickslots; ++i)
	{

		const float currDistSqr = DistBetweenVecSqr(pos, mQuickslotArray[i].mPosition);
		if (currDistSqr < closestDistSqr)
		{
			closestDistSqr = currDistSqr;
			closestQuickslot = &mQuickslotArray[i];
		}
	}

	QSLOG_INFO("Closest quickslot was %s at distance %f", closestQuickslot->mName.c_str(), sqrtf(closestDistSqr));

	return closestQuickslot;
}


int		CQuickslotManager::GetEffectiveSlot(int inSlot)
{
	if (mLeftHandedMode && inSlot <= CQuickslot::SLOT_LEFTHAND)
	{
		const int outSlot[3] = { CQuickslot::SLOT_DEFAULT, CQuickslot::SLOT_LEFTHAND, CQuickslot::SLOT_RIGHTHAND };
		return outSlot[inSlot];
	}

	return inSlot;
}

PapyrusVR::EVRButtonId CQuickslotManager::GetActivateButton() const
{
	return mActivateButton;
}


void	CQuickslotManager::Reset()
{
	mQuickslotArray.clear();

	mInGame = false;
}

void CQuickslot::PrintInfo()
{
	QSLOG_INFO("Quickslot (%s) position: (%f,%f,%f) radius: %f", this->mName.c_str(), mPosition.x, mPosition.y, mPosition.z, mRadius);
}

//Checks if player has the item in their inventory. Calls GetItemCount Native function and checks if it's bigger than 0.
bool CQuickslot::PlayerHasItem(TESForm * itemForm)
{
	return GetItemCount((*g_skyrimVM)->GetClassRegistry(), 0, (Actor*)(*g_thePlayer), itemForm) != 0;
}

bool CQuickslot::DoAction(const CQuickslotCmd& cmd, UInt32 formId)
{
	if (cmd.mAction == EQUIP_ITEM)
	{
		TESForm* itemFormObj = LookupFormByID(formId);
		if (itemFormObj != nullptr)
		{
			if (PlayerHasItem(itemFormObj)) //We check if player has the item and return false if they do not.
			{
				QSLOG_INFO("Equipping item formId: %x", formId);
				// Use papyrus equip in task delegate for stability reasons
				g_task->AddTask(new taskActorEquipItem(itemFormObj));
			}
			else
			{
				QSLOG_INFO("Player doesn't have object with formId: %x", formId);
				return false;
			}
		}
		else
		{
			QSLOG_ERR("Invalid item formId: %x", formId);
			return false;
		}
	}
	else if(cmd.mAction == EQUIP_OTHER)
	{		
		TESForm* itemFormObj = LookupFormByID(formId);		
		if (itemFormObj != nullptr)
		{
			QSLOG_INFO("Checking if player has object with formid: %x", formId);
			if (PlayerHasItem(itemFormObj)) //We check if player has the item and return false if they do not.
			{
				QSLOG_INFO("Equipping item formId: %x", formId);
				g_task->AddTask(new taskActorEquipItem(itemFormObj));
			}
			else
			{
				QSLOG_INFO("Player doesn't have object with formId: %x", formId);
				return false;
			}
		}
		else
		{
			QSLOG_ERR("Invalid item formId: %x", formId);
			return false;
		}
	}
	else if (cmd.mAction == DROP_OBJECT)
	{
		TESForm* itemFormObj = LookupFormByID(formId);
		if (itemFormObj != nullptr)
		{
			QSLOG_INFO("Checking if player has object with formid: %x", formId);
			if (PlayerHasItem(itemFormObj)) //We check if player has the item and return false if they do not.
			{
				QSLOG_INFO("Dropping item formId: %x", formId);
				DropObject((*g_skyrimVM)->GetClassRegistry(), 0, (Actor*)(*g_thePlayer), itemFormObj, cmd.mCount);
			}
			else
			{
				QSLOG_INFO("Player doesn't have object with formId: %x", formId);
				return false;
			}
		}
		else
		{
			QSLOG_ERR("Invalid item formId: %x", formId);
			return false;
		}
	}
	else if (cmd.mAction == EQUIP_SPELL)
	{
		TESForm * spellForm = LookupFormByID(formId);

		if (spellForm != nullptr)
		{
			//Check if player knows the spell to prevent cheating (special allowance for spellsiphon though)
			if ( (GetModIndex(formId) == CQuickslotManager::GetSingleton().GetSpellsiphonModIndex()) || HasSpell((*g_skyrimVM)->GetClassRegistry(), 0, (Actor*)(*g_thePlayer), spellForm))
			{
				const char* slotNames[3] = { "default", "right", "left" };  // should match eSlotType
				const size_t cmdBufferSize = 255;
				char cmdBuffer[cmdBufferSize];

				if (cmd.mSlot == SLOT_DEFAULT)  // equip in both hands if its slot default
				{
					sprintf_s(cmdBuffer, cmdBufferSize, "player.equipspell %x left", formId);
					CSkyrimConsole::RunCommand(cmdBuffer);

					sprintf_s(cmdBuffer, cmdBufferSize, "player.equipspell %x right", formId);
					CSkyrimConsole::RunCommand(cmdBuffer);
				}
				else if (cmd.mSlot <= SLOT_LEFTHAND)
				{
					sprintf_s(cmdBuffer, cmdBufferSize, "player.equipspell %x %s", formId, slotNames[CQuickslotManager::GetSingleton().GetEffectiveSlot(cmd.mSlot)]);
					CSkyrimConsole::RunCommand(cmdBuffer);
				}
				else
				{
					QSLOG_ERR("Invalid slot Type %d for spell: %x equip.", cmd.mSlot, formId);
					return false;
				}
			}
			else
			{
				QSLOG_INFO("Player doesn't know spell: %x", formId);
				return false;
			}
		}
		else
		{
			QSLOG_ERR("Invalid spell formId: %x", formId);
			return false;
		}
	}
	else if (cmd.mAction == EQUIP_SHOUT)
	{
		TESForm * spellForm = LookupFormByID(formId);

		if (spellForm != nullptr)
		{
			//Check if player knows the shout to prevent cheating (same check for spellsiphon)
			if ((GetModIndex(formId) == CQuickslotManager::GetSingleton().GetSpellsiphonModIndex()) || HasSpell((*g_skyrimVM)->GetClassRegistry(), 0, (Actor*)(*g_thePlayer), spellForm))
			{
				const size_t cmdBufferSize = 255;
				char cmdBuffer[cmdBufferSize];
				sprintf_s(cmdBuffer, cmdBufferSize, "player.equipshout %x", formId);
				CSkyrimConsole::RunCommand(cmdBuffer);
			}
			else
			{
				QSLOG_INFO("Player doesn't know shout: %x", formId);
				return false;
			}
		}
		else
		{
			QSLOG_ERR("Invalid shout formId: %x", formId);
			return false;
		}
	}
	else if (cmd.mAction == CONSOLE_CMD)
	{
		CSkyrimConsole::RunCommand(cmd.mConsoleCommand.c_str());
	}
	else
	{
		return false;
	}
	return true;
}

// Set a new action on quickslot
void CQuickslot::SetAction(PapyrusVR::VRDevice deviceId)
{
	const vr::ETrackedControllerRole deviceToControllerRoleLookup[3] = { vr::ETrackedControllerRole::TrackedControllerRole_Invalid, vr::ETrackedControllerRole::TrackedControllerRole_RightHand, vr::ETrackedControllerRole::TrackedControllerRole_LeftHand };
	// convert VRDevice id to skyrim Slot ID (left/right hand)
	const int deviceToSlotLookup[3] = { 0, SLOT_RIGHTHAND, SLOT_LEFTHAND };
	const int slot = deviceToSlotLookup[deviceId];
	const int effectiveSlot = CQuickslotManager::GetSingleton().GetEffectiveSlot(slot);

	// set action for only the current hand
	TESForm* formObj = (*g_thePlayer)->GetEquippedObject(effectiveSlot == SLOT_LEFTHAND);

	auto SetCommand = [formObj, slot](CQuickslotCmd& cmd)
	{
		if (formObj)
		{
			if (formObj->GetFormType() == kFormType_Weapon)
			{
				cmd.mAction = EQUIP_ITEM;
			}
			else
			{
				cmd.mAction = EQUIP_SPELL;
			}

			cmd.mSlot = slot;
			cmd.mFormIDList.clear();
			cmd.mFormIDList.emplace_back(formObj->formID);
			cmd.mConsoleCommand = "";
			cmd.mItemType = 0;
			cmd.mFormIdStr = "";
			cmd.mKeyword = "";
			cmd.mKeywordNot = "";
			cmd.mPluginName = "";
			cmd.mFood = 1;
			cmd.mPoison = 1;
			cmd.mPotion = 1;
			cmd.mCount = 1;
		}
	};

	// special case to change command for first element in toggle order mode
	if (mOrder == TOGGLE && mOtherCommands.size() > 0)
	{
		SetCommand(mOtherCommands[0]);

		QSLOG("Set new action formid=%x on quickslot %s, slotID=%d effectiveSlot=%d for TOGGLE order mode", formObj->formID, this->mName.c_str(), slot, effectiveSlot);
	}
	else
	{
		SetCommand(mCommand);

		QSLOG("Set new action formid=%x on quickslot %s, slotID=%d effectiveSlot=%d", formObj->formID, this->mName.c_str(), slot, effectiveSlot);
	}
}

// TODO NOTE: UnsetAction will not unset "mOtherCommands" at all ATM 
void CQuickslot::UnsetAction()
{
	auto UnsetCommand = [](CQuickslotCmd& cmd)
	{
		cmd.mAction = NO_ACTION;
		cmd.mFormIDList.clear();
		cmd.mConsoleCommand = "";
		cmd.mItemType = 0;
		cmd.mFormIdStr = "";
		cmd.mKeyword = "";
		cmd.mKeywordNot = "";
		cmd.mPluginName = "";
		cmd.mFood = 1;
		cmd.mPoison = 1;
		cmd.mPotion = 1;
		cmd.mCount = 1;
	};

	// special case to change command for first element in toggle order mode
	if (mOrder == TOGGLE && mOtherCommands.size() > 0)
	{
		UnsetCommand(mOtherCommands[0]);
	}
	else
	{
		UnsetCommand(mCommand);
		UnsetCommand(mCommandAlt);
	}
}

//ActorEquipItem TaskDelegate functions
taskActorEquipItem::taskActorEquipItem(TESForm* akItem)
{
	m_akItem = akItem;
}

void taskActorEquipItem::Run()
{
	ActorEquipItem((*g_skyrimVM)->GetClassRegistry(), 0, (Actor*)(*g_thePlayer), m_akItem, false, false);
}

void taskActorEquipItem::Dispose()
{
	delete this;
}



