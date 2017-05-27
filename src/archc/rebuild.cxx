#include "rebuild.hxx"

using namespace std;

void Rebuild::evaluateInternal(const CommandArguments& arguments)
{
    args::ArgumentParser parser("");
    parser.Prog(arguments.Program + " rebuild");
    
    args::HelpFlag help(parser, "help", "Display this help menu", {'h', "help"});
    
    parseCommand(parser, [&]() {
        parser.ParseArgs(arguments.Start, arguments.End);
    });
}

void Rebuild::executeInternal(const ExecutionArguments& arguments)
{
    arguments.Archive->rebuildFulltext(arguments.Context);
}
