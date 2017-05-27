#include "command.hxx"
#include <exception>
#include <sstream>

using namespace std;

void CommandBase::parseCommand(const args::ArgumentParser& parser, function<void()> action)
{
    try {
        action();
    }
    catch (args::Help) {
        stringstream buffer;
        buffer << parser;
        Error = buffer.str();
    }
    catch (args::ParseError e) {
        stringstream buffer;
        buffer << e.what() << std::endl;
        buffer << parser;
        Error = buffer.str();
    }
}

void CommandBase::evaluate(const CommandArguments& arguments)
{
    try {
        evaluateInternal(arguments);
    }
    catch(exception& error) {
        Error = error.what();
    }
    catch(...) {
        Error = "unknown error";
    }
}

void CommandBase::execute(const ExecutionArguments& arguments)
{
    string finishError;
    try {
        executeInternal(arguments);
        if (arguments.OnFinish != nullptr) {
            try { arguments.OnFinish(); }
            catch(exception& error){ finishError = error.what(); }
            catch(...){ finishError = "unknown error"; }
        }
    }
    catch(...) {
        if (arguments.OnFinish != nullptr) try { arguments.OnFinish(); } catch(...){ }
        throw;
    }
    
    if (finishError.empty() == false) throw runtime_error(finishError);
}
