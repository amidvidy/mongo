/*
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

#pragma once

#include <boost/optional.hpp>

#include "mongo/base/disallow_copying.h"
#include "mongo/client/read_preference.h"
#include "mongo/db/operation_context.h"

namespace mongo {
    class BSONObj;
    class BSONObjBuilder;
    class Status;
    template <typename T> class StatusWith;

namespace rpc {
namespace metadata {

    /**
     * This class comprises the request metadata fields that concern server selection, that is,
     * the conditions on which servers can execute this operation.
     */
    class ServerSelectors {
        MONGO_DISALLOW_COPYING(ServerSelectors);
    public:
        static const OperationContext::Decoration<ServerSelectors> get;

        // TODO: Remove when StatusWith supports default-constructible types (SERVER-18007).
        ServerSelectors() = default;

        ServerSelectors(ServerSelectors&&) = default;

        ServerSelectors& operator=(ServerSelectors&&) = default;

        /**
         * Loads ServerSelectors from a metadata object, and stores it on this OperationContext.
         */
        static StatusWith<ServerSelectors> readFromMetadata(const BSONObj& metadataObj);

        /**
         * Writes this operation's ServerSelectors to a metadata object.
         */
        static Status writeToMetadata(const ServerSelectors& ss, BSONObjBuilder* metadataBob);

        /**
         * Rewrites the ServerSelectors from the legacy OP_QUERY format to the metadata
         * object format. In particular, if secondaryOk is set, this will set QueryOption_SlaveOk
         * on the legacyQueryFlags. If a readPreference is set, the legacy command will be wrapped
         * in a $query element (if it is not already), and a top-level $readPreference field will
         * be set on the command.
         */
        static Status downconvert(const BSONObj& command,
                                  const BSONObj& metadata,
                                  BSONObjBuilder* legacyQueryBob,
                                  int* legacyQueryFlags);

        /**
         * Rewrites the ServerSelectors from the legacy OP_QUERY format to the metadata
         * object format.
         */
        static Status upconvert(const BSONObj& legacyCommand,
                                const int legacyQueryFlags,
                                BSONObjBuilder* commandBob,
                                BSONObjBuilder* metadataBob);
        /**
         * Returns true if this operation has been explicitly overridden to run on a secondary.
         * This replaces previous usage of QueryOption_SlaveOk.
         */
        bool secondaryOk() const;

        /**
         * Returns the ReadPreference associated with this operation. See
         * mongo/client/read_preference.h for further details.
         */
        const boost::optional<ReadPreferenceSetting>& readPreference() const;

    private:
        ServerSelectors(bool secondaryOk, boost::optional<ReadPreferenceSetting> readPreference);

        bool _secondaryOk{false};
        boost::optional<ReadPreferenceSetting> _readPreference{};
    };

}  // namespace metadata
}  // namespace rpc
}  // namespace mongo
