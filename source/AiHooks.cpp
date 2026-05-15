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

#include "Command.h"
#include "Planet.h"
#include "PlayerInfo.h"
#include "Ship.h"
#include "System.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>

using namespace std;

namespace {
	constexpr int64_t MAX_MOVEMENT_DURATION = 300;

	AiHooks::Options options;
	unique_ptr<ofstream> telemetryFile;
	bool haveLastCommandText = false;
	string lastCommandText;
	bool haveLastProcessedSeq = false;
	int64_t lastProcessedSeq = 0;
	bool missingCommandFileReported = false;

	struct ActiveMovement {
		bool active = false;
		int64_t seq = 0;
		string action;
		uint64_t activeUntil = 0;
		Command command;
	};

	ActiveMovement activeMovement;

	enum class FieldStatus {
		MISSING,
		VALUE,
		INVALID
	};

	struct StringField {
		FieldStatus status = FieldStatus::MISSING;
		string value;
	};

	struct IntegerField {
		FieldStatus status = FieldStatus::MISSING;
		int64_t value = 0;
	};

	ostream &TelemetryOutput()
	{
		return telemetryFile ? *telemetryFile : cout;
	}

	bool EnvFlagEnabled(const char *value)
	{
		if(!value || !*value)
			return false;

		string normalized = value;
		transform(normalized.begin(), normalized.end(), normalized.begin(),
			[](unsigned char ch) { return static_cast<char>(tolower(ch)); });
		return normalized != "0" && normalized != "false" && normalized != "no" && normalized != "off";
	}

	string Trim(const string &text)
	{
		auto first = find_if_not(text.begin(), text.end(),
			[](unsigned char ch) { return isspace(ch); });
		auto last = find_if_not(text.rbegin(), text.rend(),
			[](unsigned char ch) { return isspace(ch); }).base();
		if(first >= last)
			return "";
		return string(first, last);
	}

	void SkipWhitespace(const string &text, size_t &position)
	{
		while(position < text.size() && isspace(static_cast<unsigned char>(text[position])))
			++position;
	}

	bool FindFieldValue(const string &text, const string &name, size_t &position)
	{
		const string quotedName = '"' + name + '"';
		position = text.find(quotedName);
		if(position == string::npos)
			return false;

		position += quotedName.size();
		SkipWhitespace(text, position);
		if(position >= text.size() || text[position] != ':')
		{
			position = string::npos;
			return true;
		}
		++position;
		SkipWhitespace(text, position);
		return true;
	}

	bool IsHex(char ch)
	{
		return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
	}

	bool HasValueTerminator(const string &text, size_t position)
	{
		SkipWhitespace(text, position);
		return position >= text.size() || text[position] == ',' || text[position] == '}';
	}

	StringField ExtractStringField(const string &text, const string &name)
	{
		size_t position = 0;
		if(!FindFieldValue(text, name, position))
			return {};
		if(position == string::npos || position >= text.size() || text[position] != '"')
			return {FieldStatus::INVALID, {}};

		++position;
		string value;
		while(position < text.size())
		{
			const char ch = text[position++];
			if(ch == '"')
			{
				if(!HasValueTerminator(text, position))
					return {FieldStatus::INVALID, {}};
				return {FieldStatus::VALUE, value};
			}
			if(ch == '\\')
			{
				if(position >= text.size())
					return {FieldStatus::INVALID, {}};
				const char escaped = text[position++];
				switch(escaped)
				{
					case '"':
					case '\\':
					case '/':
						value += escaped;
						break;
					case 'b':
						value += '\b';
						break;
					case 'f':
						value += '\f';
						break;
					case 'n':
						value += '\n';
						break;
					case 'r':
						value += '\r';
						break;
					case 't':
						value += '\t';
						break;
					case 'u':
						if(position + 4 > text.size() || !all_of(text.begin() + position, text.begin() + position + 4, IsHex))
							return {FieldStatus::INVALID, {}};
						position += 4;
						value += '?';
						break;
					default:
						return {FieldStatus::INVALID, {}};
				}
			}
			else
			{
				if(static_cast<unsigned char>(ch) < 0x20)
					return {FieldStatus::INVALID, {}};
				value += ch;
			}
		}

		return {FieldStatus::INVALID, {}};
	}

	IntegerField ExtractIntegerField(const string &text, const string &name)
	{
		size_t position = 0;
		if(!FindFieldValue(text, name, position))
			return {};
		if(position == string::npos || position >= text.size())
			return {FieldStatus::INVALID, 0};

		size_t end = position;
		if(text[end] == '-')
			++end;
		while(end < text.size() && isdigit(static_cast<unsigned char>(text[end])))
			++end;
		if(end == position || (text[position] == '-' && end == position + 1))
			return {FieldStatus::INVALID, 0};

		int64_t value = 0;
		const auto result = from_chars(text.data() + position, text.data() + end, value);
		if(result.ec != errc() || result.ptr != text.data() + end)
			return {FieldStatus::INVALID, 0};
		if(!HasValueTerminator(text, end))
			return {FieldStatus::INVALID, 0};
		return {FieldStatus::VALUE, value};
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

	bool IsMovementAction(const string &action)
	{
		return action == "thrust" || action == "turn_left" || action == "turn_right" || action == "brake";
	}

	Command MovementCommandForAction(const string &action)
	{
		Command command;
		if(action == "thrust")
			command |= Command::FORWARD;
		else if(action == "turn_left")
			command |= Command::LEFT;
		else if(action == "turn_right")
			command |= Command::RIGHT;
		else if(action == "brake")
			command |= Command::BACK;
		return command;
	}

	void ClearActiveMovement()
	{
		activeMovement = {};
	}

	void WriteActiveCommandTelemetry(ostream &out, uint64_t tick)
	{
		out << "\"ai_control\":";
		if(activeMovement.active && tick <= activeMovement.activeUntil)
		{
			out << '{';
			out << "\"seq\":" << activeMovement.seq << ',';
			WriteStringField(out, "action", activeMovement.action);
			out << "\"active_until\":" << activeMovement.activeUntil << ',';
			out << "\"remaining\":" << (activeMovement.activeUntil - tick + 1);
			out << '}';
		}
		else
			out << "null";
	}
}



void AiHooks::Configure(const Options &newOptions)
{
	options = newOptions;
	if(options.telemetryEvery < 1)
		options.telemetryEvery = 1;

	if(!options.control && EnvFlagEnabled(getenv("ES_AI_CONTROL")))
		options.control = true;

	if(options.telemetryFile.empty())
	{
		const char *envTelemetryFile = getenv("ES_AI_TELEMETRY_FILE");
		if(envTelemetryFile)
			options.telemetryFile = envTelemetryFile;
	}

	if(options.commandFile.empty())
	{
		const char *envCommandFile = getenv("ES_AI_COMMAND_FILE");
		if(envCommandFile)
			options.commandFile = envCommandFile;
	}

	telemetryFile.reset();
	haveLastCommandText = false;
	lastCommandText.clear();
	haveLastProcessedSeq = false;
	lastProcessedSeq = 0;
	missingCommandFileReported = false;
	ClearActiveMovement();

	if((options.telemetry || options.control) && !options.telemetryFile.empty())
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



bool AiHooks::ControlEnabled()
{
	return options.control;
}



AiHooks::CommandResult AiHooks::ParseCommandText(const string &text)
{
	CommandResult result;
	const string trimmed = Trim(text);
	if(trimmed.size() < 2 || trimmed.front() != '{' || trimmed.back() != '}')
	{
		result.reason = "invalid_json";
		return result;
	}

	const StringField type = ExtractStringField(trimmed, "type");
	if(type.status == FieldStatus::INVALID)
	{
		result.reason = "invalid_json";
		return result;
	}
	if(type.status != FieldStatus::VALUE || type.value != "ai_command")
	{
		result.reason = "invalid_type";
		return result;
	}

	const IntegerField seq = ExtractIntegerField(trimmed, "seq");
	if(seq.status == FieldStatus::INVALID)
	{
		result.reason = "invalid_json";
		return result;
	}
	if(seq.status != FieldStatus::VALUE || seq.value < 0)
	{
		result.reason = "missing_seq";
		return result;
	}
	result.hasSeq = true;
	result.seq = seq.value;

	const StringField action = ExtractStringField(trimmed, "action");
	if(action.status == FieldStatus::INVALID)
	{
		result.reason = "invalid_json";
		return result;
	}
	if(action.status != FieldStatus::VALUE || action.value.empty())
	{
		result.reason = "missing_action";
		return result;
	}
	result.hasAction = true;
	result.action = action.value;

	const IntegerField duration = ExtractIntegerField(trimmed, "duration");
	if(duration.status == FieldStatus::INVALID)
	{
		result.reason = "invalid_json";
		return result;
	}
	if(duration.status == FieldStatus::VALUE)
	{
		result.hasDuration = true;
		result.duration = duration.value;
	}

	if(result.action == "noop")
		result.accepted = true;
	else if(result.action == "stop_control")
		result.accepted = true;
	else if(IsMovementAction(result.action))
	{
		if(!result.hasDuration)
			result.reason = "missing_duration";
		else if(result.duration < 1 || result.duration > MAX_MOVEMENT_DURATION)
			result.reason = "invalid_duration";
		else
			result.accepted = true;
	}
	else
		result.reason = "action_not_implemented";

	return result;
}



void AiHooks::EmitCommandResult(const CommandResult &result)
{
	ostream &out = TelemetryOutput();
	out << "{\"type\":\"ai_command_result\",";
	out << "\"telemetry_version\":1,";
	out << "\"seq\":";
	if(result.hasSeq)
		out << result.seq;
	else
		out << "null";
	out << ',';
	out << "\"accepted\":" << (result.accepted ? "true" : "false") << ',';
	out << "\"action\":";
	if(result.hasAction)
		out << '"' << JsonEscape(result.action) << '"';
	else
		out << "null";
	out << ',';
	out << "\"duration\":";
	if(result.hasDuration)
		out << result.duration;
	else
		out << "null";
	out << ',';
	out << "\"tick\":";
	if(result.hasTick)
		out << result.tick;
	else
		out << "null";
	out << ',';
	out << "\"active_until\":";
	if(result.hasActiveUntil)
		out << result.activeUntil;
	else
		out << "null";
	out << ',';
	out << "\"reason\":";
	if(result.reason.empty())
		out << "null";
	else
		out << '"' << JsonEscape(result.reason) << '"';
	out << "}" << endl;
	out.flush();
}



void AiHooks::PollCommand(const PlayerInfo &, uint64_t tick, bool inFlight)
{
	if(!options.control)
		return;

	if(options.commandFile.empty())
	{
		if(!missingCommandFileReported)
		{
			CommandResult result;
			result.reason = "missing_command_file";
			EmitCommandResult(result);
			missingCommandFileReported = true;
		}
		return;
	}

	ifstream input(options.commandFile);
	if(!input)
	{
		if(!missingCommandFileReported)
		{
			CommandResult result;
			result.reason = "missing_command_file";
			EmitCommandResult(result);
			missingCommandFileReported = true;
		}
		return;
	}
	missingCommandFileReported = false;

	const string text{istreambuf_iterator<char>(input), istreambuf_iterator<char>()};
	if(haveLastCommandText && text == lastCommandText)
		return;
	haveLastCommandText = true;
	lastCommandText = text;

	CommandResult result = ParseCommandText(text);
	result.hasTick = true;
	result.tick = tick;
	if(result.hasSeq && haveLastProcessedSeq && result.seq <= lastProcessedSeq)
	{
		result.accepted = false;
		result.reason = "duplicate_or_old_seq";
	}
	if(result.hasSeq && (!haveLastProcessedSeq || result.seq > lastProcessedSeq))
	{
		haveLastProcessedSeq = true;
		lastProcessedSeq = result.seq;
	}

	if(result.accepted && result.hasAction)
	{
		if(result.action == "stop_control")
			ClearActiveMovement();
		else if(IsMovementAction(result.action))
		{
			if(!inFlight)
			{
				result.accepted = false;
				result.reason = "not_in_flight";
			}
			else
			{
				activeMovement.active = true;
				activeMovement.seq = result.seq;
				activeMovement.action = result.action;
				activeMovement.activeUntil = tick + static_cast<uint64_t>(result.duration) - 1;
				activeMovement.command = MovementCommandForAction(result.action);
				result.hasActiveUntil = true;
				result.activeUntil = activeMovement.activeUntil;
			}
		}
	}

	EmitCommandResult(result);
}



Command AiHooks::CommandForFrame(uint64_t tick, bool inFlight)
{
	if(!options.control || !activeMovement.active)
		return {};

	if(!inFlight || tick > activeMovement.activeUntil)
	{
		ClearActiveMovement();
		return {};
	}

	return activeMovement.command;
}



void AiHooks::EmitTelemetry(const PlayerInfo &player, uint64_t tick, bool inFlight)
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
	out << "\"in_flight\":" << (inFlight ? "true" : "false") << ',';
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

	out << ',';
	WriteActiveCommandTelemetry(out, tick);

	out << "}" << endl;
	out.flush();
}



void AiHooks::EmitSelfTest()
{
	ostream &out = TelemetryOutput();
	out << "{\"type\":\"ai_telemetry_self_test\",";
	out << "\"telemetry_version\":1,";
	out << "\"ok\":true,";
	WriteStringField(out, "phase", "self_test");
	WriteStringField(out, "message", "AI telemetry output path is available", false);
	out << "}" << endl;
	out.flush();
}
