#ifndef COMMAND_HXX
#define COMMAND_HXX

#include <functional>
#include <string>
#include <vector>
#include <Ice/Ice.h>
#include "lib/Archive.h"
#include "args.hxx"

using FinishedType = std::function<void(void)>;

struct CommandArguments
{
    std::string Program;
    std::vector<std::string>::const_iterator Start;
    std::vector<std::string>::const_iterator End;
};

struct ExecutionArguments
{
    Access::ArchivePrx Archive;
    Ice::Context Context;
    FinishedType OnFinish;
};

class CommandBase
{
protected:
    std::string Error;
    void parseCommand(const args::ArgumentParser& parser, std::function<void()> action);
    virtual void evaluateInternal(const CommandArguments& arguments) = 0;
    virtual void executeInternal(const ExecutionArguments& arguments) = 0;

public:
    void evaluate(const CommandArguments& arguments);
    void execute(const ExecutionArguments& arguments);
    
    const std::string& error()
    {
        return Error;
    }
};

#endif