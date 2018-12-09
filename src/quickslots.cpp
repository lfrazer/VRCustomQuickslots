#include "quickslots.h"
#include "quickslotutil.h"

// SKSE includes
#include "skse64/PapyrusActor.h"
#include "skse64/GameAPI.h"
#include "skse64/GameForms.h"
#include "skse64/GameRTTI.h"
#include "skse64/GameTypes.h"

void	CQuickslotManager::Update(PapyrusVR::TrackedDevicePose* hmdPose, PapyrusVR::TrackedDevicePose* leftCtrlPose, PapyrusVR::TrackedDevicePose* rightCtrlPose)
{
	mHMDPose = hmdPose;
	mLeftControllerPose = leftCtrlPose;
	mRightControllerPose = rightCtrlPose;


	PapyrusVR::Vector3 hmdPos = GetPositionFromVRPose(mHMDPose);
	PapyrusVR::Matrix33 rotMatrix = CreateRotMatrixAroundY(mHMDPose->mDeviceToAbsoluteTracking);

	for(auto it = mQuickslotArray.begin(); it != mQuickslotArray.end(); ++it)
	{
		// update quickslots based on HMD rotation
		it->mPosition = MultMatrix33(rotMatrix, it->mOrigin);

		// update translation for each quickslot 
		it->mPosition = it->mPosition + hmdPos;
	}
}

void	CQuickslotManager::ButtonPress(PapyrusVR::EVRButtonId buttonId, PapyrusVR::VRDevice deviceId)
{
	// check if relevant button was pressed
	if (buttonId != mActivateButton)
	{
		return;
	}

	// find quickslot based on current hand 
	PapyrusVR::TrackedDevicePose* currControllerPose = nullptr;

	if (deviceId == PapyrusVR::VRDevice_LeftController)
	{
		currControllerPose = mLeftControllerPose;
	}
	else if (deviceId == PapyrusVR::VRDevice_RightController)
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
		// one action for each hand (right and left)
		quickslot->DoAction(quickslot->mCommand);
		quickslot->DoAction(quickslot->mCommandAlt);

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
}