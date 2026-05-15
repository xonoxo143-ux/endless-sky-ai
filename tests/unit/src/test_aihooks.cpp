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

	filesystem::remove(path);
}
