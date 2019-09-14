#ifndef MENUCHECKER_H
#define MENUCHECKER_H

#pragma once
#include "skse64/GameReferences.h"

#include "skse64/PapyrusVM.h"
#include <unordered_map>

#include "quickslotutil.h"

namespace MenuChecker
{
	extern std::vector<std::string> gameStoppingMenus;

	extern std::unordered_map<std::string, bool> menuTypes;

	bool isGameStopped();

	class AllMenuEventHandler : public BSTEventSink <MenuOpenCloseEvent> {
	public:
		virtual EventResult	ReceiveEvent(MenuOpenCloseEvent * evn, EventDispatcher<MenuOpenCloseEvent> * dispatcher);
	};

	extern AllMenuEventHandler menuEvent;
}

#endif