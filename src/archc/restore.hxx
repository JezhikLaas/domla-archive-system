#ifndef RESTORE_HXX
#define RESTORE_HXX

#include "command.hxx"

class Restore : public CommandBase
{
private:
    std::string Source;
protected:
    void evaluateInternal(const CommandArguments& arguments) override;
    void executeInternal(const ExecutionArguments& arguments) override;
};

#endif
