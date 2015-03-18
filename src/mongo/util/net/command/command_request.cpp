/**
*    Copyright (C) 2015 MongoDB Inc.
*
*    This program is free software: you can redistribute it and/or  modify
*    it under the terms of the GNU Affero General Public License, version 3,
*    as published by the Free Software Foundation.
*
*    This program is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU Affero General Public License for more details.
*
*    You should have received a copy of the GNU Affero General Public License
*    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*    As a special exception, the copyright holders give permission to link the
*    code of portions of this program with the OpenSSL library under certain
*    conditions as described in each individual source file and distribute
*    linked combinations including the program with the OpenSSL library. You
*    must comply with the GNU Affero General Public License in all respects for
*    all of the code used other than as permitted herein. If you modify file(s)
*    with this exception, you may extend this exception to your version of the
*    file(s), but you are not obligated to do so. If you do not wish to do so,
*    delete this exception statement from your version. If you delete this
*    exception statement from all source files in the program, then also delete
*    it in the license file.
*/

#include "mongo/platform/basic.h"

#include "mongo/util/net/command/command_request.h"

#include <string>

#include <utility>

#include "mongo/base/data_cursor.h"
#include "mongo/util/assert_util.h"
#include "mongo/util/net/command/command_parse_util.h"

namespace mongo {

    namespace {
        // None of these include null byte
        std::size_t kMaxDatabaseLength = 63;
        std::size_t kMinDatabaseLength = 1;

        // TODO set with MONGO_INITIALIZERs?
        std::size_t kMinCommandNameLength = 1;
        std::size_t kMaxCommandNameLength = 200;

    }  // namespace

    CommandRequest::CommandRequest(const Message& message)
        : _message(message) {
        char* begin = _message.singleData().data();
        std::size_t length = _message.singleData().dataLen();

        // TODO: can we be safer here w.r.t overflow?
        _messageEnd = begin + length;

        // TODO: uassert that we are greater than the minimum OP_COMMAND length.

        ConstDataCursor reader{begin};
        const ConstDataCursor rangeEnd{_messageEnd};

        auto parsedDatabase = readString(reader, rangeEnd,
                                         kMinDatabaseLength,
                                         kMaxDatabaseLength);
        if (!parsedDatabase.getStatus().isOK()) {
        };
        _database = parsedDatabase.getValue();

        auto parsedCommandName = readString(reader, rangeEnd,
                                            kMinCommandNameLength,
                                            kMaxCommandNameLength);
        uassertStatusOK(parsedCommandName.getStatus());
        _commandName = parsedCommandName.getValue();

        // I could use DocumentRange here, but leaving it this way will
        // make it a bit simpler to provide better error messages for invalid
        // required fields.
        auto parsedMetadata = readBSONObj(reader, rangeEnd);
        uassertStatusOK(parsedMetadata.getStatus());
        _metadata = parsedMetadata.getValue();

        auto parsedCommandArgs = readBSONObj(reader, rangeEnd);
        uassertStatusOK(parsedCommandArgs.getStatus());
        _commandArgs = parsedCommandArgs.getValue();

        _inputDocRangeBegin = std::move(reader.view());
    }

    StringData CommandRequest::getDatabase() const {
        return _database;
    }

    StringData CommandRequest::getCommandName() const {
        return _commandName;
    }

    const BSONObj& CommandRequest::getMetadata() const {
        return _metadata;
    }

    const BSONObj& CommandRequest::getCommandArgs() const {
        return _commandArgs;
    }

    DocumentRange CommandRequest::getInputDocs() const {
        return DocumentRange{ConstDataCursor{_inputDocRangeBegin},
                             ConstDataCursor{_messageEnd}};
    }

}  // namespace mongo
