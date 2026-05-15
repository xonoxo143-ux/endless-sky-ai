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



SCENARIO("AI command parser accepts a noop command", "[aihooks]")
{
	const AiHooks::CommandResult result =
		AiHooks::ParseCommandText("{\"type\":\"ai_command\",\"seq\":1,\"action\":\"noop\",\"duration\":1}");

	CHECK(result.hasSeq);
	CHECK(result.seq == 1);
	CHECK(result.hasAction);
	CHECK(result.action == "noop");
	CHECK(result.accepted);
	CHECK(result.reason.empty());
}



SCENARIO("AI command parser rejects invalid JSON", "[aihooks]")
{
	const AiHooks::CommandResult result = AiHooks::ParseCommandText("{");

	CHECK_FALSE(result.accepted);
	CHECK_FALSE(result.hasSeq);
	CHECK(result.reason == "invalid_json");

	const AiHooks::CommandResult malformedField =
		AiHooks::ParseCommandText("{\"type\":\"ai_command\",\"seq\":1abc,\"action\":\"noop\"}");
	CHECK_FALSE(malformedField.accepted);
	CHECK(malformedField.reason == "invalid_json");
}



SCENARIO("AI command parser rejects commands without an action", "[aihooks]")
{
	const AiHooks::CommandResult result = AiHooks::ParseCommandText("{\"type\":\"ai_command\",\"seq\":4}");

	CHECK_FALSE(result.accepted);
	CHECK(result.hasSeq);
	CHECK(result.seq == 4);
	CHECK_FALSE(result.hasAction);
	CHECK(result.reason == "missing_action");
}



SCENARIO("AI command parser rejects unimplemented actions", "[aihooks]")
{
	const AiHooks::CommandResult result =
		AiHooks::ParseCommandText("{\"type\":\"ai_command\",\"seq\":2,\"action\":\"thrust\",\"duration\":60}");

	CHECK_FALSE(result.accepted);
	CHECK(result.hasSeq);
	CHECK(result.seq == 2);
	CHECK(result.hasAction);
	CHECK(result.action == "thrust");
	CHECK(result.reason == "action_not_implemented");
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
	result.hasAction = true;
	result.action = "thrust";
	result.reason = "action_not_implemented";
	AiHooks::EmitCommandResult(result);

	const string output = ReadFile(path);
	CHECK(output.starts_with("{\"type\":\"ai_command_result\","));
	CHECK(output.find("\"telemetry_version\":1") != string::npos);
	CHECK(output.find("\"seq\":2") != string::npos);
	CHECK(output.find("\"accepted\":false") != string::npos);
	CHECK(output.find("\"action\":\"thrust\"") != string::npos);
	CHECK(output.find("\"reason\":\"action_not_implemented\"") != string::npos);
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
	WriteFile(commandPath, "{\"type\":\"ai_command\",\"seq\":1,\"action\":\"noop\",\"duration\":1}");

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
	CHECK(count(output.begin(), output.end(), '\n') == 1);

	AiHooks::Configure({});
	filesystem::remove(commandPath);
	filesystem::remove(resultPath);
}
