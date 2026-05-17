/* test_aihooks.cpp
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

#include "es-test.hpp"

#include "../../../source/AiHooks.h"
#include "../../../source/PlayerInfo.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>

using namespace std;

namespace {
	string ReadFile(const filesystem::path &path)
	{
		ifstream input(path);
		return string(istreambuf_iterator<char>(input), istreambuf_iterator<char>());
	}

	void WriteFile(const filesystem::path &path, const string &content)
	{
		ofstream output(path);
		output << content;
	}
}



SCENARIO("AI telemetry self-test writes valid JSONL to a dedicated file", "[aihooks]")
{
	const filesystem::path path = "aihooks-self-test.jsonl";
	filesystem::remove(path);

	AiHooks::Options options;
	options.telemetry = true;
	options.telemetryFile = path.string();
	AiHooks::Configure(options);
	AiHooks::EmitSelfTest();

	const string output = ReadFile(path);
	CHECK(output.starts_with("{\"type\":\"ai_telemetry_self_test\","));
	CHECK(output.find("\"telemetry_version\":1") != string::npos);
	CHECK(output.find("\"ok\":true") != string::npos);
	CHECK(output.find("\"phase\":\"self_test\"") != string::npos);
	CHECK(output.ends_with("}\n"));

	AiHooks::Configure({});
	filesystem::remove(path);
}



SCENARIO("AI command parser accepts a scoped noop command", "[aihooks]")
{
	const AiHooks::CommandResult result = AiHooks::ParseCommandText(
		"{\"type\":\"ai_command\",\"seq\":1,\"scope\":\"system\",\"command\":\"noop\"}");

	CHECK(result.hasSeq);
	CHECK(result.seq == 1);
	CHECK(result.hasScope);
	CHECK(result.scope == "system");
	CHECK(result.hasCommand);
	CHECK(result.command == "noop");
	CHECK(result.accepted);
	CHECK(result.applied);
	CHECK(result.reason.empty());
}



SCENARIO("AI command parser accepts takeover-stage commands without applying them yet", "[aihooks]")
{
	const AiHooks::CommandResult menu = AiHooks::ParseCommandText(
		"{\"type\":\"ai_command\",\"seq\":2,\"scope\":\"menu\",\"command\":\"new_pilot\"}");
	CHECK(menu.accepted);
	CHECK_FALSE(menu.applied);
	CHECK(menu.scope == "menu");
	CHECK(menu.command == "new_pilot");
	CHECK(menu.reason == "not_applied_yet");

	const AiHooks::CommandResult landed = AiHooks::ParseCommandText(
		"{\"type\":\"ai_command\",\"seq\":3,\"scope\":\"landed\",\"command\":\"launch\"}");
	CHECK(landed.accepted);
	CHECK_FALSE(landed.applied);
	CHECK(landed.scope == "landed");
	CHECK(landed.command == "launch");
	CHECK(landed.reason == "not_applied_yet");

	const AiHooks::CommandResult flight = AiHooks::ParseCommandText(
		"{\"type\":\"ai_command\",\"seq\":4,\"scope\":\"flight\",\"command\":\"control\"}");
	CHECK(flight.accepted);
	CHECK_FALSE(flight.applied);
	CHECK(flight.scope == "flight");
	CHECK(flight.command == "control");
	CHECK(flight.reason == "not_applied_yet");
}



SCENARIO("AI command parser rejects invalid JSON", "[aihooks]")
{
	const AiHooks::CommandResult result = AiHooks::ParseCommandText("{");

	CHECK_FALSE(result.accepted);
	CHECK_FALSE(result.hasSeq);
	CHECK(result.reason == "invalid_json");

	const AiHooks::CommandResult malformedField = AiHooks::ParseCommandText(
		"{\"type\":\"ai_command\",\"seq\":1abc,\"scope\":\"system\",\"command\":\"noop\"}");
	CHECK_FALSE(malformedField.accepted);
	CHECK(malformedField.reason == "invalid_json");
}



SCENARIO("AI command parser rejects commands without a scope or command", "[aihooks]")
{
	const AiHooks::CommandResult missingScope =
		AiHooks::ParseCommandText("{\"type\":\"ai_command\",\"seq\":4,\"command\":\"noop\"}");

	CHECK_FALSE(missingScope.accepted);
	CHECK(missingScope.hasSeq);
	CHECK(missingScope.seq == 4);
	CHECK_FALSE(missingScope.hasScope);
	CHECK(missingScope.reason == "missing_scope");

	const AiHooks::CommandResult missingCommand =
		AiHooks::ParseCommandText("{\"type\":\"ai_command\",\"seq\":5,\"scope\":\"system\"}");

	CHECK_FALSE(missingCommand.accepted);
	CHECK(missingCommand.hasSeq);
	CHECK(missingCommand.seq == 5);
	CHECK(missingCommand.hasScope);
	CHECK(missingCommand.scope == "system");
	CHECK_FALSE(missingCommand.hasCommand);
	CHECK(missingCommand.reason == "missing_command");
}



SCENARIO("AI command parser rejects unsupported command pairs", "[aihooks]")
{
	const AiHooks::CommandResult result = AiHooks::ParseCommandText(
		"{\"type\":\"ai_command\",\"seq\":6,\"scope\":\"flight\",\"command\":\"new_pilot\"}");

	CHECK_FALSE(result.accepted);
	CHECK_FALSE(result.applied);
	CHECK(result.hasSeq);
	CHECK(result.seq == 6);
	CHECK(result.hasScope);
	CHECK(result.scope == "flight");
	CHECK(result.hasCommand);
	CHECK(result.command == "new_pilot");
	CHECK(result.reason == "command_not_supported");
}



SCENARIO("AI command results are written as one JSON object per line", "[aihooks]")
{
	const filesystem::path path = "aihooks-command-result.jsonl";
	filesystem::remove(path);

	AiHooks::Options options;
	options.control = true;
	options.telemetryFile = path.string();
	AiHooks::Configure(options);

	AiHooks::CommandResult result;
	result.hasSeq = true;
	result.seq = 2;
	result.accepted = true;
	result.hasScope = true;
	result.scope = "menu";
	result.hasCommand = true;
	result.command = "new_pilot";
	result.reason = "not_applied_yet";
	AiHooks::EmitCommandResult(result);

	const string output = ReadFile(path);
	CHECK(output.starts_with("{\"type\":\"ai_command_result\","));
	CHECK(output.find("\"telemetry_version\":1") != string::npos);
	CHECK(output.find("\"seq\":2") != string::npos);
	CHECK(output.find("\"accepted\":true") != string::npos);
	CHECK(output.find("\"applied\":false") != string::npos);
	CHECK(output.find("\"scope\":\"menu\"") != string::npos);
	CHECK(output.find("\"command\":\"new_pilot\"") != string::npos);
	CHECK(output.find("\"reason\":\"not_applied_yet\"") != string::npos);
	CHECK(count(output.begin(), output.end(), '\n') == 1);
	CHECK(output.ends_with("}\n"));

	AiHooks::Configure({});
	filesystem::remove(path);
}



SCENARIO("AI command polling does not repeat results for the same command file", "[aihooks]")
{
	const filesystem::path commandPath = "aihooks-command.json";
	const filesystem::path resultPath = "aihooks-command-poll.jsonl";
	filesystem::remove(commandPath);
	filesystem::remove(resultPath);
	WriteFile(commandPath, "{\"type\":\"ai_command\",\"seq\":1,\"scope\":\"system\",\"command\":\"noop\"}");

	AiHooks::Options options;
	options.control = true;
	options.commandFile = commandPath.string();
	options.telemetryFile = resultPath.string();
	AiHooks::Configure(options);

	PlayerInfo player;
	AiHooks::PollCommand(player, 1);
	AiHooks::PollCommand(player, 2);

	const string output = ReadFile(resultPath);
	CHECK(output.find("\"type\":\"ai_command_result\"") != string::npos);
	CHECK(output.find("\"seq\":1") != string::npos);
	CHECK(output.find("\"accepted\":true") != string::npos);
	CHECK(output.find("\"applied\":true") != string::npos);
	CHECK(count(output.begin(), output.end(), '\n') == 1);

	AiHooks::Configure({});
	filesystem::remove(commandPath);
	filesystem::remove(resultPath);
}
