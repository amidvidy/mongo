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

#include "mongo/util/net/command/command_reply.h"

#include "mongo/util/net/command/command_parse_util.h"

namespace mongo {

    CommandReply::CommandReply(const Message& message)
        : _message(message) {
        char* begin = _message.singleData().data();
        std::size_t length = _message.singleData().dataLen();

        // TODO: overflow?
        _messageEnd = begin + length;

        ConstDataCursor reader{begin};
        const ConstDataCursor rangeEnd{_messageEnd};

        auto parsedMetadata = readBSONObj(reader, rangeEnd);
        uassertStatusOK(parsedMetadata.getStatus());
        _metadata = parsedMetadata.getValue();

        auto parsedCommandArgs = readBSONObj(reader, rangeEnd);
        uassertStatusOK(parsedCommandArgs.getStatus());
        _commandReply = parsedCommandArgs.getValue();

        _outputDocRangeBegin = std::move(reader.view());
    }

    const BSONObj& CommandReply::getMetadata() const {
        return _metadata;
    }

    const BSONObj& CommandReply::getCommandReply() const {
        return _commandReply;
    }

    DocumentRange CommandReply::getOutputDocs() const {
        return DocumentRange{ConstDataCursor{_outputDocRangeBegin},
                             ConstDataCursor{_messageEnd}};
    }

}  // namespace mongo
