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

#include "skse64/PluginAPI.h"		// SKSE plugin API
#include "skse64_common/skse_version.h"

#include "common/IDebugLog.h"
#include <shlobj.h>				// for use of CSIDL_MYCODUMENTS

//Headers under api/ folder
#include "api/PapyrusVRAPI.h"
#include "api/VRManagerAPI.h"


#include "quickslots.h"


static PluginHandle					g_pluginHandle = kPluginHandle_Invalid;
static SKSEPapyrusInterface         * g_papyrus = NULL;
static SKSEMessagingInterface		* g_messaging = NULL;

PapyrusVRAPI*	g_papyrusvr = nullptr;
CQuickslotManager* g_quickslotMgr = nullptr;

extern "C" {

	void OnSKSEMessage(SKSEMessagingInterface::Message* msg);

	bool SKSEPlugin_Query(const SKSEInterface * skse, PluginInfo * info) {	// Called by SKSE to learn about this plugin and check that it's safe to load it
		gLog.OpenRelative(CSIDL_MYDOCUMENTS, "\\My Games\\Skyrim VR\\SKSE\\VRCustomQuickslots.log");
		gLog.SetPrintLevel(IDebugLog::kLevel_Error);
		gLog.SetLogLevel(IDebugLog::kLevel_DebugMessage);

		// populate info structure
		info->infoVersion = PluginInfo::kInfoVersion;
		info->name = "VRCustomQuickslots";
		info->version = 2;

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
		_MESSAGE("VRCustomQuickslots loaded");

		g_papyrus = (SKSEPapyrusInterface *)skse->QueryInterface(kInterface_Papyrus);

		//Registers for SKSE Messages (PapyrusVR probably still need to load, wait for SKSE message PostLoad)
		_MESSAGE("Registering for SKSE messages");
		g_messaging = (SKSEMessagingInterface*)skse->QueryInterface(kInterface_Messaging);
		g_messaging->RegisterListener(g_pluginHandle, "SKSE", OnSKSEMessage);

		g_quickslotMgr = new CQuickslotManager;

		//wait for PapyrusVR init (during PostPostLoad SKSE Message)

		return true;
	}

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
		PapyrusVR::TrackedDevicePose* leftHandPose = g_papyrusvr->GetVRManager()->GetLeftHandPose();
		PapyrusVR::TrackedDevicePose* rightHandPose = g_papyrusvr->GetVRManager()->GetRightHandPose();
		PapyrusVR::TrackedDevicePose* hmdPose = g_papyrusvr->GetVRManager()->GetHMDPose();

		g_quickslotMgr->Update(hmdPose, leftHandPose, rightHandPose);

	}

	//Listener for PapyrusVR Messages
	void OnPapyrusVRMessage(SKSEMessagingInterface::Message* msg)
	{
		if (msg)
		{
			if (msg->type == kPapyrusVR_Message_Init && msg->data)
			{
				_MESSAGE("PapyrusVR Init Message recived with valid data, registering for pose update callback");
				g_papyrusvr = (PapyrusVRAPI*)msg->data;

				_MESSAGE("Reading XML quickslots config vrcustomquickslots.xml");
				g_quickslotMgr->ReadConfig("Data\\SKSE\\Plugins\\vrcustomquickslots.xml");

				//Registers for PoseUpdates
				//g_papyrusvr->RegisterPoseUpdateListener(OnPoseUpdate);  // deprecated
				g_papyrusvr->GetVRManager()->RegisterVRButtonListener(OnVRButtonEvent);
				g_papyrusvr->GetVRManager()->RegisterVRUpdateListener(OnVRUpdateEvent);
			}
		}
	}

	//Listener for SKSE Messages
	void OnSKSEMessage(SKSEMessagingInterface::Message* msg)
	{
		if (msg)
		{
			if (msg->type == SKSEMessagingInterface::kMessage_PostLoad)
			{
				_MESSAGE("SKSE PostLoad recived, registering for PapyrusVR messages from SkyrimVRTools");
				g_messaging->RegisterListener(g_pluginHandle, "SkyrimVRTools", OnPapyrusVRMessage);
			}
			if (msg->type == SKSEMessagingInterface::kMessage_PostLoadGame || msg->type == SKSEMessagingInterface::kMessage_NewGame)
			{
				g_quickslotMgr->SetInGame(true);
			}
		}
	}



};
