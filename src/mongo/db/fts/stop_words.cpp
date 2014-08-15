// stop_words.cpp

/**
*    Copyright (C) 2012 10gen Inc.
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

#include "mongo/db/fts/stop_words.h"

#include <boost/filesystem.hpp>
#include <boost/shared_ptr.hpp>
#include <set>
#include <string>
#include <fstream>

#include "mongo/db/fts/fts_language.h"
#include "mongo/db/fts/fts_util.h"
#include "mongo/base/init.h"
#include "mongo/util/log.h"
#include "mongo/util/mongoutils/str.h"
#include "mongo/util/string_map.h"

namespace mongo {

    namespace fts {

        void loadStopWordMap( StringMap< std::set< std::string > >* m );

        namespace {
            std::map<std::string, std::string> stopWordListPaths;
            StringMap<boost::shared_ptr<StopWords> > STOP_WORDS;
            StopWords* empty = NULL;
        }


        StopWords::StopWords(){
        }

        StopWords::StopWords( const std::set<std::string>& words ) {
            for ( std::set<std::string>::const_iterator i = words.begin(); i != words.end(); ++i )
                _words.insert( *i );
        }

        const StopWords* StopWords::getStopWords( const FTSLanguage& language ) {
            StringMap<boost::shared_ptr<StopWords> >::const_iterator i = STOP_WORDS.find( language.str() );
            if ( i == STOP_WORDS.end() )
                return empty;
            return i->second.get();
        }

        // static, called during option parsing
        void StopWords::setStopWordListPaths( const std::map<std::string, std::string>& paths ) {
            log() << "Setting stopWordListPaths" << std::endl;
            stopWordListPaths = paths;
        }

        MONGO_INITIALIZER_WITH_PREREQUISITES(StopWords, ("EndStartupOptionStorage",
                                                         "FTSAllLanguagesRegistered"))
            (InitializerContext* context) {

            
            empty = new StopWords();
            // Load
            StringMap< std::set< std::string > > raw;
            loadStopWordMap( &raw );
            for ( StringMap< std::set< std::string > >::const_iterator i = raw.begin();
                  i != raw.end();
                  ++i ) {
                STOP_WORDS[i->first] = boost::shared_ptr<StopWords>(new StopWords( i->second ));
            }

            for (std::map<std::string, std::string>::const_iterator i = stopWordListPaths.begin();
                 i != stopWordListPaths.end();
                 ++i) {

                log() << i->first << ":" << i->second << std::endl;
                
                // lookup language
                StatusWithFTSLanguage swl = FTSLanguage::make(i->first, TEXT_INDEX_VERSION_2);
                if (!swl.getStatus().isOK()) {
                    return swl.getStatus();
                }
                
                if (!boost::filesystem::is_regular_file(i->second)) {
                    return Status(ErrorCodes::BadValue,
                                  str::stream() << "Specified invalid file: " << i->second
                                  << " for " << i->first << " stop words");
                }

                // TODO ensure file encoding is UTF8 or ASCII
                // TODO unicode safety in general
                std::ifstream stopwords(i->second.c_str());

                if (!stopwords.is_open()) {
                    return Status(ErrorCodes::BadValue,
                                  str::stream() << "Unable to open file: " << i->second
                                  << " for " << i->first << " stop words");
                }

                log() << "Loading " << i->first << std::endl;

                std::set<std::string> rawWords;

                for (std::string stopword; std::getline(stopwords, stopword); ) {
                    log() << "Got " << stopword << std::endl;
                    rawWords.insert(stopword);
                }
                // use canonical name as key
                // this can result in a default stop word list getting overwritteen,
                // so we use shared_ptr to ensure no memory leaks
                STOP_WORDS[swl.getValue()->str()] = boost::shared_ptr<StopWords>(new StopWords(rawWords));
            }

            return Status::OK();
        }

    }

}
