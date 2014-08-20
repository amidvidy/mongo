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

#include <algorithm>
#include <boost/filesystem.hpp>
#include <boost/shared_ptr.hpp>
#include <set>
#include <string>
#include <fstream>

#include "mongo/base/init.h"
#include "mongo/db/fts/fts_language.h"
#include "mongo/db/fts/fts_util.h"
#include "mongo/util/log.h"
#include "mongo/util/md5.hpp"
#include "mongo/util/mongoutils/str.h"

namespace mongo {

    namespace fts {

        void loadStopWordMap( StringMap< std::set< std::string > >* m );

        namespace {
            // Configuration options
            bool userConfigurableStopWordsEnabled = false;
            std::map<std::string, std::string> stopWordListPaths;
            // set in MONGO_INITIALIZER
            StopWordsLoader* LOADER;
        }

        StopWords::StopWords() {
        }

        StopWords::StopWords( const std::set<std::string>& words ) {
            for ( std::set<std::string>::const_iterator i = words.begin(); i != words.end(); ++i )
                _words.insert( *i );
        }

        void enableUserConfigurableStopWords(const std::map<std::string, std::string>& paths) {
            userConfigurableStopWordsEnabled = true;
            stopWordListPaths = paths;
        }

        const StopWords* const StopWordsLoader::getStopWords(const FTSLanguage& language) const {
            StringMap<StopWords*>::const_iterator i = _stopWords.find( language.str() );
            if ( i == _stopWords.end() )
                return &_empty;
            return i->second;
        }

        StopWordsLoader* StopWordsLoader::getLoader() {
            invariant(LOADER);
            return LOADER;
        }

        StopWordsLoader* StopWordsLoader::setLoader(StopWordsLoader* loader) {
            std::swap(LOADER, loader);
            return loader;
        }

        const std::string& StopWordsLoader::getStopWordListsDigest() const {
            return _stopWordListsDigest;
        }

        const std::string StopWordsLoader::computeStopWordListsDigest(const StringMap<StopWords*>& stopWords) {
            MD5Builder md5;
            // Sort language names
            std::vector<std::pair<std::string, StopWords*> > languages(stopWords.begin(), stopWords.end());
            // Pair sorts by lexicographic ordering of elements
            // Since each string key is unique, ordering is well defined
            std::sort(languages.begin(), languages.end());

            for (std::vector<std::pair<std::string, StopWords*> >::const_iterator l = languages.begin();
                 l != languages.end();
                 ++l) {

                std::vector<std::string> sortedWords(l->second->_words.begin(), l->second->_words.end());
                std::sort(sortedWords.begin(), sortedWords.end());

                for (std::vector<std::string>::const_iterator sw = sortedWords.begin();
                     sw != sortedWords.end();
                     ++sw) {
                    md5 << *sw;
                }
            }
            return md5.digest();
        }



        Status StopWordsLoader::load() {
            Status status = _load();
            if (status.isOK()) {
                _stopWordListsDigest = computeStopWordListsDigest(_stopWords);
            }
            return status;
        }

        Status StopWordsLoader::_load() {
            StringMap< std::set< std::string > > raw;
            loadStopWordMap( &raw );
            for ( StringMap< std::set< std::string > >::const_iterator i = raw.begin();
                  i != raw.end();
                  ++i ) {
                _stopWords[i->first] = new StopWords( i->second );
            }
            return Status::OK();
        }

        Status UserConfigurableStopWordsLoader::_load() {
            // Load default stop words first
            StopWordsLoader::_load();

            log() << "Loading custom stopwords" << std::endl;

            for (std::map<std::string, std::string>::const_iterator i = stopWordListPaths.begin();
                 i != stopWordListPaths.end();
                 ++i) {

                // lookup language - needed to verify it is valid and to canonicalize it. Also
                // handles language aliases e.g. 'english' vs. 'en'
                StatusWithFTSLanguage swl = FTSLanguage::make(i->first, TEXT_INDEX_VERSION_2);
                if (!swl.getStatus().isOK()) {
                    return swl.getStatus();
                    // TODO: what is the error here, and how can we improve it?
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
                // this will result in a default stop word list getting overwritten
                // so we must free the memory

                // TODO: maybe switch to boost::shared_ptr to avoid the call to delete?
                // or add ability to reset the _words within a StopWords object
                StringMap<StopWords*>::const_iterator old = _stopWords.find(swl.getValue()->str());
                invariant(old != _stopWords.end());
                delete old->second;

                _stopWords[swl.getValue()->str()] = new StopWords(rawWords);
            }
            return Status::OK();
        }

        MONGO_INITIALIZER_WITH_PREREQUISITES(LoadStopWords, ("EndStartupOptionStorage",
                                                             "FTSAllLanguagesRegistered"))
            (InitializerContext* context) {

            if (userConfigurableStopWordsEnabled) {
                LOADER = new UserConfigurableStopWordsLoader(stopWordListPaths);
            }
            else {
                LOADER = new StopWordsLoader();
            }

            return LOADER->load();
    }

    }  // namespace fts
}  // namespace mongo
