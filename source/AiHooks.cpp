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

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

using namespace std;

namespace {
	AiHooks::Options options;
	unique_ptr<ofstream> telemetryFile;

	ostream &TelemetryOutput()
	{
		return telemetryFile ? *telemetryFile : cout;
	}

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

	if(options.telemetryFile.empty())
	{
		const char *envTelemetryFile = getenv("ES_AI_TELEMETRY_FILE");
		if(envTelemetryFile)
			options.telemetryFile = envTelemetryFile;
	}

	telemetryFile.reset();
	if(options.telemetry && !options.telemetryFile.empty())
	{
		telemetryFile = make_unique<ofstream>(options.telemetryFile, ios::out | ios::trunc);
		if(!*telemetryFile)
		{
			cerr << "Unable to open AI telemetry file: " << options.telemetryFile << endl;
			telemetryFile.reset();
		}
	}
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
	const string phase = !player.IsLoaded() ? "menu" : (planet ? "landed" : "flight");
	ostream &out = TelemetryOutput();

	out << "{\"type\":\"ai_telemetry\",";
	out << "\"telemetry_version\":1,";
	out << "\"tick\":" << tick << ',';
	out << "\"has_pilot\":" << (player.IsLoaded() ? "true" : "false") << ',';
	out << "\"has_flagship\":" << (flagship ? "true" : "false") << ',';
	WriteStringField(out, "phase", phase);
	WriteStringField(out, "player_first", player.FirstName());
	WriteStringField(out, "player_last", player.LastName());

	const string *systemName = system ? &system->TrueName() : nullptr;
	const string *planetName = planet ? &planet->TrueName() : nullptr;
	WriteNullableNameField(out, "system", systemName);
	WriteNullableNameField(out, "planet", planetName);
	out << "\"landed\":" << (planet ? "true" : "false") << ',';

	out << "\"credits\":" << player.Accounts().Credits() << ',';
	out << "\"ship_count\":" << player.Ships().size() << ',';

	out << "\"flagship\":";
	if(flagship)
	{
		const auto &position = flagship->Position();
		const auto &velocity = flagship->Velocity();
		out << '{';
		WriteStringField(out, "name", flagship->GivenName());
		WriteStringField(out, "model", flagship->DisplayModelName());
		out << "\"position\":{\"x\":" << position.X() << ",\"y\":" << position.Y() << "},";
		out << "\"velocity\":{\"x\":" << velocity.X() << ",\"y\":" << velocity.Y() << "},";
		out << "\"angle\":" << flagship->Facing().Degrees() << ',';
		out << "\"speed\":" << flagship->CurrentSpeed() << ',';
		out << "\"shields\":" << flagship->Shields() << ',';
		out << "\"hull\":" << flagship->Hull() << ',';
		out << "\"fuel\":" << flagship->Fuel() << ',';
		out << "\"energy\":" << flagship->Energy() << ',';
		out << "\"disabled\":" << (flagship->IsDisabled() ? "true" : "false") << ',';
		out << "\"destroyed\":" << (flagship->IsDestroyed() ? "true" : "false");
		out << '}';
	}
	else
		out << "null";

	out << "}" << endl;
	out.flush();
}
