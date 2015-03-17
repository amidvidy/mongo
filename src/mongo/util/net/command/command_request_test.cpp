/**
 * Copyright (C) 2015 MongoDB Inc.
 *
 * This program is free software: you can redistribute it and/or  modify
 * it under the terms of the GNU Affero General Public License, version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * As a special exception, the copyright holders give permission to link the
 * code of portions of this program with the OpenSSL library under certain
 * conditions as described in each individual source file and distribute
 * linked combinations including the program with the OpenSSL library. You
 * must comply with the GNU Affero General Public License in all respects
 * for all of the code used other than as permitted herein. If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so. If you do not
 * wish to do so, delete this exception statement from your version. If you
 * delete this exception statement from all source files in the program,
 * then also delete it in the license file.
 */

#include "mongo/util/net/command/command_request.h"

#include <iterator>
#include <string>
#include <vector>

#include "mongo/bson/bsonobjbuilder.h"
#include "mongo/unittest/unittest.h"
#include "mongo/util/net/message.h"

namespace {

    using namespace mongo;

    TEST(CommandRequest, ParseRequiredFields) {
        std::vector<char> opCommandData;

        auto commandName = std::string{"abababa"};
        auto database = std::string{"ookokokokok"};
        
        BSONObjBuilder metadataBob{};
        metadataBob.append("foo", "bar");
        auto metadata = metadataBob.done();
        
        BSONObjBuilder commandArgsBob{};
        commandArgsBob.append("baz", "garply");
        auto commandArgs = commandArgsBob.done();

        using std::begin;
        using std::end;

        // write database chars + null terminator.
        opCommandData.insert(end(opCommandData), begin(database), end(database));
        opCommandData.push_back('\0');
        // write command name chars + null terminator.
        opCommandData.insert(end(opCommandData), begin(commandName), end(commandName));
        opCommandData.push_back('\0');
        // write metadata
        opCommandData.insert(end(opCommandData), metadata.objdata(),
                             metadata.objdata() + metadata.objsize());
        // write commandArgs
        opCommandData.insert(end(opCommandData), commandArgs.objdata(),
                             commandArgs.objdata() + commandArgs.objsize());
        Message toSend;
        toSend.setData(dbCommand, opCommandData.data(), opCommandData.size());

        CommandRequest opCmd{toSend};

        ASSERT_EQUALS(opCmd.getCommandName(), commandName);
        ASSERT_EQUALS(opCmd.getDatabase(), database);
        ASSERT_EQUALS(opCmd.getMetadata(), metadata);
        ASSERT_EQUALS(opCmd.getCommandArgs(), commandArgs);
    }
}  // namespace
