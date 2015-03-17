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
#include <algorithm>
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

        // T must satisfy EqualityComparable concept
        // passing T by value because this will only be used with primitives.

        // TODO: the existence of this helper indicates a need for a DataRange,
        // or BoundedDataCursor primitive.
        // returns cursor
        template<typename T>
        StatusWith<std::size_t> skipUntil(ConstDataCursor& reader,
                                          const ConstDataCursor rangeEnd,
                                          T sentinel,
                                          std::size_t minValidToSkip,
                                          std::size_t maxValidToSkip) {
            // We can stop looking at the end of the range, or the last valid amount
            // of Ts to skip, whichever comes first.
            std::size_t maxToSkipOrEnd = std::min(((rangeEnd.view() - reader.view()) / sizeof(T)),
                                                  maxValidToSkip);

            for (std::size_t skipped = 0; skipped < maxToSkipOrEnd; ++skipped) {
                if (reader.readNative<T>() == sentinel) {
                    if (skipped < minValidToSkip) {
                        return StatusWith<std::size_t>(ErrorCodes::FailedToParse);
                    }
                    return StatusWith<std::size_t>(skipped);
                }
                reader.skip<T>();
            }
            return StatusWith<std::size_t>(ErrorCodes::FailedToParse);
        }

        StatusWith<StringData> readString(ConstDataCursor& reader,
                                          const ConstDataCursor rangeEnd,
                                          std::size_t minLength,
                                          std::size_t maxLength) {

            const char* string = reader.view();
            auto length = skipUntil<char>(reader, rangeEnd, '\0', minLength, maxLength);
            if (!length.getStatus().isOK()) {
                // TODO: better error message.
                return StatusWith<StringData>{ErrorCodes::FailedToParse, "Couldn't parse string"};
            }
            reader.skip<char>(); // skip null byte
            return StatusWith<StringData>{StringData(string, length.getValue())};
        }
    }  // namespace

    CommandRequest::CommandRequest(const Message& message)
        : _message(message) {
        char* begin = _message.singleData().data();
        std::size_t length = _message.singleData().dataLen();

        // TODO: can we be safer here w.r.t overflow?
        char* end = begin + length;

        // TODO: uassert that we are greater than the minimum OP_COMMAND length.
        ConstDataCursor reader{begin};
        const ConstDataCursor rangeEnd{end};

        auto parsedDatabase = readString(reader, rangeEnd,
                                         kMinDatabaseLength,
                                         kMaxDatabaseLength);
        uassertStatusOK(parsedDatabase.getStatus());
        _database = parsedDatabase.getValue();

        auto parsedCommandName = readString(reader, rangeEnd,
                                            kMinCommandNameLength,
                                            kMaxCommandNameLength);
        uassertStatusOK(parsedCommandName.getStatus());
        _commandName = parsedCommandName.getValue();

        auto parsedMetadata = readBSONObj(reader, rangeEnd);
        uassertStatusOK(parsedMetadata.getStatus());
        _metadata = parsedMetadata.getValue();

        auto parsedCommandArgs = readBSONObj(reader, rangeEnd);
        uassertStatusOK(parsedCommandArgs.getStatus());
        _commandArgs = parsedCommandArgs.getValue();
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

}  // namespace mongo
