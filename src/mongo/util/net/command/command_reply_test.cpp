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

#include "mongo/util/net/command/command_reply.h"

#include <iterator>
#include <string>
#include <vector>

#include "mongo/bson/bsonobjbuilder.h"
#include "mongo/unittest/unittest.h"
#include "mongo/util/net/message.h"

namespace {

    using namespace mongo;

    TEST(CommandReply, ParseRequiredFields) {
        std::vector<char> opCommandReplyData;
        
        using std::begin;
        using std::end;

        auto writeObj = [&opCommandReplyData](const BSONObj& obj) {
            opCommandReplyData.insert(end(opCommandReplyData), obj.objdata(),
                                 obj.objdata() + obj.objsize());
        };

        BSONObjBuilder metadataBob{};
        metadataBob.append("foo", "bar");
        auto metadata = metadataBob.done();
        writeObj(metadata);

        BSONObjBuilder commandReplyBob{};
        commandReplyBob.append("baz", "garply");
        auto commandReply = commandReplyBob.done();
        writeObj(commandReply);

        BSONObjBuilder outputDoc1Bob{};
        outputDoc1Bob.append("meep", "boop").append("meow", "chirp");
        auto outputDoc1 = outputDoc1Bob.done();
        writeObj(outputDoc1);

        BSONObjBuilder outputDoc2Bob{};
        outputDoc1Bob.append("bleep", "bop").append("woof", "squeak");
        auto outputDoc2 = outputDoc2Bob.done();
        writeObj(outputDoc2);

        Message toSend;
        toSend.setData(dbCommandReply,
                       opCommandReplyData.data(),
                       opCommandReplyData.size());

        CommandReply opCmdReply{toSend};

        ASSERT_EQUALS(opCmdReply.getMetadata(), metadata);
        ASSERT_EQUALS(opCmdReply.getCommandReply(), commandReply);

        auto outputDocRange = opCmdReply.getOutputDocs();
        auto outputDocRangeIter = outputDocRange.begin();

        ASSERT_EQUALS(*outputDocRangeIter, outputDoc1);
        // can't use assert equals since we don't have an op to print the iter.
        ASSERT_FALSE(outputDocRangeIter == outputDocRange.end());
        ++outputDocRangeIter;
        ASSERT_EQUALS(*outputDocRangeIter, outputDoc2);
        ASSERT_FALSE(outputDocRangeIter == outputDocRange.end());
        ++outputDocRangeIter;
        
        ASSERT_TRUE(outputDocRangeIter == outputDocRange.end());
    }
}  // namespace
