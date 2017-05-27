#ifndef BACKUP_HXX
#define BACKUP_HXX

#include "command.hxx"

class Backup : public CommandBase
{
private:
    std::string Target;
protected:
    void evaluateInternal(const CommandArguments& arguments) override;
    void executeInternal(const ExecutionArguments& arguments) override;
};

#endif