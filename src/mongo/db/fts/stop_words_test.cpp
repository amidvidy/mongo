// stop_words_test.cpp

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

#include <set>

#include "mongo/db/fts/fts_spec.h"
#include "mongo/db/fts/stop_words.h"
#include "mongo/unittest/unittest.h"
#include "mongo/util/md5.hpp"
#include "mongo/util/scopeguard.h"

namespace mongo {
    namespace fts {

        TEST( English, Basic1 ) {
            const StopWords* englishStopWords = StopWordsLoader::getLoader()->getStopWords( languageEnglishV2 );
            ASSERT( englishStopWords->isStopWord( "the" ) );
            ASSERT( !englishStopWords->isStopWord( "computer" ) );
        }

        class MockStopWordsLoader : public StopWordsLoader {
        public:
            void setStopWordsFor(const std::string& language, const std::set<std::string>& words) {
                _stopWords[language] = new StopWords(words);
            }
        protected:
            virtual Status _load() { return Status::OK(); }
        };

        TEST(Loader, DigestsCorrectly1) {
            MockStopWordsLoader loader;

            std::set<std::string> words;
            words.insert("x");
            words.insert("y");
            words.insert("z");

            loader.setStopWordsFor("a", words);
            loader.setStopWordsFor("b", words);
            loader.setStopWordsFor("c", words);

            // setLoader returns old loader
            ON_BLOCK_EXIT(StopWordsLoader::setLoader, StopWordsLoader::setLoader(&loader));
            ASSERT_OK(StopWordsLoader::getLoader()->load());

            MD5Builder d;
            d << "xyz" << "xyz" << "xyz";
            ASSERT_EQUALS(StopWordsLoader::getLoader()->getStopWordListsDigest(), d.digest());
        }

        //  This will probably not work correctly on windows
        //  Even though we don't support these languages, their UTF-8 code points should still
        //  digest deterministically
        TEST(Loader, DigestsUnicodeCorrectly) {
            MockStopWordsLoader loader;

            std::set<std::string> hebrew;
            hebrew.insert("א");
            hebrew.insert("ב");
            hebrew.insert("ג");
            hebrew.insert("ד");
            loader.setStopWordsFor("hebrew", hebrew);

            std::set<std::string> japanese;  // using katakana (syllables)
            japanese.insert("ヰ");
            japanese.insert("ヱ");
            japanese.insert("ヲ");
            japanese.insert("ン");
            loader.setStopWordsFor("japanese", japanese);
            
            std::set<std::string> emoji;  // couldn't resist
            emoji.insert("😋");
            emoji.insert("😌");
            emoji.insert("😍");
            emoji.insert("😏");
            loader.setStopWordsFor("emoji", emoji);

            ON_BLOCK_EXIT(StopWordsLoader::setLoader, StopWordsLoader::setLoader(&loader));
            ASSERT_OK(StopWordsLoader::getLoader()->load());
            
            MD5Builder d;

            d << "😋"  << "😌" << "😍" << "😏";
            d << "א" << "ב" << "ג" << "ד"; 
            d << "ヰ" << "ヱ" << "ヲ" << "ン";

            ASSERT_EQUALS(StopWordsLoader::getLoader()->getStopWordListsDigest(), d.digest());
        }
    }
}
