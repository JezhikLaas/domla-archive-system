#include "restore.hxx"

using namespace std;

void Restore::evaluateInternal(const CommandArguments& arguments)
{
    args::ArgumentParser parser("");
    parser.Prog(arguments.Program + " restore");
    
    args::HelpFlag help(parser, "help", "Display this help menu", {'h', "help"});
    args::ValueFlag<string> source(parser, "source-directory", "directory where the backup files are read from", {'s', "source"});
    
    parseCommand(parser, [&]() {
        parser.ParseArgs(arguments.Start, arguments.End);
        Source = bool{source} ? args::get(source) : "";
        if (Source.empty()) throw args::ParseError("no value for 'source' given");
    });
}

void Restore::executeInternal(const ExecutionArguments& arguments)
{
    arguments.Archive->restore(Source, "archc", arguments.Context);
}
