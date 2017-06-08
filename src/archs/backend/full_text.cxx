#include "full_text.hxx"
#include <memory>
#include <stdexcept>
#include <boost/algorithm/string/join.hpp>

#pragma warning( push )
#pragma warning( disable : 4251 4275 4800 4996 )
#include <xapian.h>
#pragma warning( pop )

using namespace std;
using namespace boost;
using namespace Archive::Backend;

struct FulltextIndex::Implementation
{

unique_ptr<Xapian::WritableDatabase> Writer;

};

FulltextIndex::FulltextIndex(const SettingsProvider& settings)
: Inner(new Implementation()), Settings_(settings)
{ }

FulltextIndex::~FulltextIndex()
{
    delete Inner;
    Inner = nullptr;
}

void FulltextIndex::Open()
{
    int Flags = Xapian::DB_CREATE_OR_OPEN
                |
                Settings_.DataLocation() == ":memory:" ? Xapian::DB_BACKEND_INMEMORY : Xapian::DB_BACKEND_GLASS
                ;
    auto Name = Settings_.DataLocation() == ":memory:" ? "" : Settings_.FulltextFile();
    
    Inner->Writer = make_unique<Xapian::WritableDatabase>(Name, Flags);
}

void FulltextIndex::Index(const string& id, const vector<string>& words)
{
    if (Inner->Writer == false) throw runtime_error("index is closed");
    
    Xapian::Document Entry;
    Xapian::TermGenerator Generator;
    
    Generator.set_stemmer(Xapian::Stem("de"));
    Generator.set_document(Entry);
    
    for (auto& word : words) {
        Generator.index_text(word);
    }
    
    auto IdTerm = "Q" + id;
    Entry.add_boolean_term(IdTerm);
    Entry.set_data(id);
    Inner->Writer->replace_document(IdTerm, Entry);
}

vector<string> FulltextIndex::Search(const vector<string>& words) const
{
    if (Inner->Writer == false) throw runtime_error("index is closed");
    
    vector<string> Result;
    
    Xapian::QueryParser Parser;
    Xapian::Stem stemmer("de");
    Parser.set_stemmer(stemmer);
    Parser.set_database(*Inner->Writer);
    Parser.set_stemming_strategy(Xapian::QueryParser::STEM_ALL);
    
	auto Expression = join(words, " OR ");
    Xapian::Query Query = Parser.parse_query(Expression);
    Xapian::Enquire Enquire(*Inner->Writer);
    Enquire.set_query(Query);
    
    Xapian::MSet Matches = Enquire.get_mset(0, 10);
    for (Xapian::MSetIterator Match = Matches.begin(); Match != Matches.end(); ++Match) {
        Result.push_back(Match.get_document().get_data());
    }
    
    return Result;
}
