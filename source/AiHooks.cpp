/* AiHooks.cpp
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

#include "AiHooks.h"

#include "Planet.h"
#include "PlayerInfo.h"
#include "Ship.h"
#include "System.h"

#include <iostream>
#include <string>

using namespace std;

namespace {
	AiHooks::Options options;

	string JsonEscape(const string &value)
	{
		string result;
		result.reserve(value.size() + 2);
		for(char ch : value)
		{
			switch(ch)
			{
				case '"':
					result += "\\\"";
					break;
				case '\\':
					result += "\\\\";
					break;
				case '\b':
					result += "\\b";
					break;
				case '\f':
					result += "\\f";
					break;
				case '\n':
					result += "\\n";
					break;
				case '\r':
					result += "\\r";
					break;
				case '\t':
					result += "\\t";
					break;
				default:
					result += ch;
					break;
			}
		}
		return result;
	}

	void WriteStringField(ostream &out, const string &name, const string &value, bool comma = true)
	{
		out << '"' << name << "\":\"" << JsonEscape(value) << '"';
		if(comma)
			out << ',';
	}

	void WriteNullableNameField(ostream &out, const string &name, const string *value, bool comma = true)
	{
		out << '"' << name << "\":";
		if(value)
			out << '"' << JsonEscape(*value) << '"';
		else
			out << "null";
		if(comma)
			out << ',';
	}
}



void AiHooks::Configure(const Options &newOptions)
{
	options = newOptions;
	if(options.telemetryEvery < 1)
		options.telemetryEvery = 1;
}



bool AiHooks::TelemetryEnabled()
{
	return options.telemetry;
}



void AiHooks::EmitTelemetry(const PlayerInfo &player, uint64_t tick)
{
	if(!options.telemetry || tick % static_cast<uint64_t>(options.telemetryEvery))
		return;

	const Ship *flagship = player.Flagship();
	const System *system = player.GetSystem();
	const Planet *planet = player.GetPlanet();

	cout << "{\"type\":\"ai_telemetry\",";
	cout << "\"tick\":" << tick << ',';
	WriteStringField(cout, "player_first", player.FirstName());
	WriteStringField(cout, "player_last", player.LastName());

	const string *systemName = system ? &system->TrueName() : nullptr;
	const string *planetName = planet ? &planet->TrueName() : nullptr;
	WriteNullableNameField(cout, "system", systemName);
	WriteNullableNameField(cout, "planet", planetName);
	cout << "\"landed\":" << (planet ? "true" : "false") << ',';

	cout << "\"credits\":" << player.Accounts().Credits() << ',';
	cout << "\"ship_count\":" << player.Ships().size() << ',';

	cout << "\"flagship\":";
	if(flagship)
	{
		const auto &position = flagship->Position();
		const auto &velocity = flagship->Velocity();
		cout << '{';
		WriteStringField(cout, "name", flagship->GivenName());
		WriteStringField(cout, "model", flagship->DisplayModelName());
		cout << "\"position\":{\"x\":" << position.X() << ",\"y\":" << position.Y() << "},";
		cout << "\"velocity\":{\"x\":" << velocity.X() << ",\"y\":" << velocity.Y() << "},";
		cout << "\"angle\":" << flagship->Facing().Degrees() << ',';
		cout << "\"speed\":" << flagship->CurrentSpeed() << ',';
		cout << "\"shields\":" << flagship->Shields() << ',';
		cout << "\"hull\":" << flagship->Hull() << ',';
		cout << "\"fuel\":" << flagship->Fuel() << ',';
		cout << "\"energy\":" << flagship->Energy() << ',';
		cout << "\"disabled\":" << (flagship->IsDisabled() ? "true" : "false") << ',';
		cout << "\"destroyed\":" << (flagship->IsDestroyed() ? "true" : "false");
		cout << '}';
	}
	else
		cout << "null";

	cout << "}" << endl;
}
