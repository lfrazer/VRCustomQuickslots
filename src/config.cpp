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
#include "tinyxml2.h"
#include "skse64/GameData.h"
#include "skse64/PapyrusKeyword.h"
#include "skse64/PapyrusGameData.cpp"


bool   CQuickslotManager::ReadConfig(const char* filename)
{
	// First, read some INI config from game configs

	Setting* iniLeftHandSetting = GetINISetting("bLeftHandedMode:VRInput");
	if (iniLeftHandSetting)
	{
		mLeftHandedMode = (int)iniLeftHandSetting->data.u8;
		_MESSAGE("Left Hand Mode: %d", mLeftHandedMode);
	}

	tinyxml2::XMLDocument xmldoc;
	tinyxml2::XMLError err = xmldoc.LoadFile(filename);

	if(err != tinyxml2::XMLError::XML_SUCCESS)
	{
		_MESSAGE("Error: %s opening XML doc: %s", xmldoc.ErrorName(), filename);
		_MESSAGE("Full XML Error Info: %s", xmldoc.ErrorStr());
		return false;
	}

	Reset();  // reset current quickslot data

	int quickslotCount = 0;
	tinyxml2::XMLElement* root = xmldoc.RootElement();

	for (tinyxml2::XMLElement* elem = root->FirstChildElement(); elem; elem = elem->NextSiblingElement())
	{		
		if (strcmp(elem->Name(), "options") == 0)
		{
			elem->QueryFloatAttribute("defaultradius", &mDefaultRadius);
			elem->QueryIntAttribute("debugloglevel", &mDebugLogVerb);
			elem->QueryIntAttribute("hapticfeedback", &mHapticOnOverlap);
			elem->QueryIntAttribute("alloweditslots", &mAllowEditSlots);
			elem->QueryIntAttribute("disablerawapi", &mDisableRawAPI);
			elem->QueryDoubleAttribute("longpresstime", &mLongPressTime);
			elem->QueryDoubleAttribute("hoverquickslothaptictime", &mHoverQuickslotHapticTime);
			
			int activateButtonId = 0;
			elem->QueryIntAttribute("activatebutton", &activateButtonId);
			if(activateButtonId!=0)
			{
				mActivateButton = (PapyrusVR::EVRButtonId)activateButtonId;
			}

			mControllerRadius = mDefaultRadius;

			elem->QueryFloatAttribute("controllerradius", &mControllerRadius);

			CUtil::GetSingleton().SetLogLevel(mDebugLogVerb);
		}
		else if (strcmp(elem->Name(), "quickslot") == 0)
		{
			float position[3];
			float radius = mDefaultRadius;
			std::vector<CQuickslot::CQuickslotCmd> cmdList; // two commands, one for each hand
			const char* altCmd = "none";
			const char* slotname = nullptr;
			int order = CQuickslot::eOrderType::DEFAULT;

			elem->QueryFloatAttribute("posx", &position[0]);
			elem->QueryFloatAttribute("posy", &position[1]);
			elem->QueryFloatAttribute("posz", &position[2]);
			elem->QueryFloatAttribute("radius", &radius);
			elem->QueryStringAttribute("name", &slotname);
			elem->QueryIntAttribute("order", &order);

			for(tinyxml2::XMLElement* subElem = elem->FirstChildElement(); subElem; subElem = subElem->NextSiblingElement())
			{
				//Order type eOrderType::DEFAULT only allows 2 commands(Command and CommandAlt) but other order types can get as many as they can.
				if (cmdList.size() < 2 || order != CQuickslot::eOrderType::DEFAULT) 
				{
					CQuickslot::CQuickslotCmd cmd;
					if (strcmp(subElem->Name(), "equipitem") == 0)
					{
						cmd.mAction = CQuickslot::EQUIP_ITEM;
					}
					else if (strcmp(subElem->Name(), "equipother") == 0)
					{
						cmd.mAction = CQuickslot::EQUIP_OTHER;
					}
					else if (strcmp(subElem->Name(), "equipspell") == 0)
					{
						cmd.mAction = CQuickslot::EQUIP_SPELL;
					}
					else if (strcmp(subElem->Name(), "equipshout") == 0)
					{
						cmd.mAction = CQuickslot::EQUIP_SHOUT;
					}
					else if (strcmp(subElem->Name(), "consolecmd") == 0)
					{
						cmd.mAction = CQuickslot::CONSOLE_CMD;
					}
					else if (strcmp(subElem->Name(), "dropobject") == 0)
					{
						cmd.mAction = CQuickslot::DROP_OBJECT;
					}

					DataHandler * dataHandler = DataHandler::GetSingleton();
					std::string pluginNumber = "";
					bool allPlugins = true;
					UInt8 modIndex = 0;
					std::string formIdString;
					subElem->QueryString2Attribute("formid", &formIdString);
					std::vector<std::string> formIdStringList;
					if (formIdString.length() > 0)
					{						
						trim(formIdString);
						
						//QSLOG_INFO("FormId string in slot %s -> %s", slotname, formIdString.c_str());
						
						//We save this for when we write the xml file.
						cmd.mFormIdStr = formIdString;

						//FormId attribute can get many formids. We separate it by ",".
						std::vector<std::string> splittedByPlugin = split(formIdString, ',');

						if (!splittedByPlugin.empty())
						{
							for (std::string elemFormIdStr : splittedByPlugin)
							{
								trim(elemFormIdStr);
								if (elemFormIdStr.length() > 0)
								{
									QSLOG_INFO("Adding formId string to array: %s", elemFormIdStr.c_str());
									formIdStringList.emplace_back(elemFormIdStr);
								}
							}
						}
					}

					std::string pluginName;
					subElem->QueryString2Attribute("pluginname", &pluginName);
					if(pluginName.length()>0)
					{
						cmd.mPluginName = pluginName;
						//We get the mod index of the plugin
						modIndex = dataHandler->GetLoadedModIndex(pluginName.c_str());
						if (modIndex != 255) //If plugin is in the load order.
						{
							allPlugins = false;
							if (modIndex >= 0)
							{
								pluginNumber = num2hex(modIndex, 2);
								QSLOG_INFO("%s Pluginname: %s -> %s", slotname, pluginName.c_str(), pluginNumber.c_str());
							}
						}
						else
						{
							//Plugin name doesn't exist in current load order, skipping this element
							continue;
						}
					}

					if (!formIdStringList.empty())
					{
						for (std::string formIdStrElement : formIdStringList)
						{
							if (formIdStrElement.length() == 0)
								continue;

							//We fill the missing hex chars with zeroes
							if (formIdStrElement.length() < 6)
							{
								std::string zeroes = "";
								for (int l = 0; l < 6 - formIdStrElement.length(); l++)
								{
									zeroes = "0" + zeroes;
								}
								formIdStrElement = zeroes + formIdStrElement;
							}

							//If there is a pluginName defined, we use that pluginNumber instead of the supplied one if supplied.
							if (!pluginNumber.empty())
							{
								if (formIdStrElement.length() == 8 && pluginNumber.length() == 2)
								{
									formIdStrElement = formIdStrElement.substr(2, 6);
								}
								
								formIdStrElement = pluginNumber + formIdStrElement;
							}

							UInt32 formId = getHex(formIdStrElement);  // convert hex string to int

							if (formId > 0)
							{
								//QSLOG_INFO("FormId found for slot %s: %x - formIdStrElement: %s pluginNumber: %s", slotname, formId, formIdStrElement.c_str(), pluginNumber.c_str());

								//We check if the formid is correct and corresponds to a real formid in the game.
								TESForm* formObj = LookupFormByID(formId);
								if (formObj != nullptr)
								{
									cmd.mFormIDList.emplace_back(formId);
								}
								else
								{
									QSLOG_INFO("Bad FormId for slot %s, modIndex: %x formIdstr: %s", slotname, modIndex, formIdStrElement.c_str());
								}
							}
							else
							{
								QSLOG_INFO("Bad FormId for slot %s, modIndex: %x formIdstr: %s", slotname, modIndex, formIdStrElement.c_str());
							}
						}
					}

					//This is used for dropobject only.
					subElem->QueryIntAttribute("count", &cmd.mCount);

					int itemType = 0;
					subElem->QueryIntAttribute("itemtype", &itemType);
					if(itemType != 0)
					{
						cmd.mItemType = itemType;
						QSLOG_INFO("ItemType: %d", itemType);
						BGSKeyword * foundKeyword = nullptr;
						BGSKeyword * foundKeywordNot = nullptr;
						
						std::string keyword;
						subElem->QueryString2Attribute("keyword", &keyword);
						std::string keywordNot;
						subElem->QueryString2Attribute("keywordnot", &keywordNot);

						//We get the keywords array here by string.
						std::vector<BGSKeyword*> keywordsArray;
						trim(keyword);
						if(keyword.length()>0)
						{
							cmd.mKeyword = keyword;
							for (const auto& keywordStr : split(keyword, ','))
							{
								foundKeyword = papyrusKeyword::GetKeyword(nullptr, BSFixedString (keywordStr.c_str()));
								if (foundKeyword != nullptr)
								{
									keywordsArray.emplace_back(foundKeyword);
								}
							}																				
							QSLOG_INFO("keyword count: %d", keywordsArray.size());
						}

						//We get the keywordsNot array here by string.
						std::vector<BGSKeyword*> keywordsNotArray;
						trim(keywordNot);
						if (keywordNot.length()>0)
						{
							cmd.mKeywordNot = keywordNot;
							for (const auto& keywordNotStr : split(keywordNot, ','))
							{
								foundKeywordNot = papyrusKeyword::GetKeyword(nullptr, BSFixedString (keywordNotStr.c_str()));
								if (foundKeywordNot != nullptr)
								{
									keywordsNotArray.emplace_back(foundKeywordNot);
								}
							}
							QSLOG_INFO("keywordNot count: %d", keywordsNotArray.size());
						}

						if(itemType == CQuickslot::eItemType::Ingestible)
						{
							subElem->QueryIntAttribute("potion", &cmd.mPotion);
							subElem->QueryIntAttribute("food", &cmd.mFood);
							subElem->QueryIntAttribute("poison", &cmd.mPoison);

							QSLOG_INFO("GetAllPotions...");
							std::vector<UInt32> allPotionFormIds = GetAllPotions(allPlugins, modIndex, keywordsArray, keywordsNotArray, cmd.mPotion, cmd.mFood, cmd.mPoison);
														
							QSLOG_INFO("Ingestible FormIds found for slot %s count:%d", slotname, allPotionFormIds.size());
							for (UInt32 potionFormId : allPotionFormIds)
							{
								cmd.mFormIDList.emplace_back(potionFormId);
							}
						}
						else if(itemType == CQuickslot::eItemType::Ammunition)
						{
							QSLOG_INFO("GetAllAmmo...");
							std::vector<UInt32> allAmmoFormIds = GetAllAmmo(allPlugins, modIndex, keywordsArray, keywordsNotArray);
														
							QSLOG_INFO("Ammunition FormIds found for slot %s count:%d", slotname, allAmmoFormIds.size());
							for (UInt32 ammoFormId : allAmmoFormIds)
							{
								cmd.mFormIDList.emplace_back(ammoFormId);
							}
						}
					}

					// get which slot to use
					subElem->QueryIntAttribute("slot", &cmd.mSlot);

					if (subElem->GetText())
					{
						cmd.mConsoleCommand = subElem->GetText();
					}
					cmdList.emplace_back(cmd);
				}
			}

			
			CQuickslot quickslot(PapyrusVR::Vector3(position[0], position[1], position[2]), radius, cmdList, order, slotname);
			mQuickslotArray.push_back(quickslot);

			quickslotCount++;
			QSLOG_INFO("Read in quickslot #%d, info below:", quickslotCount);
			quickslot.PrintInfo();
		}
	}

	QSLOG("Finished reading %s - debugloglevel=%d", filename, mDebugLogVerb);

	return true;
}



bool	CQuickslotManager::WriteConfig(const char* filename)
{

	// Start document
	tinyxml2::XMLDocument xmldoc;
	//tinyxml2::XMLDeclaration* declaration = xmldoc.NewDeclaration();
	//xmldoc.InsertFirstChild(declaration);

	tinyxml2::XMLElement* root = xmldoc.NewElement("vrcustomquickslots");
	xmldoc.InsertFirstChild(root);

	// write options
	tinyxml2::XMLElement* options = xmldoc.NewElement("options");
	options->SetAttribute("defaultradius", mDefaultRadius);
	options->SetAttribute("debugloglevel", mDebugLogVerb);
	options->SetAttribute("hapticfeedback", mHapticOnOverlap);
	options->SetAttribute("alloweditslots", mAllowEditSlots);
	options->SetAttribute("longpresstime", mLongPressTime);
	options->SetAttribute("disablerawapi", mDisableRawAPI);
	options->SetAttribute("controllerradius", mControllerRadius);
	options->SetAttribute("hoverquickslothaptictime", mHoverQuickslotHapticTime);
	options->SetAttribute("activatebutton", mActivateButton);	

	root->InsertFirstChild(options);

	// lambda func for processing command actions and writing to XML (will be used in loop below)
	auto WriteAction = [&](const CQuickslot::CQuickslotCmd& cmd, tinyxml2::XMLElement* qselem, int order)
	{
		tinyxml2::XMLElement* actionElem = nullptr;

		if (cmd.mAction == CQuickslot::EQUIP_ITEM)
		{
			actionElem = xmldoc.NewElement("equipitem");
		}
		else if (cmd.mAction == CQuickslot::EQUIP_OTHER)
		{
			actionElem = xmldoc.NewElement("equipother");
		}
		else if (cmd.mAction == CQuickslot::EQUIP_SPELL)
		{
			actionElem = xmldoc.NewElement("equipspell");
		}
		else if (cmd.mAction == CQuickslot::EQUIP_SHOUT)
		{
			actionElem = xmldoc.NewElement("equipshout");
		}
		else if (cmd.mAction == CQuickslot::CONSOLE_CMD)
		{
			actionElem = xmldoc.NewElement("consolecmd");
			actionElem->SetText(cmd.mConsoleCommand.c_str());
		}
		else if (cmd.mAction == CQuickslot::DROP_OBJECT)
		{
			actionElem = xmldoc.NewElement("dropobject");
			actionElem->SetAttribute("count", cmd.mCount);			
		}
		else
		{
			return;
		}

		if (cmd.mItemType > 0)
		{
			actionElem->SetAttribute("itemtype", cmd.mItemType);

			if (cmd.mItemType == CQuickslot::eItemType::Ingestible)
			{
				actionElem->SetAttribute("potion", cmd.mPotion);
				actionElem->SetAttribute("food", cmd.mFood);
				actionElem->SetAttribute("poison", cmd.mPoison);
			}
			
			if (cmd.mKeyword.length() > 0)
				actionElem->SetAttribute("keyword", cmd.mKeyword.c_str());

			if (cmd.mKeywordNot.length() > 0)
				actionElem->SetAttribute("keywordnot", cmd.mKeywordNot.c_str());
		}

		if (!cmd.mPluginName.empty())
		{
			actionElem->SetAttribute("pluginname", cmd.mPluginName.c_str());
		}

		// Special case for formID, write as hex by string
		if (!cmd.mFormIDList.empty() && cmd.mItemType == 0)
		{
			if (cmd.mFormIDList.size() == 1)
			{
				std::string hexFormIdStr = num2hex(cmd.mFormIDList[0], 8);

				std::string formIdAttribute = "";
				
				/*
				if (hexFormIdStr.length() > 2 && hexFormIdStr[0] == '0' && hexFormIdStr[1] == '0')
				{
					formIdAttribute.append(hexFormIdStr);
				}
				else
				*/

				{
					if (hexFormIdStr.length() == 8)
					{
						DataHandler * dataHandler = DataHandler::GetSingleton();

						ModInfo * modInfo = dataHandler->modList.loadedMods[getHex(hexFormIdStr.substr(0, 2))];

						if (modInfo)
						{
							actionElem->SetAttribute("pluginname", modInfo->name);

							formIdAttribute.append(hexFormIdStr.substr(2, 6));
						}
						else
						{
							formIdAttribute.append(hexFormIdStr);
						}
					}
					else
					{
						formIdAttribute.append(hexFormIdStr);
					}
				}
				actionElem->SetAttribute("formid", formIdAttribute.c_str());
			}
			else
			{
				if (cmd.mFormIdStr.length() > 0)
				{
					actionElem->SetAttribute("formid", cmd.mFormIdStr.c_str());
				}
			}
		}

		if (cmd.mAction != CQuickslot::DROP_OBJECT && cmd.mAction != CQuickslot::CONSOLE_CMD && cmd.mAction != CQuickslot::EQUIP_SHOUT)
		{
			actionElem->SetAttribute("slot", cmd.mSlot);
		}

		qselem->InsertEndChild(actionElem);

	};

	// write in each quickslot
	for (auto it = mQuickslotArray.begin(); it != mQuickslotArray.end(); ++it)
	{
		tinyxml2::XMLElement* quickslotElem = xmldoc.NewElement("quickslot");
		
		// do quickslot settings
		quickslotElem->SetAttribute("name", it->mName.c_str());
		quickslotElem->SetAttribute("radius", it->mRadius);
		// IMPORTANT: write the origin position (pre-transformed) not mPosition
		quickslotElem->SetAttribute("posx", it->mOrigin.x);
		quickslotElem->SetAttribute("posy", it->mOrigin.y);
		quickslotElem->SetAttribute("posz", it->mOrigin.z);

		if (it->mOrder != CQuickslot::eOrderType::DEFAULT)
		{
			quickslotElem->SetAttribute("order", it->mOrder);
		}
		
		// do quickslot actions
		if (it->mOrder == CQuickslot::eOrderType::DEFAULT)
		{
			WriteAction(it->mCommand, quickslotElem, it->mOrder);
			WriteAction(it->mCommandAlt, quickslotElem, it->mOrder);
		}
		else
		{
			// NOTE: new order types only write to "mOtherCommands" and normal command vars remain untouched
			for (int c = 0; c < it->mOtherCommands.size(); c++)
			{
				WriteAction(it->mOtherCommands[c], quickslotElem, it->mOrder);
			}
		}

		// store quickslot
		root->InsertEndChild(quickslotElem);
	}


	tinyxml2::XMLError err = xmldoc.SaveFile(filename);

	if (err != tinyxml2::XMLError::XML_SUCCESS)
	{
		_MESSAGE("Error: %s saving XML doc: %s", xmldoc.ErrorName(), filename);
		_MESSAGE("Full XML Save Error Info: %s", xmldoc.ErrorStr());
		return false;
	}

	QSLOG("Finished writing XML config to %s", filename);

	return true;
}

