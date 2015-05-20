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

#include "mongo/rpc/metadata.h"

#include "mongo/client/dbclientinterface.h"
#include "mongo/db/jsobj.h"
#include "mongo/rpc/metadata/server_selectors.h"

namespace mongo {
namespace rpc {
namespace metadata {

    BSONObj empty() {
        return BSONObj();
    }

    Status read(OperationContext* txn, const BSONObj& metadataObj) {
        auto swServerSelectors = ServerSelectors::readFromMetadata(metadataObj);
        if (!swServerSelectors.isOK()) {
            return swServerSelectors.getStatus();
        }
        ServerSelectors::get(txn) = std::move(swServerSelectors.getValue());

        return Status::OK();
    }

    Status write(OperationContext* txn, BSONObjBuilder* metadataBob) {
        auto ssStatus = ServerSelectors::writeToMetadata(ServerSelectors::get(txn), metadataBob);
        if (!ssStatus.isOK()) {
            return ssStatus;
        }
        return Status::OK();
    }

    StatusWith<CommandAndMetadata> upconvertRequest(BSONObj legacyCmdObj, int queryFlags) {
        BSONObjBuilder commandBob;
        BSONObjBuilder metadataBob;

        auto upconvertStatus = ServerSelectors::upconvert(legacyCmdObj,
                                                          queryFlags,
                                                          &commandBob,
                                                          &metadataBob);
        if (!upconvertStatus.isOK()) {
            return upconvertStatus;
        }

        return std::make_tuple(commandBob.obj(), metadataBob.obj());
    }

    StatusWith<LegacyCommandAndFlags> downconvertRequest(BSONObj cmdObj, BSONObj metadata) {
        int legacyQueryFlags = 0;
        BSONObjBuilder legacyCommandBob;

        auto downconvertStatus = ServerSelectors::downconvert(cmdObj,
                                                              metadata,
                                                              &legacyCommandBob,
                                                              &legacyQueryFlags);
        if (!downconvertStatus.isOK()) {
            return downconvertStatus;
        }

        return std::make_tuple(legacyCommandBob.obj(), std::move(legacyQueryFlags));
    }

}  // namespace metadata
}  // namespace rpc
}  // namespace mongo
