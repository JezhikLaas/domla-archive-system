#include "backup.hxx"

using namespace std;

void Backup::evaluateInternal(const CommandArguments& arguments)
{
    args::ArgumentParser parser("");
    parser.Prog(arguments.Program + " backup");
    
    args::HelpFlag help(parser, "help", "Display this help menu", {'h', "help"});
    args::ValueFlag<string> target(parser, "target-directory", "directory, where the backup will be placed", {'t', "target"});
    
    parseCommand(parser, [&]() {
        parser.ParseArgs(arguments.Start, arguments.End);
        Target = bool{target} ? args::get(target) : "";
        if (Target.empty()) throw args::ParseError("no value for 'target' given");
    });
}

void Backup::executeInternal(const ExecutionArguments& arguments)
{
    arguments.Archive->backup(Target, arguments.Context);
}
