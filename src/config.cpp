#include "quickslots.h"
#include "tinyxml2.h"


bool   CQuickslotManager::ReadConfig(const char* filename)
{

	float defaultRadius = 1.0f;

	tinyxml2::XMLDocument xmldoc;
	tinyxml2::XMLError err = xmldoc.LoadFile(filename);

	if(err != tinyxml2::XMLError::XML_SUCCESS)
	{
		return false;
	}

	tinyxml2::XMLElement* root = xmldoc.RootElement();

	for (tinyxml2::XMLElement* elem = root->FirstChildElement(); elem; elem = elem->NextSiblingElement())
	{
		if (strcmp(elem->Name(), "quickslot") == 0)
		{
			float position[3];
			float radius = defaultRadius;
			const char* cmd = "none";
			const char* slotname = nullptr;

			elem->QueryFloatAttribute("posx", &position[0]);
			elem->QueryFloatAttribute("posy", &position[1]);
			elem->QueryFloatAttribute("posz", &position[2]);
			elem->QueryFloatAttribute("radius", &radius);
			elem->QueryStringAttribute("command", &cmd);
			elem->QueryStringAttribute("name", &slotname);

			CQuickslot quickslot(PapyrusVR::Vector3(position[0], position[1], position[2]), radius, cmd, slotname);
			mQuickslotArray.push_back(quickslot);

		}
	}

	return true;
}