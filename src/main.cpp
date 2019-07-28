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

#include "skse64/PluginAPI.h"		// SKSE plugin API
#include "skse64_common/skse_version.h"

#include "common/IDebugLog.h"
#include <shlobj.h>				// for use of CSIDL_MYCODUMENTS
#include <algorithm>

#include "quickslots.h"
#include "quickslotutil.h"


static PluginHandle					g_pluginHandle = kPluginHandle_Invalid;
static SKSEPapyrusInterface         * g_papyrus = NULL;
static SKSEMessagingInterface		* g_messaging = NULL;

PapyrusVRAPI*	g_papyrusvr = nullptr;
CQuickslotManager* g_quickslotMgr = nullptr;
CUtil*			g_Util = nullptr;
vr::IVRSystem*	g_VRSystem = nullptr; // only set by new RAW api from Hook Mgr

const char* kConfigFile = "Data\\SKSE\\Plugins\\vrcustomquickslots.xml";
const char* kConfigFileUniqueId = "Data\\SKSE\\Plugins\\vrcustomquickslots_%s.xml";
const int VRCUSTOMQUICKSLOTS_VERSION = 5;

extern "C" {

	void OnSKSEMessage(SKSEMessagingInterface::Message* msg);

	bool SKSEPlugin_Query(const SKSEInterface * skse, PluginInfo * info) {	// Called by SKSE to learn about this plugin and check that it's safe to load it
		gLog.OpenRelative(CSIDL_MYDOCUMENTS, "\\My Games\\Skyrim VR\\SKSE\\VRCustomQuickslots.log");
		gLog.SetPrintLevel(IDebugLog::kLevel_Error);
		gLog.SetLogLevel(IDebugLog::kLevel_DebugMessage);

		// populate info structure
		info->infoVersion = PluginInfo::kInfoVersion;
		info->name = "VRCustomQuickslots";
		info->version = VRCUSTOMQUICKSLOTS_VERSION;

		// store plugin handle so we can identify ourselves later
		g_pluginHandle = skse->GetPluginHandle();

		if (skse->isEditor)
		{
			_MESSAGE("loaded in editor, marking as incompatible");

			return false;
		}
		else if (skse->runtimeVersion != RUNTIME_VR_VERSION_1_4_15)
		{
			_MESSAGE("unsupported runtime version %08X", skse->runtimeVersion);

			return false;
		}

		// ### do not do anything else in this callback
		// ### only fill out PluginInfo and return true/false

		// supported runtime version
		return true;
	}

	bool SKSEPlugin_Load(const SKSEInterface * skse) {	// Called by SKSE to load this plugin
		
		g_quickslotMgr = new CQuickslotManager;
		g_Util = new CUtil;
		
		_MESSAGE("VRCustomQuickslots loaded");

		g_papyrus = (SKSEPapyrusInterface *)skse->QueryInterface(kInterface_Papyrus);

		//Registers for SKSE Messages (PapyrusVR probably still need to load, wait for SKSE message PostLoad)
		_MESSAGE("Registering for SKSE messages");
		g_messaging = (SKSEMessagingInterface*)skse->QueryInterface(kInterface_Messaging);
		g_messaging->RegisterListener(g_pluginHandle, "SKSE", OnSKSEMessage);



		//wait for PapyrusVR init (during PostPostLoad SKSE Message)

		return true;
	}

	// Legacy API event handlers
	void OnVRButtonEvent(PapyrusVR::VREventType type, PapyrusVR::EVRButtonId buttonId, PapyrusVR::VRDevice deviceId)
	{
		// Use button presses here
		if (type == PapyrusVR::VREventType_Pressed)
		{
			//_MESSAGE("VR Button press deviceId: %d buttonId: %d", deviceId, buttonId);

			g_quickslotMgr->ButtonPress(buttonId, deviceId);
		}
		else if (type == PapyrusVR::VREventType_Released)
		{
			g_quickslotMgr->ButtonRelease(buttonId, deviceId);
		}
	}

	void OnVRUpdateEvent(float deltaTime)
	{
		// Get render poses by default
		PapyrusVR::TrackedDevicePose* leftHandPose = g_papyrusvr->GetVRManager()->GetLeftHandPose();
		PapyrusVR::TrackedDevicePose* rightHandPose = g_papyrusvr->GetVRManager()->GetRightHandPose();
		PapyrusVR::TrackedDevicePose* hmdPose = g_papyrusvr->GetVRManager()->GetHMDPose();

		g_quickslotMgr->Update(hmdPose, leftHandPose, rightHandPose);

	}

	// New RAW API event handlers
	bool OnControllerStateChanged(vr::TrackedDeviceIndex_t unControllerDeviceIndex, const vr::VRControllerState_t* pControllerState, uint32_t unControllerStateSize, vr::VRControllerState_t* pOutputControllerState)
	{
		static uint64_t lastButtonPressedData[PapyrusVR::VRDevice_LeftController + 1] = {0}; // should be size 3 array at least for all 3 vr devices
		static_assert(PapyrusVR::VRDevice_LeftController + 1 >= 3, "lastButtonPressedData array size too small!");
		
		vr::TrackedDeviceIndex_t leftcontroller = g_VRSystem->GetTrackedDeviceIndexForControllerRole(vr::ETrackedControllerRole::TrackedControllerRole_LeftHand);
		vr::TrackedDeviceIndex_t rightcontroller = g_VRSystem->GetTrackedDeviceIndexForControllerRole(vr::ETrackedControllerRole::TrackedControllerRole_RightHand);

		// NOTE: DO NOT check the packetNum on ControllerState, it seems to cause problems (maybe it should only be checked per controllers?) - at any rate, it seems to change every frame anyway.  

		PapyrusVR::VRDevice deviceId = PapyrusVR::VRDevice_Unknown;
			
		if (unControllerDeviceIndex == leftcontroller)
		{
			deviceId = PapyrusVR::VRDevice_LeftController;
		}
		else if(unControllerDeviceIndex == rightcontroller)
		{
			deviceId = PapyrusVR::VRDevice_RightController;
		}

		if (deviceId != PapyrusVR::VRDevice_Unknown)
		{
			// right now only check for trigger press.  In future support input binding?
			if (pControllerState->ulButtonPressed & ButtonMaskFromId(vr::k_EButton_SteamVR_Trigger) && !(lastButtonPressedData[deviceId] & ButtonMaskFromId(vr::k_EButton_SteamVR_Trigger)))
			{
				bool retVal = g_quickslotMgr->ButtonPress(PapyrusVR::k_EButton_SteamVR_Trigger, deviceId);

				if (retVal) // mask out input if we touched a quickslot (block the game from receiving it)
				{
					pOutputControllerState->ulButtonPressed &= ~ButtonMaskFromId(vr::k_EButton_SteamVR_Trigger);
				}

				QSLOG_INFO("Trigger pressed for deviceIndex: %d deviceId: %d", unControllerDeviceIndex, deviceId);
			}
			else if (!(pControllerState->ulButtonPressed & ButtonMaskFromId(vr::k_EButton_SteamVR_Trigger)) && (lastButtonPressedData[deviceId] & ButtonMaskFromId(vr::k_EButton_SteamVR_Trigger)))
			{
				g_quickslotMgr->ButtonRelease(PapyrusVR::k_EButton_SteamVR_Trigger, deviceId);

				QSLOG_INFO("Trigger released for deviceIndex: %d deviceId: %d", unControllerDeviceIndex, deviceId);
			}
			
			lastButtonPressedData[deviceId] = pControllerState->ulButtonPressed;
		}
		
		return true;
	}


	vr::EVRCompositorError OnGetPosesUpdate(VR_ARRAY_COUNT(unRenderPoseArrayCount) vr::TrackedDevicePose_t* pRenderPoseArray, uint32_t unRenderPoseArrayCount,
			VR_ARRAY_COUNT(unGamePoseArrayCount) vr::TrackedDevicePose_t* pGamePoseArray, uint32_t unGamePoseArrayCount)
	{
		// actually, we do need to copy the pose memory data here because ButtonPress/Release will refer to it by pointer later.
		static PapyrusVR::TrackedDevicePose sPoseData[vr::k_unMaxTrackedDeviceCount];

		// NOTE: PapyrusVR tracked pose structure must match structure from OpenVR.h exactly.  Be wary of OpenVR updates!  original code from artumino engineered like this ¯\_(@)_/¯
		PapyrusVR::TrackedDevicePose* hmdPose = nullptr;
		PapyrusVR::TrackedDevicePose* leftCtrlPose = nullptr;
		PapyrusVR::TrackedDevicePose* rightCtrlPose = nullptr;

		memcpy_s(sPoseData, sizeof(PapyrusVR::TrackedDevicePose) * vr::k_unMaxTrackedDeviceCount, pRenderPoseArray, sizeof(PapyrusVR::TrackedDevicePose) * unRenderPoseArrayCount);
		

		for (uint32_t i = 0; i < unRenderPoseArrayCount; ++i)
		{
			if (i == PapyrusVR::k_unTrackedDeviceIndex_Hmd)
			{
				hmdPose = (PapyrusVR::TrackedDevicePose*)&sPoseData[i];
			}
			else if (i == g_VRSystem->GetTrackedDeviceIndexForControllerRole(vr::ETrackedControllerRole::TrackedControllerRole_LeftHand))
			{
				leftCtrlPose = (PapyrusVR::TrackedDevicePose*)&sPoseData[i];
			}
			else if (i == g_VRSystem->GetTrackedDeviceIndexForControllerRole(vr::ETrackedControllerRole::TrackedControllerRole_RightHand))
			{
				rightCtrlPose = (PapyrusVR::TrackedDevicePose*)&sPoseData[i];
			}
		}

		g_quickslotMgr->Update(hmdPose, leftCtrlPose, rightCtrlPose);

		return vr::VRCompositorError_None;
	}

	//Listener for PapyrusVR Messages
	void OnPapyrusVRMessage(SKSEMessagingInterface::Message* msg)
	{
		if (msg)
		{
			if (msg->type == kPapyrusVR_Message_Init && msg->data)
			{
				_MESSAGE("PapyrusVR Init Message received with valid data, waiting for init.");
				g_papyrusvr = (PapyrusVRAPI*)msg->data;

			}
		}
	}

	//Listener for SKSE Messages
	void OnSKSEMessage(SKSEMessagingInterface::Message* msg)
	{
		// path to XML config file for unique character ID - will be set on PreLoadGame message
		static char sConfigUniqueIdPath[MAX_PATH] = { 0 };

		if (msg)
		{
			if (msg->type == SKSEMessagingInterface::kMessage_PostLoad)
			{
				_MESSAGE("SKSE PostLoad message received, registering for PapyrusVR messages from SkyrimVRTools");  // This log msg may happen before XML is loaded
				g_messaging->RegisterListener(g_pluginHandle, "SkyrimVRTools", OnPapyrusVRMessage);
			}
			else if (msg->type == SKSEMessagingInterface::kMessage_DataLoaded)
			{
				if (g_papyrusvr)
				{
					_MESSAGE("Initializing VRCustomQuickslots data.");
					_MESSAGE("Reading XML quickslots config vrcustomquickslots.xml");
					g_quickslotMgr->ReadConfig(kConfigFile);

					QSLOG("XML config load complete.");
					QSLOG("VRCustomQuickslots Plugin version: %d", VRCUSTOMQUICKSLOTS_VERSION);

					OpenVRHookManagerAPI* hookMgrAPI = RequestOpenVRHookManagerObject();
					if (hookMgrAPI && !g_quickslotMgr->DisableRawAPI())
					{
						QSLOG("Using new RAW OpenVR Hook API.");

						g_quickslotMgr->SetHookMgr(hookMgrAPI);
						g_VRSystem = hookMgrAPI->GetVRSystem(); // setup VR system before callbacks
						hookMgrAPI->RegisterControllerStateCB(OnControllerStateChanged);
						hookMgrAPI->RegisterGetPosesCB(OnGetPosesUpdate);

					}
					else
					{
						QSLOG("Using legacy PapyrusVR API.");

						//Registers for PoseUpdates
						g_papyrusvr->GetVRManager()->RegisterVRButtonListener(OnVRButtonEvent);
						g_papyrusvr->GetVRManager()->RegisterVRUpdateListener(OnVRUpdateEvent);
					}
				}
				else
				{
					_MESSAGE("PapyrusVR was not initialized!");
				}
			}
			else if (msg->type == SKSEMessagingInterface::kMessage_PreLoadGame)
			{
				char saveGameDataFile[MAX_PATH] = { 0 };
				const int NUM_SAVE_TOKENS = 20;
				char* saveNameTokens[NUM_SAVE_TOKENS] = { 0 };
				char* strtok_context;
				int currTokenIdx = 0;

				strncpy_s(saveGameDataFile, (char*)msg->data, std::min<size_t>(MAX_PATH - 1, msg->dataLen));
				
				// split up the string to extract the unique hash from the savefile name that identifies a save game for a specific character
				char* currTok = strtok_s(saveGameDataFile, "_", &strtok_context);
				saveNameTokens[currTokenIdx++] = currTok;
				while (currTok != nullptr)
				{
					currTok = strtok_s(nullptr, "_", &strtok_context);
					if (currTok && currTokenIdx < NUM_SAVE_TOKENS)
					{
						saveNameTokens[currTokenIdx++] = currTok;
					}
				}
				
				// index 1 will always be the uniqueId based on the format of save game names
				const char* playerUID = saveNameTokens[1];
				_MESSAGE("SKSE PreLoadGame message received, save file name: %s UniqueID: %s", (char*)msg->data, playerUID ? playerUID : "NULL");

				
				if (playerUID)
				{
					sprintf_s(sConfigUniqueIdPath, kConfigFileUniqueId, playerUID);
					
					// check if unique ID config file exists
					FILE* fp = nullptr;
					fopen_s(&fp, sConfigUniqueIdPath, "r");
					if (fp)
					{
						fclose(fp); // if so, close handle and try to load XML
						g_quickslotMgr->ReadConfig(sConfigUniqueIdPath);
						QSLOG("XML config for unique playerID load complete.");
					}
					else
					{
						QSLOG("Unable to find player specific config file: %s", sConfigUniqueIdPath);
					}
				}




			}
			else if (msg->type == SKSEMessagingInterface::kMessage_PostLoadGame || msg->type == SKSEMessagingInterface::kMessage_NewGame)
			{
				QSLOG("SKSE PostLoadGame or NewGame message received, type: %d", msg->type);
				g_quickslotMgr->SetInGame(true);
			}
			else if (msg->type == SKSEMessagingInterface::kMessage_SaveGame)
			{
				QSLOG("SKSE SaveGame message received.");

				if (g_quickslotMgr->AllowEdit())
				{
					if (strlen(sConfigUniqueIdPath) > 0) // if we have a unique character config path, write to that instead
					{
						g_quickslotMgr->WriteConfig(sConfigUniqueIdPath);
					}
					else
					{
						g_quickslotMgr->WriteConfig(kConfigFile);
					}
				}
			}
		}
	}



};
