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

class PlayerInfo;



// Minimal AI Lab hook surface.
//
// v0.2 is still intentionally read-only. It exports lightweight player/flagship
// telemetry as JSON Lines when explicitly enabled from the command line.
class AiHooks {
public:
	struct Options {
		bool telemetry = false;
		int telemetryEvery = 60;
		std::string telemetryFile;
	};

public:
	static void Configure(const Options &options);
	static bool TelemetryEnabled();
	static void EmitTelemetry(const PlayerInfo &player, std::uint64_t tick);
};
