#ifndef REBUILD_HXX
#define REBUILD_HXX

#include "command.hxx"

class Rebuild : public CommandBase
{
protected:
    void evaluateInternal(const CommandArguments& arguments) override;
    void executeInternal(const ExecutionArguments& arguments) override;
};

#endif