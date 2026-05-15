/* AiHooks.h
Copyright (c) 2026 by Endless Sky AI contributors

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program. If not, see <https://www.gnu.org/licenses/>.
*/

#pragma once

#include <cstdint>
#include <string>

class Command;
class PlayerInfo;



// Minimal AI Lab hook surface.
//
// v0.4 keeps game control opt-in and limited to duration-bounded movement
// commands that are routed through the existing player input command path.
class AiHooks {
public:
	struct Options {
		bool telemetry = false;
		int telemetryEvery = 60;
		std::string telemetryFile;
		bool control = false;
		std::string commandFile;
	};

	struct CommandResult {
		bool hasSeq = false;
		std::int64_t seq = 0;
		bool accepted = false;
		bool hasAction = false;
		std::string action;
		bool hasDuration = false;
		std::int64_t duration = 0;
		bool hasTick = false;
		std::uint64_t tick = 0;
		bool hasActiveUntil = false;
		std::uint64_t activeUntil = 0;
		std::string reason;
	};

public:
	static void Configure(const Options &options);
	static bool TelemetryEnabled();
	static bool ControlEnabled();
	static void EmitTelemetry(const PlayerInfo &player, std::uint64_t tick);
	static void EmitSelfTest();
	static CommandResult ParseCommandText(const std::string &text);
	static void PollCommand(const PlayerInfo &player, std::uint64_t tick, bool inFlight = false);
	static Command CommandForFrame(std::uint64_t tick, bool inFlight);
	static void EmitCommandResult(const CommandResult &result);
};
