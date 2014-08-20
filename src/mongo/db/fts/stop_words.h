// stop_words.h

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


#pragma once

#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <set>
#include <string>

#include "mongo/platform/unordered_set.h"
#include "mongo/util/string_map.h"

namespace mongo {

    namespace fts {
        class FTSLanguage;

        void enableUserConfigurableStopWords( const std::map<std::string, std::string>& paths );
        //const bool userConfigurableStopWordsEnabled();

        // Represents stop words for a particular language
        class StopWords {
            friend class StopWordsLoader;
        public:
            StopWords();
            StopWords( const std::set<std::string>& words );
            ~StopWords() {}
            bool isStopWord( const std::string& word ) const {
                return _words.count( word ) > 0;
            }

            size_t numStopWords() const { return _words.size(); }

        private:
            unordered_set<std::string> _words;
        };

        // Responsible for loading stopwords. Used to reduce dependence on global state and to increase
        // testability
        class StopWordsLoader : public boost::noncopyable {
        public:
            StopWordsLoader() {}
            virtual ~StopWordsLoader() {}

            Status load();

            const StopWords* const getStopWords(const FTSLanguage& language) const;

            // Instance is set during static initialization
            static StopWordsLoader* getLoader();
            // For testing
            static StopWordsLoader* setLoader(StopWordsLoader* loader);

            const std::string& getStopWordListsDigest() const;

            static const std::string computeStopWordListsDigest(const StringMap<StopWords*>& stopWords);
        protected:
            StringMap<StopWords*> _stopWords;
            virtual Status _load();
        private:
            StopWords _empty;
            std::string _stopWordListsDigest;
        };

        class UserConfigurableStopWordsLoader : public StopWordsLoader {
        public:
            UserConfigurableStopWordsLoader(const std::map<std::string, std::string>& stopWordLists)
                : _stopWordLists(stopWordLists)
            {}
            virtual ~UserConfigurableStopWordsLoader() {}
        protected:
            virtual Status _load();
            // Paths to user configured stop words
            const std::map<std::string, std::string> _stopWordLists;
        };

    }
}
