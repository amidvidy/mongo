/**
*    Copyright (C) 2014 MongoDB Inc.
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

#include <algorithm>

#include "mongo/base/init.h"
#include "mongo/db/commands.h"
#include "mongo/db/fts/stop_words.h"
#include "mongo/util/string_map.h"

namespace mongo {

    namespace fts {

    class GetStopWordsCommand : public Command {
    public:
        GetStopWordsCommand() : Command("getStopWords") {}

        virtual bool slaveOk() const { return true; }
        virtual bool isWriteCommandForConfigServer() const { return false; }
        virtual void addRequiredPrivileges(const std::string& dbname,
                                           const BSONObj& cmdObj,
                                           std::vector<Privilege>* out) {} // TODO
        virtual bool run(OperationContext* txn, const string& dbname, BSONObj& cmdObj, int, string& errmsg,
                 BSONObjBuilder& result, bool fromRepl) {

            // Add digest
            result.append("stopWordListsDigest",
                          StopWordsLoader::getLoader()->getStopWordListsDigest());

            const StringMap<StopWords*>& stopWords(StopWordsLoader::getLoader()->getStopWords());
            // Order of each language is undefined, but stop words are sorted per language
            for (StringMap<StopWords*>::const_iterator i = stopWords.begin();
                 i != stopWords.end();
                 ++i) {
                std::vector<std::string> sortedWords(i->second->begin(), i->second->end());
                std::sort(sortedWords.begin(), sortedWords.end());
                result.append(i->first, sortedWords);
            }
            return true;
        }

    };
        // Need to wait until after the StopWordsLoader is created
    MONGO_INITIALIZER_WITH_PREREQUISITES(RegisterGetStopWordsCommand, ("LoadStopWords"))
        (InitializerContext* context) {
            // Intentionally leaked, commands register themselves
        new GetStopWordsCommand();

        return Status::OK();
    }

    }
}
